/* Copyright (c) 2019-2022 Hans-Kristian Arntzen for Valve Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
//==-----------------------------------------------------------------------===//
//
// dxil-spirv CFG structurizer adopted for LLVM use
// ref: https://github.com/HansKristian-Work/dxil-spirv
// @ 09b2677af3535316a8b98ac0a4dd01b96577718b
//
//===----------------------------------------------------------------------===//

#pragma once

#include <assert.h>
#include <initializer_list>
#include <stdint.h>
#include <vector>
#include <optional>
#include <type_traits>
#include "llvm/IR/CFG.h"

// A simple IR representation which allows the CFGStructurizer to do some simple
// rewrites of blocks, PHI nodes in particular.

namespace llvm {

enum class MergeType { None, Loop, Selection };

class ConstantInt;
struct CFGNode;

enum class SpvLoopControlMask : uint32_t {
  None = 0,
  Unroll = 0x00000001,
  DontUnroll = 0x00000002,
  DependencyInfinite = 0x00000004,
  DependencyLength = 0x00000008,
  MinIterations = 0x00000010,
  MaxIterations = 0x00000020,
  IterationMultiple = 0x00000040,
  PeelCount = 0x00000080,
  PartialCount = 0x00000100,
  InitiationIntervalINTEL = 0x00010000,
  MaxConcurrencyINTEL = 0x00020000,
  DependencyArrayINTEL = 0x00040000,
  PipelineEnableINTEL = 0x00080000,
  LoopCoalesceINTEL = 0x00100000,
  MaxInterleavingINTEL = 0x00200000,
  SpeculatedIterationsINTEL = 0x00400000,
  NoFusionINTEL = 0x00800000,
  LoopCountINTEL = 0x01000000,
  MaxReinvocationDelayINTEL = 0x02000000,
};

enum class SpvSelectionControlMask : uint32_t {
  None = 0,
  Flatten = 0x00000001,
  DontFlatten = 0x00000002,
};

enum class SpvInstructionFlags : uint8_t {
	None = 0,
	SinkableBit = 1 << 0,
	DependencySinkableBit = 1 << 1,
	AutoGroupSharedBarrier = 1 << 2,
	// Inserted after analysis passes are done.
	SubgroupSyncPre = 1 << 3,
	SubgroupSyncPost = 1 << 4
};
static inline constexpr SpvInstructionFlags operator|(const SpvInstructionFlags& e0, const SpvInstructionFlags& e1) {
	return (SpvInstructionFlags)((std::underlying_type_t<SpvInstructionFlags>)e0 |
								 (std::underlying_type_t<SpvInstructionFlags>)e1);
}
static inline constexpr SpvInstructionFlags& operator|=(SpvInstructionFlags& e0, const SpvInstructionFlags& e1) {
	e0 = e0 | e1;
	return e0;
}
static inline constexpr SpvInstructionFlags operator&(const SpvInstructionFlags& e0, const SpvInstructionFlags& e1) {
	return (SpvInstructionFlags)((std::underlying_type_t<SpvInstructionFlags>)e0 &
								 (std::underlying_type_t<SpvInstructionFlags>)e1);
}
static inline constexpr SpvInstructionFlags& operator&=(SpvInstructionFlags& e0, const SpvInstructionFlags& e1) {
	e0 = e0 & e1;
	return e0;
}
static inline constexpr SpvInstructionFlags operator~(const SpvInstructionFlags& e0) {
	return (SpvInstructionFlags)(~((std::underlying_type_t<SpvInstructionFlags>)e0));
}

struct MergeInfo {
  MergeType merge_type = MergeType::None;
  CFGNode *merge_block = nullptr;
  CFGNode *continue_block = nullptr;
  SpvLoopControlMask loop_control_mask = SpvLoopControlMask::None;
  // by default, we always want flatten
  SpvSelectionControlMask selection_control_mask =
      SpvSelectionControlMask::Flatten;
};

struct IncomingValue {
  CFGNode *block = nullptr;
  Value *value = nullptr;
};

struct PHI {
  PHINode *phi = nullptr;
  bool relaxed = false;
  std::vector<IncomingValue> incoming;
};

struct Terminator {
  enum class Type { Unreachable, Branch, Condition, Switch, Return, Kill, Exit };

  // NOTE: this may be nullptr for newly created conditions
  Instruction *terminator = nullptr;

  Type type = Type::Unreachable;

  // Branch
  CFGNode *direct_block = nullptr;

  // Conditional Branch and Switch
  Value *condition = nullptr;

  // Condition
  CFGNode *true_block = nullptr;
  CFGNode *false_block = nullptr;

  // Switch
  struct Case {
    CFGNode *node = nullptr;
    uint64_t global_order = 0;
    ConstantInt *value = nullptr;
    bool is_default = false;
  };
  std::vector<Case> cases;

  // Return
  Value *return_value = nullptr;

  bool force_unroll = false;
  bool force_loop = false;
  bool force_flatten = false;
  bool force_branch = false;
};

struct IRBlock {
  std::vector<PHI> phi;
  std::vector<Instruction *> operations;
  MergeInfo merge_info;
  Terminator terminator;
};

} // namespace llvm
