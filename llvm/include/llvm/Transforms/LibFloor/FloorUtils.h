//===- FloorUtils.h - libfloor utility functions --------------------------===//
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
// libfloor utility functions
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TRANSFORMS_LIBFLOOR_FLOORUTILS_H
#define LLVM_TRANSFORMS_LIBFLOOR_FLOORUTILS_H

#include <functional>
#include <span>
#include "llvm/IR/CallingConv.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Instructions.h"

namespace libfloor_utils {

template <bool is_const, typename F>
static inline void for_all_users_impl(std::conditional_t<is_const, const llvm::Value&, llvm::Value&> val,
									  F&& func_cb /* void(const User&) or void(User&) */,
									  const llvm::Function* restrict_to_function = nullptr) {
	// gather direct and single-indirect users
	std::vector<std::conditional_t<is_const, const llvm::User*, llvm::User*>> users;
	for (auto user : val.users()) {
		bool delay_user_add = false;
		if (restrict_to_function) {
			if (const auto instr_user = dyn_cast_or_null<const llvm::Instruction>(user); instr_user) {
				if (!instr_user->getParent() || instr_user->getParent()->getParent() != restrict_to_function) {
					continue;
				}
			} else {
				// -> will determine the restriction later
				assert(isa<llvm::ConstantExpr>(user));
				delay_user_add = true;
			}
		}
		
		if (!delay_user_add) {
			users.emplace_back(user);
		}
		if (auto const_expr = dyn_cast<std::conditional_t<is_const, const llvm::ConstantExpr, llvm::ConstantExpr>>(user)) {
			// allow single recursion into constant expression
			for (auto ce_user : const_expr->users()) {
				if (restrict_to_function) {
					if (const auto ce_instr_user = dyn_cast_or_null<const llvm::Instruction>(ce_user); ce_instr_user) {
						if (!ce_instr_user->getParent() || ce_instr_user->getParent()->getParent() != restrict_to_function) {
							continue;
						}
						if (delay_user_add) {
							// can now add this
							delay_user_add = false;
							users.emplace_back(user);
						}
					} else {
						assert(false && "unhandled ConstantExpr user");
						continue;
					}
				}
				
				users.emplace_back(ce_user);
			}
		}
	}
	// call user callback for all users
	for (auto& user : users) {
		func_cb(*user);
	}
}

//! execute specified "func_cb(User&)" on all users of "val"
template <typename F>
static inline void for_all_users(llvm::Value& val, F&& func_cb /* void(const User&) */,
								 const llvm::Function* restrict_to_function = nullptr) {
	for_all_users_impl<false>(val, func_cb, restrict_to_function);
}

//! execute specified "func_cb(const User&)" on all users of "val" (const variant)
template <typename F>
static inline void for_all_users(const llvm::Value& val, F&& func_cb /* void(const User&) */,
								 const llvm::Function* restrict_to_function = nullptr) {
	for_all_users_impl<true>(val, func_cb, restrict_to_function);
}

template <bool is_const, typename F>
static inline void for_all_instruction_users_impl(std::conditional_t<is_const, const llvm::Value&, llvm::Value&> val,
												  F&& func_cb /* void(const Instruction&) or void(Instruction&) */,
												  const llvm::Function* restrict_to_function = nullptr) {
	// gather direct and single-indirect instruction users
	std::vector<std::conditional_t<is_const, const llvm::Instruction*, llvm::Instruction*>> instr_users;
	for (auto user : val.users()) {
		if (auto instr = dyn_cast<std::conditional_t<is_const, const llvm::Instruction, llvm::Instruction>>(user)) {
			if (restrict_to_function && (!instr->getParent() || instr->getParent()->getParent() != restrict_to_function)) {
				continue;
			}
			instr_users.emplace_back(instr);
		} else if (auto const_expr = dyn_cast<std::conditional_t<is_const, const llvm::ConstantExpr, llvm::ConstantExpr>>(user)) {
			// allow single recursion into constant expression
			for (auto ce_user : const_expr->users()) {
				if (auto ce_instr = dyn_cast<std::conditional_t<is_const, const llvm::Instruction, llvm::Instruction>>(ce_user)) {
					if (restrict_to_function && (!ce_instr->getParent() || ce_instr->getParent()->getParent() != restrict_to_function)) {
						continue;
					}
					instr_users.emplace_back(ce_instr);
				}
			}
		}
	}
	// call user callback for all instructions
	for (auto& instr : instr_users) {
		func_cb(*instr);
	}
}

//! execute specified "func_cb(Instruction&)" on all instruction users of "val"
template <typename F>
static inline void for_all_instruction_users(llvm::Value& val, F&& func_cb /* void(Instruction&) */,
											 const llvm::Function* restrict_to_function = nullptr) {
	for_all_instruction_users_impl<false>(val, func_cb, restrict_to_function);
}

//! execute specified "func_cb(const Instruction&)" on all instruction users of "val" (const variant)
template <typename F>
static inline void for_all_instruction_users(const llvm::Value& val, F&& func_cb /* void(const Instruction&) */,
											 const llvm::Function* restrict_to_function = nullptr) {
	for_all_instruction_users_impl<true>(val, func_cb, restrict_to_function);
}

//! wrapper around Value::replaceUsesWithIf that only replaces the uses of "val" inside the specified function "F"
static inline void replace_all_uses_with_in_function(llvm::Value& val, llvm::Value& new_val, const llvm::Function& F) {
	val.replaceUsesWithIf(&new_val, [&F](llvm::Use& use) {
		// we only replace the use if it is inside an instruction of the specified function
		auto user = use.getUser();
		if (!user) {
			return false;
		}
		
		if (auto cnst = dyn_cast_or_null<llvm::Constant>(user); cnst) {
			for (const auto& ce_user : cnst->users()) {
				assert(isa<llvm::Instruction>(ce_user));
				if (const auto instr = dyn_cast_or_null<llvm::Instruction>(ce_user);
					instr && instr->getParent() && instr->getParent()->getParent() == &F) {
					return true;
				}
			}
			return false;
		}
		
		assert(isa<llvm::Instruction>(user));
		if (const auto instr = dyn_cast_or_null<llvm::Instruction>(user);
			instr && instr->getParent() && instr->getParent()->getParent() == &F) {
			return true;
		}
		return false;
	});
}

//! tries to simplify the specified constant integer value to 32-bit,
//! returns the simplified value if one could be created, nullptr otherwise
static inline llvm::ConstantInt* simplify_const_integer_to_32bit(llvm::ConstantInt& const_val) {
	// integer constant -> use signed 32-bit instead if constant is small enough
	auto const_value = const_val.getZExtValue();
	if (const_val.getBitWidth() > 32 && const_value <= 0x7FFF'FFFFull) {
		return llvm::ConstantInt::get(llvm::Type::getInt32Ty(const_val.getContext()), int32_t(const_value));
	}
	return nullptr;
}

//! tries to simplify the specified integer value to 32-bit,
//! returns the simplified value if one could be created, nullptr otherwise
//! if "erase_unused_origin" is true, any originating values that are no longer used will be erased
//! if "func_cb" is specified, it will be called with the new simplified value prior to the "erase unused" check
static inline llvm::Value* simplify_integer_to_32bit(llvm::Value& val, const bool erase_unused_origin = false,
													 std::function<void(llvm::Value*)> func_cb = {}) {
	if (!val.getType()->isIntegerTy()) {
		return nullptr;
	}
	
	if (auto const_val = dyn_cast_or_null<llvm::ConstantInt>(&val); const_val) {
		auto new_const_val = simplify_const_integer_to_32bit(*const_val);
		if (new_const_val && func_cb) {
			func_cb(new_const_val);
		}
		return new_const_val;
	}
	
	// dynamic integer value
	if (auto cast_instr = dyn_cast_or_null<llvm::CastInst>(&val); cast_instr) {
		switch (cast_instr->getOpcode()) {
			case llvm::Instruction::BitCast: {
				break;
			}
			case llvm::Instruction::SExt:
			case llvm::Instruction::ZExt: {
				// if the original is a 32-bit integer, use that instead
				auto orig_int = cast_instr->getOperand(0);
				if (orig_int->getType()->isIntegerTy() &&
					((const llvm::IntegerType*)orig_int->getType())->getBitWidth() <= 32) {
					if (func_cb) {
						func_cb(orig_int);
					}
					
					// kill cast if we are the only user (left)
					if (erase_unused_origin && cast_instr->getNumUses() == 0) {
						cast_instr->eraseFromParent();
					}
					
					return orig_int;
				}
				break;
			}
			default:
				// -> keep as-is
				break;
		}
	}
	return nullptr;
}

//! simplifies GEP indices:
//!  * convert constant integers into i32-typed constants if possible
//!  * remove i32 -> i64 casts of indices (use original i32 index directly)
//! returns true if GEP was modified
static inline bool simplify_gep_indices(llvm::LLVMContext& ctx, llvm::GetElementPtrInst &I) {
	using namespace llvm;
	
	bool did_modify = false;
	for (auto& op : I.operands()) {
		if (op->getType()->isIntegerTy() && !op->getType()->isIntegerTy(32)) {
			// using the callback rather than the return value to allow for proper unused removal
			simplify_integer_to_32bit(*op, true /* kill unused */,
									  [&op, &did_modify](llvm::Value* new_op) {
				op.set(new_op);
				did_modify = true;
			});
		}
	}
	return did_modify;
}
// TODO: should do the same for extractelement/insertelement/extractvalue/insertvalue

//! returns the underlying bitcast operand of "val" if value is a bitcast,
//! will recursively look through bitcasts if "val" contains a chain of bitcasts
static inline llvm::Value* get_underlying_bitcast_operand_or_null(llvm::Value* val) {
	llvm::Value* op = val;
	do {
		if (auto bc = dyn_cast_or_null<llvm::BitCastInst>(op); bc) {
			op = bc->getOperand(0);
		} else if (auto cexpr = dyn_cast_or_null<llvm::ConstantExpr>(op);
				   cexpr && cexpr->getOpcode() == llvm::Instruction::BitCast) {
			op = cexpr->getOperand(0);
		} else {
			break;
		}
	} while (true);
	return (op != val ? op : nullptr);
}

//! returns the underlying elemental type of the specified "type",
//! i.e. the innermost type that can not be decomposed further,
//! returns nullptr for invalid types
static inline llvm::Type* get_elemental_type(llvm::Type* type) {
	if (!type) {
		return nullptr;
	}
	
	// struct types: return the elemental type of the first field (recursively if necessary)
	if (auto st_type = dyn_cast_or_null<llvm::StructType>(type)) {
		if (st_type->getStructNumElements() == 0) {
			return nullptr;
		}
		return get_elemental_type(st_type->getStructElementType(0));
	}
	
	// array types: can just use the element type, then recurse
	if (auto arr_type = dyn_cast_or_null<llvm::ArrayType>(type)) {
		return get_elemental_type(arr_type->getElementType());
	}
	
	// else: assume we already have an elemental type
	return type;
}

//! adds LLVM MD_range [min, max] info on the specified instruction "I"
static inline void add_range_info(llvm::LLVMContext& ctx, llvm::Instruction& I, const uint64_t min_range, const uint64_t max_range) {
	auto range_int_type = llvm::Type::getInt32Ty(ctx);
	llvm::Metadata* range_md[2] {
		llvm::ConstantAsMetadata::get(llvm::ConstantInt::get(range_int_type, min_range, false)),
		llvm::ConstantAsMetadata::get(llvm::ConstantInt::get(range_int_type, max_range, false))
	};
	I.setMetadata(llvm::LLVMContext::MD_range, llvm::MDNode::get(ctx, range_md));
}

struct call_options_t {
	bool is_convergent { false };
	bool is_argmem_only { false };
	bool is_read_only { false };
	bool is_write_only { false };
	bool is_nounwind { true };
	bool is_noreturn { false };
	bool is_tail_call { false };
	llvm::CallingConv::ID calling_convention { llvm::CallingConv::C };
};

//! replaces the specified "instr" with a new function call using the specified parameters
static inline llvm::CallInst* replace_instruction_with_call(llvm::Instruction& instr, llvm::Module& M,
															const std::string& function_name, const std::string& call_var_name,
															llvm::Type* return_type,
															llvm::ArrayRef<llvm::Type*> func_arg_types,
															llvm::ArrayRef<llvm::Value*> func_args,
															const call_options_t opts) {
	auto ctx = &M.getContext();
	
	llvm::AttrBuilder attr_builder(*ctx);
	if (opts.is_convergent) {
		attr_builder.addAttribute(llvm::Attribute::Convergent);
	}
	if (opts.is_argmem_only) {
		attr_builder.addAttribute(llvm::Attribute::ArgMemOnly);
	}
	if (opts.is_read_only) {
		attr_builder.addAttribute(llvm::Attribute::ReadOnly);
	}
	if (opts.is_write_only) {
		attr_builder.addAttribute(llvm::Attribute::WriteOnly);
	}
	if (opts.is_nounwind) {
		attr_builder.addAttribute(llvm::Attribute::NoUnwind);
	}
	if (opts.is_noreturn) {
		attr_builder.addAttribute(llvm::Attribute::NoReturn);
	}
	auto func_attrs = llvm::AttributeList::get(*ctx, ~0, attr_builder);
	
	const auto func_type = llvm::FunctionType::get(return_type, func_arg_types, false);
	auto call = llvm::CallInst::Create(M.getOrInsertFunction(function_name, func_type, func_attrs), func_args, call_var_name, &instr);
	if (opts.calling_convention != llvm::CallingConv::C) {
		call->setCallingConv(opts.calling_convention);
	}
	
	// keep metadata and debug location
	call->copyMetadata(instr);
	call->setDebugLoc(instr.getDebugLoc());
	if (opts.is_convergent) {
		call->setConvergent();
	}
	if (opts.is_argmem_only) {
		call->setOnlyAccessesArgMemory();
	}
	if (opts.is_read_only) {
		call->setOnlyReadsMemory();
	}
	if (opts.is_write_only) {
		call->setOnlyWritesMemory();
	}
	if (opts.is_nounwind) {
		call->setDoesNotThrow();
	}
	if (opts.is_noreturn) {
		call->setDoesNotReturn();
	}
	if (opts.is_tail_call) {
		call->setTailCall();
	}
	
	instr.replaceAllUsesWith(call);
	instr.eraseFromParent();
	
	return call;
}

} // namespace libfloor_utils

#endif
