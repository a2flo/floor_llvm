//===--- SPIR.h - Declare SPIR target feature support -----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares SPIR TargetInfo objects.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_BASIC_TARGETS_SPIR_H
#define LLVM_CLANG_LIB_BASIC_TARGETS_SPIR_H

#include "clang/Basic/TargetInfo.h"
#include "clang/Basic/TargetOptions.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Support/Compiler.h"

namespace clang {
namespace targets {

static const unsigned SPIRDefIsPrivMap[] = {
    0, // Default
    1, // opencl_global
    3, // opencl_local
    2, // opencl_constant
    0, // opencl_private
    4, // opencl_generic
    100, // opencl_global_device
    101, // opencl_global_host
    0, // vulkan_input
    0, // cuda_device
    0, // cuda_constant
    0, // cuda_shared
    // SYCL address space values for this map are dummy
    0, // sycl_global
    0, // sycl_global_device
    0, // sycl_global_host
    0, // sycl_local
    0, // sycl_private
    0, // ptr32_sptr
    0, // ptr32_uptr
    0  // ptr64
};

static const unsigned SPIRDefIsGenMap[] = {
    4, // Default
    // OpenCL address space values for this map are dummy and they can't be used
    0, // opencl_global
    0, // opencl_local
    0, // opencl_constant
    0, // opencl_private
    0, // opencl_generic
    0, // opencl_global_device
    0, // opencl_global_host
    0, // vulkan_input
    0, // cuda_device
    0, // cuda_constant
    0, // cuda_shared
    1, // sycl_global
    5, // sycl_global_device
    6, // sycl_global_host
    3, // sycl_local
    0, // sycl_private
    0, // ptr32_sptr
    0, // ptr32_uptr
    0  // ptr64
};

// Vulkan/SPIR-V uses its own storage classes
static const unsigned VulkanAddrSpaceMap[] = {
    0, // Default
	12, // opencl_global == SPIRAS_StorageBuffer
    3, // opencl_local
    2, // opencl_constant
    0, // opencl_private
    4, // opencl_generic
    0, // opencl_global_device
    0, // opencl_global_host
    6, // vulkan_input == SPIRAS_Input
    0, // cuda_device
    0, // cuda_constant
    0, // cuda_shared
    // SYCL address space values for this map are dummy
    0, // sycl_global
    0, // sycl_global_device
    0, // sycl_global_host
    0, // sycl_local
    0, // sycl_private
    0, // ptr32_sptr
    0, // ptr32_uptr
    0  // ptr64
};

class LLVM_LIBRARY_VISIBILITY SPIRTargetInfo : public TargetInfo {
private:
  // true for spir-unknown-* and spir64-unknown-* (-> false for AIR/Metal)
  const bool is_pure_spir;
  const bool is_vulkan;
public:
  SPIRTargetInfo(const llvm::Triple &Triple, const TargetOptions &)
      : TargetInfo(Triple), is_pure_spir(Triple.getVendorName().str() == "unknown"),
        is_vulkan(Triple.getEnvironment() == llvm::Triple::EnvironmentType::Vulkan) {
    TLSSupported = false;
    VLASupported = false;
    LongWidth = LongAlign = 64;
    AddrSpaceMap = (!is_vulkan ? &SPIRDefIsPrivMap : &VulkanAddrSpaceMap);
    UseAddrSpaceMapMangling = true;
    HasLegalHalfType = true;
    HasFloat16 = true;
    // Define available target features
    // These must be defined in sorted order!
    NoAsmVariants = true;
  }

  void getTargetDefines(const LangOptions &Opts,
                        MacroBuilder &Builder) const override;

  bool hasFeature(StringRef Feature) const override {
    return Feature == "spir";
  }

  bool isCLZForZeroUndef() const override { return false; }
  bool isVulkan() const { return is_vulkan; }

  // SPIR supports the half type and the only llvm intrinsic allowed in SPIR is
  // memcpy as per section 3 of the SPIR spec.
  bool useFP16ConversionIntrinsics() const override { return false; }

  ArrayRef<Builtin::Info> getTargetBuiltins() const override { return None; }

  const char *getClobbers() const override { return ""; }

  ArrayRef<const char *> getGCCRegNames() const override { return None; }

  bool validateAsmConstraint(const char *&Name,
                             TargetInfo::ConstraintInfo &info) const override {
    return true;
  }

  ArrayRef<TargetInfo::GCCRegAlias> getGCCRegAliases() const override {
    return None;
  }

  BuiltinVaListKind getBuiltinVaListKind() const override {
    return TargetInfo::VoidPtrBuiltinVaList;
  }

  Optional<unsigned>
  getDWARFAddressSpace(unsigned AddressSpace) const override {
    return AddressSpace;
  }

  CallingConvCheckResult checkCallingConvention(CallingConv CC) const override {
    if (!is_pure_spir) return CCCR_OK;
    if (CC == CC_FloorFunction ||
        CC == CC_FloorVertex ||
        CC == CC_FloorFragment ||
        CC == CC_FloorKernel) {
        return CCCR_OK;
    }
    return CCCR_Warning;
  }

  CallingConv getDefaultCallingConv() const override {
    return (is_pure_spir ? CC_FloorFunction : CC_C);
  }

  void setAddressSpaceMap(bool DefaultIsGeneric) {
    AddrSpaceMap = (is_vulkan ? &VulkanAddrSpaceMap : (DefaultIsGeneric ? &SPIRDefIsGenMap : &SPIRDefIsPrivMap));
  }

  void adjust(DiagnosticsEngine &Diags, LangOptions &Opts) override {
    TargetInfo::adjust(Diags, Opts);
    // FIXME: SYCL specification considers unannotated pointers and references
    // to be pointing to the generic address space. See section 5.9.3 of
    // SYCL 2020 specification.
    // Currently, there is no way of representing SYCL's default address space
    // language semantic along with the semantics of embedded C's default
    // address space in the same address space map. Hence the map needs to be
    // reset to allow mapping to the desired value of 'Default' entry for SYCL.
    setAddressSpaceMap(/*DefaultIsGeneric=*/Opts.SYCLIsDevice);
  }

  void setSupportedOpenCLOpts() override {
    // Assume all OpenCL extensions and optional core features are supported
    // for SPIR since it is a generic target.
    supportAllOpenCLOpts();
  }

  bool hasExtIntType() const override { return true; }

  bool hasInt128Type() const override { return true; }

  llvm::Optional<LangAS> getConstantAddressSpace() const override {
    return LangAS::opencl_constant;
  }
};

class LLVM_LIBRARY_VISIBILITY SPIR32TargetInfo : public SPIRTargetInfo {
public:
  SPIR32TargetInfo(const llvm::Triple &Triple, const TargetOptions &Opts)
      : SPIRTargetInfo(Triple, Opts) {
    PointerWidth = PointerAlign = 32;
    SizeType = TargetInfo::UnsignedInt;
    PtrDiffType = IntPtrType = TargetInfo::SignedInt;
    resetDataLayout("e-p:32:32-i64:64-v16:16-v24:32-v32:32-v48:64-"
                    "v96:128-v192:256-v256:256-v512:512-v1024:1024"
                    "-n8:16:32:64");
  }

  void getTargetDefines(const LangOptions &Opts,
                        MacroBuilder &Builder) const override;
};

class LLVM_LIBRARY_VISIBILITY SPIR64TargetInfo : public SPIRTargetInfo {
public:
  SPIR64TargetInfo(const llvm::Triple &Triple, const TargetOptions &Opts)
      : SPIRTargetInfo(Triple, Opts) {
    PointerWidth = PointerAlign = 64;
    SizeType = TargetInfo::UnsignedLong;
    PtrDiffType = IntPtrType = TargetInfo::SignedLong;
    resetDataLayout("e-p:64:64-i64:64-v16:16-v24:32-v32:32-v48:64-"
                    "v96:128-v192:256-v256:256-v512:512-v1024:1024"
                    "-n8:16:32:64");
  }

  void getTargetDefines(const LangOptions &Opts,
                        MacroBuilder &Builder) const override;
};

// Metal/AIR target based on SPIR
class LLVM_LIBRARY_VISIBILITY AIR64TargetInfo : public SPIRTargetInfo {
public:
  AIR64TargetInfo(const llvm::Triple &Triple, const TargetOptions &TO) : SPIRTargetInfo(Triple, TO) {
    PointerWidth = PointerAlign = 64;
    SizeType     = TargetInfo::UnsignedLong;
    PtrDiffType = IntPtrType = TargetInfo::SignedLong;
    if(Triple.getOS() == llvm::Triple::IOS) {
      resetDataLayout("e-i64:64-f80:128-v16:16-v24:32-v32:32-v48:64-v96:128-v192:256-v256:256-v512:512-v1024:1024-n8:16:32");
    } else { // macOS, or default
      resetDataLayout("e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-f80:128:128-v16:16:16-v24:32:32-v32:32:32-v48:64:64-v64:64:64-v96:128:128-v128:128:128-v192:256:256-v256:256:256-v512:512:512-v1024:1024:1024-f80:128:128-n8:16:32");
    }
  }
  
  void getTargetDefines(const LangOptions &Opts,
                        MacroBuilder &Builder) const override;

  CallingConvCheckResult checkCallingConvention(CallingConv CC) const override {
    if (CC == CC_FloorFunction ||
        CC == CC_FloorVertex ||
        CC == CC_FloorFragment ||
        CC == CC_FloorKernel) {
        return CCCR_OK;
    }
    return CCCR_Warning;
  }

  CallingConv getDefaultCallingConv() const override {
    return CC_FloorFunction;
  }
};

} // namespace targets
} // namespace clang
#endif // LLVM_CLANG_LIB_BASIC_TARGETS_SPIR_H
