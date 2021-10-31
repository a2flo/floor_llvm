//===-- FloorGPUTTI.h - LibFloor GPU TTI implementation ---------*- C++ -*-===//
//
//  Flo's Open libRary (floor)
//  Copyright (C) 2004 - 2021 Florian Ziesche
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
// TODO !
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TRANSFORMS_LIBFLOOR_FLOORGPUTTI_H
#define LLVM_TRANSFORMS_LIBFLOOR_FLOORGPUTTI_H

#include "clang/Basic/CodeGenOptions.h"
#include "clang/Basic/LangOptions.h"
#include "clang/Basic/TargetOptions.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/CodeGen/BasicTTIImpl.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/IR/Module.h"

namespace llvm {

//! (dummy) libfloor GPU target machine
class LibFloorGPUTargetMachine : public LLVMTargetMachine {
public:
	// TODO: DataLayout, TargetOptions
	explicit LibFloorGPUTargetMachine() :
	LLVMTargetMachine({}, "", {}, "", "", {},
					  Reloc::Static, CodeModel::Tiny, CodeGenOpt::Aggressive) {}
	
	static LibFloorGPUTargetMachine* get_instance() {
		static LibFloorGPUTargetMachine instance;
		return &instance;
	}
	
	bool isNoopAddrSpaceCast(unsigned FromAS, unsigned ToAS) const override {
		return (FromAS == ToAS);
	}
	
};

//! (dummy) libfloor GPU sub-target
class LibFloorGPUSubTarget : public TargetSubtargetInfo {
public:
	// TODO: Triple, "CPU"?
	LibFloorGPUSubTarget() :
	TargetSubtargetInfo({}, "", "", "", {}, {}, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr) {}
	
	static LibFloorGPUSubTarget* get_instance() {
		static LibFloorGPUSubTarget instance;
		return &instance;
	}
	
	// TODO: getSchedModel, getCacheSize, getCacheLineSize, getPrefetchDistance, getMinPrefetchStride, getMaxPrefetchIterationsAhead, enableWritePrefetching, getCacheAssociativity, useAA,
	
};

//! (dummy) libfloor GPU target lowering
class LibFloorGPUTargetLowering : public TargetLowering {
public:
	explicit LibFloorGPUTargetLowering() :
	TargetLowering(*LibFloorGPUTargetMachine::get_instance()) {}
	
	static LibFloorGPUTargetLowering* get_instance() {
		static LibFloorGPUTargetLowering instance;
		return &instance;
	}
	
	// TODO: allowsMisalignedMemoryAccesses,
	
};

//! libfloor target transform implementation
class LibFloorGPUTTIImpl : public BasicTTIImplBase<LibFloorGPUTTIImpl> {
public:
	using crtp_base_class = BasicTTIImplBase<LibFloorGPUTTIImpl>;
	friend crtp_base_class;

	explicit LibFloorGPUTTIImpl(const Function& F, const clang::CodeGenOptions& CodeGenOpts_,
								const clang::TargetOptions& TargetOpts_,
								const clang::LangOptions& LangOpts_,
								Module& M_) :
	crtp_base_class(nullptr /* TargetMachine is not necessary */, F.getParent()->getDataLayout()),
	CodeGenOpts(CodeGenOpts_), TargetOpts(TargetOpts_), LangOpts(LangOpts_), M(M_) {}
	
	//! restrict to width of 4
	unsigned getMaximumVF(unsigned, unsigned) const { return 4; }
	
	//! yes - this is a GPU target
	bool hasBranchDivergence() const { return true; }
	
	//! yes - this is a GPU target
	bool useGPUDivergenceAnalysis() const { return true; }
	
	//! yes - we'll inline everything anyways
	bool areInlineCompatible(const Function*, const Function*) const { return true; }
	
	/*InstructionCost getIntrinsicInstrCost(const IntrinsicCostAttributes &ICA,
										  TTI::TargetCostKind CostKind) {
		;;
	}*/
	InstructionCost getMemcpyCost(const Instruction *I) const {
		return 255;
	}
	
protected:
	const clang::CodeGenOptions& CodeGenOpts;
	const clang::TargetOptions& TargetOpts;
	const clang::LangOptions& LangOpts;
	const Module& M;
	
	const LibFloorGPUSubTarget* ST {
		LibFloorGPUSubTarget::get_instance()
	};
	const LibFloorGPUTargetLowering* TLI {
		LibFloorGPUTargetLowering::get_instance()
	};
	
public:
	const LibFloorGPUSubTarget* getST() const {
		return ST;
	}
	
	const LibFloorGPUTargetLowering* getTLI() const {
		return TLI;
	}
	
};

} // namespace llvm

#endif
