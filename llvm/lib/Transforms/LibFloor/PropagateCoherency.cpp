//===- PropagateCoherency.cpp - propagate memory coherency pass -----------===//
//
//  Flo's Open libRary (floor)
//  Copyright (C) 2004 - 2025 Florian Ziesche
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
// This pass propagates memory coherency and implements backend specific transformations.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/BasicAliasAnalysis.h"
#include "llvm/Analysis/GlobalsModRef.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/LoopInfo.h"
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

#define DEBUG_TYPE "PropagateCoherency"

#if 1
#define DBG(x)
#else
#define DBG(x) x
#endif

namespace {
	// PropagateCoherency
	struct PropagateCoherency : public FunctionPass, InstVisitor<PropagateCoherency> {
		static char ID; // Pass identification, replacement for typeid
		
		static const uint32_t AIRAS_global = 1;
		static const uint32_t AIRAS_constant = 2;
		static const uint32_t SPIRAS_StorageBuffer = 12;
		static const uint32_t SPIRAS_PhysicalStorageBuffer = 5349;
		
		Module* M { nullptr };
		LLVMContext* ctx { nullptr };
		Function* func { nullptr };
		
		bool is_metal { false };
		bool is_vulkan { false };
		bool was_modified { false };
		
		PropagateCoherency() :
		FunctionPass(ID) {
			initializePropagateCoherencyPass(*PassRegistry::getPassRegistry());
		}
		
		StringRef getPassName() const override {
			return "Propagate Coherency";
		}
		
		void getAnalysisUsage(AnalysisUsage &AU) const override {
			AU.setPreservesCFG();
			FunctionPass::getAnalysisUsage(AU);
		}
		
		bool runOnFunction(Function &F) override {
			// exit if empty function
			if (F.empty()) return false;
			
			//
			M = F.getParent();
			ctx = &M->getContext();
			func = &F;
			was_modified = false;
			
			is_metal = (Triple(M->getTargetTriple()).getArch() == Triple::ArchType::air64);
			is_vulkan = (Triple(M->getTargetTriple()).getArch() == Triple::ArchType::spir64 &&
						 Triple(M->getTargetTriple()).getEnvironment() == Triple::EnvironmentType::Vulkan);
			
			// no need to do anything here if not Metal/Vulkan
			if (!is_vulkan && !is_metal) {
				return false;
			}
			
			// find all args that were marked as floor_coherent
			uint32_t arg_idx = 0u;
			for (auto& arg : F.args()) {
				const auto arg_type = arg.getType();
				const auto ptr_type = dyn_cast<llvm::PointerType>(arg_type);
				if (!ptr_type) {
					++arg_idx;
					continue;
				}
				
				const auto attr_arg_idx = llvm::AttributeList::FirstArgIndex + arg_idx++;
				const auto coherent_attr = F.getAttributeAtIndex(attr_arg_idx, "floor_coherent");
				const auto is_coherent = (coherent_attr.getRawPointer() != nullptr);
				
				// Metal: check if this is an argument buffer (which may contain coherent buffers)
				// NOTE: for Vulkan, we don't need to do this as we already unfold the arg buffer, i.e. each field is handled as an arg itself
				bool is_mtl_arg_buffer = false;
				if (is_metal) {
					// right now, we don't flag argument buffers in Metal, but this must at least be a constant-AS ptr and a struct type
					is_mtl_arg_buffer = (ptr_type->getAddressSpace() == AIRAS_constant &&
										 ptr_type->getPointerElementType()->isStructTy());
				}
				
				if (!is_coherent && !is_mtl_arg_buffer) {
					continue;
				}
				
				if (is_coherent) {
					propagate(&arg);
					
					// no longer need the attribute
					if (is_metal) {
						F.removeAttributeAtIndex(attr_arg_idx, "floor_coherent");
					}
				} else if (is_mtl_arg_buffer) {
					// check if we have metadata for this struct / have flagged any field as coherent
					const auto st_type = dyn_cast_or_null<StructType>(ptr_type->getPointerElementType());
					const auto global_coherent_md = M->getNamedMetadata("floor.coherent." + st_type->getName().str());
					if (global_coherent_md) {
						// retrieve all fields indices / coherent buffers
						llvm::SmallSet<uint32_t, 8> coherent_field_indices;
						const auto md_ops = global_coherent_md->getOperand(0);
						for (const auto& md_op : md_ops->operands()) {
							if (const llvm::ConstantAsMetadata* constant_md = dyn_cast_or_null<llvm::ConstantAsMetadata>(md_op.get())) {
								if (const llvm::ConstantInt* field_idx_int = dyn_cast_or_null<llvm::ConstantInt>(constant_md->getValue())) {
									coherent_field_indices.insert(uint32_t(field_idx_int->getZExtValue()));
								}
							}
						}
						
						// go over all GEPs of our argument and perform coherency propagation for all GEPs that reference coherent buffers/fields
						if (!coherent_field_indices.empty()) {
							llvm::SmallVector<llvm::Value*, 16> coherent_input_values;
							libfloor_utils::for_all_instruction_users(arg, [&coherent_field_indices, &coherent_input_values](Instruction& instr) {
								if (auto GEP = dyn_cast_or_null<GetElementPtrInst>(&instr); GEP) {
									assert(GEP->getNumIndices() >= 2);
									const auto gep_st_field_idx = dyn_cast_or_null<ConstantInt>(GEP->idx_begin() + 1);
									assert(gep_st_field_idx); // this must always be const when GEP'ing into a struct?
									if (coherent_field_indices.contains(uint32_t(gep_st_field_idx->getZExtValue()))) {
										// we generally have pairs of "GEP(0, st-idx) + ld GEP" -> coherent buffer pointer,
										// for which we only want to handle/propagate the loaded pointer, not the loading of the pointer itself
										if (GEP->getNumIndices() == 2u) {
											libfloor_utils::for_all_instruction_users(*GEP, [&coherent_input_values](Instruction& gep_user) {
												if (isa<LoadInst>(gep_user)) {
													coherent_input_values.push_back(&gep_user);
												} else {
													llvm::errs() << "unhandled coherent buffer GEP user: " << gep_user << "\n";
												}
											});
										} else {
											// for all other GEPs, i.e. GEP(0, st-idx, ...), we want to fully handle/propagate the GEP themselves
											coherent_input_values.push_back(GEP);
										}
									}
								}
							});
							if (!coherent_input_values.empty()) {
								propagate(coherent_input_values);
							}
						}
					}
				}
			}
			
			return was_modified;
		}
		
		void propagate(llvm::Value* coherent_input_value) {
			std::array<llvm::Value*, 1> meh {{ coherent_input_value }};
			propagate(meh);
		}
		
		void propagate(std::span<llvm::Value*> coherent_input_values) {
			// recursively go through all users and their users (and so on), and find all loads/stores affected by the argument
			std::unordered_set<Instruction*> child_instrs;
			std::vector<Instruction*> load_store_instrs;
			std::deque<Value*> todo_values(coherent_input_values.begin(), coherent_input_values.end());
			while (!todo_values.empty()) {
				auto val = todo_values.front();
				todo_values.pop_front();
				libfloor_utils::for_all_instruction_users(*val, [&child_instrs, &todo_values, &load_store_instrs](Instruction& instr) {
					const auto [_, did_emplace] = child_instrs.emplace(&instr);
					if (did_emplace) {
						// we treat load and store instructions as leaf nodes, i.e. no need to recurse further
						if (!isa<LoadInst>(instr) && !isa<StoreInst>(instr)) {
							todo_values.emplace_back(&instr);
						} else {
							load_store_instrs.emplace_back(&instr);
						}
					}
				});
			}
			
			// handle loads/stores
			was_modified = !load_store_instrs.empty();
			for (const auto& child_instr : load_store_instrs) {
				if (auto load_instr = dyn_cast_or_null<LoadInst>(child_instr); load_instr) {
					if (is_metal) {
						handle_coherent_load_metal(*load_instr);
					} else if (is_vulkan) {
						handle_coherent_load_vulkan(*load_instr);
					}
				} else if (auto store_instr = dyn_cast_or_null<StoreInst>(child_instr); store_instr) {
					if (is_metal) {
						handle_coherent_store_metal(*store_instr);
					} else if (is_vulkan) {
						handle_coherent_store_vulkan(*store_instr);
					}
				}
			}
		}
		
		void handle_coherent_load_metal(LoadInst& load_instr) {
			// only handle loads from global/device memory
			const auto ptr_type = load_instr.getOperand(0)->getType();
			if (auto ptr_as = ptr_type->getPointerAddressSpace(); ptr_as != AIRAS_global && ptr_as != AIRAS_constant) {
				return;
			}
			
			// can only transform native types and vectors thereof
			const auto load_type = load_instr.getType();
			const auto metal_typename = metal::get_metal_native_typename(load_type, false, false);
			if (!metal_typename) {
				ctx->emitError(&load_instr, "failed to make load instruction coherent (can't handle type)");
				assert(false && "unhandled load instruction");
				return;
			}
			
			std::string func_name = "air.load.device_coherent." + *metal_typename + ".p1" + *metal_typename;
			SmallVector<llvm::Type*, 1> func_arg_types { ptr_type };
			SmallVector<llvm::Value*, 1> func_args { load_instr.getOperand(0) };
			libfloor_utils::replace_instruction_with_call(load_instr, *M, func_name, load_instr.getName().str() + "coh_load",
														  load_type, func_arg_types, func_args, libfloor_utils::call_options_t {
				.is_argmem_only = true,
				.is_read_only = true,
				.is_nounwind = true,
			});
		}
		
		void handle_coherent_load_vulkan(LoadInst& load_instr) {
			// only handle loads from global/device memory (SSBOs)
			const auto ptr_type = load_instr.getOperand(0)->getType();
			const auto ptr_as = ptr_type->getPointerAddressSpace();
			if (ptr_as != SPIRAS_StorageBuffer && ptr_as != SPIRAS_PhysicalStorageBuffer) {
				return;
			}
			
			// flag load as coherent for later handling in SPIRVWriter
			load_instr.addAnnotationMetadata("floor_coherent");
		}
		
		void handle_coherent_store_metal(StoreInst& store_instr) {
			// only handle stores to global/device memory
			const auto ptr_type = store_instr.getOperand(1)->getType();
			if (auto ptr_as = ptr_type->getPointerAddressSpace(); ptr_as != AIRAS_global && ptr_as != AIRAS_constant) {
				return;
			}
			
			// NOTE: can only transform native types and vectors thereof
			const auto store_type = store_instr.getOperand(0)->getType();
			const auto metal_typename = metal::get_metal_native_typename(store_type, false, false);
			if (!metal_typename) {
				ctx->emitError(&store_instr, "failed to make store instruction coherent (can't handle type)");
				assert(false && "unhandled store instruction");
				return;
			}
			
			std::string func_name = "air.store.device_coherent." + *metal_typename + ".p1" + *metal_typename;
			SmallVector<llvm::Type*, 2> func_arg_types { store_type, ptr_type };
			SmallVector<llvm::Value*, 2> func_args { store_instr.getOperand(0), store_instr.getOperand(1) };
			libfloor_utils::replace_instruction_with_call(store_instr, *M, func_name, "",
														  llvm::Type::getVoidTy(*ctx), func_arg_types, func_args,
														  libfloor_utils::call_options_t {
				.is_argmem_only = true,
				.is_nounwind = true,
				.is_tail_call = true,
			});
		}
		
		void handle_coherent_store_vulkan(StoreInst& store_instr) {
			// only handle stores to global/device memory (SSBOs)
			const auto ptr_type = store_instr.getOperand(1)->getType();
			const auto ptr_as = ptr_type->getPointerAddressSpace();
			if (ptr_as != SPIRAS_StorageBuffer && ptr_as != SPIRAS_PhysicalStorageBuffer) {
				return;
			}
			
			// flag store as coherent for later handling in SPIRVWriter
			store_instr.addAnnotationMetadata("floor_coherent");
		}
		
	};
	
}

char PropagateCoherency::ID = 0;
FunctionPass *llvm::createPropagateCoherencyPass() {
	return new PropagateCoherency();
}
INITIALIZE_PASS_BEGIN(PropagateCoherency, "PropagateCoherency", "PropagateCoherency Pass", false, false)
INITIALIZE_PASS_END(PropagateCoherency, "PropagateCoherency", "PropagateCoherency Pass", false, false)
