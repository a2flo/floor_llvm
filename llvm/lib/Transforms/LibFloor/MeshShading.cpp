//===-- MeshShading.cpp - mesh shading transformations ----------*- C++ -*-===//
//
//  Flo's Open libRary (floor)
//  Copyright (C) 2004 - 2026 Florian Ziesche
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; version 2 of the License only.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License along
//  with this program; if not, write to the Free Software Foundation, Inc.,
//  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//
//===----------------------------------------------------------------------===//
//
// This implements mesh shading related lowering for Metal and Vulkan.
//
//===----------------------------------------------------------------------===//

#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <memory>
#include <string>
#include <array>
#include <vector>
#include <span>

#include "llvm/Pass.h"
#include "llvm/PassRegistry.h"
#include "llvm/InitializePasses.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Transforms/LibFloor.h"
#include "llvm/Transforms/LibFloor/FloorUtils.h"
#include "llvm/Transforms/LibFloor/MetalTypes.h"
using namespace llvm;

namespace llvm {

struct MeshShadingBasePass : public FunctionPass, InstVisitor<MeshShadingBasePass> {
	friend class InstVisitor<MeshShadingBasePass>;
	
	explicit MeshShadingBasePass(char &ID) : FunctionPass(ID) {}
	
	bool runOnFunction(Function &F) override;
	
	using InstVisitor<MeshShadingBasePass>::visit;
	void visit(Instruction& I);
	void visitCallBase(CallBase& CB);
	
	virtual void handle_set_vertex(CallBase& CB, Value* mesh_arg, Value* position_idx_arg, Value* obj_arg,
								   const std::span<MESH_ATTRIBUTE> attrs) = 0;
	virtual void handle_set_primitive(CallBase& CB, Value* mesh_arg, Value* position_idx_arg, Value* obj_arg,
									  const std::span<MESH_ATTRIBUTE> attrs) = 0;
	
protected:
	Module* M { nullptr };
	LLVMContext* ctx { nullptr };
	Function* func { nullptr };
	std::shared_ptr<IRBuilder<>> builder;
	bool was_modified { false };
	bool is_metal { false };
	
	//! iterates over an object (vertex/primitive) and runs a callback for each contained field
	//! NOTE: to make things easier, Vulkan uses the same native type names as the Metal backend
	template <typename F>
	void for_each_obj_value(CallBase& CB, Value* obj_arg, const std::span<MESH_ATTRIBUTE> attrs, F&& cb) {
		assert(obj_arg->getType()->isPointerTy());
		auto obj_ld = builder->CreateLoad(obj_arg->getType()->getPointerElementType(), obj_arg);
		auto obj_type = obj_ld->getType();
		if (auto st_type = dyn_cast_or_null<StructType>(obj_type); st_type) {
			for (uint32_t st_idx = 0, out_idx = 0, st_count = st_type->getNumElements(); st_idx < st_count; ++st_idx) {
				const auto attr = attrs[st_idx];
				auto field = builder->CreateExtractValue(obj_ld, st_idx);
				auto field_type_name = metal::get_metal_native_typename(field->getType(), false, false);
				if (!field_type_name) {
					ctx->emitError(&CB, "invalid mesh object field type");
					return;
				}
				cb(field, out_idx, attr, *field_type_name);
				
				// Metal: only advance output index when this is not an attribute
				// Vulkan: we don't differentiate these and simply used fully indexed outputs
				if (!is_metal || attr == MESH_ATTRIBUTE::NONE) {
					++out_idx;
				}
			}
		} else {
			assert(isa<FixedVectorType>(obj_type));
			auto type_name = metal::get_metal_native_typename(obj_type, false, false);
			if (!type_name) {
				ctx->emitError(&CB, "invalid mesh object type");
				return;
			}
			cb(obj_ld, 0u, attrs[0], *type_name);
		}
	}
	
};

bool MeshShadingBasePass::runOnFunction(Function &F) {
	// exit if empty function
	if (F.empty()) {
		return false;
	}
	
	// reset
	M = F.getParent();
	ctx = &M->getContext();
	func = &F;
	builder = std::make_shared<llvm::IRBuilder<>>(*ctx);
	was_modified = false;
	
	visit(F);
	return was_modified;
}

void MeshShadingBasePass::visit(Instruction& I) {
	InstVisitor<MeshShadingBasePass>::visit(I);
}

void MeshShadingBasePass::visitCallBase(CallBase& CB) {
	const auto func = CB.getCalledFunction();
	if (!func) {
		return;
	}
	
	const auto full_func_name = func->getName();
	if (full_func_name.startswith("floor.mesh_")) {
		builder->SetInsertPoint(&CB);
		
		const bool is_set_vertex = full_func_name.startswith("floor.mesh_set_vertex.");
		const bool is_set_primitive = full_func_name.startswith("floor.mesh_set_primitive.");
		if (!is_set_vertex && !is_set_primitive) {
			ctx->emitError(&CB, "unknown mesh shading function");
			return;
		}
		if (CB.arg_size() != 4) {
			ctx->emitError(&CB, "invalid mesh shading argument count");
			return;
		}
		
		auto mesh_arg = CB.getArgOperand(0);
		assert(mesh_arg->getType()->isPointerTy() &&
			   mesh_arg->getType()->getPointerElementType()->isStructTy() &&
			   !mesh_arg->getType()->getPointerElementType()->isSized());
		auto position_idx_arg = CB.getArgOperand(1);
		assert(position_idx_arg->getType()->isIntegerTy());
		
		auto obj_arg = CB.getArgOperand(2);
		assert(obj_arg->getType()->isPointerTy());
		uint32_t attr_count = 0u;
		if (auto st_type = dyn_cast_or_null<StructType>(obj_arg->getType()->getPointerElementType()); st_type) {
			attr_count = st_type->getNumElements();
		} else {
			attr_count = 1u;
		}
		
		auto attrs_arg = CB.getArgOperand(3);
		std::vector<MESH_ATTRIBUTE> attributes(attr_count, MESH_ATTRIBUTE::NONE);
		if (const auto const_attrs = dyn_cast_or_null<ConstantDataArray>(attrs_arg); const_attrs) {
			assert(const_attrs->getNumElements() == attr_count);
			for (uint32_t attr_idx = 0u; attr_idx < attr_count; ++attr_idx) {
				attributes[attr_idx] = (MESH_ATTRIBUTE)const_attrs->getElementAsInteger(attr_idx);
			}
		} else if (const auto const_null = dyn_cast_or_null<Constant>(attrs_arg); const_null && const_null->isZeroValue()) {
			// nop, already NONE
		} else {
			ctx->emitError(&CB, "invalid mesh shading attributes");
			return;
		}
		
		if (is_set_vertex) {
			handle_set_vertex(CB, mesh_arg, position_idx_arg, obj_arg, attributes);
		} else if (is_set_primitive) {
			handle_set_primitive(CB, mesh_arg, position_idx_arg, obj_arg, attributes);
		}
		assert(CB.use_empty());
		CB.eraseFromParent();
		was_modified = true;
	}
}

} // namespace llvm

struct MetalMesh : public MeshShadingBasePass {
	static char ID;
	
	MetalMesh() : MeshShadingBasePass(ID) {
		initializeMetalMeshPass(*PassRegistry::getPassRegistry());
		is_metal = true;
	}
	
	void handle_set_vertex(CallBase& CB, Value* mesh_arg, Value* position_idx_arg, Value* obj_arg,
						   const std::span<MESH_ATTRIBUTE> attrs) override {
		for_each_obj_value(CB, obj_arg, attrs, [this, &CB, &mesh_arg, &position_idx_arg](Value* field, const uint32_t idx,
																						 const MESH_ATTRIBUTE attr,
																						 const std::string& type_name) {
			std::string func_name;
			SmallVector<llvm::Type*, 4> func_arg_types;
			SmallVector<llvm::Value*, 4> func_args;
			switch (attr) {
				case MESH_ATTRIBUTE::POSITION:
					assert(field->getType()->isVectorTy());
					func_name = "air.set_position_mesh";
					func_arg_types = { mesh_arg->getType(), position_idx_arg->getType(), field->getType() };
					func_args = { mesh_arg, position_idx_arg, field };
					break;
				case MESH_ATTRIBUTE::POINT_SIZE:
					assert(field->getType()->isFloatTy());
					func_name = "air.set_point_size_mesh";
					func_arg_types = { mesh_arg->getType(), position_idx_arg->getType(), field->getType() };
					func_args = { mesh_arg, position_idx_arg, field };
					break;
				default:
					func_name = "air.set_vertex_data_mesh." + type_name;
					func_arg_types = { mesh_arg->getType(), builder->getInt32Ty(), position_idx_arg->getType(), field->getType() };
					func_args = { mesh_arg, ConstantInt::get(builder->getInt32Ty(), idx), position_idx_arg, field };
					break;
			}
			libfloor_utils::create_call(CB, *M, func_name, "", builder->getVoidTy(), func_arg_types, func_args, {
				.is_argmem_only = true,
				.is_nounwind = true,
				.is_will_return = true,
			});
		});
	}
	
	void handle_set_primitive(CallBase& CB, Value* mesh_arg, Value* position_idx_arg, Value* obj_arg,
							  const std::span<MESH_ATTRIBUTE> attrs) override {
		for_each_obj_value(CB, obj_arg, attrs, [this, &CB, &mesh_arg, &position_idx_arg](Value* field, const uint32_t idx,
																						 const MESH_ATTRIBUTE attr,
																						 const std::string& type_name) {
			std::string func_name;
			SmallVector<llvm::Type*, 4> func_arg_types;
			SmallVector<llvm::Value*, 4> func_args;
			switch (attr) {
				case MESH_ATTRIBUTE::CULLED:
					assert(field->getType()->isIntegerTy());
					func_name = "air.set_primitive_culled_mesh";
					func_arg_types = { mesh_arg->getType(), position_idx_arg->getType(), field->getType() };
					func_args = { mesh_arg, position_idx_arg, field };
					break;
				default:
					func_name = "air.set_primitive_data_mesh." + type_name;
					func_arg_types = { mesh_arg->getType(), builder->getInt32Ty(), position_idx_arg->getType(), field->getType() };
					func_args = { mesh_arg, ConstantInt::get(builder->getInt32Ty(), idx), position_idx_arg, field };
					break;
			}
			libfloor_utils::create_call(CB, *M, func_name, "", builder->getVoidTy(), func_arg_types, func_args, {
				.is_argmem_only = true,
				.is_nounwind = true,
				.is_will_return = true,
			});
		});
	}
};

struct VulkanMesh : public MeshShadingBasePass {
	static char ID;
	
	VulkanMesh() : MeshShadingBasePass(ID) {
		initializeVulkanMeshPass(*PassRegistry::getPassRegistry());
	}
	
	void handle_set_vertex(CallBase& CB, Value* mesh_arg, Value* position_idx_arg, Value* obj_arg,
						   const std::span<MESH_ATTRIBUTE> attrs) override {
		for_each_obj_value(CB, obj_arg, attrs, [this, &CB, &position_idx_arg](Value* field, const uint32_t idx,
																			  const MESH_ATTRIBUTE attr,
																			  const std::string& type_name) {
			std::string func_name = "floor.mesh.set_vertex_data." + type_name;
			SmallVector<llvm::Type*, 4> func_arg_types {
				builder->getInt32Ty(), builder->getInt32Ty(), position_idx_arg->getType(), field->getType()
			};
			SmallVector<llvm::Value*, 4> func_args {
				ConstantInt::get(builder->getInt32Ty(), uint32_t(attr)), ConstantInt::get(builder->getInt32Ty(), idx),
				position_idx_arg, field
			};
			libfloor_utils::create_call(CB, *M, func_name, "", builder->getVoidTy(), func_arg_types, func_args, {
				.is_argmem_only = true,
				.is_nounwind = true,
				.is_will_return = true,
			});
		});
	}
	
	void handle_set_primitive(CallBase& CB, Value* mesh_arg, Value* position_idx_arg, Value* obj_arg,
							  const std::span<MESH_ATTRIBUTE> attrs) override {
		for_each_obj_value(CB, obj_arg, attrs, [this, &CB, &position_idx_arg](Value* field, const uint32_t idx,
																			  const MESH_ATTRIBUTE attr,
																			  const std::string& type_name) {
			std::string func_name = "floor.mesh.set_primitive_data." + type_name;
			SmallVector<llvm::Type*, 4> func_arg_types {
				builder->getInt32Ty(),builder->getInt32Ty(), position_idx_arg->getType(), field->getType()
			};
			SmallVector<llvm::Value*, 4> func_args {
				ConstantInt::get(builder->getInt32Ty(), uint32_t(attr)), ConstantInt::get(builder->getInt32Ty(), idx),
				position_idx_arg, field
			};
			libfloor_utils::create_call(CB, *M, func_name, "", builder->getVoidTy(), func_arg_types, func_args, {
				.is_argmem_only = true,
				.is_nounwind = true,
				.is_will_return = true,
			});
		});
	}
};

char MetalMesh::ID = 0;
INITIALIZE_PASS_BEGIN(MetalMesh, "MetalMesh", "MetalMesh Pass", false, false)
INITIALIZE_PASS_END(MetalMesh, "MetalMesh", "MetalMesh Pass", false, false)
FunctionPass *llvm::createMetalMeshPass() {
	return new MetalMesh();
}

char VulkanMesh::ID = 0;
INITIALIZE_PASS_BEGIN(VulkanMesh, "VulkanMesh", "VulkanMesh Pass", false, false)
INITIALIZE_PASS_END(VulkanMesh, "VulkanMesh", "VulkanMesh Pass", false, false)
FunctionPass *llvm::createVulkanMeshPass() {
	return new VulkanMesh();
}
