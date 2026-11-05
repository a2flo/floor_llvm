//===- PointerBCFixup.cpp - fix/improve pointer bitcasts pass -------------===//
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
// This pass fixes/improves unfortunate pointer bitcasts.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/BasicAliasAnalysis.h"
#include "llvm/Analysis/GlobalsModRef.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/InitializePasses.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/CallingConv.h"
#include "llvm/IR/ConstantFolder.h"
#include "llvm/IR/ConstantRange.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Pass.h"
#include "llvm/PassRegistry.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Transforms/LibFloor.h"
#include "llvm/Transforms/LibFloor/FloorUtils.h"
#include "llvm/Transforms/LibFloor/MetalTypes.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include <algorithm>
#include <cstdarg>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <array>
using namespace llvm;

#define DEBUG_TYPE "PointerBCFixup"

#if 1
#define DBG(x)
#else
#define DBG(x) x
#endif

namespace {
	// PointerBCFixup
	struct PointerBCFixup : public FunctionPass, InstVisitor<PointerBCFixup> {
		friend class InstVisitor<PointerBCFixup>;
		
		static char ID; // Pass identification, replacement for typeid
		
		Module* M { nullptr };
		const DataLayout* DL { nullptr };
		LLVMContext* ctx { nullptr };
		Function* func { nullptr };
		bool is_metal { false };
		bool is_vulkan { false };
		bool is_kernel_func { false };
		bool is_vertex_func { false };
		bool is_fragment_func { false };
		bool is_task_func { false };
		bool is_mesh_func { false };
		bool was_modified { false };
		ConstantFolder folder {};
		
		// gather pointer bitcast instructions
		std::vector<BitCastInst*> ptr_bc_instrs;
		struct constant_vector_rejection {
			StringRef reason;
			StringRef opcode;
		};
		std::unordered_map<const BitCastInst*, constant_vector_rejection> constant_vector_rejections;
		
		PointerBCFixup() :
		FunctionPass(ID) {
			initializePointerBCFixupPass(*PassRegistry::getPassRegistry());
		}
		
		StringRef getPassName() const override {
			return "PointerBCFixup";
		}
		
		void getAnalysisUsage(AnalysisUsage &AU) const override {
			AU.addRequired<AAResultsWrapperPass>();
			AU.addRequired<GlobalsAAWrapperPass>();
			AU.addRequired<AssumptionCacheTracker>();
			AU.addRequired<TargetLibraryInfoWrapperPass>();
			AU.addRequired<AssumptionCacheTracker>();
			AU.addRequired<DominatorTreeWrapperPass>();
			AU.addRequired<TargetTransformInfoWrapperPass>();
		}
		
		bool runOnFunction(Function &F) override {
			is_kernel_func = F.getCallingConv() == CallingConv::FLOOR_KERNEL;
			is_vertex_func = F.getCallingConv() == CallingConv::FLOOR_VERTEX;
			is_fragment_func = F.getCallingConv() == CallingConv::FLOOR_FRAGMENT;
			is_task_func = F.getCallingConv() == CallingConv::FLOOR_TASK;
			is_mesh_func = F.getCallingConv() == CallingConv::FLOOR_MESH;
			if (!is_kernel_func &&
				!is_vertex_func &&
				!is_fragment_func &&
				!is_task_func &&
				!is_mesh_func) {
				return false;
			}
			
			//
			M = F.getParent();
			DL = &M->getDataLayout();
			ctx = &M->getContext();
			func = &F;
			ptr_bc_instrs.clear();
			
			is_metal = (Triple(M->getTargetTriple()).getArch() == Triple::ArchType::air64);
			is_vulkan = (Triple(M->getTargetTriple()).getArch() == Triple::ArchType::spir64 &&
						 Triple(M->getTargetTriple()).getEnvironment() == Triple::EnvironmentType::Vulkan);
			
			// no need to do anything here if not Metal/Vulkan
			if (!is_vulkan && !is_metal) {
				return false;
			}
			
			// gather all stuff
			was_modified = false;
			visit(F);
			
			// fix invalid pointer bitcasts
			if (!ptr_bc_instrs.empty()) {
				was_modified |= fix_pointer_bitcasts();
			}
			
			return was_modified;
		}
		
		// InstVisitor overrides...
		using InstVisitor<PointerBCFixup>::visit;
		void visit(Instruction& I) {
			InstVisitor<PointerBCFixup>::visit(I);
		}
		
		void visitBitCastInst(BitCastInst& BC) {
			if (BC.getSrcTy()->isPointerTy() && BC.getDestTy()->isPointerTy()) {
				ptr_bc_instrs.emplace_back(&BC);
				return;
			}
			assert(!BC.getSrcTy()->isPointerTy() && !BC.getDestTy()->isPointerTy()); // just in case ...
		}
		
		struct struct_element_t {
			std::vector<llvm::Value*> indices;
			std::vector<uint32_t> const_indices;
			llvm::Type* type { nullptr };
		};
		static std::vector<struct_element_t> get_struct_elements(BitCastInst& BC, llvm::Type* in_st_type, LLVMContext& ctx) {
			assert(in_st_type->isStructTy());
			auto st_type = cast<StructType>(in_st_type);
			
			std::vector<struct_element_t> elems;
			std::vector<llvm::Value*> indices;
			std::vector<uint32_t> const_indices;
			for (;;) {
				const auto elem_count = st_type->getNumElements();
				assert(elem_count > 0);
				if (elem_count == 1 && st_type->getElementType(0)->isStructTy()) {
					// recurse
					const_indices.emplace_back(0u);
					indices.emplace_back(ConstantInt::get(llvm::Type::getInt32Ty(ctx), const_indices.back()));
					st_type = cast<StructType>(st_type->getElementType(0));
					continue;
				}
				
				for (uint32_t i = 0; i < elem_count; ++i) {
					auto elem_type = st_type->getElementType(i);
					if (elem_type->isStructTy()) {
						ctx.emitError(&BC, "invalid pointer bitcast: can't handle nested structs");
						return {};
					}
					auto elem_indices = indices;
					const_indices.emplace_back(i);
					elem_indices.emplace_back(ConstantInt::get(llvm::Type::getInt32Ty(ctx), const_indices.back()));
					elems.emplace_back(struct_element_t {
						.indices = std::move(elem_indices),
						.const_indices = std::move(const_indices),
						.type = elem_type,
					});
				}
				break;
			}
			return elems;
		}
		
		static inline void extract_vec_struct_ldst(Value*& src_value, PointerType*& src_ptr_type, Type*& src_type,
												   GetElementPtrInst*& src_gep,
												   uint64_t& src_size, const uint64_t dst_size,
												   const DataLayout* DL, std::vector<Instruction*>& cleanup_instrs,
												   bool& is_vec_struct_ldst) {
			auto gep = dyn_cast_or_null<GetElementPtrInst>(src_value);
			if (!gep || !gep->getSourceElementType()->isStructTy()) {
				return;
			}
			llvm::Instruction* src = gep;
			
			const auto src_elem_type = gep->getSourceElementType();
			SmallVector<Value*> indices;
			for (auto& idx : gep->indices()) {
				indices.emplace_back(idx);
			}
			
			for (size_t i = 1, count = indices.size(); i < count; ++i) {
				// last index must be 0 for this to work
				const auto last_idx = dyn_cast_or_null<ConstantInt>(indices.back());
				if (!last_idx || last_idx->getZExtValue() != 0) {
					break;
				}
				indices.pop_back();
				
				auto higher_src_type = GetElementPtrInst::getIndexedType(src_elem_type, indices);
				if (higher_src_type) {
					if (auto new_src_size = DL->getTypeStoreSize(higher_src_type).getFixedSize(); new_src_size >= dst_size) {
						// found it, create a new GEP with the current indices
						src_gep = GetElementPtrInst::Create(gep->getSourceElementType(), gep->getOperand(0),
															indices, src->getName() + ".adj", src);
						src_gep->setIsInBounds(gep->isInBounds());
						src_gep->setDebugLoc(gep->getDebugLoc());
						
						// set new src
						auto new_src_ptr_type = PointerType::get(higher_src_type, src_ptr_type->getPointerAddressSpace());
						src_ptr_type = new_src_ptr_type;
						src_size = new_src_size;
						src_type = higher_src_type;
						src = src_gep;
						cleanup_instrs.emplace_back(src);
						
						is_vec_struct_ldst = true;
						break;
					}
				}
			}
		}
		
		std::optional<bool> fix_pointer_bitcast_with_loads(BitCastInst& BC, Function& F, const std::vector<LoadInst*>& loads) {
			if (!isa<Instruction>(BC.getOperand(0))) {
				return {};
			}
			
			const auto dst_type = cast<PointerType>(BC.getDestTy())->getPointerElementType();
			const auto dst_size = DL->getTypeStoreSize(dst_type).getFixedSize();
			
			auto src_ptr_type = cast<PointerType>(BC.getSrcTy());
			auto src_type = cast<PointerType>(BC.getSrcTy())->getPointerElementType();
			auto src_size = DL->getTypeStoreSize(src_type).getFixedSize();
			auto src = BC.getOperand(0);
			GetElementPtrInst* src_gep = nullptr;
			
			std::vector<Instruction*> cleanup_instrs { &BC };
			
			// direct struct<->vector bitcast+load?
			bool is_vec_struct_load = ((src_type->isVectorTy() && dst_type->isStructTy()) ||
									   (src_type->isStructTy() && dst_type->isVectorTy()));
			
			// indirect struct->vector bitcast+load?
			// src might already point to the lowest element of a struct -> need to go up
			if (!is_vec_struct_load && src_size < dst_size && dst_type->isVectorTy()) {
				extract_vec_struct_ldst(src, src_ptr_type, src_type, src_gep, src_size, dst_size, DL, cleanup_instrs, is_vec_struct_load);
			}
			
			if (is_vec_struct_load) {
				if (loads.size() > 1) {
					ctx->emitError(&BC, "invalid pointer bitcast: can't replace more than one load");
					return {};
				}
				if (!src_gep) {
					src_gep = dyn_cast_or_null<GetElementPtrInst>(src);
					cleanup_instrs.emplace_back(src_gep);
				}
				
				// if either side is a struct and the other is a vector type,
				// we need to do a (full) extraction and insertion of elements
				const auto src_st_elements = (src_type->isStructTy() ? get_struct_elements(BC, src_type, *ctx) : std::vector<struct_element_t> {});
				const auto dst_st_elements = (dst_type->isStructTy() ? get_struct_elements(BC, dst_type, *ctx) : std::vector<struct_element_t> {});
				const auto src_elem_count = (src_type->isStructTy() ? src_st_elements.size() : size_t(cast<VectorType>(src_type)->getElementCount().getFixedValue()));
				const auto dst_elem_count = (dst_type->isStructTy() ? dst_st_elements.size() : size_t(cast<VectorType>(dst_type)->getElementCount().getFixedValue()));
				if (src_elem_count == 0 || dst_elem_count == 0) {
					ctx->emitError(&BC, "invalid pointer bitcast: invalid destination or source vector type (no or invalid elements)");
					return {};
				}
				if (dst_elem_count > src_elem_count) {
					ctx->emitError(&BC, "invalid pointer bitcast: destination vector type has more elements than the source vector type");
					return {};
				}
				
				//
				auto& ld = loads[0];
				auto insertion_point = ld;
				
				// only do this for as many dst elements that we have
				llvm::Value* new_dst = UndefValue::get(dst_type);
				for (uint32_t i = 0, count = uint32_t(dst_elem_count); i < count; ++i) {
					// extract
					llvm::Value* src_elem = nullptr;
					if (!src_st_elements.empty()) {
						// extract struct elem
						const auto& elem = src_st_elements[i];
						GetElementPtrInst* elem_gep = nullptr;
						if (src_gep) {
							SmallVector<Value*> indices;
							for (auto& idx : src_gep->indices()) {
								indices.emplace_back(idx);
							}
							for (auto& idx : elem.indices) {
								indices.emplace_back(idx);
							}
							elem_gep = GetElementPtrInst::Create(src_gep->getSourceElementType(), src_gep->getOperand(0), indices, "", insertion_point);
						} else {
							// NOTE/TODO: untested path!
							elem_gep = GetElementPtrInst::Create(src_type, src, elem.indices, "", insertion_point);
						}
						elem_gep->setIsInBounds(true);
						elem_gep->setDebugLoc(ld->getDebugLoc());
						auto elem_ld = new LoadInst(elem.type, elem_gep, "", false, insertion_point);
						src_elem = elem_ld;
					} else {
						// extract vector elem
						auto extract_elem = ExtractElementInst::Create(src, ConstantInt::get(llvm::Type::getInt32Ty(*ctx), i), "", insertion_point);
						extract_elem->setDebugLoc(ld->getDebugLoc());
						src_elem = extract_elem;
					}
					
					// insert
					if (!dst_st_elements.empty()) {
						// NOTE/TODO: untested path!
						// insert struct elem
						const auto& elem = dst_st_elements[i];
						auto insert_val = InsertValueInst::Create(new_dst, src_elem, elem.const_indices, "", insertion_point);
						insert_val->setDebugLoc(ld->getDebugLoc());
						new_dst = insert_val;
					} else {
						// insert vector elem
						auto insert_elem = InsertElementInst::Create(new_dst, src_elem, ConstantInt::get(llvm::Type::getInt32Ty(*ctx), i), "", insertion_point);
						insert_elem->setDebugLoc(ld->getDebugLoc());
						new_dst = insert_elem;
					}
				}
				
				// finally: replace load with newly created/loaded construct
				ld->replaceAllUsesWith(new_dst);
				ld->eraseFromParent();
			} else {
				// -> non vector<->struct(-array) BC+load
				
				// if the source size is larger, the source pointer likely orignates from a struct GEP at a higher level
				// -> drill down
				assert(src_size >= dst_size && "source must always be >= destination");
				if (src_size > dst_size) {
					src_gep = dyn_cast_or_null<GetElementPtrInst>(src);
					if (!src_gep) {
						ctx->emitError(&BC, "invalid pointer bitcast: invalid src -> dst cast (can't replace non-GEP src)");
						return {};
					}
					cleanup_instrs.emplace_back(src_gep);
					
					SmallVector<llvm::Value*> indices;
					for (auto& idx : src_gep->indices()) {
						indices.emplace_back(idx);
					}
					
					for (;;) {
						auto src_st_type = dyn_cast_or_null<StructType>(src_type);
						if (!src_st_type) {
							if (auto arr_type = dyn_cast_or_null<ArrayType>(src_type); arr_type) {
								// if this is an array, add one zero index / indirection, the element type must be a struct type either way
								src_type = arr_type->getElementType();
								src_st_type = dyn_cast_or_null<StructType>(src_type);
								indices.emplace_back(ConstantInt::get(llvm::Type::getInt32Ty(*ctx), 0u));
							}
						}
						if (!src_st_type) {
							ctx->emitError(&BC, "invalid pointer bitcast: invalid src -> dst cast (src is not a struct type)");
							return {};
						}
						indices.emplace_back(ConstantInt::get(llvm::Type::getInt32Ty(*ctx), 0u));
						src_type = src_st_type->getStructElementType(0);
						src_size = DL->getTypeStoreSize(src_type).getFixedSize();
						if (src_size == dst_size) {
							// if this is still a struct type, do another round (struct containing a single element)
							// also: if this matches the dst type (for some reason, which shouldn't occur ...), use it straight away
							if (src_type->isStructTy() && src_type != dst_type) {
								assert(cast<StructType>(src_type)->getStructNumElements() == 1);
								continue;
							}
							
							auto new_gep = GetElementPtrInst::Create(src_gep->getSourceElementType(), src_gep->getOperand(0), indices,
																	 src_gep->getName(), src_gep);
							new_gep->setIsInBounds(src_gep->isInBounds());
							new_gep->setDebugLoc(src_gep->getDebugLoc());
							src = new_gep;
							break;
						}
					}
				}
				
				// neither type must be a struct type, or if either is a struct type, the other must not be integer
				assert((!src_type->isStructTy() && !dst_type->isStructTy()) ||
					   ((src_type->isStructTy() && !dst_type->isIntegerTy()) ||
						(!src_type->isIntegerTy() && dst_type->isStructTy())));
				
				// fix up by emitting a load of the original (src) pointer, then bitcast to the dst type
				// NOTE: I would expect there to only be one load, but handle all just in case
				for (auto& ld : loads) {
					auto src_ld = new LoadInst(src_type, src, ld->getName(), ld->isVolatile(), ld->getAlign(), ld);
					src_ld->copyMetadata(*ld);
					src_ld->setDebugLoc(ld->getDebugLoc());
					
					auto src_bc = new BitCastInst(src_ld, dst_type, ld->getName() + ".bc", ld);
					src_bc->setDebugLoc(ld->getDebugLoc());
					
					// cleanup
					ld->replaceAllUsesWith(src_bc);
					ld->eraseFromParent();
				}
			}
			
			// cleanup
			std::sort(cleanup_instrs.begin(), cleanup_instrs.end());
			if (auto last_iter = std::unique(cleanup_instrs.begin(), cleanup_instrs.end()); last_iter != cleanup_instrs.end()) {
				cleanup_instrs.erase(last_iter, cleanup_instrs.end());
			}
			for (auto& cleanup_instr : cleanup_instrs) {
				if (cleanup_instr && cleanup_instr->users().empty() && cleanup_instr->uses().empty()) {
					cleanup_instr->eraseFromParent();
				}
			}
			
			return true;
		}
		
		std::optional<bool> fix_pointer_bitcast_with_stores(BitCastInst& BC, Function& F, const std::vector<StoreInst*>& stores) {
			const auto dst_type = cast<PointerType>(BC.getDestTy())->getPointerElementType();
			const auto dst_size = DL->getTypeStoreSize(dst_type).getFixedSize();
			
			auto src_ptr_type = cast<PointerType>(BC.getSrcTy());
			auto src_type = cast<PointerType>(BC.getSrcTy())->getPointerElementType();
			auto src_size = DL->getTypeStoreSize(src_type).getFixedSize();
			//auto src = cast<Instruction>(BC.getOperand(0));
			auto src = BC.getOperand(0);
			GetElementPtrInst* src_gep = nullptr;
			
			std::vector<Instruction*> cleanup_instrs { &BC };
			
			// direct struct<->vector bitcast+store?
			bool is_vec_struct_store = ((src_type->isVectorTy() && dst_type->isStructTy()) ||
										(src_type->isStructTy() && dst_type->isVectorTy()));
			
			// indirect struct->vector bitcast+store?
			// src might already point to the lowest element of a struct -> need to go up
			if (!is_vec_struct_store && src_size < dst_size && dst_type->isVectorTy()) { // TODO: is this correct?
				extract_vec_struct_ldst(src, src_ptr_type, src_type, src_gep, src_size, dst_size, DL, cleanup_instrs, is_vec_struct_store);
			}
			
			if (is_vec_struct_store) {
				if (!src_gep) {
					src_gep = dyn_cast_or_null<GetElementPtrInst>(src);
					if (!src_gep) {
						return false /* no modification */;
					}
					cleanup_instrs.emplace_back(src_gep);
				}
				
				for (auto& store : stores) {
					if (store->getOperand(1)->getType() == src_gep->getType()) {
						store->setOperand(1, src_gep);
					} else {
#if 0 // keep as-is for now
						ctx->emitError(&BC, "invalid pointer bitcast: case not implemented yet");
						return {};
#endif
					}
				}
			} else {
				// -> non vector<->struct BC+store
				if (!is_vulkan) { // only needed for Vulkan
					return false /* no modification */;
				}
				
				// if the source size is larger, the source pointer likely orignates from a struct GEP at a higher level
				// -> drill down
				if (src_size > dst_size) { // TODO: is this correct?
					src_gep = dyn_cast_or_null<GetElementPtrInst>(src);
					if (!src_gep) {
						ctx->emitError(&BC, "invalid pointer bitcast: invalid src -> dst cast (can't replace non-GEP src)");
						return {};
					}
					cleanup_instrs.emplace_back(src_gep);
					
					SmallVector<llvm::Value*> indices;
					for (auto& idx : src_gep->indices()) {
						indices.emplace_back(idx);
					}
					
					for (;;) {
						auto src_st_type = dyn_cast_or_null<StructType>(src_type);
						if (!src_st_type) {
							ctx->emitError(&BC, "invalid pointer bitcast: invalid src -> dst cast (src is not a struct type)");
							return {};
						}
						indices.emplace_back(ConstantInt::get(llvm::Type::getInt32Ty(*ctx), 0u));
						src_type = src_st_type->getStructElementType(0);
						src_size = DL->getTypeStoreSize(src_type).getFixedSize();
						if (src_size == dst_size) {
							// if this is still a struct type, do another round (struct containing a single element)
							// also: if this matches the dst type (for some reason, which shouldn't occur ...), use it straight away
							if (src_type->isStructTy() && src_type != dst_type) {
								assert(cast<StructType>(src_type)->getStructNumElements() == 1);
								continue;
							}
							
							auto new_gep = GetElementPtrInst::Create(src_gep->getSourceElementType(), src_gep->getOperand(0), indices,
																	 src_gep->getName(), src_gep);
							new_gep->setIsInBounds(src_gep->isInBounds());
							new_gep->setDebugLoc(src_gep->getDebugLoc());
							src = new_gep;
							break;
						}
					}
					// TODO: implement this
					assert(false && "unhandled bitcast store replacement");
				} else if (src_size < dst_size) {
					[[maybe_unused]] bool is_multi_store = false; // just for informational purposes right now
					if (stores.size() > 1) {
						// generally we don't support multi-stores here (yet), because they are generally too complex too handle,
						// however, for certain trivial cases where we only replace simple integer stores (both src and dst are integer) we can allow this
						if (!src_type->isIntegerTy() || !dst_type->isIntegerTy()) {
							ctx->emitError(&BC, "invalid pointer bitcast: can't replace more than one store");
							return {};
						} else {
							is_multi_store = true;
						}
					}
					
					src_gep = dyn_cast_or_null<GetElementPtrInst>(src);
					if (!src_gep) {
						ctx->emitError(&BC, "invalid pointer bitcast: invalid src -> dst cast (can't replace non-GEP src)");
						return {};
					}
					
					// if this happens, a larger value/type is stored to a pointer of lower bit depth (e.g. i64 into i8*)
					// -> split up value into parts that fit into the used pointer element type
					assert((dst_size % src_size) == 0u && "uneven store using a larger value type into a smaller pointer type");
					const auto split_count = (dst_size / src_size);
					const auto src_bitness = src_size * 8u;
					
					SmallVector<Value*, 8> src_gep_indices;
					for (auto& idx : src_gep->indices()) {
						src_gep_indices.emplace_back(idx);
					}
					
					for (const auto& store : stores) {
						auto store_value = store->getValueOperand();
						Type* store_value_int_type = nullptr;
						if (!store_value->getType()->isIntegerTy()) {
							// if the source value is not an integer, we need to bitcast it to an integer
							// NOTE/TODO: this probably won't work in all cases ...
							assert(dst_size <= 8 && "source value is too large");
							store_value_int_type = IntegerType::get(*ctx, dst_size * 8u);
							store_value = new BitCastInst(store_value, store_value_int_type, "store_value_bc", store);
						} else {
							store_value_int_type = store_value->getType();
						}
						
						cleanup_instrs.emplace_back(store);
						for (uint32_t split_idx = 0u; split_idx < split_count; ++split_idx) {
							Value* shifted_value = nullptr;
							GetElementPtrInst* store_gep = nullptr;
							if (split_idx > 0) {
								// right shift by bitness * split-index
								shifted_value = BinaryOperator::CreateLShr(store_value,
																		   ConstantInt::get(store_value_int_type, split_idx * src_bitness),
																		   "st_split_shift", store);
								
								// advance GEP by one
								auto adj_gep_indices = src_gep_indices;
								auto last_gep_idx = adj_gep_indices.back();
								Value* adv_idx = nullptr;
								// in certain cases (e.g. struct indices), this already must be a constant here, so we can't rely on it being constant folded later
								auto adv_constant = ConstantInt::get(last_gep_idx->getType(), split_idx);
								if (auto folded_adv_idx = folder.FoldAdd(last_gep_idx, adv_constant); folded_adv_idx) {
									adv_idx = folded_adv_idx;
								} else {
									adv_idx = BinaryOperator::CreateAdd(last_gep_idx, adv_constant, "st_src_gep_idx_adv", src_gep);
								}
								adj_gep_indices[adj_gep_indices.size() - 1] = adv_idx;
								
								store_gep = llvm::GetElementPtrInst::Create(src_gep->getSourceElementType(), src_gep->getPointerOperand(),
																			adj_gep_indices, "st_src_gep_adv", src_gep);
								if (src_gep->isInBounds()) {
									store_gep->setIsInBounds();
								}
								store_gep->copyMetadata(*src_gep);
								store_gep->setDebugLoc(src_gep->getDebugLoc());
							} else {
								// first iteration: use value and GEP as is
								shifted_value = store_value;
								store_gep = src_gep;
							}
							auto trunc_shifted_value = new TruncInst(shifted_value, src_type, "st_trunc_split_shift", store);
							auto repl_st = new StoreInst(trunc_shifted_value, store_gep, store->isVolatile(),
														 store->getAlign(), store->getOrdering(), store->getSyncScopeID(),
														 store);
							repl_st->copyMetadata(*store);
							repl_st->setDebugLoc(store->getDebugLoc());
						}
					}
				} else { // src_size == dst_size
					// TODO: implement this?
					// -> ignore for now, since sizes do match
				}
			}
			
			// cleanup
			std::sort(cleanup_instrs.begin(), cleanup_instrs.end());
			if (auto last_iter = std::unique(cleanup_instrs.begin(), cleanup_instrs.end()); last_iter != cleanup_instrs.end()) {
				cleanup_instrs.erase(last_iter, cleanup_instrs.end());
			}
			for (auto& cleanup_instr : cleanup_instrs) {
				if (cleanup_instr && cleanup_instr->users().empty() && cleanup_instr->uses().empty()) {
					cleanup_instr->eraseFromParent();
				}
			}
			
			return true;
		}
		
		struct constant_pointer_slot {
			BitCastInst* cast { nullptr };
			Value* address { nullptr };
			PointerType* value_type { nullptr };
			SmallVector<Value*, 8> indices;
		};

		// InstCombine may load an integer-vector pointer through a retyped
		// argument-buffer slot. Recover only the slot's explicitly typed first
		// pointer field, never a pointer type guessed from equal pointer widths.
		bool get_constant_pointer_slot(LoadInst& load, constant_pointer_slot& slot) {
			auto value_type = dyn_cast<PointerType>(load.getType());
			auto slot_cast = dyn_cast<BitCastInst>(load.getPointerOperand());
			if (!load.isSimple() || !value_type || value_type->isOpaque() ||
				value_type->getAddressSpace() != 2 || !slot_cast) return false;
			auto src = dyn_cast<PointerType>(slot_cast->getSrcTy());
			auto dst = dyn_cast<PointerType>(slot_cast->getDestTy());
			if (!src || !dst || src->isOpaque() || dst->isOpaque() ||
				src->getAddressSpace() != dst->getAddressSpace() ||
				dst->getPointerElementType() != value_type) return false;
			auto element = src->getPointerElementType();
			if (!element->isSized()) return false;
			if (!element->isPointerTy()) {
				slot.indices.push_back(ConstantInt::get(Type::getInt32Ty(*ctx), 0));
				for (;;) {
					if (auto st = dyn_cast<StructType>(element)) {
						if (!st->getNumElements() || DL->getStructLayout(st)->getElementOffset(0) != 0) return false;
						element = st->getElementType(0);
					} else if (auto arr = dyn_cast<ArrayType>(element)) {
						if (!arr->getNumElements()) return false;
						element = arr->getElementType();
					} else return false;
					slot.indices.push_back(ConstantInt::get(Type::getInt32Ty(*ctx), 0));
					if (element->isPointerTy()) break;
				}
			}
			auto original_type = cast<PointerType>(element);
			if (original_type->isOpaque() || original_type->getAddressSpace() != 2 ||
				DL->getTypeStoreSize(original_type) != DL->getTypeStoreSize(value_type) ||
				DL->getTypeAllocSize(original_type) != DL->getTypeAllocSize(value_type)) return false;
			slot.cast = slot_cast;
			slot.address = slot_cast->getOperand(0);
			slot.value_type = original_type;
			return true;
		}

		// InstCombine can fold a zero-offset constant-buffer array access into
		// a struct* -> vector* bitcast followed by vector/scalar GEPs. Rebuild
		// the read-only pointer graph with the actual element types, then bitcast
		// the loaded values, never the pointers. Do not generalize this to
		// padding, regrouped vector lanes, pointer escapes, or writable memory.
		bool fix_metal_constant_vector_loads(BitCastInst& BC) {
			auto src_ptr_type = cast<PointerType>(BC.getSrcTy());
			auto dst_ptr_type = cast<PointerType>(BC.getDestTy());
			// AIR address space 2 is constant storage.
			if (!is_metal || src_ptr_type->getAddressSpace() != 2 || dst_ptr_type->getAddressSpace() != 2 ||
				src_ptr_type->isOpaque() || dst_ptr_type->isOpaque()) {
				return false;
			}
			auto src_type = src_ptr_type->getPointerElementType();
			auto dst_vec = dyn_cast<FixedVectorType>(dst_ptr_type->getPointerElementType());
			Instruction* seed = &BC;
			// Inverse casts must restore an exactly proven native pointer type.
			// A direct slot is checked here; PHI/select/root provenance is checked
			// with the complete component below. Aggregate GEPs stay unchanged.
			if (auto reverse_vec = dyn_cast<FixedVectorType>(src_type);
				reverse_vec && dst_ptr_type->getPointerElementType()->isStructTy()) {
				auto load = dyn_cast<LoadInst>(BC.getOperand(0));
				constant_pointer_slot slot;
				if (!load || !get_constant_pointer_slot(*load, slot) || slot.value_type != dst_ptr_type) {
					auto input = dyn_cast<Instruction>(BC.getOperand(0));
					if (!input || (!isa<PHINode>(input) && !isa<SelectInst>(input) && !isa<BitCastInst>(input))) {
						constant_vector_rejections[&BC] = { "unproved reverse pointer slot",
							input ? input->getOpcodeName() : "" };
						return false;
					}
					seed = input;
				} else {
					seed = load;
				}
				src_type = dst_ptr_type->getPointerElementType();
				dst_vec = reverse_vec;
			}
			if (!src_type->isStructTy() || !src_type->isSized() || !dst_vec) {
				return false;
			}

			// Only walk zero-offset, single-field wrappers and nonempty arrays.
			// The final GEP names the original storage rather than loading a
			// whole buffer or pretending the buffer has a different layout.
			auto element_type = src_type;
			for (;;) {
				if (auto st = dyn_cast<StructType>(element_type)) {
					if (st->getNumElements() != 1 || DL->getStructLayout(st)->getElementOffset(0) != 0) {
						return false;
					}
					element_type = st->getElementType(0);
				} else if (auto arr = dyn_cast<ArrayType>(element_type)) {
					if (!arr->getNumElements()) {
						return false;
					}
					element_type = arr->getElementType();
				} else {
					break;
				}
			}
			auto src_vec = dyn_cast<FixedVectorType>(element_type);
			if (!src_vec || src_vec->getNumElements() != dst_vec->getNumElements()) {
				return false;
			}
			auto src_scalar = src_vec->getElementType();
			auto dst_scalar = dst_vec->getElementType();
			if ((!src_scalar->isIntegerTy() && !src_scalar->isFloatingPointTy()) ||
				(!dst_scalar->isIntegerTy() && !dst_scalar->isFloatingPointTy()) ||
				!CastInst::isBitCastable(src_scalar, dst_scalar) ||
				DL->getTypeStoreSize(src_scalar) != DL->getTypeStoreSize(dst_scalar) ||
				DL->getTypeAllocSize(src_scalar) != DL->getTypeAllocSize(dst_scalar) ||
				DL->getTypeStoreSize(src_vec) != DL->getTypeStoreSize(dst_vec) ||
				DL->getTypeAllocSize(src_vec) != DL->getTypeAllocSize(dst_vec)) {
				return false;
			}

			// Validate the whole read-only pointer SSA component in both directions.
			// A merge is safe only when every arm has the same typed storage view.
			// Unknown pointer roots, casts with regrouped lanes, and escapes fail
			// before any mutation. Native source pointers are unchanged boundaries.
			SmallVector<Instruction*, 16> pointers { seed };
			SmallPtrSet<Instruction*, 32> seen { seed };
			SmallVector<Instruction*, 4> roots;
			std::unordered_map<Instruction*, SmallVector<Value*, 8>> root_indices;
			std::unordered_map<LoadInst*, constant_pointer_slot> slot_loads;
			std::unordered_map<BitCastInst*, LoadInst*> native_recasts;
			std::unordered_map<BitCastInst*, Instruction*> merged_native_recasts;
			std::unordered_map<BitCastInst*, GetElementPtrInst*> canonical_gep_recasts;
			SmallVector<Instruction*, 8> native_pointers;
			SmallPtrSet<Instruction*, 32> native_seen;
			bool has_native_load = false;
			SmallVector<PHINode*, 4> phis;
			SmallVector<Instruction*, 8> derived;
			SmallVector<LoadInst*, 8> loads;
			// Fixed prose and one compiler-defined opcode only. Retain reasons
			// across candidate attempts, but emit nothing unless legacy rejects.
			auto reject = [&](StringRef reason, const Instruction* culprit = nullptr) {
				constant_vector_rejection detail { reason, culprit ? culprit->getOpcodeName() : "" };
				constant_vector_rejections[&BC] = detail;
				for (auto& recast : native_recasts) constant_vector_rejections[recast.first] = detail;
				for (auto& recast : merged_native_recasts) constant_vector_rejections[recast.first] = detail;
				for (auto& recast : canonical_gep_recasts) constant_vector_rejections[recast.first] = detail;
				for (auto node : seen) {
					if (auto root = dyn_cast<BitCastInst>(node)) constant_vector_rejections[root] = detail;
				}
				return false;
			};
			auto add_pointer = [&](Value* value) {
				auto ptr_type = dyn_cast<PointerType>(value->getType());
				auto instr = dyn_cast<Instruction>(value);
				if (!instr) return reject("unmapped argument or constant pointer root");
				if (!ptr_type || ptr_type->isOpaque() || ptr_type->getAddressSpace() != 2 ||
					(ptr_type->getPointerElementType() != dst_vec && ptr_type->getPointerElementType() != dst_scalar)) {
					return reject("pointer type or address-space mismatch", instr);
				}
				if (seen.insert(instr).second) pointers.push_back(instr);
				return true;
			};
			auto add_root = [&](Instruction* root, PointerType* input_type) {
				auto output_element = root->getType()->getPointerElementType();
				auto expected = output_element == dst_vec ? static_cast<Type*>(src_vec) : src_scalar;
				auto input_element = input_type->getPointerElementType();
				SmallVector<Value*, 8> path;
				if (input_element != expected) {
					if (output_element != dst_vec || !input_element->isStructTy() || !input_element->isSized())
						return reject("root scalar or vector storage shape mismatch", root);
					path.push_back(ConstantInt::get(Type::getInt32Ty(*ctx), 0));
					for (;;) {
						if (auto st = dyn_cast<StructType>(input_element)) {
							if (st->getNumElements() != 1 || DL->getStructLayout(st)->getElementOffset(0) != 0)
								return reject("root wrapper layout mismatch", root);
							input_element = st->getElementType(0);
						} else if (auto arr = dyn_cast<ArrayType>(input_element)) {
							if (!arr->getNumElements()) return reject("empty root array", root);
							input_element = arr->getElementType();
						} else break;
						path.push_back(ConstantInt::get(Type::getInt32Ty(*ctx), 0));
					}
					if (input_element != src_vec) return reject("root vector element type mismatch", root);
				}
				roots.push_back(root);
				root_indices.emplace(root, std::move(path));
				return true;
			};
			// Exact inverse recasts retain their existing native pointer graph.
			// Check its entire forward use closure for reads only, without changing
			// any aggregate indices or the types of other native merge inputs.
			auto check_native_users = [&](BitCastInst* recast) {
				if (native_seen.insert(recast).second) native_pointers.push_back(recast);
				for (size_t n = 0; n < native_pointers.size(); ++n) {
					auto pointer = native_pointers[n];
					for (auto user : pointer->users()) {
						if (auto load = dyn_cast<LoadInst>(user)) {
							if (!load->isSimple() || load->getPointerOperand() != pointer ||
								load->getType() != pointer->getType()->getPointerElementType())
								return reject("unsupported native-view load", load);
							has_native_load = true;
						} else if (auto bitcast = dyn_cast<BitCastInst>(user)) {
							if (!add_pointer(bitcast)) return false;
						} else if (isa<GetElementPtrInst>(user) || isa<PHINode>(user) || isa<SelectInst>(user)) {
							auto instruction = cast<Instruction>(user);
							auto type = dyn_cast<PointerType>(instruction->getType());
							if (!type || type->isOpaque() || type->getAddressSpace() != 2)
								return reject("native-view address-space mismatch", instruction);
							if (native_seen.insert(instruction).second) native_pointers.push_back(instruction);
						} else return reject("unsupported native-view user", dyn_cast<Instruction>(user));
					}
				}
				return true;
			};
			for (size_t i = 0; i < pointers.size(); ++i) {
				auto ptr = pointers[i];
				if (auto root = dyn_cast<BitCastInst>(ptr)) {
					auto input_type = dyn_cast<PointerType>(root->getSrcTy());
					if (!input_type || input_type->isOpaque() || input_type->getAddressSpace() != 2 ||
						root->getSrcTy() == root->getDestTy()) return reject("unsupported root cast", root);
					if (!add_root(root, input_type)) return false;
				} else if (auto load = dyn_cast<LoadInst>(ptr)) {
					constant_pointer_slot slot;
					if (!get_constant_pointer_slot(*load, slot)) return reject("unproved pointer slot", load);
					if (!add_root(load, slot.value_type)) return false;
					slot_loads.emplace(load, std::move(slot));
				} else if (auto gep = dyn_cast<GetElementPtrInst>(ptr)) {
					auto gep_type = gep->getSourceElementType();
					if ((gep_type != dst_vec && gep_type != dst_scalar) || gep->getNumIndices() == 0 ||
						gep->getNumIndices() > (gep_type == dst_vec ? 2u : 1u)) return reject("GEP element or index shape mismatch", gep);
					if (!add_pointer(gep->getPointerOperand())) return false;
					for (auto& index : gep->indices()) if (!index->getType()->isIntegerTy()) return reject("vector GEP index", gep);
					derived.push_back(gep);
				} else if (auto phi = dyn_cast<PHINode>(ptr)) {
					for (auto& incoming : phi->incoming_values()) if (!add_pointer(incoming)) return false;
					phis.push_back(phi);
				} else if (auto select = dyn_cast<SelectInst>(ptr)) {
					if (!select->getCondition()->getType()->isIntegerTy(1)) return reject("vector select condition", select);
					if (!add_pointer(select->getTrueValue()) || !add_pointer(select->getFalseValue())) return false;
					derived.push_back(select);
				} else return reject("unmapped pointer-producing instruction", ptr);

				for (auto user : ptr->users()) {
					if (auto ld = dyn_cast<LoadInst>(user)) {
						if (!ld->isSimple()) return reject("volatile or atomic load", ld);
						if (ld->getPointerOperand() != ptr || ld->getType() != ptr->getType()->getPointerElementType())
							return reject("load pointer or value type mismatch", ld);
						loads.push_back(ld);
					} else if (auto recast = dyn_cast<BitCastInst>(user)) {
						auto load = dyn_cast<LoadInst>(ptr);
						if (load && slot_loads.count(load) && recast->getDestTy() == slot_loads.at(load).value_type) {
							// Preserve the existing direct-slot case, including vector
							// stored-pointer types and unrelated nominal wrapper roots.
							native_recasts.emplace(recast, load);
						} else {
							auto native_type = dyn_cast<PointerType>(recast->getDestTy());
							if (!native_type || native_type->isOpaque() || native_type->getAddressSpace() != 2)
								return reject("inverse pointer type or address-space mismatch", ptr);
							const bool vector_input = ptr->getType()->getPointerElementType() == dst_vec;
							auto canonical_element = vector_input ? static_cast<Type*>(src_vec) : src_scalar;
							if (native_type->getPointerElementType() == canonical_element) {
								// The validated GEP already has an exact canonical storage
								// replacement. Preserve every original index and inbounds
								// flag; do not infer zero offset or an aggregate object.
								auto gep = dyn_cast<GetElementPtrInst>(ptr);
								if (!gep) return reject("canonical inverse input is not a GEP", ptr);
								canonical_gep_recasts.emplace(recast, gep);
							} else if (!vector_input) {
								auto destination = native_type->getPointerElementType();
								if (destination->isAggregateType()) return reject("scalar-to-aggregate inverse pointer cast", ptr);
								if (destination->isVectorTy()) return reject("scalar-to-vector inverse pointer cast", ptr);
								if (destination->isPointerTy()) return reject("scalar-to-pointer inverse pointer cast", ptr);
								if (destination->isIntegerTy() || destination->isFloatingPointTy())
									return reject("noncanonical scalar inverse pointer cast", ptr);
								return reject("scalar inverse destination category mismatch", ptr);
							} else if (!native_type->getPointerElementType()->isStructTy()) {
								auto destination = native_type->getPointerElementType();
								if (destination->isVectorTy()) return reject("noncanonical vector inverse pointer cast", ptr);
								if (destination->isPointerTy()) return reject("vector-to-pointer inverse pointer cast", ptr);
								if (destination->isArrayTy()) return reject("vector-to-array inverse pointer cast", ptr);
								if (destination->isIntegerTy() || destination->isFloatingPointTy())
									return reject("vector-to-scalar inverse pointer cast", ptr);
								return reject("vector inverse destination category mismatch", ptr);
							} else {
								merged_native_recasts.emplace(recast, ptr);
							}
						}
						if (!check_native_users(recast)) return false;
					} else if (isa<GetElementPtrInst>(user) || isa<PHINode>(user) || isa<SelectInst>(user)) {
						if (!add_pointer(user)) return false;
					} else return reject("unsupported pointer user", dyn_cast<Instruction>(user));
				}
			}
			if (loads.empty() && !has_native_load) return reject("no ordinary load");
			for (auto root : roots) {
				Value* value = root->getOperand(0);
				if (auto load = dyn_cast<LoadInst>(root)) value = slot_loads.at(load).address;
				if (auto source = dyn_cast<Instruction>(value); source && seen.count(source))
					return reject("root depends on rewritten pointer component", root);
			}

			struct native_view_proof {
				BitCastInst* recast;
				Instruction* input;
				SmallVector<Instruction*, 8> nodes;
			};
			SmallVector<native_view_proof, 4> native_views;
			// Prove each inverse input separately. A canonical component can contain
			// unrelated nominal wrapper types; equal layout is not native identity.
			for (auto& recast : merged_native_recasts) {
				native_view_proof proof { recast.first, recast.second, { recast.second } };
				SmallPtrSet<Instruction*, 32> visited { proof.input };
				SmallPtrSet<Instruction*, 32> anchored;
				auto add_native_input = [&](Value* value) {
					auto node = dyn_cast<Instruction>(value);
					if (!node || !seen.count(node) || node->getType() != proof.input->getType())
						return reject("unproved inverse pointer input", node);
					if (visited.insert(node).second) proof.nodes.push_back(node);
					return true;
				};
				for (size_t i = 0; i < proof.nodes.size(); ++i) {
					auto node = proof.nodes[i];
					if (root_indices.count(node)) {
						Type* original_type;
						Value* original;
						if (auto load = dyn_cast<LoadInst>(node)) {
							original_type = slot_loads.at(load).value_type;
							original = slot_loads.at(load).address;
						} else {
							original = node->getOperand(0);
							original_type = original->getType();
						}
						if (original_type != proof.recast->getDestTy())
							return reject("inverse root nominal type mismatch", node);
						if (isa<ConstantPointerNull>(original) || isa<UndefValue>(original) || isa<PoisonValue>(original))
							return reject("unproved inverse native root", node);
						// A load or scalar conversion can hide a dependency beyond
						// the native pointer-use closure. Check the entire SSA ancestry
						// of new-case roots before constructing any native placeholders.
						SmallVector<Value*, 16> dependencies { original };
						SmallPtrSet<Instruction*, 32> dependency_seen;
						for (size_t d = 0; d < dependencies.size(); ++d) {
							auto source = dyn_cast<Instruction>(dependencies[d]);
							if (!source || !dependency_seen.insert(source).second) continue;
							if (seen.count(source) || native_seen.count(source))
								return reject("inverse root depends on rewritten native graph", node);
							for (auto& operand : source->operands()) dependencies.push_back(operand);
						}
						anchored.insert(node);
					} else if (auto phi = dyn_cast<PHINode>(node)) {
						for (auto& value : phi->incoming_values()) if (!add_native_input(value)) return false;
					} else if (auto select = dyn_cast<SelectInst>(node)) {
						if (!add_native_input(select->getTrueValue()) || !add_native_input(select->getFalseValue())) return false;
					} else return reject("unsupported inverse pointer input", node);
				}
				// Reachability to a root is a fixed point, not a DFS cycle shortcut.
				// Every visited node must be anchored, including subcycles behind
				// a merge which also has an independently valid incoming root.
				for (;;) {
					auto previous_size = anchored.size();
					for (auto node : proof.nodes) {
						if (anchored.count(node)) continue;
						if (auto phi = dyn_cast<PHINode>(node)) {
							for (auto& value : phi->incoming_values()) if (anchored.count(cast<Instruction>(value))) {
								anchored.insert(node);
								break;
							}
						} else {
							auto select = cast<SelectInst>(node);
							if (anchored.count(cast<Instruction>(select->getTrueValue())) ||
								anchored.count(cast<Instruction>(select->getFalseValue()))) anchored.insert(node);
						}
					}
					if (previous_size == anchored.size()) break;
				}
				for (auto node : proof.nodes) if (!anchored.count(node))
					return reject("unanchored inverse pointer cycle", node);
				native_views.push_back(std::move(proof));
			}

			// PHI placeholders break loop-carried dependencies. Plan the rest in
			// dependency order before mutation, rejecting any unexpected cycle.
			SmallPtrSet<Value*, 32> ready;
			for (auto root : roots) ready.insert(root);
			for (auto phi : phis) ready.insert(phi);
			SmallVector<Instruction*, 8> ordered;
			while (ordered.size() != derived.size()) {
				auto previous_size = ordered.size();
				for (auto instr : derived) {
					if (ready.count(instr)) continue;
					if (auto gep = dyn_cast<GetElementPtrInst>(instr)) {
						if (!ready.count(gep->getPointerOperand())) continue;
					} else {
						auto select = cast<SelectInst>(instr);
						if (!ready.count(select->getTrueValue()) || !ready.count(select->getFalseValue())) continue;
					}
					ready.insert(instr);
					ordered.push_back(instr);
				}
				if (previous_size == ordered.size()) return reject("non-PHI dependency cycle");
			}

			std::unordered_map<Value*, Value*> replacements;
			std::unordered_map<LoadInst*, LoadInst*> native_loads;
			for (auto& entry : slot_loads) {
				auto load = entry.first;
				auto& slot = entry.second;
				Value* address = slot.address;
				if (!slot.indices.empty()) {
					auto gep = GetElementPtrInst::Create(address->getType()->getPointerElementType(), address,
						slot.indices, load->getName() + ".slot", load);
					gep->setDebugLoc(load->getDebugLoc());
					address = gep;
				}
				auto native_load = new LoadInst(slot.value_type, address, load->getName() + ".pointer",
					false, load->getAlign(), load);
				// Both values are the same AS2 pointer. Pointer-valued facts remain
				// valid, as do memory facts for the identical pointer-sized slot.
				native_load->copyMetadata(*load);
				native_load->setDebugLoc(load->getDebugLoc());
				native_loads.emplace(load, native_load);
			}
			std::unordered_map<BitCastInst*, Value*> merged_native_values;
			for (auto& proof : native_views) {
				std::unordered_map<Value*, Value*> native_values;
				SmallPtrSet<Instruction*, 32> included;
				for (auto node : proof.nodes) {
					included.insert(node);
					if (root_indices.count(node)) {
						Value* native = node->getOperand(0);
						if (auto load = dyn_cast<LoadInst>(node)) native = native_loads.at(load);
						native_values.emplace(node, native);
					} else if (auto phi = dyn_cast<PHINode>(node)) {
						auto native_phi = PHINode::Create(proof.recast->getDestTy(), phi->getNumIncomingValues(),
							phi->getName() + ".native", phi);
						native_phi->copyMetadata(*phi);
						native_phi->setDebugLoc(phi->getDebugLoc());
						native_values.emplace(phi, native_phi);
					}
				}
				for (auto node : ordered) if (included.count(node)) {
					auto select = cast<SelectInst>(node);
					auto native_select = SelectInst::Create(select->getCondition(), native_values.at(select->getTrueValue()),
						native_values.at(select->getFalseValue()), select->getName() + ".native", select);
					native_select->copyMetadata(*select);
					native_select->setDebugLoc(select->getDebugLoc());
					native_values.emplace(select, native_select);
				}
				for (auto node : proof.nodes) if (auto phi = dyn_cast<PHINode>(node)) {
					auto native_phi = cast<PHINode>(native_values.at(phi));
					for (unsigned i = 0; i < phi->getNumIncomingValues(); ++i)
						native_phi->addIncoming(native_values.at(phi->getIncomingValue(i)), phi->getIncomingBlock(i));
				}
				merged_native_values.emplace(proof.recast, native_values.at(proof.input));
			}
			for (auto& recast : native_recasts) recast.first->replaceAllUsesWith(native_loads.at(recast.second));
			for (auto& recast : merged_native_values) recast.first->replaceAllUsesWith(recast.second);
			for (auto root : roots) {
				auto& path = root_indices.at(root);
				Value* base = root->getOperand(0);
				if (auto load = dyn_cast<LoadInst>(root)) base = native_loads.at(load);
				if (!path.empty()) {
					auto storage = GetElementPtrInst::Create(base->getType()->getPointerElementType(), base,
						path, root->getName() + ".storage", root);
					storage->setDebugLoc(root->getDebugLoc());
					base = storage;
				}
				replacements.emplace(root, base);
			}
			for (auto phi : phis) {
				auto element = phi->getType()->getPointerElementType() == dst_vec ? static_cast<Type*>(src_vec) : src_scalar;
				auto typed_phi = PHINode::Create(PointerType::get(element, 2), phi->getNumIncomingValues(), phi->getName() + ".typed", phi);
				typed_phi->copyMetadata(*phi);
				typed_phi->setDebugLoc(phi->getDebugLoc());
				replacements.emplace(phi, typed_phi);
			}
			for (auto instr : ordered) {
				Instruction* typed;
				if (auto gep = dyn_cast<GetElementPtrInst>(instr)) {
					auto ptr = replacements.at(gep->getPointerOperand());
					SmallVector<Value*, 2> gep_indices;
					for (auto& index : gep->indices()) gep_indices.push_back(index);
					auto typed_gep = GetElementPtrInst::Create(ptr->getType()->getPointerElementType(), ptr,
						gep_indices, gep->getName() + ".typed", gep);
					typed_gep->setIsInBounds(gep->isInBounds());
					typed = typed_gep;
				} else {
					auto select = cast<SelectInst>(instr);
					typed = SelectInst::Create(select->getCondition(), replacements.at(select->getTrueValue()),
						replacements.at(select->getFalseValue()), select->getName() + ".typed", select);
				}
				typed->copyMetadata(*instr);
				typed->setDebugLoc(instr->getDebugLoc());
				replacements.emplace(instr, typed);
			}
			for (auto phi : phis) {
				auto typed_phi = cast<PHINode>(replacements.at(phi));
				for (unsigned i = 0; i < phi->getNumIncomingValues(); ++i) {
					typed_phi->addIncoming(replacements.at(phi->getIncomingValue(i)), phi->getIncomingBlock(i));
				}
			}
			// Unlike wrapper recovery, this path uses the existing canonical GEP
			// itself, including nonzero/dynamic signed indices. Wait until all
			// typed nodes exist, then update cached root aliases as well as IR uses:
			// a native-to-integer root may refer directly to a consumed recast.
			for (auto& recast : canonical_gep_recasts) {
				auto value = replacements.at(recast.second);
				for (auto& replacement : replacements) {
					if (replacement.second == recast.first) replacement.second = value;
				}
				recast.first->replaceAllUsesWith(value);
			}
			for (auto ld : loads) {
				auto ptr = replacements.at(ld->getPointerOperand());
				auto typed_ld = new LoadInst(ptr->getType()->getPointerElementType(), ptr,
											 ld->getName() + ".typed", false, ld->getAlign(), ld);
				// Keep memory facts, but not value facts such as integer !range:
				// those may be invalid metadata on the new floating-point load.
				typed_ld->copyMetadata(*ld, { LLVMContext::MD_tbaa, LLVMContext::MD_alias_scope,
					LLVMContext::MD_noalias, LLVMContext::MD_nontemporal, LLVMContext::MD_invariant_load,
					LLVMContext::MD_mem_parallel_loop_access, LLVMContext::MD_access_group });
				typed_ld->setDebugLoc(ld->getDebugLoc());
				Value* value = typed_ld;
				if (typed_ld->getType() != ld->getType()) {
					auto bits = new BitCastInst(typed_ld, ld->getType(), ld->getName() + ".bits", ld);
					bits->setDebugLoc(ld->getDebugLoc());
					value = bits;
				}
				ld->replaceAllUsesWith(value);
				ld->eraseFromParent();
			}
			// A component can consume other bitcasts from the outer worklist.
			// Null their slots without invalidating the active range iterator.
			for (auto& pending : ptr_bc_instrs) {
				if (seen.count(pending) || native_recasts.count(pending) || merged_native_recasts.count(pending) ||
					canonical_gep_recasts.count(pending)) {
					constant_vector_rejections.erase(pending);
					pending = nullptr;
				}
			}
			// Loads are gone and all remaining users belong to the validated
			// graph. Sever cycles before deleting any original pointer node.
			for (auto& recast : native_recasts) recast.first->dropAllReferences();
			for (auto& recast : merged_native_recasts) recast.first->dropAllReferences();
			for (auto& recast : canonical_gep_recasts) recast.first->dropAllReferences();
			for (auto ptr : pointers) ptr->dropAllReferences();
			for (auto& recast : native_recasts) recast.first->eraseFromParent();
			for (auto& recast : merged_native_recasts) recast.first->eraseFromParent();
			for (auto& recast : canonical_gep_recasts) recast.first->eraseFromParent();
			for (auto ptr : pointers) ptr->eraseFromParent();
			// Slot-address casts can be shared with other loads; only erase dead
			// ones, after nulling every matching raw outer-worklist entry.
			SmallPtrSet<BitCastInst*, 8> slot_casts;
			for (auto& entry : slot_loads) slot_casts.insert(entry.second.cast);
			for (auto slot_cast : slot_casts) if (slot_cast->use_empty()) {
				for (auto& pending : ptr_bc_instrs) if (pending == slot_cast) pending = nullptr;
				constant_vector_rejections.erase(slot_cast);
				slot_cast->eraseFromParent();
			}
			return true;
		}

		bool fix_pointer_bitcasts() {
			constant_vector_rejections.clear();
			bool did_modify = false;
			// Normalize complete Metal components before legacy per-cast checks:
			// a vector/scalar arm may precede its struct seed in function order.
			for (auto BC : ptr_bc_instrs) {
				if (BC && fix_metal_constant_vector_loads(*BC)) did_modify = true;
			}
			for (auto& BC : ptr_bc_instrs) {
				if (!BC) continue;
				const auto src_ptr_type = cast<PointerType>(BC->getSrcTy());
				const auto dst_ptr_type = cast<PointerType>(BC->getDestTy());
				
				// bitcasts aren't technically allowed to bitcast address spaces, but still check this
				if (src_ptr_type->getAddressSpace() != dst_ptr_type->getAddressSpace()) {
					ctx->emitError(BC, "invalid pointer bitcast: address space cast is not allowed");
					return false;
				}
				
				// we will only replace the pointer bitcast if all users are simple loads or stores
				bool all_users_are_loads_or_stores = true;
				std::vector<LoadInst*> loads;
				std::vector<StoreInst*> stores;
				std::string unhandled_user_opcodes;
				unsigned unhandled_user_count = 0;
				libfloor_utils::for_all_instruction_users(*BC, [this, &all_users_are_loads_or_stores, &loads, &stores,
															 &unhandled_user_opcodes, &unhandled_user_count](Instruction& instr) {
					if (auto ld = dyn_cast_or_null<LoadInst>(&instr); ld) {
						if (is_vulkan) { // only need this for Vulkan
							loads.emplace_back(ld);
						}
					} else if (auto st = dyn_cast_or_null<StoreInst>(&instr); st) {
						stores.emplace_back(st);
					} else if (dyn_cast_or_null<CallInst>(&instr)) {
						// ignore external function calls (may e.g. be used for atomic functions)
					} else {
						all_users_are_loads_or_stores = false;
						// Only compiler-defined opcode names, never source names or IR.
						// Saturate the count so this diagnostic stays bounded.
						if (unhandled_user_count < 4) {
							if (unhandled_user_count != 0) unhandled_user_opcodes += ", ";
							unhandled_user_opcodes += instr.getOpcodeName();
							++unhandled_user_count;
						} else if (unhandled_user_count == 4) {
							unhandled_user_opcodes += ", ...";
							++unhandled_user_count;
						}
					}
				});
				if (!all_users_are_loads_or_stores) {
					// The instruction overload appends full IR, which local privacy
					// filtering must discard. Keep this error useful without it.
					std::string error = "invalid pointer bitcast: failed to run bitcast fixup (unhandled instructions); unsupported users: " +
						unhandled_user_opcodes;
					if (auto reason = constant_vector_rejections.find(BC); reason != constant_vector_rejections.end()) {
						error += "; constant-vector graph: " + reason->second.reason.str();
						if (!reason->second.opcode.empty()) error += " (" + reason->second.opcode.str() + ")";
					}
					ctx->emitError(error);
					return false;
				}
				if (!stores.empty() && !loads.empty()) {
					ctx->emitError(BC, "invalid pointer bitcast: failed to run bitcast fixup (can't handle both loads and stores)");
					return false;
				}
				if (loads.empty() && stores.empty()) {
					// ignore this bitcast
					continue;
				}
				
				if (!loads.empty()) {
					auto result = fix_pointer_bitcast_with_loads(*BC, *func, loads);
					if (!result) {
						return false;
					}
					did_modify |= *result;
				}

				// NOTE: this is still very much a WIP and may fail catastrophically
				if (!stores.empty()) {
					auto result = fix_pointer_bitcast_with_stores(*BC, *func, stores);
					if (!result) {
						return false;
					}
					did_modify |= *result;
				}
			}
			return did_modify;
		}
	};
	
}

char PointerBCFixup::ID = 0;
FunctionPass *llvm::createPointerBCFixupPass() {
	return new PointerBCFixup();
}
INITIALIZE_PASS_BEGIN(PointerBCFixup, "PointerBCFixup", "PointerBCFixup Pass", false, false)
INITIALIZE_PASS_DEPENDENCY(AAResultsWrapperPass)
INITIALIZE_PASS_DEPENDENCY(GlobalsAAWrapperPass)
INITIALIZE_PASS_DEPENDENCY(AssumptionCacheTracker)
INITIALIZE_PASS_DEPENDENCY(CallGraphWrapperPass)
INITIALIZE_PASS_DEPENDENCY(TargetLibraryInfoWrapperPass)
INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
INITIALIZE_PASS_END(PointerBCFixup, "PointerBCFixup", "PointerBCFixup Pass", false, false)
