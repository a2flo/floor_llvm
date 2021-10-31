add_llvm_component_library(LLVMMetalLib
  MetalLibWriter.cpp
  MetalLibWriterPass.cpp

  DEPENDS
  intrinsics_gen
  LLVMBitWriter50

  LINK_COMPONENTS
  Analysis
  Core
  MC
  Object
  Support
  )
