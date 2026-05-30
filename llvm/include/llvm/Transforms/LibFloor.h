//===-- LibFloor.h - LibFloor Transformations -------------------*- C++ -*-===//
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
// This header file defines prototypes for accessor functions that expose passes
// in the LibFloor transformations library.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TRANSFORMS_LIBFLOOR_H
#define LLVM_TRANSFORMS_LIBFLOOR_H

#include "llvm/Transforms/Utils/SimplifyCFGOptions.h"
#include <functional>
#include <cstdint>

namespace llvm {

class Function;
class FunctionPass;
class ModulePass;
class Pass;

//! mesh shading attributes used for vertex and primitive data
enum class MESH_ATTRIBUTE : uint32_t {
	NONE = 0u,
	POSITION = 1u,
	POINT_SIZE = 2u,
	CULLED = 3u,
	__MAX_MESH_ATTRIBUTE
};

//! mesh shading primitive topology
enum class MESH_TOPOLOGY : uint32_t {
	POINT = 0u,
	LINE = 1u,
	TRIANGLE = 2u,
	__MAX_MESH_TOPOLOGY
};

//===----------------------------------------------------------------------===//
//
// AddressSpaceFix - This pass fixes (intentionally) broken uses of addrspace
// pointers that should be non-addrspace pointers.
//
ModulePass *createAddressSpaceFixPass();

//===----------------------------------------------------------------------===//
//
// CUDAImage - This pass applies CUDA-specific floor image transformations.
//
FunctionPass *createCUDAImagePass(const uint32_t image_capabilities = 0);

//===----------------------------------------------------------------------===//
//
// CUDAFinal - final pass, making CUDA related IR changes.
//
FunctionPass *createCUDAFinalPass();

//===----------------------------------------------------------------------===//
//
// MetalFirst - This pass fixes Metal/AIR issues.
//
FunctionPass *createMetalFirstPass(const bool enable_intel_workarounds = false);

//===----------------------------------------------------------------------===//
//
// MetalFinal - This pass fixes Metal/AIR issues.
//
FunctionPass *createMetalFinalPass(const bool enable_intel_workarounds = false);

//===----------------------------------------------------------------------===//
//
// MetalFinalModuleCleanup - This pass removes any calling convention attributes
// and removes unused functions/prototypes/externs.
//
ModulePass *createMetalFinalModuleCleanupPass();

//===----------------------------------------------------------------------===//
//
// MetalMemopLowering - Lowers memops where beneficial.
//
FunctionPass *createMetalMemopLoweringPass();

//===----------------------------------------------------------------------===//
//
// MetalImage - This pass applies Metal-specific floor image transformations.
//
FunctionPass *createMetalImagePass(const uint32_t image_capabilities = 0);

//===----------------------------------------------------------------------===//
//
// MetalMesh - This pass perform Metal specific mesh shading lowering.
//
FunctionPass *createMetalMeshPass();

//===----------------------------------------------------------------------===//
//
// SPIRFinal - This pass fixes LLVM IR to be SPIR-compliant.
//
FunctionPass *createSPIRFinalPass();

//===----------------------------------------------------------------------===//
//
// SPIRFinalModule - Fixes LLVM IR to be SPIR-compliant at the module level.
//
ModulePass *createSPIRFinalModulePass();

//===----------------------------------------------------------------------===//
//
// SPIRImage - This pass applies SPIR-specific floor image transformations.
//
FunctionPass *createSPIRImagePass(const uint32_t image_capabilities = 0,
                                  const bool enable_intel_workarounds = false);

//===----------------------------------------------------------------------===//
//
// CFGStructurization - This pass transforms the CFG into a structurized CFG.
//
FunctionPass *createCFGStructurizationPass();

//===----------------------------------------------------------------------===//
//
// VulkanEarlyArgBufferFunctionClone - Clones functions for arg buffer use.
//
ModulePass *createVulkanEarlyArgBufferFunctionClonePass();

//===----------------------------------------------------------------------===//
//
// VulkanImage - This pass applies SPIR-V-specific floor image transformations.
//
FunctionPass *createVulkanImagePass(const uint32_t image_capabilities = 0);

//===----------------------------------------------------------------------===//
//
// VulkanMesh - This pass perform Vulkan specific mesh shading lowering.
//
FunctionPass *createVulkanMeshPass();

//===----------------------------------------------------------------------===//
//
// VulkanFinal - This pass fixes Vulkan/SPIR-V issues.
//
FunctionPass *createVulkanFinalPass();

//===----------------------------------------------------------------------===//
//
// VulkanBuiltinParamHandling - This pass handles builtin -> parameter
// replacement for Vulkan.
//
FunctionPass *createVulkanBuiltinParamHandlingPass();

//===----------------------------------------------------------------------===//
//
// VulkanPreFinal - This pass fixes Vulkan/SPIR-V issues, prior to CFG
// structurization and VulkanFinal.
//
FunctionPass *createVulkanPreFinalPass();

//===----------------------------------------------------------------------===//
//
// VulkanFinalModuleCleanup - This pass removes unused functions/etc.
//
ModulePass *createVulkanFinalModuleCleanupPass();

//===----------------------------------------------------------------------===//
//
// PropagateCoherency - This pass propagates memory coherency and implements
// backend specific transformations.
//
FunctionPass *createPropagateCoherencyPass();

//===----------------------------------------------------------------------===//
//
// PointerBCFixup - This pass fixes/improves unfortunate pointer bitcasts.
//
FunctionPass *createPointerBCFixupPass();

//===----------------------------------------------------------------------===//
//
// PropagateRangeInfo - This pass propagates range metadata info.
//
FunctionPass *createPropagateRangeInfoPass();

//===----------------------------------------------------------------------===//
//
// FMACombiner - This pass combines and recombines fmul/fadd/fsub/fneg/fma
// instructions to fma instructions.
//
FunctionPass *createFMACombinerPass();

//===----------------------------------------------------------------------===//
//
// FloorModuleCleanup - Performs final module cleanup in all backends.
//
ModulePass *createFloorModuleCleanupPass();

} // End llvm namespace

#endif
