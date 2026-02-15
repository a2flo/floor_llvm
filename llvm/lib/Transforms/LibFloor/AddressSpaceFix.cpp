//===- AddressSpaceFix.cpp - OpenCL/SPIR and related addrspace fixes ------===//
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
// This file implements an address space fixer for OpenCL/Metal/Vulkan.
//
// This is implemented as a module pass that iterates over all functions, then
// over all call instructions in there, fixing all calls that require a
// different address space then what is provided by the called function.
// Since this requires the address space information "from the top",
// this can't be implemented as a bottom-up SCC pass.
// Note that this will duplicate any functions that don't have matching address
// space parameters and thus heavily depends on proper inlining later on.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/BasicAliasAnalysis.h"
#include "llvm/Analysis/GlobalsModRef.h"
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
#include "llvm/Transforms/Utils/Cloning.h"
#include <algorithm>
#include <unordered_set>
#include <cstdarg>
#include <memory>
#include <cxxabi.h>
using namespace llvm;

#define DEBUG_TYPE "AddressSpaceFix"

#if 1
#define DBG(x)
#else
#define DBG(x) x
#endif

namespace {
	// AddressSpaceFix
	struct AddressSpaceFix : public ModulePass, InstVisitor<AddressSpaceFix> {
		friend class InstVisitor<AddressSpaceFix>;
		
		static char ID; // Pass identification, replacement for typeid
		
		Module* M { nullptr };
		LLVMContext* ctx { nullptr };
		bool was_modified { false };
		bool needs_rerun { false };
		
		AddressSpaceFix() : ModulePass(ID) {
			initializeAddressSpaceFixPass(*PassRegistry::getPassRegistry());
		}
		
		void getAnalysisUsage(AnalysisUsage &AU) const override {
			AU.addRequired<AAResultsWrapperPass>();
			AU.addRequired<GlobalsAAWrapperPass>();
			AU.addRequired<AssumptionCacheTracker>();
			AU.addRequired<TargetLibraryInfoWrapperPass>();
		}
		
		bool runOnModule(Module& Mod) override {
			M = &Mod;
			ctx = &M->getContext();
			
			DBG(errs() << Mod << "\n");
			
			bool module_modified = false;
			needs_rerun = true;
			while (needs_rerun) {
				needs_rerun = false;
				for (auto& func : Mod) {
					// ignore non-C++ functions (e.g. ones that were created in here)
					if (func.getName().count('.') > 0) {
						continue;
					}
					module_modified |= runOnFunction(func);
				}
			}
			DBG(if (module_modified) { errs() << "address space fixed module:\n" << Mod << "\n"; })
			return module_modified;
		}
		
		bool runOnFunction(Function& F) {
			// visit everything in this function
			was_modified = false; // reset every time
			DBG(errs() << "in func: "; errs().write_escaped(F.getName()) << '\n';)
			visit(F);
			if(was_modified) {
				DBG(errs() << "!! modified function: ";)
				DBG(errs().write_escaped(F.getName()) << '\n';)
			}
			return was_modified;
		}
		
		// InstVisitor overrides...
		using InstVisitor<AddressSpaceFix>::visit;
		void visit(Instruction& I) {
			InstVisitor<AddressSpaceFix>::visit(I);
		}
		
		template <bool fix_call_instrs = true>
		static void rec_fix_users(Pass* pass, LLVMContext& ctx,
								  Instruction* instr, Value* parent, const uint32_t address_space,
								  const bool fix_inner_ptr, std::vector<ReturnInst*>& returns,
								  bool& was_modified, bool& needs_rerun) {
			// recursively fix all users
			libfloor_utils::for_all_instruction_users(*instr, [&pass, &ctx, &instr, address_space, fix_inner_ptr,
																&returns, &was_modified, &needs_rerun](Instruction& user_instr) {
				DBG(errs() << ">> replacing rec use: " << user_instr << " -> as: " << address_space << "\n";)
				switch (user_instr.getOpcode()) {
					case Instruction::GetElementPtr:
					case Instruction::BitCast:
					case Instruction::Call:
					case Instruction::Ret:
					case Instruction::Load:
					case Instruction::Store:
					case Instruction::PHI:
					case Instruction::Select:
						fix_users<fix_call_instrs>(pass, ctx, &user_instr, instr, address_space, fix_inner_ptr, returns, was_modified, needs_rerun);
						break;
					case Instruction::AddrSpaceCast:
					case Instruction::Invoke:
						// bad, should never happen
						ctx.emitError(&user_instr, "encountered unsupported instruction");
						break;
					default: break;
				}
			});
		}

		template <bool fix_call_instrs = true>
		static void fix_users(Pass* pass, LLVMContext& ctx,
							  Instruction* instr, Value* parent, const uint32_t address_space,
							  const bool fix_inner_ptr, std::vector<ReturnInst*>& returns,
							  bool& was_modified, bool& needs_rerun) {
			// fix instruction
			bool need_users_update = true;
			switch(instr->getOpcode()) {
				case Instruction::GetElementPtr: {
					auto GEP = cast<GetElementPtrInst>(instr);
					if (auto *GEP_ptr_type = dyn_cast<PointerType>(GEP->getType()); GEP_ptr_type) {
						if (GEP->getAddressSpace() != GEP_ptr_type->getAddressSpace()) {
							auto new_ptr_type = PointerType::get(GEP_ptr_type->getPointerElementType(), address_space);
							DBG(errs() << ">> GEP: " << *GEP << ", type: " << *GEP->getType();)
							GEP->mutateType(new_ptr_type);
							DBG(errs() << " -> " << *GEP->getType() << "\n";)
						}
					}
					// else: can't happen?
					break;
				}
				case Instruction::BitCast: {
					auto BC = cast<BitCastInst>(instr);
					if(BC->getDestTy()->isPointerTy()) {
						auto new_ptr_type = PointerType::get(BC->getDestTy()->getPointerElementType(), address_space);
						DBG(errs() << ">> BC: " << *BC << ", type: " << *BC->getType();)
						BC->mutateType(new_ptr_type);
						DBG(errs() << " -> " << *BC->getType() << "\n";)
					}
					// else: can't do anything (TODO: warn/error?)
					else {
						need_users_update = false;
					}
					break;
				}
				case Instruction::Call: {
					// always allow trivial fixes of memcpy
					// TODO: memset, memmove
					if (auto memcpy_instr = dyn_cast_or_null<MemCpyInst>(instr); memcpy_instr) {
						// need a temporary builder for this
						auto builder = std::make_unique<llvm::IRBuilder<>>(ctx);
						builder->SetInsertPoint(memcpy_instr);
						CallInst* new_memcpy_instr = nullptr;
						if (memcpy_instr->getIntrinsicID() != Intrinsic::memcpy_inline) {
							assert(memcpy_instr->getIntrinsicID() == Intrinsic::memcpy);
							new_memcpy_instr = builder->CreateMemCpy(memcpy_instr->getDest(), memcpy_instr->getDestAlign(),
																	 memcpy_instr->getSource(), memcpy_instr->getSourceAlign(),
																	 memcpy_instr->getLength(), memcpy_instr->isVolatile(),
																	 memcpy_instr->getMetadata(LLVMContext::MD_tbaa),
																	 memcpy_instr->getMetadata(LLVMContext::MD_tbaa_struct),
																	 memcpy_instr->getMetadata(LLVMContext::MD_alias_scope),
																	 memcpy_instr->getMetadata(LLVMContext::MD_noalias));
						} else {
							new_memcpy_instr = builder->CreateMemCpyInline(memcpy_instr->getDest(), memcpy_instr->getDestAlign(),
																		   memcpy_instr->getSource(), memcpy_instr->getSourceAlign(),
																		   memcpy_instr->getLength(), memcpy_instr->isVolatile(),
																		   memcpy_instr->getMetadata(LLVMContext::MD_tbaa),
																		   memcpy_instr->getMetadata(LLVMContext::MD_tbaa_struct),
																		   memcpy_instr->getMetadata(LLVMContext::MD_alias_scope),
																		   memcpy_instr->getMetadata(LLVMContext::MD_noalias));
						}
						new_memcpy_instr->setDebugLoc(memcpy_instr->getDebugLoc());
						memcpy_instr->replaceAllUsesWith(new_memcpy_instr);
						memcpy_instr->eraseFromParent();
						need_users_update = false;
						break;
					}
					
					if constexpr (fix_call_instrs) {
						// TODO: should accumulate all users to *this* call instruction (there can be multiple), might want to delay this until done with the function?
						auto CI = cast<CallInst>(instr);
						DBG(errs() << ">> call: " << *CI << "\n";)
						// -> recurse (note that the argument will already have the correct address space)
						assert(pass);
						fix_call_instr(*pass, *CI, ctx, false, was_modified, needs_rerun);
					}
					break;
				}
				case Instruction::Ret: {
					// the function return type will be changed once all returns (and return types) have been accumulated
					returns.push_back(cast<ReturnInst>(instr));
					return;
				}
				case Instruction::Load: {
					auto LD = cast<LoadInst>(instr);
					// don't replace load pointer type if it's already correct (and may differ from "address_space"!)
					if (parent->getType()->isPointerTy() &&
						parent->getType()->getPointerElementType() == LD->getType()) {
						// don't update any recursive users as the AS is already correct (users must use the AS of the load, not our fix AS)
						need_users_update = false;
						break;
					}
					if(LD->getType()->isPointerTy()) {
						auto new_ptr_type = PointerType::get(LD->getType()->getPointerElementType(), address_space);
						DBG(errs() << ">> LD: " << *LD << ", type: " << *LD->getType();)
						LD->mutateType(new_ptr_type);
						DBG(errs() << " -> " << *LD->getType() << "\n";)
					}
					break;
				}
				case Instruction::Store: {
					auto ST = cast<StoreInst>(instr);
					if(ST->getType()->isPointerTy()) {
						auto new_ptr_type = PointerType::get(ST->getType()->getPointerElementType(), address_space);
						DBG(errs() << ">> ST: " << *ST << ", type: " << *ST->getType();)
						ST->mutateType(new_ptr_type);
						DBG(errs() << " -> " << *ST->getType() << "\n";)
					}
					break;
				}
				case Instruction::PHI: {
					auto phi = cast<PHINode>(instr);
					if(phi->getType()->isPointerTy()) {
						PointerType* new_ptr_type = nullptr;
						if (fix_inner_ptr) {
							auto inner_ptr = phi->getType()->getPointerElementType();
							if (inner_ptr->isPointerTy()) {
								new_ptr_type = PointerType::get(PointerType::get(inner_ptr->getPointerElementType(), address_space),
																phi->getType()->getPointerAddressSpace());
							}
						}
						if (!new_ptr_type) {
							new_ptr_type = PointerType::get(phi->getType()->getPointerElementType(), address_space);
						}
						DBG(errs() << ">> PHI: " << *phi << ", type: " << *phi->getType();)
						if (phi->getType() != new_ptr_type) {
							phi->mutateType(new_ptr_type);
							DBG(errs() << " -> " << *phi->getType() << "\n";)
						} else {
							DBG(errs() << " -> no update needed: " << *phi->getType() << "\n";)
							need_users_update = false;
						}
					} else {
						need_users_update = false;
					}
					break;
				}
				case Instruction::Select: {
					auto sel = cast<SelectInst>(instr);
					if (sel->getType()->isPointerTy()) {
						if (address_space != instr->getType()->getPointerAddressSpace()) {
							auto new_ptr_type = PointerType::get(sel->getType()->getPointerElementType(), address_space);
							DBG(errs() << ">> select: " << *sel << ", type: " << *sel->getType();)
							sel->mutateType(new_ptr_type);
							DBG(errs() << " -> " << *sel->getType() << "\n";)
						} else {
							need_users_update = false;
						}
					}
					break;
				}

				case Instruction::AddrSpaceCast:
				case Instruction::Invoke:
					// bad, should never happen
					ctx.emitError(instr, "encountered unsupported instruction");
					return;

				// uninteresting instructions:
				default:
					// nothing has changed, bail out
					return;
			}
			
			// don't recurse if instruction users don't need to be updated
			if (!need_users_update) {
				return;
			}
			
			// recursively fix all users
			rec_fix_users<fix_call_instrs>(pass, ctx, instr, parent, address_space, fix_inner_ptr, returns, was_modified, needs_rerun);
		}
		
		//! fixes the return type of the specified function if "returns" specify a different return type,
		//! returns true if the return type was modified
		static bool fix_returns(llvm::Function* func, std::vector<ReturnInst*>& returns) {
			if (returns.empty()) {
				return false;
			}
			
			DBG(errs() << ">> fixing returns: " << returns.size() << "\n";)
			LLVMContext* ctx = &func->getContext();
			std::unordered_set<Type*> ret_types;
			for (const auto& ret : returns) {
				// shouldn't occur (there'd be no initial user for this)
				if (!ret->getReturnValue()) {
					continue;
				}
				ret_types.emplace(ret->getReturnValue()->getType());
			}
			
			// this shouldn't happen if we get here, but still check for it
			if (ret_types.empty()) {
				ctx->emitError("must have a return type in function " + func->getName().str());
				return false;
			}
			// if there is more than one expected return type we have a problem
			else if (ret_types.size() > 1) {
				// TODO: should try and fix this properly (create alloca in caller + store result in there?)
				// TODO: for targets with a generic address space, might want to use that instead
				ctx->emitError("more than one return type in function " + func->getName().str());
				return false;
			}
			
			if ((*ret_types.begin())->isPointerTy() &&
				(*ret_types.begin())->getPointerAddressSpace() != func->getReturnType()->getPointerAddressSpace()) {
				// fix func return type
				std::vector<Type*> param_types;
				for(const auto& arg : func->args()) {
					param_types.push_back(arg.getType());
				}
				auto new_ret_type = *ret_types.begin();
				auto new_func_type = FunctionType::get(new_ret_type, param_types, false);
				DBG({
					auto old_func_type = func->getFunctionType();
					errs() << ">> fixing return type: " << func->getName() << ": " << *old_func_type << " -> " << *new_func_type << "\n";
				})
				func->mutateType(PointerType::get(new_func_type, 0));
				func->mutateFunctionType(new_func_type);
				return true;
			}
			return false;
		}
		
		struct as_fix_arg_info {
			uint32_t index;
			uint32_t address_space;
			bool read_only_fix;
		};
		
		// returns true if the return type changed
		static void fix_function(Pass& pass, llvm::Function* func, const std::vector<as_fix_arg_info>& args, const bool is_top_call,
								 const bool fix_inner_ptr, bool& was_modified, bool& needs_rerun) {
			LLVMContext* ctx = &func->getContext();
			std::vector<ReturnInst*> returns; // returns to fix
			for(const auto& arg : args) {
				if(arg.read_only_fix) continue;
				
				Argument& func_arg = *(std::next(func->arg_begin(), arg.index));
				libfloor_utils::for_all_instruction_users(func_arg, [&pass, &func_arg, &arg, fix_inner_ptr, &returns, ctx,
																	 &was_modified, &needs_rerun](Instruction& instr) {
					DBG(errs() << ">> replacing use: " << instr << "\n";)
					fix_users(&pass, *ctx, &instr, &func_arg, arg.address_space, fix_inner_ptr, returns, was_modified, needs_rerun);
				});
				DBG(errs() << "<< fixed arg: " << arg.index << "\n";)
			}
			
			fix_returns(func, returns);
			
			DBG(errs() << "<< fixed func\n";)
		}
		
		static void fix_call(Pass& pass, CallInst& CI, const std::vector<as_fix_arg_info>& args, const bool is_top_call, bool& was_modified, bool& needs_rerun) {
			bool need_clone = false, need_read_only_fix = false;
			for(const auto& arg : args) {
				if(!arg.read_only_fix) need_clone = true;
				else need_read_only_fix = true;
			}
			
			// find first non-alloca instruction in the entry block of the function
			// -> this will be the insert position for new alloca instructions
			Instruction* alloca_insert { nullptr };
			if(need_read_only_fix) {
				for(auto& instr : *CI.getParent()->getParent()->begin()) {
					if(!isa<AllocaInst>(instr)) {
						alloca_insert = &instr;
						break;
					}
				}
			}
			
			// first: fix all read-only args
			if(need_read_only_fix) {
				for(const auto& arg : args) {
					if(!arg.read_only_fix) continue;
					
					//  * create a temporary object (of the element/pointee type of the address space pointer)
					//  * load data from the address space pointer to the temp object
					//  * replace the respective call operand/argument with a pointer to the temp object
					DBG(errs() << "\tread-only fix: arg #" << arg.index << "\n";)
					
					// TODO: handle alignment?
					
					auto call_arg = CI.getOperand(arg.index);
					
					auto tmp = new AllocaInst(call_arg->getType()->getPointerElementType(),
											  0 /* no AS */,
											  // what about arrays?
											  nullptr,
											  // give it a nice name
											  "asfixtmp",
											  // insert alloca at function entry
											  alloca_insert);
					
					// insert load+store before call
					auto ld = new LoadInst(call_arg->getType()->getPointerElementType(), call_arg, "", &CI);
					new StoreInst(ld, tmp, &CI);
					
					CI.setOperand(arg.index, tmp);
				}
			}
			
			// second: create cloned function if this is necessary
			if(need_clone) {
				// fix it:
				//  * clone the called function and modify the appropriate argument so that it uses the correct address space
				//  * recursively go through the cloned called function and appropriately change all uses of our modified argument
				//    NOTE: this can very well recursively clone and fix called functions in there (and so on ...)
				//  * modify this call so that it calls the fixed/cloned function
				
				auto called_func = CI.getCalledFunction();
				DBG(errs() << "\tclone fix: " << called_func->getName() << "\n";)
				
				std::vector<Type*> param_types;
				std::string func_name = called_func->getName().str();
				uint32_t arg_num = 0;
				for(const auto& func_arg : called_func->args()) {
					auto call_arg = CI.getOperand(arg_num);
					
					bool arg_clone_fix = false;
					for(const auto& arg : args) {
						if(arg.index != arg_num ||
						   arg.read_only_fix) {
							continue;
						}
						
						arg_clone_fix = true;
						func_name += "." + std::to_string(arg.index) + "_" + std::to_string(arg.address_space);
						break;
					}
					
					if(!arg_clone_fix) {
						// use original type
						param_types.push_back(func_arg.getType());
					}
					else {
						// use "called with" type
						param_types.push_back(call_arg->getType());
					}
					
					++arg_num;
				}
				
				// check if cloned function already exists
				auto M = CI.getModule();
				auto cloned_func = M->getFunction(func_name);
				if(cloned_func == nullptr) {
					// only do this once
					auto cloned_func_type = FunctionType::get(called_func->getReturnType(), param_types, false);
					cloned_func = dyn_cast<Function>(M->getOrInsertFunction(func_name, cloned_func_type).getCallee());
					
					ValueToValueMapTy VMap;
					Function::arg_iterator DestI = cloned_func->arg_begin();
					for (const auto& I : called_func->args()) {
						DestI->setName(I.getName());
						VMap[&I] = &*DestI++;
					}
					
					SmallVector<llvm::ReturnInst*, 8> returns;
					llvm::CloneFunctionInto(cloned_func, called_func, VMap, CloneFunctionChangeType::GlobalChanges /* must be set for debug info */, returns);
					
#if 0 // for debugging purposes
					cloned_func->addFnAttr(Attribute::NoInline);
					cloned_func->removeFnAttr(Attribute::AlwaysInline);
#endif
					
					DBG(errs() << "\n>> before <<\n" << *cloned_func);
					
					//
					fix_function(pass, cloned_func, args, is_top_call, false, was_modified, needs_rerun);
					CI.setCalledFunction(cloned_func, true);
					CI.mutateType(cloned_func->getReturnType());
					
					DBG(errs() << "\n>> after <<\n" << *cloned_func);
				}
				else {
					DBG(errs() << "\t" << func_name << " already cloned\n";)
					CI.setCalledFunction(cloned_func, true);
					CI.mutateType(cloned_func->getReturnType());
				}
			}
		}
		
		void visitCallInst(CallInst& CI) {
			fix_call_instr(*this, CI, *ctx, true, was_modified, needs_rerun);
		}
		
		static void fix_return_users(Pass& pass, CallInst& CI, LLVMContext& ctx, const PointerType* ret_ptr_type, bool& was_modified, bool& needs_rerun) {
			const auto func = CI.getParent()->getParent();
			const auto address_space = ret_ptr_type->getAddressSpace();
			
			// recursively fix all users
			std::vector<ReturnInst*> returns;
			rec_fix_users<false>(&pass, ctx, &CI, nullptr, address_space, false /* must keep inner ptrs as-is */, returns, was_modified, needs_rerun);
			
			// for non-entry-points we may need to recursively change the return type of *this* function itself,
			// in which case we also need to fix all external callers
			const auto is_entry_point = CallingConv::isFloorEntryPoint(func->getCallingConv());
			if (!is_entry_point && !returns.empty()) {
				if (fix_returns(func, returns)) {
					// return type changed -> need to update external callers
					// however: since we're currently deep within an instruction visitor, we can't easily do this here -> perform another pass on the outside (ModulePass)
					needs_rerun = true;
				}
			}
		}
		
		static void fix_call_instr(Pass& pass, CallInst& CI, LLVMContext& ctx, const bool is_top_call, bool& was_modified, bool& needs_rerun) {
			auto called_func = CI.getCalledFunction();
			if (!called_func) {
				// TODO: properly handle intrinsics
				return;
			}
			
			PointerType* FPTy = cast<PointerType>(CI.getCalledOperand()->getType());
			FunctionType* FTy = cast<FunctionType>(FPTy->getPointerElementType());
			
			// before all else: check if the return type still mismatches
			// -> if it doesn't, there may have been a previous fix that modified the address space, so fix up all users first
			const auto func_ret_type = FTy->getReturnType();
			if (func_ret_type != CI.getType()) {
				if (func_ret_type->isPointerTy() && CI.getType()->isPointerTy()) {
					assert(func_ret_type->getPointerElementType() == CI.getType()->getPointerElementType());
					CI.setCalledFunction(called_func, true);
					CI.mutateType(func_ret_type);
					fix_return_users(pass, CI, ctx, dyn_cast<PointerType>(func_ret_type), was_modified, needs_rerun);
					was_modified = true;
				} else {
					ctx.emitError(&CI, "encountered invalid call instruction return type mismatch");
					return;
				}
			}
			
			std::vector<as_fix_arg_info> fix_args;
			for (unsigned i = 0, e = FTy->getNumParams(); i != e; ++i) {
				// check if there is a type mismatch
				if(CI.getOperand(i)->getType() != FTy->getParamType(i)) {
					// both types must be pointers
					auto arg = CI.getOperand(i);
					auto called_arg_type = arg->getType();
					auto expected_arg_type = FTy->getParamType(i);
					if(!called_arg_type->isPointerTy() ||
					   !expected_arg_type->isPointerTy()) {
						// emit original verifier assertion (TODO: fix it there!)
						assert(false && "#1: Call parameter type does not match function signature!");
						continue;
					}
					
					// check if the mismatch is _only_ due to the addrspace
					auto as_ptr = cast<PointerType>(called_arg_type);
					if(PointerType::get(as_ptr->getPointerElementType(),
										expected_arg_type->getPointerAddressSpace()) !=
					   expected_arg_type) {
						// emit original verifier assertion (TODO: fix it there!)
						assert(false && "#2: Call parameter type does not match function signature!");
						continue;
					}
					// else: yup, only addrspace mismatch
					DBG(errs() << "#####################################################\n";)
					DBG(errs() << "\t>> call to: "; called_func->llvm::Value::getType()->dump();)
					DBG(errs() << "\n\t>> call: " << CI << "\n";)
					DBG(errs() << "\t>> full: " << called_func->getName() << "\n";)
					DBG(errs() << "\treplacing arg #" << i << "!\n";)
					DBG(errs() << "\t" << called_arg_type->getPointerAddressSpace() << ", ";)
					DBG(errs() << expected_arg_type->getPointerAddressSpace() << "\n";)
					DBG(errs() << "\tgot: "; called_arg_type->dump();)
					DBG(errs() << "\texpected: "; expected_arg_type->dump();)
					DBG({
						int err = 0;
						const char* demangled_name = abi::__cxa_demangle(called_func->getName().data(), 0, 0, &err);
						errs() << "\tfunc: " << (demangled_name != nullptr ? demangled_name : called_func->getName().data()) << "\n";
						if(demangled_name != nullptr) {
							free((void*)demangled_name);
						}
					})
					
					// abort if arg is an addrspacecast and pretend everything is fine ("someone else" is already making it fit)
					if(isa<AddrSpaceCastInst>(arg)) {
						DBG(errs() << "\tabort due to existing addrspacecast: " << *arg << "\n";)
						continue;
					}
					
					// the same goes for bitcasts, unless src and dst have the same address space
					if(const auto BCI = dyn_cast_or_null<BitCastInst>(arg)) {
						if(BCI->getSrcTy()->getPointerAddressSpace() != BCI->getDestTy()->getPointerAddressSpace()) {
							DBG(errs() << "\tabort due to existing bitcast: " << *arg << "\n";)
							continue;
						}
						// else: perform address space fix, b/c this is a simple pointer/type cast
						DBG(errs() << "\tkeeping bitcast: " << *arg << "\n";)
					}
					
					// abort if expected param address space is not 0, this is not supported (or would end in a good way ...)
					// TODO: figure out if it would be a good idea to allow things like loading from e.g. local AS and calling a global AS function
					if(expected_arg_type->getPointerAddressSpace() != 0) {
						DBG(errs() << "\tabort due to expected AS not being 0\n";)
						continue;
					}
					
					// abort if current argument is in address space 0 (can't be moved to an address space)
					if(called_arg_type->getPointerAddressSpace() == 0) {
						DBG(errs() << "\tabort due to arg already being in AS 0\n";)
						continue;
					}
					
					// query information that decides if a store would be necessary later on,
					// i.e. the arg is not read-only and access to it must happen using the actual arg
					const bool is_constant_as = (as_ptr->getPointerAddressSpace() == 2);
					const bool is_readonly = CI.onlyReadsMemory(i);
					const bool is_load = isa<LoadInst>(arg);
					const auto is_intrinsic = called_func->isIntrinsic();
					const auto returns_ptr = called_func->getReturnType()->isPointerTy();
					// don't allow cloning/alloca-read-only-fix for certain constructs (e.g. intrinsics or too expensive or not allowed, especially arrays)
					bool is_clonable = !is_intrinsic;
					const auto elem_type = as_ptr->getPointerElementType();
					if (is_clonable) {
						if (elem_type->isArrayTy()) {
							is_clonable = false;
						} else if (elem_type->isStructTy()) {
							// if this is a struct type, we need to fully traverse it to figure out if it may be cloneable or not
							const std::function<void(const llvm::StructType*)> traverse_st_type = [&traverse_st_type, &is_clonable, &is_constant_as](const llvm::StructType* st_type) {
								if (st_type->getName().startswith("class.floor_image::image") ||
									st_type->getName().startswith("class.fl::floor_image::image")) {
									is_clonable = false;
									return;
								}
								for (const auto& field_type : st_type->elements()) {
									const auto array_type = (field_type->isArrayTy() ? cast<llvm::ArrayType>(field_type) :
															 (field_type->isPointerTy() && field_type->getPointerElementType()->isArrayTy() ?
															  cast<llvm::ArrayType>(field_type->getPointerElementType()) : nullptr));
									if (array_type) {
										const auto elem_type = array_type->getElementType();
										if (!elem_type->isSized() || elem_type->isPointerTy()) {
											// -> can only clone sized and non-pointer types
											is_clonable = false;
										} else {
											if (elem_type->isStructTy()) {
												// -> recurse
												traverse_st_type(cast<llvm::StructType>(elem_type));
											}
											// -> don't clone arrays that are too large and don't originate from the constant address space
											if (!is_constant_as && is_clonable) {
												if (array_type->getNumElements() > 128) {
													is_clonable = false;
												}
											}
										}
										if (!is_clonable) {
											return;
										}
									} else if (field_type->isStructTy()) {
										traverse_st_type(cast<llvm::StructType>(field_type));
										if (!is_clonable) {
											return;
										}
									}
								}
							};
							traverse_st_type(cast<llvm::StructType>(elem_type));
						}
					}
					
					DBG(errs() << "\tread-only: " << is_constant_as << ", " << is_readonly << ", " << is_load << "; " << is_clonable << "; " << returns_ptr << "\n";)
					fix_args.push_back(as_fix_arg_info {
						i,
						as_ptr->getPointerAddressSpace(),
						// NOTE: when a function returns a pointer, we can currently not know where this pointer may come from,
						//       i.e. it might derive the pointer from *this* argument, in which case we must not use a read-only fix
						//       and have to clone the function instead
						(is_constant_as || is_readonly || is_load) && is_clonable && !returns_ptr,
					});
				}
			}
			
			if(!fix_args.empty()) {
				// retrieving AA directly in a module pass (as a dep) in llvm 3.8 is apparantly no longer possible,
				// so we have to this localized AA instead (specific to the current function)
				auto func = CI.getParent()->getParent();
				BasicAAResult BAR(createLegacyPMBasicAAResult(pass, *func));
				AAResults AA(createLegacyPMAAResults(pass, *func, BAR));
				
				// check if we have both clone and read-only fixes
				bool has_ro_fix = false, has_clone_fix = false;
				for(const auto& arg : fix_args) {
					if(arg.read_only_fix) has_ro_fix = true;
					else has_clone_fix = true;
				}
				if(has_ro_fix && has_clone_fix) {
					// if so, check AA for all read-only fix args and check if these alias with any clone fix arg
					for(auto& ro_arg : fix_args) {
						if(!ro_arg.read_only_fix) continue;
						for(const auto& clone_arg : fix_args) {
							if(clone_arg.read_only_fix) continue;
							const auto aa_res = AA.alias(CI.getOperand(ro_arg.index), CI.getOperand(clone_arg.index));
							if(aa_res != AliasResult::NoAlias) {
								// -> might or must alias
								// disable the read-only fix for this arg and use clone instead
								ro_arg.read_only_fix = false;
								DBG(errs() << "\tdisabling read-only fix for arg #" << ro_arg.index << " due to aliasing with clone arg #" << clone_arg.index << "\n";)
								break;
							}
						}
					}
				}
				
				// fix the call (+detect return type change)
				auto orig_ret_type = called_func->getReturnType();
				fix_call(pass, CI, fix_args, is_top_call, was_modified, needs_rerun);
				called_func = CI.getCalledFunction();
				auto fixed_ret_type = called_func->getReturnType();
				
				if (is_top_call && orig_ret_type != fixed_ret_type) {
					// if this is a top call and the return type changed: update users if return type is a pointer
					DBG(errs() << "\ttop call return type changed: " << *orig_ret_type << " -> " << *fixed_ret_type << " (in: " << func->getName() << ")\n");
					if (auto ret_ptr_type = dyn_cast<PointerType>(fixed_ret_type); ret_ptr_type) {
						fix_return_users(pass, CI, ctx, ret_ptr_type, was_modified, needs_rerun);
					}
				}
				
				// done, signal that the function was modified
				was_modified = true;
			}
		}
	};
}

namespace llvm {
void fix_instruction_users(LLVMContext &ctx,
						   Instruction &instr,
						   Value &parent,
						   const uint32_t address_space,
						   const bool fix_inner_ptr,
						   std::vector<ReturnInst *> &returns) {
	// NOTE: we can't fix call instructions here
	bool was_modified = false, needs_rerun = false;
	AddressSpaceFix::fix_users<false>(nullptr, ctx, &instr, &parent, address_space, fix_inner_ptr, returns, was_modified, needs_rerun);
	(void)was_modified;
	(void)needs_rerun;
	assert(!needs_rerun);
}

bool fix_instruction_users_with_calls(Pass& pass,
									  LLVMContext &ctx,
									  Instruction &instr,
									  Value &parent,
									  const uint32_t address_space,
									  const bool fix_inner_ptr,
									  std::vector<ReturnInst *> &returns) {
	bool was_modified = false, needs_rerun = false;
	AddressSpaceFix::fix_users<true>(&pass, ctx, &instr, &parent, address_space, fix_inner_ptr, returns, was_modified, needs_rerun);
	assert(!needs_rerun);
	return was_modified;
}
}

char AddressSpaceFix::ID = 0;
INITIALIZE_PASS_BEGIN(AddressSpaceFix, "AddressSpaceFix", "AddressSpaceFix Pass", false, false)
// add all the things (not fully depending on this in getAnalysisUsage)
INITIALIZE_PASS_DEPENDENCY(AAResultsWrapperPass)
INITIALIZE_PASS_DEPENDENCY(GlobalsAAWrapperPass)
INITIALIZE_PASS_DEPENDENCY(AssumptionCacheTracker)
INITIALIZE_PASS_DEPENDENCY(CallGraphWrapperPass)
INITIALIZE_PASS_DEPENDENCY(TargetLibraryInfoWrapperPass)
INITIALIZE_PASS_DEPENDENCY(PostOrderFunctionAttrsLegacyPass) // TODO: what's the new method of doing this?
INITIALIZE_PASS_END(AddressSpaceFix, "AddressSpaceFix", "AddressSpaceFix Pass", false, false)

ModulePass *llvm::createAddressSpaceFixPass() {
	return new AddressSpaceFix();
}
