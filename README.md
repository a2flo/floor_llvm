# LibFloor LLVM Toolchain

This repository provides the modified clang/LLVM/libc++ toolchain that enables same-source CUDA/Host/Metal/OpenCL/Vulkan C++ programming as used by [libfloor](https://github.com/a2flo/floor).

To build this, clone the libfloor repository and run: https://github.com/a2flo/floor/blob/master/etc/llvm140/build.sh

Automated Linux/macOS/Windows builds can be found at: https://libfloor.org/builds/toolchain

Note that this includes a modified [SPIRV-LLVM-Translator](https://github.com/a2flo/floor_llvm_spirv) repository as the SPIR-V backend for Vulkan and OpenCL. Please ensure to run `git submodule init` initially and `git submodule update` on each repository update.
