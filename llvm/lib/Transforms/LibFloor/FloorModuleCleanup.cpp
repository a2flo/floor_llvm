//===- FloorModuleCleanup.cpp - final module cleanup ----------------------===//
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

#include "llvm/InitializePasses.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/PassRegistry.h"
#include "llvm/Transforms/LibFloor.h"
#include "llvm/Transforms/LibFloor/FloorUtils.h"
using namespace llvm;

#if defined(DEBUG_TYPE)
#undef DEBUG_TYPE
#endif
#define DEBUG_TYPE "FloorModuleCleanup"

#if 1
#define DBG(x)
#else
#define DBG(x) x
#endif

namespace {

struct FloorModuleCleanup : public ModulePass, InstVisitor<FloorModuleCleanup> {
	static char ID; // Pass identification, replacement for typeid
	
	Module* M { nullptr };
	LLVMContext* ctx { nullptr };
	bool was_modified { false };
	
	FloorModuleCleanup() : ModulePass(ID) {
		initializeFloorModuleCleanupPass(*PassRegistry::getPassRegistry());
	}
	
	void runOnFunction(Function& F) {
		visit(F);
		
		for (auto& BB : F) {
			for (auto& I : BB) {
				// strip !range info from instructions that shouldn't have it (any more)
				if (!isa<LoadInst>(I) && !isa<CallInst>(I) && !isa<InvokeInst>(I)) {
					if (I.getMetadata(LLVMContext::MD_range) != nullptr) {
						I.eraseMetadata(LLVMContext::MD_range);
					}
				}
			}
		}
	}
	
	// prefer i32 (or any originating type <= 32-bit) indices where we can
	void visitGetElementPtrInst(GetElementPtrInst &I) {
		was_modified |= libfloor_utils::simplify_gep_indices(*ctx, I);
	}
	void visitExtractElement(ExtractElementInst& EEI) {
		const auto idx_op = EEI.getIndexOperand();
		const auto idx_type = idx_op->getType();
		if (!idx_type->isIntegerTy(32)) {
			libfloor_utils::simplify_integer_to_32bit(*idx_op, true /* kill unused */, true,
													  [&EEI, this](llvm::Value* new_op) {
				EEI.setOperand(1 /* idx op */, new_op);
				was_modified = true;
			});
		}
	}
	void visitInsertElement(InsertElementInst& IEI) {
		const auto idx_op = IEI.getOperand(2);
		const auto idx_type = idx_op->getType();
		if (!idx_type->isIntegerTy(32)) {
			libfloor_utils::simplify_integer_to_32bit(*idx_op, true /* kill unused */, true,
													  [&IEI, this](llvm::Value* new_op) {
				IEI.setOperand(2 /* idx op */, new_op);
				was_modified = true;
			});
		}
	}
	
	
	void visitAlloca(AllocaInst& alloca) {
		if (ctx->get_libfloor_options().error_on_alloca) {
			ctx->emitError(&alloca, "leftover alloca after optimization");
		}
		if (ctx->get_libfloor_options().error_on_ptr_type_alloca &&
			alloca.getAllocatedType()->isPointerTy()) {
			ctx->emitError(&alloca, "leftover alloca with a pointer type after optimization");
		}
	}
	
	bool runOnModule(Module& Mod) override {
		M = &Mod;
		ctx = &M->getContext();
		
		was_modified = false;
		for (auto& F : Mod) {
			runOnFunction(F);
		}
		return was_modified;
	}
	
};

} // namespace

char FloorModuleCleanup::ID = 0;
ModulePass *llvm::createFloorModuleCleanupPass() {
	return new FloorModuleCleanup();
}
INITIALIZE_PASS_BEGIN(FloorModuleCleanup, "final module cleanup", "final module cleanup Pass", false, false)
INITIALIZE_PASS_END(FloorModuleCleanup, "final module cleanup", "final module cleanup Pass", false, false)
