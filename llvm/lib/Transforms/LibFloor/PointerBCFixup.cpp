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
		ConstantFolder folder;
		
		// gather pointer bitcast instructions
		std::vector<BitCastInst*> ptr_bc_instrs;
		
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
				// -> non vector<->struct BC+load
				
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
					if (stores.size() > 1) {
						ctx->emitError(&BC, "invalid pointer bitcast: can't replace more than one store");
						return {};
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
					
					const auto store = stores[0];
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
							auto adv_idx = BinaryOperator::CreateAdd(last_gep_idx,
																	 ConstantInt::get(last_gep_idx->getType(), split_idx),
																	 "st_src_gep_idx_adv", src_gep);
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
		
		bool fix_pointer_bitcasts() {
			bool did_modify = false;
			for (auto& BC : ptr_bc_instrs) {
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
				libfloor_utils::for_all_instruction_users(*BC, [this, &all_users_are_loads_or_stores, &loads, &stores](Instruction& instr) {
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
					}
				});
				if (!all_users_are_loads_or_stores) {
					ctx->emitError(BC, "invalid pointer bitcast: failed to run bitcast fixup (unhandled instructions)");
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
