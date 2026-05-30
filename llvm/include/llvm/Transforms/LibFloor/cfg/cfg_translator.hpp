/*
 * Copyright 2021 - 2026 Florian Ziesche
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
// LLVM compat/translator for the dxil-spirv CFG structurizer
//
//===----------------------------------------------------------------------===//

#pragma once

#include <memory>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <cstdint>
#include "llvm/IR/CFG.h"
#include "llvm/IR/Function.h"
#include "llvm/Transforms/LibFloor/cfg/node.hpp"
#include "llvm/Transforms/LibFloor/cfg/ir.hpp"

namespace llvm {

class cfg_translator {
public:
  //! initializes/connects the CFG from/with an LLVM function
  cfg_translator(Function &F_, LLVMContext &ctx_, CFGNodePool &pool_);
  CFGNode *get_entry_block() const { return entry; }

  //! on CFG transform/structurization completion:
  //! translate any CFG changes back to LLVM IR
  //! since the entry block may change, "updated_entry_block" sets the new entry
  //! block
  void cfg_to_llvm_ir(CFGNode *updated_entry_block,
                      const bool add_merge_annotations);

protected:
  Function &F;
  Module &M;
  LLVMContext &ctx;
  CFGNodePool &pool;

  CFGNode *entry = nullptr;
  std::unordered_map<BasicBlock *, CFGNode *> bb_map;

  void run();
  void translate_bb(CFGNode &node);

  void add_or_update_terminator(CFGNode &node);
  bool needs_fake_selection(CFGNode &node);

  CallInst *insert_merge_block_marker(BasicBlock *merge_block);
  CallInst *insert_continue_block_marker(BasicBlock *continue_block);
  void create_loop_merge(Instruction *insert_before, BasicBlock *bb_merge,
                         BasicBlock *bb_continue,
                         SpvLoopControlMask loop_control);
  void create_selection_merge(Instruction *insert_before,
                              BasicBlock *merge_block,
                              SpvSelectionControlMask sel_control);

  BasicBlock *create_fake_loop_continue(CFGNode &node);
  BasicBlock *create_fake_loop_merge(CFGNode &node);
};

} // namespace llvm
