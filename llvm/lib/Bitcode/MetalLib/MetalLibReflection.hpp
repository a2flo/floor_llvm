//===-- MetalLibReflection.hpp - Metal AIR reflection handling --*- C++ -*-===//
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
// This extracts AIR metadata from LLVM IR and converts it into the RBUF/AIRR
// flatbuffer format that can be used in metallibs.
//
//===----------------------------------------------------------------------===//

#pragma once

#include <utility>
#include <memory>
#include <cstdint>
#include "llvm/IR/Module.h"

namespace metal::reflection {

std::vector<uint8_t> create_reflection(llvm::Module& M);

} // namespace metal::reflection
