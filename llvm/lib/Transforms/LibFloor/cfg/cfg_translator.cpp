/*
 * Copyright 2021 - 2023 Florian Ziesche
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

#include "llvm/Transforms/LibFloor/cfg/cfg_translator.hpp"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Module.h"
#include <algorithm>
#include <assert.h>

namespace llvm {

cfg_translator::cfg_translator(Function &F_, LLVMContext &ctx_,
                               CFGNodePool &pool_)
    : F(F_), M(*F.getParent()), ctx(ctx_), pool(pool_) {
  run();
}

void cfg_translator::run() {
  // create nodes for all BBs first
  for (auto &BB : F) {
    bb_map[&BB] = pool.create_node(BB.getName().str(), &BB);
  }

  // translate instructions in all BBs + connect BBs
  for (auto &BB : F) {
    translate_bb(*bb_map.at(&BB));
  }

  // set entry block
  entry = bb_map.at(&F.getEntryBlock());
}

static inline Terminator::Type get_terminator_type(Instruction &instr) {
  if (auto br = dyn_cast_or_null<BranchInst>(&instr)) {
    return (br->isConditional() ? Terminator::Type::Condition
                                : Terminator::Type::Branch);
  } else if (auto ret = dyn_cast_or_null<ReturnInst>(&instr)) {
    return Terminator::Type::Return;
  } else if (auto unreachable = dyn_cast_or_null<UnreachableInst>(&instr)) {
    if (auto CI = dyn_cast_or_null<CallInst>(instr.getPrevNode());
        CI && CI->getCalledFunction()->getName() == "floor.discard_fragment") {
      return Terminator::Type::Kill;
    }
    return Terminator::Type::Unreachable;
  } else if (auto sw = dyn_cast_or_null<SwitchInst>(&instr)) {
    return Terminator::Type::Switch;
  }
  assert(false && "unsupported terminator instruction");
  abort();
}

void cfg_translator::translate_bb(CFGNode &node) {
  // translate all BB content
  for (auto &instr : node.BB) {
    if (auto phi = dyn_cast_or_null<PHINode>(&instr)) {
      PHI ph{.phi = phi};
      std::unordered_set<BasicBlock *> phi_bbs;
      for (uint32_t i = 0, count = phi->getNumIncomingValues(); i < count;
           ++i) {
        auto in_bb = phi->getIncomingBlock(i);
        if (phi_bbs.count(in_bb) > 0) {
          // ignore duplicate incoming blocks
          continue;
        } else {
          phi_bbs.emplace(in_bb);
        }

        ph.incoming.emplace_back(IncomingValue{
            .block = bb_map.at(in_bb),
            .value = phi->getIncomingValue(i),
        });
      }
      node.ir.phi.emplace_back(ph);
    } else if (instr.isTerminator()) {
      node.ir.terminator.terminator = &instr;
      node.ir.terminator.type = get_terminator_type(instr);
      switch (node.ir.terminator.type) {
      case Terminator::Type::Condition: {
        auto br = dyn_cast<BranchInst>(&instr);
        node.ir.terminator.condition = br->getCondition();
        node.ir.terminator.true_block = bb_map.at(br->getSuccessor(0));
        node.ir.terminator.false_block = bb_map.at(br->getSuccessor(1));
        node.add_branch(node.ir.terminator.true_block);
        node.add_branch(node.ir.terminator.false_block);
        break;
      }
      case Terminator::Type::Branch: {
        auto br = dyn_cast<BranchInst>(&instr);
        node.ir.terminator.direct_block = bb_map.at(br->getSuccessor(0));
        node.add_branch(node.ir.terminator.direct_block);
        break;
      }
      case Terminator::Type::Return: {
        auto ret = dyn_cast<ReturnInst>(&instr);
        node.ir.terminator.return_value = ret->getReturnValue();
        break;
      }
      case Terminator::Type::Unreachable:
      case Terminator::Type::Kill:
        // NOTE: we don't have a specific terminator for Kill instructions
        // (reuses Unreachable)
        break;
      case Terminator::Type::Switch: {
        auto sw = dyn_cast<SwitchInst>(&instr);
        node.ir.terminator.condition = sw->getCondition();
        node.ir.terminator.cases.emplace_back(Terminator::Case{
            .node = bb_map.at(sw->getDefaultDest()),
            .value = nullptr,
            .is_default = true,
        });
        node.add_branch(bb_map.at(sw->getDefaultDest()));
        for (auto &cs : sw->cases()) {
          node.ir.terminator.cases.emplace_back(Terminator::Case{
              .node = bb_map.at(cs.getCaseSuccessor()),
              .value = cs.getCaseValue(),
              .is_default = false,
          });
          node.add_branch(node.ir.terminator.cases.back().node);
        }
        break;
      }
      }
    } else {
      // normal instruction
      node.ir.operations.emplace_back(&instr);
    }
  }
}

bool cfg_translator::needs_fake_selection(CFGNode &node) {
  if (node.merge == MergeType::Loop &&
      node.ir.terminator.type == Terminator::Type::Condition &&
      node.ir.terminator.true_block != node.ir.merge_info.merge_block &&
      node.ir.terminator.true_block != node.ir.merge_info.continue_block &&
      node.ir.terminator.false_block != node.ir.merge_info.merge_block &&
      node.ir.terminator.false_block != node.ir.merge_info.continue_block) {
    return true;
  }
  return false;
}

void cfg_translator::add_or_update_terminator(CFGNode &node) {
  // remove existing if there is one
  if (auto existing_term = node.BB.getTerminator(); existing_term) {
    existing_term->eraseFromParent();
  }

  // add new LLVM terminator
  auto &term = node.ir.terminator;
  switch (term.type) {
  case Terminator::Type::Condition: {
    if (needs_fake_selection(node)) {
      // NOTE: need to insert these *after* the current node
      auto fake_selection_bb = BasicBlock::Create(
          ctx, node.name + ".fake_selection", &F, node.BB.getNextNode());
      auto unreachable_bb = BasicBlock::Create(ctx, node.name + ".unreachable",
                                               &F, node.BB.getNextNode());

      // -> now branches to fake selection BB
      BranchInst::Create(fake_selection_bb, &node.BB);

      // must create this before calling create_selection_merge()
      new UnreachableInst(ctx, unreachable_bb);

      // -> fake selection now contains the actual conditional branch
      auto cond_br =
          BranchInst::Create(&term.true_block->BB, &term.false_block->BB,
                             term.condition, fake_selection_bb);
      create_selection_merge(cond_br, unreachable_bb);

      // we need to replace PHI incoming BBs later on (from this BB to the new
      // fake selection)
      node.phi_override = fake_selection_bb;
    } else {
      BranchInst::Create(&term.true_block->BB, &term.false_block->BB,
                         term.condition, &node.BB);
    }
    break;
  }
  case Terminator::Type::Branch: {
    BranchInst::Create(&term.direct_block->BB, &node.BB);
    break;
  }
  case Terminator::Type::Return: {
    if (term.return_value) {
      ReturnInst::Create(ctx, term.return_value, &node.BB);
    } else {
      ReturnInst::Create(ctx, &node.BB);
    }
    break;
  }
  case Terminator::Type::Kill: {
    bool create_discard_fragment = false;
    if (node.BB.empty()) {
      create_discard_fragment = true;
    } else {
      if (auto CI = dyn_cast_or_null<CallInst>(&node.BB.back());
          CI &&
          CI->getCalledFunction()->getName() == "floor.discard_fragment") {
        // -> already exists
      } else {
        create_discard_fragment = true;
      }
    }
    if (create_discard_fragment) {
      Function *discard_func = M.getFunction("floor.discard_fragment");
      assert(discard_func &&
             "discard function must have already existed if we get here");
      CallInst *discard_call = CallInst::Create(discard_func, "", &node.BB);
      discard_call->setCallingConv(CallingConv::FLOOR_FUNC);
      discard_call->setCannotDuplicate();
    }
  }
    LLVM_FALLTHROUGH;
  case Terminator::Type::Unreachable:
    new UnreachableInst(ctx, &node.BB);
    break;
  case Terminator::Type::Switch: {
    BasicBlock *default_bb = nullptr;
    for (auto &cs : term.cases) {
      if (cs.is_default) {
        default_bb = &cs.node->BB;
        break;
      }
    }
    assert(default_bb != nullptr && "no default case in switch");

    auto sw = SwitchInst::Create(term.condition, default_bb, term.cases.size(),
                                 &node.BB);
    for (auto &cs : term.cases) {
      if (cs.is_default) {
        continue;
      }
      sw->addCase(cs.value, &cs.node->BB);
    }
    break;
  }
  }
}

void cfg_translator::cfg_to_llvm_ir(CFGNode *updated_entry_block,
                                    const bool add_merge_annotations) {
  if (entry != updated_entry_block) {
    // move new entry to the front
    F.getBasicBlockList().erase(&updated_entry_block->BB);
    F.getBasicBlockList().push_front(&updated_entry_block->BB);
  }
  entry = updated_entry_block;

  // update terminators
  pool.for_each_node([this](CFGNode &node) {
    auto terminator = node.BB.getTerminator();
    if (!terminator) {
      // probably a new BB w/o an LLVM terminator -> create one
      add_or_update_terminator(node);
    } else {
      // check if the existing terminator is equal to the CFG terminator
      const auto term_type = get_terminator_type(*terminator);
      if (term_type != node.ir.terminator.type || needs_fake_selection(node)) {
        // different type or fake selection required -> update
        add_or_update_terminator(node);
      } else {
        // equal type, check if operands are the same
        switch (term_type) {
        case Terminator::Type::Condition: {
          auto br = dyn_cast<BranchInst>(terminator);
          assert(br->getNumSuccessors() == 2);
          assert(node.ir.terminator.true_block &&
                 node.ir.terminator.false_block);
          if (br->getCondition() != node.ir.terminator.condition ||
              br->getSuccessor(0) != &node.ir.terminator.true_block->BB ||
              br->getSuccessor(1) != &node.ir.terminator.false_block->BB) {
            add_or_update_terminator(node);
          }
          break;
        }
        case Terminator::Type::Branch: {
          auto br = dyn_cast<BranchInst>(terminator);
          assert(br->getNumSuccessors() == 1);
          assert(node.ir.terminator.direct_block);
          if (br->getSuccessor(0) != &node.ir.terminator.direct_block->BB) {
            add_or_update_terminator(node);
          }
          break;
        }
        case Terminator::Type::Return: {
          auto ret = dyn_cast<ReturnInst>(terminator);
          if (ret->getReturnValue() != node.ir.terminator.return_value) {
            add_or_update_terminator(node);
          }
          break;
        }
        case Terminator::Type::Unreachable:
        case Terminator::Type::Kill:
          // no operands to check
          break;
        case Terminator::Type::Switch: {
          auto sw = dyn_cast<SwitchInst>(terminator);
          BasicBlock *default_bb = nullptr;
          for (auto &cs : node.ir.terminator.cases) {
            if (cs.is_default) {
              default_bb = &cs.node->BB;
              break;
            }
          }
          assert(default_bb != nullptr && "no default case in switch");
          if (sw->getDefaultDest() != default_bb ||
              sw->getCondition() != node.ir.terminator.condition ||
              sw->getNumCases() != node.ir.terminator.cases.size()) {
            add_or_update_terminator(node);
          } else {
            auto case_iter = sw->case_begin();
            for (uint32_t i = 0, count = sw->getNumCases(); i < count;
                 ++i, ++case_iter) {
              auto &llvm_case = *case_iter;
              auto &cfg_case = node.ir.terminator.cases[i];
              if (llvm_case.getCaseSuccessor() != &cfg_case.node->BB ||
                  llvm_case.getCaseValue() != cfg_case.value) {
                add_or_update_terminator(node);
                break;
              }
            }
          }
          break;
        }
        }
      }
    }
  });

  // compute (simple) reachability
  std::unordered_set<const BasicBlock *> reachable_blocks;
  std::function<void(const BasicBlock &BB)> compute_simple_reachability =
      [&compute_simple_reachability, &reachable_blocks](const BasicBlock &BB) {
        // already visited?
        if (reachable_blocks.count(&BB) > 0) {
          return;
        }
        reachable_blocks.emplace(&BB);

        // visit children
        auto term = BB.getTerminator();
        assert(term != nullptr);
        if (auto br = dyn_cast_or_null<BranchInst>(term)) {
          for (auto succ : br->successors()) {
            compute_simple_reachability(*succ);
          }
        } else if (auto sw = dyn_cast_or_null<SwitchInst>(term)) {
          for (uint32_t succ_idx = 0, succ_count = sw->getNumSuccessors();
               succ_idx < succ_count; ++succ_idx) {
            compute_simple_reachability(*sw->getSuccessor(succ_idx));
          }
        } else if (auto ret = dyn_cast_or_null<ReturnInst>(term)) {
          // nop
        } else if (auto ur = dyn_cast_or_null<UnreachableInst>(term)) {
          // nop
        } else {
          assert(false && "unknown/unhandled terminator type");
        }
      };
  compute_simple_reachability(entry->BB);

  // update PHIs
  pool.for_each_node(
      [&reachable_blocks](CFGNode &node) {
        assert(!node.BB.empty());

        // skip unreachable BBs (i.e. w/o a predecessor / unreachable ones ->
        // will be killed later)
        if (reachable_blocks.count(&node.BB) == 0) {
          return;
        }

        for (auto &instr : node.BB) {
          auto phi = dyn_cast_or_null<PHINode>(&instr);
          if (!phi) {
            continue;
          }

          // remove existing incoming values
          // NOTE: this also ensures that unreachable BBs are cleared out
          // -> no longer have any users
          for (uint32_t idx = 0, count = phi->getNumIncomingValues();
               idx < count; ++idx) {
            phi->removeIncomingValue(0u, false /* do NOT delete the phi once no incoming values are left */);
          }
          assert(phi->getNumIncomingValues() == 0);

          // find corresponding PHI entry
          auto piter =
              find_if(node.ir.phi.begin(), node.ir.phi.end(),
                      [&phi](const PHI &elem) { return elem.phi == phi; });
          if (piter == node.ir.phi.end()) {
            assert(false && "couldn't find corresponding PHI");
            continue;
          }
          assert(!piter->incoming.empty());

          // add actual/updated incoming values
          for (auto &incoming : piter->incoming) {
            auto incoming_bb = &incoming.block->BB;
            if (incoming.block->phi_override) {
              incoming_bb = incoming.block->phi_override;
            }
            if (reachable_blocks.count(incoming_bb) > 0) {
              phi->addIncoming(incoming.value, incoming_bb);
            } else {
              assert(false && "phi incoming BB unreachable");
            }
          }

          // handle the awkwardness that is duplicate predecessor blocks in LLVM
          // ...
          if (node.BB.hasNPredecessorsOrMore(phi->getNumIncomingValues() + 1)) {
            std::unordered_map<const BasicBlock *, Value *> unique_preds;
            std::vector<BasicBlock *> dup_preds;
            for (auto pred : predecessors(&node.BB)) {
              if (reachable_blocks.count(pred) == 0) {
                // ignore unreachable predecessors that will be removed next
                continue;
              }
              if (unique_preds.count(pred) == 0) {
                unique_preds.emplace(pred, phi->getIncomingValueForBlock(pred));
              } else {
                dup_preds.emplace_back(pred);
              }
            }
            for (auto &dup_pred : dup_preds) {
              phi->addIncoming(unique_preds.at(dup_pred), dup_pred);
            }
          }
        }
      });

  // remove BBs without a predecessor
  // -> pass #1: gather all instructions inside BBs and drop their references
  std::vector<Instruction *> instr_kill_list;
  pool.for_each_node([&reachable_blocks, &instr_kill_list](CFGNode &node) {
    if (reachable_blocks.count(&node.BB) > 0) {
      return;
    }
    for (auto &instr : node.BB) {
      // need to drop all references, because this might be ref'ed by another
      // instruction that will be deleted
      instr.dropAllReferences();
      instr_kill_list.emplace_back(&instr);
    }
  });
  // -> pass #2: remove all gathered instructions
  for (auto &instr : instr_kill_list) {
    instr->eraseFromParent();
  }
  // -> pass #3: actually remove the BBs
  std::vector<CFGNode *> rem_nodes;
  pool.for_each_node([&reachable_blocks, &rem_nodes](CFGNode &node) {
    if (reachable_blocks.count(&node.BB) > 0) {
      return;
    }
    assert(node.BB.users().empty() && "unreachable BB still has users");
    assert(node.BB.uses().empty() && "unreachable BB still has uses");
    node.BB.eraseFromParent();
    rem_nodes.emplace_back(&node);
  });
  for (auto &rem_node : rem_nodes) {
    pool.remove_node(*rem_node);
  }

  // add merge annotations
  if (add_merge_annotations) {
    pool.for_each_node([this](CFGNode &node) {
      if (node.merge == MergeType::None) {
        return;
      }

      auto term = node.BB.getTerminator();
      assert(term != nullptr &&
             "BB with merge annotation must have a terminator");
      if (node.merge == MergeType::Selection) {
        // special case: no selection merge block, because at least one BB exits
        if (node.selection_merge_block == nullptr &&
            node.selection_merge_exit) {
          if (node.ir.terminator.type == Terminator::Type::Condition) {
            auto br = dyn_cast_or_null<BranchInst>(term);
            assert(br != nullptr);
            assert(br->getNumSuccessors() == 2);
            auto unreach_0 = dyn_cast_or_null<UnreachableInst>(
                br->getSuccessor(0)->getTerminator());
            auto unreach_1 = dyn_cast_or_null<UnreachableInst>(
                br->getSuccessor(1)->getTerminator());
            if (unreach_0 && !unreach_1) {
              // 0 is unreachable, 1 is not -> merge to 1
              create_selection_merge(term, br->getSuccessor(1));
            } else if (!unreach_0 && unreach_1) {
              // 1 is unreachable, 0 is not -> merge to 0
              create_selection_merge(term, br->getSuccessor(0));
            }
            // else: ignore this
            return;
          } else if (node.ir.terminator.type == Terminator::Type::Switch) {
            assert(
                false &&
                "can't handle exit selection merge on switch instruction yet");
            return;
          }
          assert(false && "invalid terminator");
        }

        if (node.ir.terminator.type == Terminator::Type::Condition ||
            node.ir.terminator.type == Terminator::Type::Switch) {
          if (node.ir.terminator.type == Terminator::Type::Condition) {
            auto br = dyn_cast_or_null<BranchInst>(term);
            assert(br != nullptr);
            assert(br->getNumSuccessors() == 2);
          } else if (node.ir.terminator.type == Terminator::Type::Switch) {
            auto sw = dyn_cast_or_null<SwitchInst>(term);
            assert(sw != nullptr);
            assert(sw->getNumSuccessors() > 0);
          }
          if (node.selection_merge_block) {
            create_selection_merge(term, &node.selection_merge_block->BB);
          } else {
            // no selection merge block exists
            // -> create a fake unreachable one
            assert(node.ir.terminator.type == Terminator::Type::Condition);
            auto fake_merge = BasicBlock::Create(ctx, node.name + ".fake_merge",
                                                 &F, &node.BB);
            new UnreachableInst(ctx, fake_merge);
            create_selection_merge(term, fake_merge);
          }
        } else {
          llvm_unreachable("invalid selection merge");
        }
      } else if (node.merge == MergeType::Loop) {
        if (node.ir.merge_info.merge_block != nullptr) {
          if (node.ir.merge_info.continue_block != nullptr) {
            auto continue_bb =
                (&node.ir.merge_info.continue_block->BB == &node.BB &&
                         node.phi_override
                     ? node.phi_override
                     : &node.ir.merge_info.continue_block->BB);
            create_loop_merge(term, &node.ir.merge_info.merge_block->BB,
                              continue_bb);
          } else {
            if (&node == entry) {
              // if this is the entry node, we can't simply place a fake
              // continue block before it, because it wouldn't be counted as a
              // back-edge
              // -> solve this by creating a second fake block that will act as
              // the new entry block (this is incredibly stupid, but so are
              // structured control flow requirements)
              auto new_entry_block = BasicBlock::Create(
                  ctx, node.name + ".new_entry.fake_continue", &F, &node.BB);
              BranchInst::Create(&node.BB, new_entry_block);
            }

            // continue block does not exist
            // -> need to create a fake incoming block
            auto continue_block = BasicBlock::Create(
                ctx, node.name + ".fake_continue", &F, &node.BB);
            BranchInst::Create(&node.BB, continue_block);
            create_loop_merge(term, &node.ir.merge_info.merge_block->BB,
                              continue_block);

            // update phis: need to insert incoming undef value for the new
            // continue block
            for (auto &phi : node.BB.phis()) {
              phi.addIncoming(UndefValue::get(phi.getType()), continue_block);
            }
          }
        } else if (node.ir.merge_info.continue_block != nullptr) {
          // merge block does not exist
          // -> need to create a fake one
          auto merge_block = BasicBlock::Create(ctx, node.name + ".fake_merge",
                                                &F, node.BB.getNextNode());
          new UnreachableInst(ctx, merge_block);
          auto continue_bb =
              (&node.ir.merge_info.continue_block->BB == &node.BB &&
                       node.phi_override
                   ? node.phi_override
                   : &node.ir.merge_info.continue_block->BB);
          create_loop_merge(term, merge_block, continue_bb);
        } else {
          llvm_unreachable("invalid loop merge");
        }
      }
    });
  }
}

CallInst *cfg_translator::insert_merge_block_marker(BasicBlock *merge_block) {
  const std::string merge_block_func_name = "floor.merge_block";
  Function *merge_block_func = M.getFunction(merge_block_func_name);
  if (merge_block_func == nullptr) {
    FunctionType *merge_block_type =
        FunctionType::get(llvm::Type::getVoidTy(ctx), false);
    merge_block_func =
        (Function *)M
            .getOrInsertFunction(merge_block_func_name, merge_block_type)
            .getCallee();
    merge_block_func->setCallingConv(CallingConv::FLOOR_FUNC);
    merge_block_func->setCannotDuplicate();
    merge_block_func->setConvergent();
    merge_block_func->setDoesNotRecurse();
  }

  assert(merge_block->getTerminator());
  CallInst *merge_block_call =
      CallInst::Create(merge_block_func, "", merge_block->getTerminator());
  merge_block_call->setCallingConv(CallingConv::FLOOR_FUNC);
  merge_block_call->setConvergent();
  merge_block_call->setCannotDuplicate();
  return merge_block_call;
}

CallInst *
cfg_translator::insert_continue_block_marker(BasicBlock *continue_block) {
  const std::string continue_block_func_name = "floor.continue_block";
  Function *continue_block_func = M.getFunction(continue_block_func_name);
  if (continue_block_func == nullptr) {
    FunctionType *continue_block_type =
        FunctionType::get(llvm::Type::getVoidTy(ctx), false);
    continue_block_func =
        (Function *)M
            .getOrInsertFunction(continue_block_func_name, continue_block_type)
            .getCallee();
    continue_block_func->setCallingConv(CallingConv::FLOOR_FUNC);
    continue_block_func->setCannotDuplicate();
    continue_block_func->setConvergent();
    continue_block_func->setDoesNotRecurse();
  }

  assert(continue_block->getTerminator());
  CallInst *continue_block_call = CallInst::Create(
      continue_block_func, "", continue_block->getTerminator());
  continue_block_call->setCallingConv(CallingConv::FLOOR_FUNC);
  continue_block_call->setConvergent();
  continue_block_call->setCannotDuplicate();
  return continue_block_call;
}

void cfg_translator::create_loop_merge(Instruction *insert_before,
                                       BasicBlock *bb_merge,
                                       BasicBlock *bb_continue) {
  Function *loop_merge_func = M.getFunction("floor.loop_merge");
  if (loop_merge_func == nullptr) {
    llvm::Type *loop_merge_arg_types[]{llvm::Type::getLabelTy(ctx),
                                       llvm::Type::getLabelTy(ctx)};
    FunctionType *loop_merge_type = FunctionType::get(
        llvm::Type::getVoidTy(ctx), loop_merge_arg_types, false);
    loop_merge_func =
        (Function *)M.getOrInsertFunction("floor.loop_merge", loop_merge_type)
            .getCallee();
    loop_merge_func->setCallingConv(CallingConv::FLOOR_FUNC);
    loop_merge_func->setCannotDuplicate();
    loop_merge_func->setDoesNotThrow();
    loop_merge_func->setNotConvergent();
    loop_merge_func->setDoesNotRecurse();
  }
  llvm::Value *merge_vars[]{bb_merge, bb_continue};
  assert(insert_before);
  CallInst *loop_merge_call =
      CallInst::Create(loop_merge_func, merge_vars, "", insert_before);
  loop_merge_call->setCallingConv(CallingConv::FLOOR_FUNC);

  insert_merge_block_marker(bb_merge);
  insert_continue_block_marker(bb_continue);
}

void cfg_translator::create_selection_merge(Instruction *insert_before,
                                            BasicBlock *merge_block) {
  Function *sel_merge_func = M.getFunction("floor.selection_merge");
  if (sel_merge_func == nullptr) {
    llvm::Type *sel_merge_arg_types[]{llvm::Type::getLabelTy(ctx)};
    FunctionType *sel_merge_type = FunctionType::get(
        llvm::Type::getVoidTy(ctx), sel_merge_arg_types, false);
    sel_merge_func =
        (Function *)M
            .getOrInsertFunction("floor.selection_merge", sel_merge_type)
            .getCallee();
    sel_merge_func->setCallingConv(CallingConv::FLOOR_FUNC);
    sel_merge_func->setCannotDuplicate();
    sel_merge_func->setDoesNotThrow();
    sel_merge_func->setNotConvergent();
    sel_merge_func->setDoesNotRecurse();
  }
  llvm::Value *merge_vars[]{merge_block};
  assert(insert_before);
  CallInst *sel_merge_call =
      CallInst::Create(sel_merge_func, merge_vars, "", insert_before);
  sel_merge_call->setCallingConv(CallingConv::FLOOR_FUNC);

  insert_merge_block_marker(merge_block);
}

} // namespace llvm
