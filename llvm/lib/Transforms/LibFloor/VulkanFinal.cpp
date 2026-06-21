//===- VulkanFinal.cpp - Vulkan final pass --------------------------------===//
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
// This file fixes certain post-codegen issues and transforms specific builtin
// functions with uses of new function arguments, as well as transforming
// input/output variables/arguments for later use in the SPIR-V backend.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/BasicAliasAnalysis.h"
#include "llvm/Analysis/GlobalsModRef.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/InitializePasses.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/CallingConv.h"
#include "llvm/IR/ConstantRange.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Pass.h"
#include "llvm/PassRegistry.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Transforms/LibFloor.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/LowerMemIntrinsics.h"
#include "llvm/Transforms/LibFloor/AddressSpaceFix.h"
#include "llvm/Transforms/LibFloor/FloorUtils.h"
#include <algorithm>
#include <cstdarg>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <array>
#include <optional>
using namespace llvm;

#define DEBUG_TYPE "VulkanFinal"

#if 1
#define DBG(x)
#else
#define DBG(x) x
#endif

namespace {
	// -> SPIRVInternal.h (can't include, b/c it's not in a proper path)
	static const uint32_t SPIRAS_Constant = 2;
	static const uint32_t SPIRAS_Local = 3;
	static const uint32_t SPIRAS_Uniform = 5;
	//static const uint32_t SPIRAS_Input = 6;
	static const uint32_t SPIRAS_Output = 7;
	static const uint32_t SPIRAS_StorageBuffer = 12;
	//static const uint32_t SPIRAS_PhysicalStorageBuffer = 5349;

	// VulkanBuiltinParamHandling
	struct VulkanBuiltinParamHandling : public FunctionPass, InstVisitor<VulkanBuiltinParamHandling> {
		friend class InstVisitor<VulkanBuiltinParamHandling>;
		
		static char ID; // Pass identification, replacement for typeid
		
		std::shared_ptr<llvm::IRBuilder<>> builder;
		
		Module* M { nullptr };
		LLVMContext* ctx { nullptr };
		Function* func { nullptr };
		bool is_kernel_func { false };
		bool is_vertex_func { false };
		bool is_fragment_func { false };
		bool is_task_func { false };
		bool is_mesh_func { false };
		
		struct per_function_state_t {
			uint32_t kernel_dim { 1 };
			uint32_t kernel_simd_width { 0 };
			uint32_t kernel_local_size[3] { 0, 0, 0 };
			bool has_fixed_local_size() const {
				return (kernel_local_size[0] != 0 &&
						kernel_local_size[1] != 0 &&
						kernel_local_size[2] != 0);
			}
			uint32_t fixed_local_size_extent() const {
				assert(has_fixed_local_size());
				uint32_t extent = kernel_local_size[0];
				if (kernel_dim >= 2) {
					extent *= kernel_local_size[1];
				}
				if (kernel_dim >= 3) {
					extent *= kernel_local_size[2];
				}
				return extent;
			}
			
			// added kernel/task/mesh function args
			Argument* group_id { nullptr };
			Argument* group_size { nullptr };
			GlobalVariable* local_size { nullptr };
			Argument* sub_group_id { nullptr };
			Argument* sub_group_local_id { nullptr };
			Argument* sub_group_size { nullptr };
			Argument* num_sub_groups { nullptr };
			
			// added general shader function args
			Argument* view_index { nullptr }; // VS/FS/MS
			
			// added vertex function args
			Argument* vertex_id { nullptr };
			Argument* base_vertex_id { nullptr };
			Argument* instance_id { nullptr };
			Argument* base_instance_id { nullptr };
			
			// added fragment function args
			Argument* point_coord { nullptr };
			Argument* front_facing { nullptr };
			Argument* frag_coord { nullptr };
			Argument* primitive_id { nullptr };
			Argument* barycentric_coord { nullptr };
			
			// any function args
			Argument* soft_printf { nullptr };
			
			
			// ID/arg handling: these contain performed loads/etc
			std::array<Value*, 3> ld_group_id {{ nullptr, nullptr, nullptr }};
			std::array<Value*, 3> ld_group_size {{ nullptr, nullptr, nullptr }};
			std::array<Value*, 3> ld_local_id {{ nullptr, nullptr, nullptr }};
			std::array<Value*, 3> ld_local_size {{ nullptr, nullptr, nullptr }};
			std::array<Value*, 3> ld_global_id {{ nullptr, nullptr, nullptr }};
			std::array<Value*, 3> ld_global_size {{ nullptr, nullptr, nullptr }};
			Value* ld_sub_group_id { nullptr };
			Value* ld_sub_group_local_id { nullptr };
			Value* ld_sub_group_size { nullptr };
			Value* ld_num_sub_groups { nullptr };
			
			std::vector<Instruction*> kill_list;
		} state;
		
		VulkanBuiltinParamHandling() :
		FunctionPass(ID) {
			initializeVulkanBuiltinParamHandlingPass(*PassRegistry::getPassRegistry());
		}
		
		void getAnalysisUsage(AnalysisUsage &AU) const override {
			AU.addRequired<AAResultsWrapperPass>();
			AU.addRequired<GlobalsAAWrapperPass>();
			AU.addRequired<AssumptionCacheTracker>();
			AU.addRequired<TargetLibraryInfoWrapperPass>();
		}
		
		enum VULKAN_KERNEL_ARG_REV_IDX : int32_t {
			VULKAN_GROUP_ID = -6,
			VULKAN_GROUP_SIZE = -5,
			VULKAN_SUB_GROUP_ID = -4,
			VULKAN_SUB_GROUP_LOCAL_ID = -3,
			VULKAN_SUB_GROUP_SIZE = -2,
			VULKAN_NUM_SUB_GROUPS = -1,
			
			VULKAN_KERNEL_ARG_COUNT = 6,
		};
		
		enum VULKAN_TASK_ARG_REV_IDX : int32_t {
			VULKAN_TASK_GROUP_ID = -6,
			VULKAN_TASK_GROUP_SIZE = -5,
			VULKAN_TASK_SUB_GROUP_ID = -4,
			VULKAN_TASK_SUB_GROUP_LOCAL_ID = -3,
			VULKAN_TASK_SUB_GROUP_SIZE = -2,
			VULKAN_TASK_NUM_SUB_GROUPS = -1,
			
			VULKAN_TASK_ARG_COUNT = 6,
		};
		
		enum VULKAN_MESH_ARG_REV_IDX : int32_t {
			VULKAN_MESH_GROUP_ID = -7,
			VULKAN_MESH_GROUP_SIZE = -6,
			VULKAN_MESH_SUB_GROUP_ID = -5,
			VULKAN_MESH_SUB_GROUP_LOCAL_ID = -4,
			VULKAN_MESH_SUB_GROUP_SIZE = -3,
			VULKAN_MESH_NUM_SUB_GROUPS = -2,
			VULKAN_MESH_VIEW_INDEX = -1,
			
			VULKAN_MESH_ARG_COUNT = 7,
		};
		
		enum VULKAN_VERTEX_ARG_REV_IDX : int32_t {
			VULKAN_VERTEX_ID = -5,
			VULKAN_BASE_VERTEX_ID = -4,
			VULKAN_VERTEX_VIEW_INDEX = -3,
			VULKAN_INSTANCE_ID = -2,
			VULKAN_BASE_INSTANCE_ID = -1,
			
			VULKAN_VERTEX_ARG_COUNT = 3,
		};
		
		enum VULKAN_FRAGMENT_ARG_REV_IDX : int32_t {
			VULKAN_POINT_COORD = -4,
			VULKAN_FRAG_COORD = -3,
			VULKAN_FRAGMENT_VIEW_INDEX = -2,
			VULKAN_FRONT_FACING = -1,
			
			VULKAN_FRAGMENT_ARG_COUNT = 4,
		};
		
		// TODO: tessellation arg enums/handling
		
		bool runOnFunction(Function &F) override {
			// exit if empty function
			if(F.empty()) return false;
			DBG(errs() << "in func: "; errs().write_escaped(F.getName()) << '\n';)
			
			// determine this function type + exit if it isn't a kernel or shader function
			is_kernel_func = F.getCallingConv() == CallingConv::FLOOR_KERNEL;
			is_vertex_func = F.getCallingConv() == CallingConv::FLOOR_VERTEX;
			is_fragment_func = F.getCallingConv() == CallingConv::FLOOR_FRAGMENT;
			is_task_func = F.getCallingConv() == CallingConv::FLOOR_TASK;
			is_mesh_func = F.getCallingConv() == CallingConv::FLOOR_MESH;
			if (!is_kernel_func &&
				!is_vertex_func &&
				!is_fragment_func &&
				!is_task_func &&
				!is_mesh_func) {
				return false;
			}
			
			//
			M = F.getParent();
			ctx = &M->getContext();
			func = &F;
			builder = std::make_shared<llvm::IRBuilder<>>(*ctx);
			state = {};
			
			// always insert into entry block
			builder->SetInsertPoint(&func->getEntryBlock().front());
			
			const auto get_arg_by_idx = [&F](const int32_t& rev_idx) -> llvm::Argument* {
				auto arg_iter = F.arg_end();
				std::advance(arg_iter, rev_idx);
				return &*arg_iter;
			};
			
			// check for optional features: soft-printf, primitive id, barycentric coord
			bool has_soft_printf = false, has_primitive_id = false, has_barycentric_coord = false;
			if (auto soft_printf_meta = M->getNamedMetadata("floor.soft_printf"); soft_printf_meta) {
				has_soft_printf = true;
			}
			if (auto primitive_id_meta = M->getNamedMetadata("floor.primitive_id"); primitive_id_meta) {
				has_primitive_id = true;
			}
			if (auto barycentric_coord_meta = M->getNamedMetadata("floor.barycentric_coord"); barycentric_coord_meta) {
				has_barycentric_coord = true;
			}
			
			DBG(errs() << "> adding built-in args ...\n";)
			// add args if this is a kernel function
			if (is_kernel_func || is_task_func || is_mesh_func) {
				auto kernel_dim_node = F.getMetadata("kernel_dim");
				assert(kernel_dim_node);
				if (kernel_dim_node->getNumOperands() > 0) {
					auto& op = kernel_dim_node->getOperand(0);
					state.kernel_dim = (uint32_t)mdconst::extract<ConstantInt>(op)->getZExtValue();
					assert(state.kernel_dim >= 1 && state.kernel_dim <= 3);
				}
				
				auto kernel_simd_width_node = F.getMetadata("kernel_simd_width");
				if (kernel_simd_width_node && kernel_simd_width_node->getNumOperands() == 1) {
					auto& op = kernel_simd_width_node->getOperand(0);
					state.kernel_simd_width = (uint32_t)mdconst::extract<ConstantInt>(op)->getZExtValue();
					assert(state.kernel_simd_width >= 1 && state.kernel_simd_width <= 128);
				}
				
				MDNode* wg_size_node = func->getMetadata("reqd_work_group_size");
				if (wg_size_node && wg_size_node->getNumOperands() == 3) {
					state.kernel_local_size[0] = mdconst::extract<ConstantInt>(wg_size_node->getOperand(0))->getZExtValue();
					state.kernel_local_size[1] = mdconst::extract<ConstantInt>(wg_size_node->getOperand(1))->getZExtValue();
					state.kernel_local_size[2] = mdconst::extract<ConstantInt>(wg_size_node->getOperand(2))->getZExtValue();
					assert(state.kernel_local_size[0] > 0 && state.kernel_local_size[1] > 0 && state.kernel_local_size[2] > 0);
				}
				
				if (is_kernel_func && F.arg_size() >= VULKAN_KERNEL_ARG_COUNT + (has_soft_printf ? 1 : 0)) {
					state.group_id = get_arg_by_idx(VULKAN_GROUP_ID);
					state.group_size = get_arg_by_idx(VULKAN_GROUP_SIZE);
					state.sub_group_id = get_arg_by_idx(VULKAN_SUB_GROUP_ID);
					state.sub_group_local_id = get_arg_by_idx(VULKAN_SUB_GROUP_LOCAL_ID);
					state.sub_group_size = get_arg_by_idx(VULKAN_SUB_GROUP_SIZE);
					state.num_sub_groups = get_arg_by_idx(VULKAN_NUM_SUB_GROUPS);
					if (has_soft_printf) {
						state.soft_printf = get_arg_by_idx(-(VULKAN_KERNEL_ARG_COUNT + 1));
					}
				} else if (is_task_func && F.arg_size() >= VULKAN_TASK_ARG_COUNT + (has_soft_printf ? 1 : 0)) {
					state.group_id = get_arg_by_idx(VULKAN_TASK_GROUP_ID);
					state.group_size = get_arg_by_idx(VULKAN_TASK_GROUP_SIZE);
					state.sub_group_id = get_arg_by_idx(VULKAN_TASK_SUB_GROUP_ID);
					state.sub_group_local_id = get_arg_by_idx(VULKAN_TASK_SUB_GROUP_LOCAL_ID);
					state.sub_group_size = get_arg_by_idx(VULKAN_TASK_SUB_GROUP_SIZE);
					state.num_sub_groups = get_arg_by_idx(VULKAN_TASK_NUM_SUB_GROUPS);
					if (has_soft_printf) {
						state.soft_printf = get_arg_by_idx(-(VULKAN_TASK_ARG_COUNT + 1));
					}
				} else if (is_mesh_func && F.arg_size() >= VULKAN_MESH_ARG_COUNT + (has_soft_printf ? 1 : 0)) {
					state.group_id = get_arg_by_idx(VULKAN_MESH_GROUP_ID);
					state.group_size = get_arg_by_idx(VULKAN_MESH_GROUP_SIZE);
					state.sub_group_id = get_arg_by_idx(VULKAN_MESH_SUB_GROUP_ID);
					state.sub_group_local_id = get_arg_by_idx(VULKAN_MESH_SUB_GROUP_LOCAL_ID);
					state.sub_group_size = get_arg_by_idx(VULKAN_MESH_SUB_GROUP_SIZE);
					state.num_sub_groups = get_arg_by_idx(VULKAN_MESH_NUM_SUB_GROUPS);
					state.view_index = get_arg_by_idx(VULKAN_MESH_VIEW_INDEX);
					if (has_soft_printf) {
						state.soft_printf = get_arg_by_idx(-(VULKAN_MESH_ARG_COUNT + 1));
					}
				} else {
					errs() << "invalid kernel/task/mesh function (" << F.getName() << ") argument count: " << F.arg_size() << "\n";
				}
			}
			
			// add args if this is a vertex function
			if (is_vertex_func) {
				if (F.arg_size() >= VULKAN_VERTEX_ARG_COUNT + (has_soft_printf ? 1 : 0)) {
					// TODO: this should be optional / only happen on request
					state.vertex_id = get_arg_by_idx(VULKAN_VERTEX_ID);
					state.base_vertex_id = get_arg_by_idx(VULKAN_BASE_VERTEX_ID);
					state.view_index = get_arg_by_idx(VULKAN_VERTEX_VIEW_INDEX);
					state.instance_id = get_arg_by_idx(VULKAN_INSTANCE_ID);
					state.base_instance_id = get_arg_by_idx(VULKAN_BASE_INSTANCE_ID);
					if (has_soft_printf) {
						state.soft_printf = get_arg_by_idx(-(VULKAN_VERTEX_ARG_COUNT + 1));
					}
				} else {
					errs() << "invalid vertex function (" << F.getName() << ") argument count: " << F.arg_size() << "\n";
				}
			}
			
			// add args if this is a fragment function
			if (is_fragment_func) {
				const uint32_t opt_arg_count = (has_soft_printf ? 1u : 0u) + (has_primitive_id ? 1u : 0u) + (has_barycentric_coord ? 1u : 0u);
				if (F.arg_size() >= VULKAN_FRAGMENT_ARG_COUNT + opt_arg_count) {
					state.point_coord = get_arg_by_idx(VULKAN_POINT_COORD);
					state.frag_coord = get_arg_by_idx(VULKAN_FRAG_COORD);
					state.view_index = get_arg_by_idx(VULKAN_FRAGMENT_VIEW_INDEX);
					state.front_facing = get_arg_by_idx(VULKAN_FRONT_FACING);
					
					// NOTE: reverse order!
					uint32_t opt_arg_counter = 1;
					if (has_barycentric_coord) {
						state.barycentric_coord = get_arg_by_idx(-(VULKAN_FRAGMENT_ARG_COUNT + opt_arg_counter++));
					}
					if (has_primitive_id) {
						state.primitive_id = get_arg_by_idx(-(VULKAN_FRAGMENT_ARG_COUNT + opt_arg_counter++));
					}
					if (has_soft_printf) {
						state.soft_printf = get_arg_by_idx(-(VULKAN_FRAGMENT_ARG_COUNT + opt_arg_counter++));
					}
				} else {
					errs() << "invalid fragment function (" << F.getName() << ") argument count: " << F.arg_size() << "\n";
				}
			}
			
			// emit local size variable for this function (initialized externally at "run-time" / SPIR-V spec)
			// NOTE: if the function has a reqd_work_group_size (constant required work-group size), we don't need this
			if ((is_kernel_func || is_task_func || is_mesh_func) && !func->hasFnAttribute("reqd_work_group_size")) {
				auto local_size_type = llvm::FixedVectorType::get(llvm::Type::getInt32Ty(*ctx), 3);
				state.local_size = new GlobalVariable(*M,
													  local_size_type,
													  true,
													  GlobalVariable::ExternalLinkage,
													  nullptr,
													  F.getName().str() + ".vulkan_constant.local_size",
													  nullptr,
													  GlobalValue::NotThreadLocal,
													  0,
													  true);
			}
			
			// visit everything in this function
			DBG(errs() << "> handling instructions ...\n";)
			visit(F);
			for (auto& instr : state.kill_list) {
				instr->eraseFromParent();
			}
			
			// always modified
			DBG(errs() << "> " << F.getName() << " done\n";)
			return true;
		}
		
		// InstVisitor overrides...
		using InstVisitor<VulkanBuiltinParamHandling>::visit;
		void visit(Instruction& I) {
			InstVisitor<VulkanBuiltinParamHandling>::visit(I);
		}
		
		llvm::Value* handle_vec_load(Argument* id, uint32_t dim_idx, std::string_view name) {
			if (id == nullptr) {
				llvm::errs() << "failed to get id arg, probably not in a kernel function\n";
				llvm::errs().flush();
				return nullptr;
			}
			return builder->CreateExtractElement(builder->CreateLoad(id->getType()->getPointerElementType(), id), dim_idx,
												 std::string(name) + "_" + std::to_string(dim_idx));
		}
		
		enum class id {
			group_id,
			group_size,
			local_id,
			local_size,
			global_id,
			global_size,
			sub_group_id,
			sub_group_local_id,
			sub_group_size,
			num_sub_groups,
		};
		constexpr const std::string_view id_to_str(id id_val) {
			switch (id_val) {
				case id::group_id:
					return "group_id";
				case id::group_size:
					return "group_size";
				case id::local_id:
					return "local_id";
				case id::local_size:
					return "local_size";
				case id::global_id:
					return "global_id";
				case id::global_size:
					return "global_size";
				case id::sub_group_id:
					return "sub_group_id";
				case id::sub_group_local_id:
					return "sub_group_local_id";
				case id::sub_group_size:
					return "sub_group_size";
				case id::num_sub_groups:
					return "num_sub_groups";
			}
		}
		// helper function to load a specific ID value, cache it and return it (or just return from cache)
		template <id id_val>
		llvm::Value* get_id(uint32_t dim_idx) {
			if constexpr (id_val == id::group_id ||
						  id_val == id::group_size) {
				std::array<Value*, 3>* ld = nullptr;
				Argument* id = nullptr;
				
				switch (id_val) {
					case id::group_id:
						ld = &state.ld_group_id;
						id = state.group_id;
						break;
					case id::group_size:
						ld = &state.ld_group_size;
						id = state.group_size;
						break;
				}
				
				if (!(*ld)[dim_idx]) {
					(*ld)[dim_idx] = handle_vec_load(id, dim_idx, id_to_str(id_val));
				}
				return (*ld)[dim_idx];
			} else if constexpr (id_val == id::sub_group_id ||
								 id_val == id::sub_group_local_id ||
								 id_val == id::sub_group_size ||
								 id_val == id::num_sub_groups) {
				Value** ld = nullptr;
				Argument* id = nullptr;
				
				switch (id_val) {
					case id::sub_group_id:
						ld = &state.ld_sub_group_id;
						id = state.sub_group_id;
						break;
					case id::sub_group_local_id:
						ld = &state.ld_sub_group_local_id;
						id = state.sub_group_local_id;
						break;
					case id::sub_group_size:
						// use fixed SIMD width if available
						if (state.kernel_simd_width > 0) {
							return ConstantInt::get(Type::getInt32Ty(*ctx), state.kernel_simd_width);
						}
						ld = &state.ld_sub_group_size;
						id = state.sub_group_size;
						break;
					case id::num_sub_groups:
						// if both the SIMD width and the local size are fixed, we also have a fixed #sub-groups,
						// because the local size extent must be a multiple of the SIMD width
						if (state.kernel_simd_width > 0 && state.has_fixed_local_size()) {
							uint32_t local_size_extent = state.fixed_local_size_extent();
							assert((local_size_extent % state.kernel_simd_width) == 0u);
							return ConstantInt::get(Type::getInt32Ty(*ctx), local_size_extent / state.kernel_simd_width);
						}
						ld = &state.ld_num_sub_groups;
						id = state.num_sub_groups;
						break;
				}
				
				if (!(*ld)) {
					*ld = builder->CreateLoad(id->getType()->getPointerElementType(), id, id_to_str(id_val));
				}
				return *ld;
			} else if constexpr (id_val == id::local_size) {
				if (!state.ld_local_size[dim_idx]) {
					// this doesn't have a direct built-in equivalent, but must be loaded from the WorkgroupSize run-time constant
					// however: if a fixed work-group size is set, we can use a constant
					if (state.has_fixed_local_size()) {
						state.ld_local_size[dim_idx] = ConstantInt::get(Type::getInt32Ty(*ctx), state.kernel_local_size[dim_idx]);
					} else {
						assert(state.local_size);
						state.ld_local_size[dim_idx] = builder->CreateExtractElement(builder->CreateLoad(state.local_size->getType()->getPointerElementType(), state.local_size), dim_idx,
																					 std::string(id_to_str(id_val)) + "_" + std::to_string(dim_idx));
					}
				}
				return state.ld_local_size[dim_idx];
			} else if constexpr (id_val == id::global_size) {
				if (!state.ld_global_size[dim_idx]) {
					// this doesn't have a direct built-in equivalent, but must be computed from the WorkgroupSize constant
					// TODO/NOTE: this might need some more work on the spir-v side, right now this is always constant folded
					state.ld_global_size[dim_idx] = builder->CreateMul(get_id<id::group_size>(dim_idx), get_id<id::local_size>(dim_idx), "global_size_" + std::to_string(dim_idx));
				}
				return state.ld_global_size[dim_idx];
			} else if constexpr (id_val == id::local_id) {
				if (!state.ld_local_id[dim_idx]) {
					// since sub-group functionality is always enabled, we can no longer make use of LocalInvocationId, because:
					// Vulkan (1.3 spec): 15.9. Built-In Variables: "There is no direct relationship between SubgroupLocalInvocationId and LocalInvocationId"
					// we do however need to guarantee a direct relationship -> need to compute the local id from the sub-group IDs/sizes
					auto sglid = get_id<id::sub_group_local_id>(0);
					auto sgid = get_id<id::sub_group_id>(0);
					auto sgsize = get_id<id::sub_group_size>(0);
					auto linear_idx = builder->CreateAdd(builder->CreateMul(sgid, sgsize), sglid, "local_linear_id");
					// set range metadata on local_linear_id when we have a fixed local size
					if (state.has_fixed_local_size()) {
						libfloor_utils::add_range_info(*ctx, *(llvm::Instruction*)linear_idx, 0u, state.fixed_local_size_extent());
					}
					
					const auto handle_const_2D_lsize = [this, &linear_idx](const uint32_t lsize_x, Value* const_lsize_0, uint32_t dim) {
						// NOTE: since the SIMD width must be a power-of-two and work-group X dim must be a multiple of it
						//       -> work-group X dim must also be a power-of-two
						if (__builtin_popcount(lsize_x) != 1) {
							llvm::errs() << "work-group dim X must always be a power-of-two: in kernel function" << func->getName() << ":\n";
							llvm::errs() << *const_lsize_0 << " is not a power-of-two\n";
							llvm::errs().flush();
							return false;
						}
						// with POT work-group size: X = idx & (local-size-x - 1), Y = idx >> log2(local-size-x)
						if (dim == 0) {
							state.ld_local_id[0] = builder->CreateAnd(linear_idx, builder->getInt32(lsize_x - 1u), "local_id_0");
						} else if (dim == 1) {
							state.ld_local_id[1] = builder->CreateAShr(linear_idx, builder->getInt32(__builtin_ctz(lsize_x)), "local_id_1");
						} else if (dim == 2) {
							state.ld_local_id[2] = builder->getInt32(0);
						} else {
							assert(false);
							return false;
						}
						return true;
					};
					
					switch (state.kernel_dim) {
						case 1:
							// trivial case
							state.ld_local_id[dim_idx] = linear_idx;
							break;
						case 2: {
							// need to distribute 1D index to 2D dim (for now: sequentially, TODO: might want to try quad/morton/hilbert?)
							auto lsize_0 = get_id<id::local_size>(0);
							if (auto const_lsize_0 = dyn_cast_or_null<ConstantInt>(lsize_0); const_lsize_0) {
								// -> if the local size is constant, we can do this more optimally
								const auto lsize_x = uint32_t(const_lsize_0->getZExtValue());
								assert(lsize_x >= 1);
								auto const_lsize_1 = dyn_cast_or_null<ConstantInt>(get_id<id::local_size>(1));
								assert(const_lsize_1 && const_lsize_1->getZExtValue() >= 1);
								const auto lsize_y = uint32_t(const_lsize_1->getZExtValue());
								if (lsize_y == 1) {
									// -> nothing to do here
									state.ld_local_id[dim_idx] = (dim_idx == 0 ? linear_idx : builder->getInt32(0));
									break;
								}
								if (!handle_const_2D_lsize(lsize_x, const_lsize_0, dim_idx)) {
									return nullptr;
								}
							} else {
								// -> dynamic: similar to the constant variant, but at run-time
								// NOTE: we can't check/enforce the power-of-twoness of the work-group X dim (-> must rely on run-time)
								if (dim_idx == 0) {
									state.ld_local_id[0] = builder->CreateAnd(linear_idx, builder->CreateNUWSub(lsize_0, builder->getInt32(1)), "local_id_0");
								} else {
									state.ld_local_id[1] = builder->CreateAShr(linear_idx, builder->CreateIntrinsic(Intrinsic::cttz, { lsize_0->getType() }, { lsize_0, builder->getInt1(0) }), "local_id_1");
								}
							}
							break;
						}
						case 3: {
							// need to distribute 1D index to 3D dim (for now: sequentially)
							auto lsize_0 = get_id<id::local_size>(0);
							auto lsize_1 = get_id<id::local_size>(1);
							if (auto const_lsize_0 = dyn_cast_or_null<ConstantInt>(lsize_0); const_lsize_0) {
								// -> if the local size is constant, we can do this more optimally
								const auto lsize_x = uint32_t(const_lsize_0->getZExtValue());
								assert(lsize_x >= 1);
								auto const_lsize_1 = dyn_cast_or_null<ConstantInt>(get_id<id::local_size>(1));
								assert(const_lsize_1 && const_lsize_1->getZExtValue() >= 1);
								auto const_lsize_2 = dyn_cast_or_null<ConstantInt>(get_id<id::local_size>(2));
								assert(const_lsize_2 && const_lsize_2->getZExtValue() >= 1);
								const auto lsize_y = uint32_t(const_lsize_1->getZExtValue());
								const auto lsize_z = uint32_t(const_lsize_2->getZExtValue());
								if (lsize_y == 1 && lsize_z == 1) {
									// -> nothing to do here
									state.ld_local_id[dim_idx] = (dim_idx == 0 ? linear_idx : builder->getInt32(0));
									break;
								} else if (lsize_z == 1) {
									// same as 2D
									if (!handle_const_2D_lsize(lsize_x, const_lsize_0, dim_idx)) {
										return nullptr;
									}
									break;
								}
								// NOTE: see 2D
								if (__builtin_popcount(lsize_x) != 1) {
									llvm::errs() << "work-group dim X must always be a power-of-two: in kernel function" << func->getName() << ":\n";
									llvm::errs() << *const_lsize_0 << " is not a power-of-two\n";
									llvm::errs().flush();
									return nullptr;
								}
								if (lsize_y == 1) {
									// almost the same as 2D
									if (dim_idx == 0) {
										state.ld_local_id[0] = builder->CreateAnd(linear_idx, builder->getInt32(lsize_x - 1u), "local_id_0");
									} else if (dim_idx == 1) {
										state.ld_local_id[1] = builder->getInt32(0);
									} else if (dim_idx == 2) {
										state.ld_local_id[2] = builder->CreateAShr(linear_idx, builder->getInt32(__builtin_ctz(lsize_x)), "local_id_2");
									}
									break;
								}
								// -> full 3D local size dim
								// X = idx & (local-size-x - 1) // same as before, since X is POT
								// Y = (idx >> log2(local-size-x)) % local-size-y
								// Z = (idx >> log2(local-size-x)) / local-size-y
								// NOTE/TODO: could be optimized further if local-size-y is POT + could use magic div (but let other optimizers deal with it for now)
								if (dim_idx == 0) {
									state.ld_local_id[0] = builder->CreateAnd(linear_idx, builder->getInt32(lsize_x - 1u), "local_id_0");
								} else if (dim_idx == 1) {
									state.ld_local_id[1] = builder->CreateURem(builder->CreateAShr(linear_idx, builder->getInt32(__builtin_ctz(lsize_x))), builder->getInt32(lsize_y), "local_id_1");
								} else if (dim_idx == 2) {
									state.ld_local_id[2] = builder->CreateUDiv(builder->CreateAShr(linear_idx, builder->getInt32(__builtin_ctz(lsize_x))), builder->getInt32(lsize_y), "local_id_2");
								}
							} else {
								// -> dynamic: similar to the constant variant, but at run-time
								// NOTE: we can't check/enforce the power-of-twoness of the work-group X dim (-> must rely on run-time)
								if (dim_idx == 0) {
									state.ld_local_id[0] = builder->CreateAnd(linear_idx, builder->CreateNUWSub(lsize_0, builder->getInt32(1)), "local_id_0");
								} else if (dim_idx == 1) {
									state.ld_local_id[1] = builder->CreateURem(builder->CreateAShr(linear_idx, builder->CreateIntrinsic(Intrinsic::cttz, { lsize_0->getType() }, { lsize_0, builder->getInt1(0) })), lsize_1, "local_id_1");
								} else if (dim_idx == 2) {
									state.ld_local_id[2] = builder->CreateUDiv(builder->CreateAShr(linear_idx, builder->CreateIntrinsic(Intrinsic::cttz, { lsize_0->getType() }, { lsize_0, builder->getInt1(0) })), lsize_1, "local_id_2");
								}
							}
							break;
						}
					}
				}
				return state.ld_local_id[dim_idx];
			} else if constexpr (id_val == id::global_id) {
				if (!state.ld_global_id[dim_idx]) {
					state.ld_global_id[dim_idx] = builder->CreateAdd(builder->CreateMul(get_id<id::group_id>(dim_idx), get_id<id::local_size>(dim_idx)), get_id<id::local_id>(dim_idx), "global_id_" + std::to_string(dim_idx));
				}
				return state.ld_global_id[dim_idx];
			} else {
				static_assert([]() constexpr { return false; }(), "unhandled id value");
			}
		}
		
		//
		void visitCallInst(CallInst &I) {
			const auto func_name = I.getCalledFunction()->getName();
			if(!func_name.startswith("floor.builtin.")) return;
			
			// helper function to retrieve a constant dim index for a id variable load
			const auto get_dim_idx = [this](Value* dim_op, Instruction* originating_call_or_null) {
				const auto const_dim_op = dyn_cast_or_null<ConstantInt>(dim_op);
				if (!const_dim_op) {
					llvm::errs() << "dim index in " << state.kernel_dim << "D kernel " << func->getName() << " is not constant:\n";
					if (originating_call_or_null) {
						llvm::errs() << *originating_call_or_null << "\n";
					}
					llvm::errs().flush();
					return 0u;
				}
				
				const auto dim_idx = const_dim_op->getZExtValue();
				if ((dim_idx + 1) > state.kernel_dim) {
					llvm::errs() << "out-of-bounds dim index " << dim_idx << " in " << state.kernel_dim << "D kernel " << func->getName() << ":\n";
					if (originating_call_or_null) {
						llvm::errs() << *originating_call_or_null << "\n";
					}
					llvm::errs().flush();
					return 0u;
				}
				return uint32_t(dim_idx);
			};
			
			// figure out which one we need
			if (func_name == "floor.builtin.global_id.i32") {
				I.replaceAllUsesWith(get_id<id::global_id>(get_dim_idx(I.getOperand(0), &I)));
			} else if (func_name == "floor.builtin.local_id.i32") {
				I.replaceAllUsesWith(get_id<id::local_id>(get_dim_idx(I.getOperand(0), &I)));
			} else if (func_name == "floor.builtin.group_id.i32") {
				I.replaceAllUsesWith(get_id<id::group_id>(get_dim_idx(I.getOperand(0), &I)));
			} else if (func_name == "floor.builtin.group_size.i32") {
				I.replaceAllUsesWith(get_id<id::group_size>(get_dim_idx(I.getOperand(0), &I)));
			} else if (func_name == "floor.builtin.local_size.i32") {
				I.replaceAllUsesWith(get_id<id::local_size>(get_dim_idx(I.getOperand(0), &I)));
			} else if (func_name == "floor.builtin.global_size.i32") {
				I.replaceAllUsesWith(get_id<id::global_size>(get_dim_idx(I.getOperand(0), &I)));
			} else if (func_name == "floor.builtin.work_dim.i32") {
				I.replaceAllUsesWith(builder->getInt32(state.kernel_dim));
			} else if (func_name == "floor.builtin.sub_group_id.i32") {
				I.replaceAllUsesWith(get_id<id::sub_group_id>(0));
			} else if (func_name == "floor.builtin.sub_group_local_id.i32") {
				I.replaceAllUsesWith(get_id<id::sub_group_local_id>(0));
			} else if (func_name == "floor.builtin.sub_group_size.i32") {
				I.replaceAllUsesWith(get_id<id::sub_group_size>(0));
			} else if (func_name == "floor.builtin.num_sub_groups.i32") {
				I.replaceAllUsesWith(get_id<id::num_sub_groups>(0));
			} else if (func_name == "floor.builtin.vertex_id.i32") {
				if(state.vertex_id == nullptr) {
					DBG(printf("failed to get vertex_id arg, probably not in a vertex function?\n"); fflush(stdout);)
					return;
				}
				I.replaceAllUsesWith(builder->CreateLoad(state.vertex_id->getType()->getPointerElementType(), state.vertex_id, "vertex_index"));
			} else if (func_name == "floor.builtin.base_vertex_id.i32") {
				if(state.base_vertex_id == nullptr) {
					DBG(printf("failed to get base_vertex_id arg, probably not in a vertex function?\n"); fflush(stdout);)
					return;
				}
				I.replaceAllUsesWith(builder->CreateLoad(state.base_vertex_id->getType()->getPointerElementType(), state.vertex_id, "base_vertex_index"));
			} else if (func_name == "floor.builtin.instance_id.i32") {
				if(state.instance_id == nullptr) {
					DBG(printf("failed to get instance_id arg, probably not in a vertex function?\n"); fflush(stdout);)
					return;
				}
				I.replaceAllUsesWith(builder->CreateLoad(state.instance_id->getType()->getPointerElementType(), state.instance_id, "instance_index"));
			} else if (func_name == "floor.builtin.base_instance_id.i32") {
				if(state.base_instance_id == nullptr) {
					DBG(printf("failed to get base_instance_id arg, probably not in a vertex function?\n"); fflush(stdout);)
					return;
				}
				I.replaceAllUsesWith(builder->CreateLoad(state.base_instance_id->getType()->getPointerElementType(), state.base_instance_id, "base_instance_index"));
			} else if (func_name == "floor.builtin.point_coord.float2") {
				if(state.point_coord == nullptr) {
					DBG(printf("failed to get point_coord arg, probably not in a fragment function?\n"); fflush(stdout);)
					return;
				}
				I.replaceAllUsesWith(builder->CreateLoad(state.point_coord->getType()->getPointerElementType(), state.point_coord, "point_coord"));
			} else if (func_name == "floor.builtin.front_facing.bool") {
				if(state.front_facing == nullptr) {
					DBG(printf("failed to get front_facing arg, probably not in a fragment function?\n"); fflush(stdout);)
					return;
				}
				I.replaceAllUsesWith(builder->CreateLoad(state.front_facing->getType()->getPointerElementType(), state.front_facing, "front_facing"));
			} else if (func_name == "floor.builtin.frag_coord.float4") {
				llvm::errs() << "direct use of frag coord is not allowed: in function" << func->getName() << "\n";
				return;
			} else if (func_name == "floor.builtin.get_printf_buffer") {
				if(state.soft_printf == nullptr) {
					DBG(printf("failed to get printf_buffer arg, probably not in a kernel/vertex/fragment function?\n"); fflush(stdout);)
					return;
				}
				// special case
				I.replaceAllUsesWith(state.soft_printf);
			} else if (func_name == "floor.builtin.view_index.i32") {
				if(state.view_index == nullptr) {
					DBG(printf("failed to get view_index arg, probably not in a shader function?\n"); fflush(stdout);)
					return;
				}
				I.replaceAllUsesWith(builder->CreateLoad(state.view_index->getType()->getPointerElementType(), state.view_index, "view_index"));
			} else if (func_name == "floor.builtin.primitive_id.i32") {
				if (state.primitive_id == nullptr) {
					llvm::errs() << "failed to get primitive_id arg, not in a fragment function or feature is not enabled\n";
					llvm::errs().flush();
					return;
				}
				I.replaceAllUsesWith(builder->CreateLoad(state.primitive_id->getType()->getPointerElementType(), state.primitive_id, "primitive_id"));
			} else if (func_name == "floor.builtin.barycentric_coord.float3") {
				if (state.barycentric_coord == nullptr) {
					llvm::errs() << "failed to get barycentric_coord arg, not in a fragment function or feature is not enabled\n";
					llvm::errs().flush();
					return;
				}
				I.replaceAllUsesWith(builder->CreateLoad(state.barycentric_coord->getType()->getPointerElementType(), state.barycentric_coord, "barycentric_coord"));
			}
			// NOTE: we can't erase "I" from the parent directly, because this will mess up the builder insertion point -> need to do it at the end
			state.kill_list.emplace_back(&I);
		}
	};

	struct vulkan_function_scope_type_replacement {
		// we must reuse cloned types here
		static std::unordered_map<Type*, Type*> clone_type_map;
		
		static StructType* clone_logical_type(StructType* st_type) {
			SmallVector<Type*, 8> field_types;
			for (uint32_t i = 0, count = st_type->getStructNumElements(); i < count; ++i) {
				auto field_type = st_type->getStructElementType(i);
				// recurse?
				if (auto clone_type_iter = clone_type_map.find(field_type); clone_type_iter != clone_type_map.end()) {
					field_type = clone_type_iter->second;
				} else if (auto field_st_type = dyn_cast_or_null<StructType>(field_type); field_st_type) {
					field_type = clone_logical_type(field_st_type);
					clone_type_map.emplace(field_st_type, field_type);
				} else if (auto field_arr_type = dyn_cast_or_null<ArrayType>(field_type); field_arr_type) {
					field_type = clone_logical_type(field_arr_type);
					clone_type_map.emplace(field_arr_type, field_type);
				}
				field_types.emplace_back(field_type);
			}
			return StructType::create(field_types, st_type->getName().str() + ".clone", st_type->isPacked());
		}
		
		static ArrayType* clone_logical_type(ArrayType* arr_type) {
			auto elem_type = arr_type->getElementType();
			// recurse?
			if (auto clone_type_iter = clone_type_map.find(elem_type); clone_type_iter != clone_type_map.end()) {
				elem_type = clone_type_iter->second;
			} else if (auto elem_st_type = dyn_cast_or_null<StructType>(elem_type); elem_st_type) {
				elem_type = clone_logical_type(elem_st_type);
				clone_type_map.emplace(elem_st_type, elem_type);
			} else if (auto elem_arr_type = dyn_cast_or_null<ArrayType>(elem_type); elem_arr_type) {
				elem_type = clone_logical_type(elem_arr_type);
				clone_type_map.emplace(elem_arr_type, elem_type);
			}
			// else: nop
			
			return ArrayType::get(elem_type, arr_type->getNumElements());
		}
		
		static void update_initializer(Constant* init, Type* new_type) {
			init->mutateType(new_type);
			
			// recurse?
			if (auto const_st = dyn_cast_or_null<ConstantStruct>(init); const_st) {
				auto st_type = dyn_cast_or_null<StructType>(new_type);
				assert(st_type);
				for (auto field_idx = 0u, field_count = const_st->getNumOperands(); field_idx < field_count; ++field_idx) {
					update_initializer(const_st->getOperand(field_idx), st_type->getStructElementType(field_idx));
				}
			} else if (auto const_arr = dyn_cast_or_null<ConstantArray>(init); const_arr) {
				auto arr_type = dyn_cast_or_null<ArrayType>(new_type);
				assert(arr_type);
				auto arr_elem_type = arr_type->getElementType();
				for (auto elem_idx = 0u, elem_count = const_arr->getNumOperands(); elem_idx < elem_count; ++elem_idx) {
					update_initializer(const_arr->getOperand(elem_idx), arr_elem_type);
				}
			}
		}
		
		static bool handle_function_scope_variable(Value* val, Function* restrict_function = nullptr) {
			// Vulkan SPIR-V requires structs/arrays in Function/Private scope to have a logical layout (i.e. no decorations),
			// which means we're very likely to run into this issue when we have an alloca or constant GV of a struct or array
			// -> clone and replace the type and hope for the best (this may break things ...)
			
			auto val_type = val->getType();
			auto val_ptr_type = dyn_cast_or_null<PointerType>(val_type);
			if (!val_ptr_type) {
				return false;
			}
			auto elem_type = val_ptr_type->getPointerElementType();
			
			Type* cloned_type = nullptr;
			if (auto clone_type_iter = clone_type_map.find(elem_type); clone_type_iter != clone_type_map.end()) {
				cloned_type = clone_type_iter->second;
			} else if (auto st_type = dyn_cast_or_null<StructType>(elem_type); st_type) {
				cloned_type = clone_logical_type(st_type);
				clone_type_map.emplace(elem_type, cloned_type);
			} else if (auto arr_type = dyn_cast_or_null<ArrayType>(elem_type); arr_type) {
				cloned_type = clone_logical_type(arr_type);
				clone_type_map.emplace(elem_type, cloned_type);
			}
			
			// exit if we don't need to do anything
			if (!cloned_type) {
				return false;
			}
			
			// mutate
			if (auto alloca = dyn_cast_or_null<AllocaInst>(val); alloca) {
				alloca->setAllocatedType(cloned_type);
			} else if (auto GVar = dyn_cast_or_null<GlobalVariable>(val); GVar) {
				GVar->mutateValueType(cloned_type);
				if (GVar->hasInitializer()) {
					update_initializer(GVar->getInitializer(), cloned_type);
				}
			} else if (auto GV = dyn_cast_or_null<GlobalValue>(val); GV) {
				GV->mutateValueType(cloned_type);
			}
			val->mutateType(PointerType::get(cloned_type, val_ptr_type->getAddressSpace()));
			
			// update users
			// NOTE: this only runs on direct users, it does not recurse deeper, which may be necessary ...
			libfloor_utils::for_all_instruction_users(*val, [&cloned_type, &elem_type](Instruction& instr) {
				if (auto GEP = dyn_cast_or_null<GetElementPtrInst>(&instr); GEP) {
					assert(GEP->getSourceElementType() == elem_type);
					GEP->setSourceElementType(cloned_type);
				} else {
					llvm::errs() << "unhandled function scope replacement instruction:\n\t" << instr << "\n";
				}
			}, restrict_function /* restrict to the specified function (or none if nullptr) */);
			
			return true;
		}
	};
	std::unordered_map<Type*, Type*> vulkan_function_scope_type_replacement::clone_type_map;
	
	// VulkanFinal
	struct VulkanFinal : public FunctionPass, InstVisitor<VulkanFinal> {
		friend class InstVisitor<VulkanFinal>;
		
		static char ID; // Pass identification, replacement for typeid
		
		std::shared_ptr<llvm::IRBuilder<>> builder;
		
		Module* M { nullptr };
		LLVMContext* ctx { nullptr };
		Function* func { nullptr };
		bool is_kernel_func { false };
		bool is_vertex_func { false };
		bool is_fragment_func { false };
		bool is_task_func { false };
		bool is_mesh_func { false };
		
		VulkanFinal() :
		FunctionPass(ID) {
			initializeVulkanFinalPass(*PassRegistry::getPassRegistry());
		}
		
		void getAnalysisUsage(AnalysisUsage &AU) const override {
			AU.addRequired<AAResultsWrapperPass>();
			AU.addRequired<GlobalsAAWrapperPass>();
			AU.addRequired<AssumptionCacheTracker>();
			AU.addRequired<TargetLibraryInfoWrapperPass>();
		}
		
		// retrieves the target Vulkan version from metadata
		static std::pair<uint32_t, uint32_t> get_vulkan_version(Module& M) {
			const llvm::NamedMDNode *VulkanVersion =
			M.getNamedMetadata("vulkan.version");
			if (VulkanVersion == nullptr || VulkanVersion->getNumOperands() != 1) {
				return {};
			}
			
			const MDNode *vulkan_version_md = VulkanVersion->getOperand(0);
			if (vulkan_version_md->getNumOperands() < 2) {
				return {};
			}
			
			uint64_t version_major = 0, version_minor = 0;
			
			const MDOperand &version_major_op = vulkan_version_md->getOperand(0);
			if (const ConstantAsMetadata *version_major_md =
				dyn_cast_or_null<ConstantAsMetadata>(version_major_op.get())) {
				if (const ConstantInt *version_major_int =
					dyn_cast_or_null<ConstantInt>(version_major_md->getValue())) {
					version_major = version_major_int->getZExtValue();
				} else {
					return {};
				}
			} else {
				return {};
			}
			
			const MDOperand &version_minor_op = vulkan_version_md->getOperand(1);
			if (const ConstantAsMetadata *version_minor_md =
				dyn_cast_or_null<ConstantAsMetadata>(version_minor_op.get())) {
				if (const ConstantInt *version_minor_int =
					dyn_cast_or_null<ConstantInt>(version_minor_md->getValue())) {
					version_minor = version_minor_int->getZExtValue();
				} else {
					return {};
				}
			} else {
				return {};
			}
			
			return { version_major, version_minor };
		}
		
		struct combined_st_entry_t {
			llvm::Type* type;
			llvm::Value* value;
			size_t value_size;
			size_t elemental_size;
		};

		bool runOnFunction(Function &F) override {
			// exit if empty function
			if(F.empty()) return false;
			DBG(errs() << "in func: "; errs().write_escaped(F.getName()) << '\n';)
			
			// determine this function type + exit if it isn't a kernel or shader function
			is_kernel_func = F.getCallingConv() == CallingConv::FLOOR_KERNEL;
			is_vertex_func = F.getCallingConv() == CallingConv::FLOOR_VERTEX;
			is_fragment_func = F.getCallingConv() == CallingConv::FLOOR_FRAGMENT;
			is_task_func = F.getCallingConv() == CallingConv::FLOOR_TASK;
			is_mesh_func = F.getCallingConv() == CallingConv::FLOOR_MESH;
			if (!is_kernel_func &&
				!is_vertex_func &&
				!is_fragment_func &&
				!is_task_func &&
				!is_mesh_func) {
				return false;
			}
			
			//
			M = F.getParent();
			ctx = &M->getContext();
			func = &F;
			builder = std::make_shared<llvm::IRBuilder<>>(*ctx);
			
			//const auto vulkan_version = get_vulkan_version(*M);
			
			// transform all ConstantExpr GEPs to normal GEPs,
			// since we don't have this in Vulkan + require this to perform valid transforms later on
			transform_constexpr_geps(F);
			
			// visit everything in this function
			DBG(errs() << "> handling instructions ...\n";)
			visit(F);
			
			DBG(errs() << "> updating function signature / parameters ...\n";)
			{
				// update function signature / parameters:
				//  * change the return type to void (vs/fs returns have already been modified)
				//  * transform constant AS pointers to either Uniform or StorageBuffer AS
				//  * transform StorageBuffer AS image pointers to Uniform AS
				//  * enclose Uniform parameters in a struct (note that enclosing SSBOs happens later)
				//  * handle SSBO array transforms
				// NOTE: must be called after visiting rets and other ret type/val users
				std::vector<Type*> param_types;
				DBG(errs() << "func: " << F.getName().str() << "\n";)
				for (auto& arg : F.args()) {
					auto arg_type = arg.getType();
					DBG(errs() << "\targ: " << arg.getName().str() << ": " << *arg_type;)
					
					// arg type must be a pointer
					if (const auto ptr_type = dyn_cast<llvm::PointerType>(arg_type);
						ptr_type && (ptr_type->getAddressSpace() == SPIRAS_Constant || ptr_type->getAddressSpace() == SPIRAS_StorageBuffer)) {
						const auto ptr_as = ptr_type->getAddressSpace();
						auto elem_type = ptr_type->getPointerElementType();
						const auto arg_idx = param_types.size();
						
						// handle IUBs
						const auto iub_attr = F.getAttributeAtIndex(llvm::AttributeList::FirstArgIndex + arg_idx, "vulkan_iub");
						const auto is_iub = (iub_attr.getRawPointer() != nullptr);
						assert((!is_iub || (is_iub && arg.onlyReadsMemory())) && "IUB must be read-only");
						DBG(
							if (is_iub) {
								errs() << " (IUB)";
							}
						)
						
						// handle SSBO arrays
						const auto ssbo_array_attr = F.getAttributeAtIndex(llvm::AttributeList::FirstArgIndex + arg_idx, "vulkan_ssbo_array");
						const auto is_ssbo_array = (ssbo_array_attr.getRawPointer() != nullptr);
						assert(!(is_ssbo_array && is_iub) && "can't be both IUB and SSBO array");
						assert((!is_ssbo_array || (is_ssbo_array && ptr_as == SPIRAS_StorageBuffer)) && "wrong SSBO array address space");
						
						// since there is a limit on how many IUBs we can have and how large they can be, some arguments might fall back to using SSBOs
						// NOTE: DereferenceableOrNull is not considered as SSBO-uniform, because it likely (always?) means that the argument is a
						//       "runtime array" of some sort, which must not be enclosed in a struct here, but will be dealt with in the SPIR-V backend
						const auto is_ssbo_uniform = (!is_iub && !is_ssbo_array && arg.onlyReadsMemory() &&
													  arg.hasAttribute(Attribute::Dereferenceable));
						
						// any image/opaque type is unsized
						const auto is_sized = elem_type->isSized();
						
						// decide storage class
						const auto storage_class = (is_iub || !is_sized ? SPIRAS_Uniform : SPIRAS_StorageBuffer);
						
						// transform storage class if necessary +
						// for IUBs/SSBO-Uniform: enclose in unique struct type, because we later need to add a "Block" decoration on it,
						//                        for which we need to have a unique LLVM type to not run into nested Block/BufferBlock issues
						if (is_iub || is_ssbo_uniform) {
							const auto value_size = M->getDataLayout().getTypeStoreSize(elem_type).getFixedValue();
							arg_type = enclose_in_struct({ combined_st_entry_t { elem_type, &arg, value_size, value_size } }, { elem_type }, arg, nullptr, F, storage_class);
						} else if (is_ssbo_array) {
							// perform SSBO array transforms
							arg_type = handle_ssbo_array_transforms(F, arg);
						} else if (storage_class != ptr_as) {
							arg_type = elem_type->getPointerTo(storage_class);
							arg.mutateType(arg_type);
							
							// update users
							std::vector<ReturnInst*> returns; // returns to fix -> there shouldn't be any here
							libfloor_utils::for_all_instruction_users(arg, [this, &arg, &storage_class, &returns](Instruction& instr) {
								fix_instruction_users(*ctx, instr, arg, storage_class, false, returns);
							});
							assert(returns.empty() && "unexpected return type change");
						}
						DBG(errs() << " -> " << *arg_type;)
					}
					DBG(errs() << "\n";)
					
					param_types.push_back(arg_type);
				}

				auto new_func_type = FunctionType::get(llvm::Type::getVoidTy(*ctx), param_types, false);
				F.mutateType(PointerType::get(new_func_type, 0));
				F.mutateFunctionType(new_func_type);
				F.removeRetAttr(Attribute::NoUndef);
			}
			
			// ensure all work-group/local memory variables are enclosed inside a unique struct + code is updated accordingly
			DBG(errs() << "> updating work-group/local memory variables ...\n";)
			enclose_work_group_memory();
			
			// always modified
			DBG(errs() << "> " << F.getName() << " done\n";)
			return true;
		}
		
		void enclose_work_group_memory() {
			// find all GVs / local memory variables used in this function
			std::vector<llvm::GlobalVariable*> lmem_vars;
			for (auto& GV : M->globals()) {
				if (GV.getType()->getAddressSpace() != SPIRAS_Local) {
					continue;
				}
				
				// check if GV is used in this function
				bool is_used = false;
				libfloor_utils::for_all_instruction_users(GV, [&is_used](const Instruction&) {
					// always true with restriction below
					is_used = true;
				}, func /* restrict to this function */);
				if (!is_used) {
					continue;
				}
				lmem_vars.emplace_back(&GV);
			}
			if (lmem_vars.empty()) {
				return;
			}
			
			// -> enclose
			
			// move all local memory variables into a single combined struct:
			std::vector<combined_st_entry_t> combined_st;
			uint64_t total_size = 0u;
			// -> gather all
			for (auto& GV : lmem_vars) {
				const auto value_size = M->getDataLayout().getTypeStoreSize(GV->getValueType()).getFixedValue();
				const auto elemental_type_size = M->getDataLayout().getTypeStoreSize(libfloor_utils::get_elemental_type(GV->getValueType())).getFixedValue();
				combined_st.emplace_back(GV->getValueType(), GV, value_size, elemental_type_size);
			}
			// -> sort from largest to smallest
			std::stable_sort(combined_st.begin(), combined_st.end(), [](const combined_st_entry_t& lhs, const combined_st_entry_t& rhs) {
				return lhs.value_size > rhs.value_size;
			});
			// -> add padding
			for (auto field_iter = combined_st.begin(); field_iter != combined_st.end(); ) {
				const auto& field = *field_iter;
				total_size += field.value_size;
				++field_iter;
				
				if (auto cur_alignment = total_size % field.elemental_size; cur_alignment != 0u) {
					// need to add padding
					auto padding = field.elemental_size - cur_alignment;
					total_size += padding;
					// if the padding is small enough, just add small types
					if (padding < 4) {
						if (padding == 3) {
							field_iter = combined_st.insert(field_iter, combined_st_entry_t { llvm::Type::getInt8Ty(*ctx), nullptr, 1u, 1u });
							++field_iter;
							field_iter = combined_st.insert(field_iter, combined_st_entry_t { llvm::Type::getInt16Ty(*ctx), nullptr, 2u, 2u });
							++field_iter;
						} else if (padding == 2) {
							field_iter = combined_st.insert(field_iter, combined_st_entry_t { llvm::Type::getInt16Ty(*ctx), nullptr, 2u, 2u });
							++field_iter;
						} else {
							assert(padding == 1);
							field_iter = combined_st.insert(field_iter, combined_st_entry_t { llvm::Type::getInt8Ty(*ctx), nullptr, 1u, 1u });
							++field_iter;
						}
					} else {
						// otherwise: add an array
						const auto pad_array_type = llvm::ArrayType::get(llvm::Type::getInt8Ty(*ctx), padding);
						const auto value_size = M->getDataLayout().getTypeStoreSize(pad_array_type).getFixedValue();
						assert(value_size == padding);
						field_iter = combined_st.insert(field_iter, combined_st_entry_t { pad_array_type, nullptr, value_size, 1u });
						++field_iter;
					}
				}
			}
			assert(total_size > 0u);
			
			std::vector<llvm::Type*> combined_elem_types;
			combined_elem_types.reserve(combined_st.size());
			for (const auto& field : combined_st) {
				combined_elem_types.emplace_back(field.type);
			}
			
			const auto combined_name = "wg.enclose." + func->getName().str();
			auto combined_st_type = llvm::StructType::create(*ctx, combined_elem_types, combined_name + ".struct");
			GlobalVariable* combined_st_gv = new GlobalVariable(*M, combined_st_type, false, GlobalValue::InternalLinkage,
																nullptr, combined_name, lmem_vars[0] /* insert before first original GV */,
																GlobalValue::NotThreadLocal, SPIRAS_Local, false);
			
			// adjust all instructions accordingly
			enclose_in_struct(std::move(combined_st), std::move(combined_elem_types), *combined_st_gv, combined_st_type, *func,
							  SPIRAS_Local /* no change */, false /* already prefixed */);
			
			// add dummy initializer
			combined_st_gv->setInitializer(UndefValue::get(combined_st_gv->getValueType()));
			
			// add aliased types
			const auto add_lmem_alias = [this, &total_size, &lmem_vars](llvm::Type* base_type, const std::string& base_type_name) {
				const auto arr_elem_size = M->getDataLayout().getTypeStoreSize(base_type).getFixedValue();
				const auto arr_type = ArrayType::get(base_type, total_size / arr_elem_size);
				const auto st_arr_name = "wg.alias." + func->getName().str() + "." + base_type_name;
				const auto st_arr_type = llvm::StructType::create(*ctx, arr_type, st_arr_name + ".struct");
				GlobalVariable* alias_arr_gv = new GlobalVariable(*M, st_arr_type, false, GlobalValue::InternalLinkage,
																  nullptr, st_arr_name, lmem_vars[0] /* insert before first original GV */,
																  GlobalValue::NotThreadLocal, SPIRAS_Local, false);
				alias_arr_gv->setAlignment(MaybeAlign { arr_elem_size });
				alias_arr_gv->setInitializer(UndefValue::get(alias_arr_gv->getValueType()));
			};
			add_lmem_alias(llvm::Type::getInt8Ty(*ctx), "i8");
			if (total_size >= 2) {
				add_lmem_alias(llvm::Type::getInt16Ty(*ctx), "i16");
				// TODO: check float16 support?
			}
			if (total_size >= 4) {
				add_lmem_alias(llvm::Type::getInt32Ty(*ctx), "i32");
				add_lmem_alias(llvm::Type::getFloatTy(*ctx), "f32");
			}
			
			// TODO: replace work-group/local pointer bitcasts with accesses into aliased memory (fix broken optimizations, maybe functional float atomic workaround?)
		}
		
		//! vals + elem_types: all values being enclosed in the struct + their resp. element types
		//! root_st: new root variable (struct)
		//! existing_enclosed_st_type: an enclosing struct type can also be created outside this function and used in here, rather than this function creating its own
		llvm::Type* enclose_in_struct(std::vector<combined_st_entry_t> fields, std::vector<llvm::Type*> elem_types, llvm::Value& root_st,
									  llvm::StructType* existing_enclosed_st_type,
									  const llvm::Function& parent_function,
									  const uint32_t storage_class,
									  const bool add_enclose_name = true) {
			assert(!fields.empty() && !elem_types.empty() && fields.size() == elem_types.size());
			auto enclosed_type = (existing_enclosed_st_type ? existing_enclosed_st_type->getPointerTo(storage_class) : nullptr);
			if (!enclosed_type) {
				assert(fields.size() == 1);
				std::string st_name = (add_enclose_name ? "enclose." : "");
				if (!fields[0].value->getName().empty()) {
					st_name += fields[0].value->getName().str() + ".";
				} else {
					st_name += (add_enclose_name ? "" : ".");
				}
				st_name += "st";
				auto st_type = llvm::StructType::create(*ctx, elem_types, st_name);
				enclosed_type = st_type->getPointerTo(storage_class);
				fields[0].value->mutateType(enclosed_type);
			}
			
			// replace users
			for (uint32_t st_elem_idx = 0, st_elem_count = uint32_t(elem_types.size()); st_elem_idx < st_elem_count; ++st_elem_idx) {
				if (!fields[st_elem_idx].value) {
					// padding values are nullptr
					continue;
				}
				llvm::Value* idx_list[] {
					llvm::ConstantInt::get(llvm::Type::getInt32Ty(*ctx), 0),
					llvm::ConstantInt::get(llvm::Type::getInt32Ty(*ctx), st_elem_idx),
				};
				auto& val = *fields[st_elem_idx].value;
				libfloor_utils::for_all_users(val, [this, &root_st, &idx_list, &storage_class](User& user) {
					if (auto instr = dyn_cast<Instruction>(&user); instr) {
						if (auto load_instr = dyn_cast_or_null<LoadInst>(instr); load_instr) {
							const auto cexpr_gep = dyn_cast_or_null<ConstantExpr>(load_instr->getOperand(0));
							if (!cexpr_gep || cexpr_gep->getOpcode() != Instruction::GetElementPtr) {
								auto elem_gep = llvm::GetElementPtrInst::CreateInBounds(root_st.getType()->getScalarType()->getPointerElementType(),
																						&root_st, idx_list, "", instr);
								auto repl_instr = new LoadInst(elem_gep->getType()->getPointerElementType(), elem_gep, instr->getName(),
															   false, instr);
								repl_instr->setDebugLoc(instr->getDebugLoc());
								instr->replaceAllUsesWith(repl_instr);
								instr->eraseFromParent();
							}
							// else: assume the constexpr GEP was already replaced
						} else if (auto store_instr = dyn_cast_or_null<StoreInst>(instr); store_instr) {
							const auto cexpr_gep = dyn_cast_or_null<ConstantExpr>(store_instr->getOperand(1));
							if (!cexpr_gep || cexpr_gep->getOpcode() != Instruction::GetElementPtr) {
								auto elem_gep = llvm::GetElementPtrInst::CreateInBounds(root_st.getType()->getScalarType()->getPointerElementType(),
																						&root_st, idx_list, "", instr);
								auto repl_instr = new StoreInst(store_instr->getOperand(0), elem_gep, store_instr->isVolatile(),
																store_instr->getAlign(), store_instr->getOrdering(), store_instr->getSyncScopeID(),
																instr);
								repl_instr->setDebugLoc(instr->getDebugLoc());
								instr->replaceAllUsesWith(repl_instr);
								instr->eraseFromParent();
							}
							// else: assume the constexpr GEP was already replaced
						} else if (auto GEP = dyn_cast_or_null<GetElementPtrInst>(instr); GEP) {
							// copy/create new GEP with "0" index at the front
							SmallVector<Value*, 8> indices;
							if (storage_class == SPIRAS_Local) {
								// for work-group/local memory: drop the first index of the GEP (which must be 0) and prefix GEP with two 0's
								indices = { idx_list[0], idx_list[1] };
								assert(isa<ConstantInt>(GEP->idx_begin()->get()));
								assert(dyn_cast<ConstantInt>(GEP->idx_begin()->get())->isZero());
								auto gep_idx_range = make_range(GEP->idx_begin() + 1, GEP->idx_end());
								for (auto& idx : gep_idx_range) {
									indices.emplace_back(idx);
								}
							} else {
								indices = { idx_list[1] };
								for (auto& idx : GEP->indices()) {
									indices.emplace_back(idx);
								}
							}
							auto pointee_type = root_st.getType()->getPointerElementType();
							auto repl_instr = llvm::GetElementPtrInst::Create(pointee_type, &root_st, indices, "", instr);
							if (GEP->isInBounds()) {
								repl_instr->setIsInBounds();
							}
							repl_instr->setDebugLoc(instr->getDebugLoc());
							instr->replaceAllUsesWith(repl_instr, true /* allow address space change */);
							instr->eraseFromParent();
							
							// recursively fix address space in all instruction users
							std::vector<ReturnInst*> returns; // returns to fix -> there shouldn't be any here
							libfloor_utils::for_all_instruction_users(*repl_instr,
																	  [this, &repl_instr, &storage_class, &returns](Instruction& instr) {
								fix_instruction_users(*ctx, instr, *repl_instr, storage_class, false, returns);
							});
							assert(returns.empty() && "unexpected return type change");
						} else if (auto BC = dyn_cast_or_null<BitCastInst>(instr); BC) {
							auto dst_type = BC->getDestTy();
							assert(dst_type->isPointerTy());
							assert(dst_type->getPointerAddressSpace() != storage_class);
							auto dst_elem_type = dst_type->getPointerElementType();
							dst_type = dst_elem_type->getPointerTo(storage_class);
							auto src = BC->User::getOperand(0);
							auto repl_instr = new BitCastInst(src, dst_type, "", instr);
							repl_instr->setDebugLoc(instr->getDebugLoc());
							instr->replaceAllUsesWith(repl_instr, true /* allow address space change */);
							instr->eraseFromParent();
						} else {
							DBG(errs() << "\nunhandled val user: " << *instr << "\n";)
							DBG(errs().flush();)
							assert(false && "unhandled val user");
						}
					} else if (auto cnst = dyn_cast_or_null<Constant>(&user); cnst) {
						assert(false && "unhandled constant val user"); // should not be here ...
					} else {
						DBG(errs() << "\nval user is not an instruction\n";)
						DBG(errs().flush();)
						assert(false && "val user is not an instruction");
					}
				}, &parent_function);
			}
			
			return enclosed_type;
		}
		
		void transform_constexpr_geps(Function& F) {
			for (auto& BB : F) {
				for (auto& I : BB) {
					for (uint32_t op_idx = 0, op_count = I.getNumOperands(); op_idx < op_count; ++op_idx) {
						auto op = I.getOperand(op_idx);
						if (auto cexpr = dyn_cast_or_null<ConstantExpr>(op);
							cexpr && cexpr->getOpcode() == Instruction::GetElementPtr) {
							auto new_gep_instr = cexpr->getAsInstruction(&I);
							I.setOperand(op_idx, new_gep_instr);
						}
					}
				}
			}
		}
		
		llvm::Type* handle_ssbo_array_transforms(Function& F, Argument& arg) {
			// * transform array element pointer address space:
			//   even though we technically have a pointer to an array of SSBOs (pointers to storage buffer address space),
			//   Vulkan/SPIR-V handle this differently and instead only puts the pointer to the array into storage buffer address space,
			//   but all inner "pointers" do not have an address space (they are runtime arrays, i.e. no address space)
			// * LLVM will emit chains of GEP-load-GEP: transform these into single GEPs
			
			auto new_arg_type = handle_ssbo_array_transform_array_elem_ptr(F, arg);
			handle_ssbo_array_transform_fuse_gep_chains(F, arg, new_arg_type);
			return new_arg_type;
		}
		
		llvm::Type* handle_ssbo_array_transform_array_elem_ptr(Function& F, Argument& arg) {
			auto arg_type = arg.getType();
			assert(arg_type->isPointerTy());
			auto array_type = arg_type->getPointerElementType();
			assert(array_type->isArrayTy());
			auto array_elem_type = array_type->getArrayElementType();
			assert(array_elem_type->isPointerTy());
			if (array_elem_type->getPointerAddressSpace() == 0) {
				// already has the correct address space
				return arg_type;
			}
			
			// set new argument type
			auto new_array_elem_type = llvm::PointerType::get(array_elem_type->getPointerElementType(), 0);
			auto new_array_type = llvm::ArrayType::get(new_array_elem_type, array_type->getArrayNumElements());
			auto new_arg_type = llvm::PointerType::get(new_array_type, SPIRAS_StorageBuffer);
			arg.mutateType(new_arg_type);
			
			// update users
			libfloor_utils::for_all_users(arg, [&arg, &new_array_type](User& user) {
				if (auto instr = dyn_cast<Instruction>(&user)) {
					// we expect the top level users to all be GEPs
					if (auto GEP = dyn_cast_or_null<GetElementPtrInst>(instr); GEP) {
						// create new GEP with updated type
						SmallVector<Value*, 8> indices;
						for (auto& idx : GEP->indices()) {
							indices.emplace_back(idx);
						}
						auto repl_instr = llvm::GetElementPtrInst::Create(new_array_type, &arg, indices,
																		  (GEP->hasName() ? GEP->getName() + "." : "") + "ssboarrgep",
																		  instr);
						if (GEP->isInBounds()) {
							repl_instr->setIsInBounds();
						}
						repl_instr->setDebugLoc(instr->getDebugLoc());
						instr->replaceAllUsesWith(repl_instr, true /* allow address space change */);
						instr->eraseFromParent();
					} else {
						DBG(errs() << "\nunhandled arg user: " << *instr << "\n";)
						DBG(errs().flush();)
						assert(false && "unhandled arg user");
					}
				} else {
					DBG(errs() << "\narg user is not an instruction\n";)
					DBG(errs().flush();)
					assert(false && "arg user is not an instruction");
				}
			});
			
			return new_arg_type;
		}
		
		void handle_ssbo_array_transform_fuse_gep_chains(Function& F, Argument& arg, llvm::Type* new_arg_type) {
			libfloor_utils::for_all_users(arg, [this, &arg, &new_arg_type](User& user) {
				if (auto instr = dyn_cast<Instruction>(&user)) {
					// all top level users should be GEPs
					if (auto GEP = dyn_cast_or_null<GetElementPtrInst>(instr); GEP) {
						handle_ssbo_array_transform_fuse_gep_chain_ld(arg, new_arg_type, GEP);
						assert(GEP->getNumUses() == 0 && "top level GEP should have no uses left");
						GEP->eraseFromParent();
					} else {
						DBG(errs() << "\nunhandled arg user: " << *instr << "\n";)
						DBG(errs().flush();)
						assert(false && "unhandled arg user");
					}
				} else {
					DBG(errs() << "\narg user is not an instruction\n";)
					DBG(errs().flush();)
					assert(false && "arg user is not an instruction");
				}
			});
		}
		
		void handle_ssbo_array_transform_fuse_gep_chain_ld(Argument& arg, llvm::Type* new_arg_type, GetElementPtrInst* top_gep) {
			std::vector<User*> top_gep_users;
			for (auto* user : top_gep->users()) {
				top_gep_users.emplace_back(user);
			}
			for (auto& user : top_gep_users) {
				if (auto instr = dyn_cast<Instruction>(user)) {
					// all top level GEP users should be load instructions
					if (auto ld = dyn_cast_or_null<LoadInst>(instr); ld) {
						// this essentially loads a pointer from the SSBO array
						assert(ld->getOperand(0) == top_gep);
						handle_ssbo_array_transform_fuse_gep_chain_ld_gep(arg, new_arg_type, top_gep, ld);
						assert(ld->getNumUses() == 0 && "load should have no uses left");
						ld->eraseFromParent();
					} else {
						DBG(errs() << "\nunhandled GEP user: " << *instr << "\n";)
						DBG(errs().flush();)
						assert(false && "unhandled GEP user");
					}
				} else {
					DBG(errs() << "\nGEP user is not an instruction\n";)
					DBG(errs().flush();)
					assert(false && "GEP user is not an instruction");
				}
			}
		}
		
		void handle_ssbo_array_transform_fuse_gep_chain_ld_gep(Argument& arg, llvm::Type* new_arg_type, GetElementPtrInst* top_gep, LoadInst* ld) {
			std::vector<User*> ld_users;
			for (auto* user : ld->users()) {
				ld_users.emplace_back(user);
			}
			for (auto& user : ld_users) {
				if (auto instr = dyn_cast<Instruction>(user)) {
					// all load users should be GEPs
					if (auto GEP = dyn_cast_or_null<GetElementPtrInst>(instr); GEP) {
						// can now fuse GEP-load-GEP into a single GEP
						
						SmallVector<Value*, 8> indices;
						assert(top_gep->getNumIndices() >= 2);
						for (auto& idx : top_gep->indices()) {
							indices.emplace_back(idx);
						}
						// the first index must be 0
						assert(dyn_cast_or_null<ConstantInt>(indices[0]) &&
							   dyn_cast_or_null<ConstantInt>(indices[0])->getZExtValue() == 0 &&
							   "invalid top level index");
						// we need to swizzle the first two indices, because array/struct/SSBO layout is different on the SPIR-V side
						std::swap(indices[0], indices[1]);
						
						for (auto& idx : GEP->indices()) {
							indices.emplace_back(idx);
						}
						
						std::vector<Value*> params;
						std::vector<Type*> param_types;
						params.push_back(&arg);
						param_types.push_back(new_arg_type);
						for (const auto& idx : indices) {
							params.push_back(idx);
							param_types.push_back(idx->getType());
						}
						
						const auto func_type = llvm::FunctionType::get(GEP->getType(), param_types, false);
						
						// need a unique function name for this
						static uint32_t func_ident_counter = 0;
						auto func_ident = std::to_string(func_ident_counter++);
						
						auto CI = CallInst::Create(M->getOrInsertFunction("floor.ssbo_array_gep." + func_ident, func_type),
												   params, (GEP->hasName() ? GEP->getName() + "." : "") + "ssbo_array_gep", GEP);
						
						// if dynamic access (bitcast), then bitcast has the debug location, use gep location otherwise
						CI->setDebugLoc(GEP->getDebugLoc());
						
						GEP->replaceAllUsesWith(CI, true /* allow address space change */);
						GEP->eraseFromParent();
					} else {
						DBG(errs() << "\nunhandled LoadInst user: " << *instr << "\n";)
						DBG(errs().flush();)
						assert(false && "unhandled LoadInst user");
					}
				} else {
					DBG(errs() << "\nLoadInst user is not an instruction\n";)
					DBG(errs().flush();)
					assert(false && "LoadInst user is not an instruction");
				}
			}
		}
		
		// InstVisitor overrides...
		using InstVisitor<VulkanFinal>::visit;
		void visit(Instruction& I) {
			InstVisitor<VulkanFinal>::visit(I);
		}
		
		// prefer i32 indices so that we don't need the Int64 capability
		void visitExtractElement(ExtractElementInst& EEI) {
			const auto idx_op = EEI.getIndexOperand();
			const auto idx_type = idx_op->getType();
			if(!idx_type->isIntegerTy(32)) {
				if(const auto const_idx_op = dyn_cast_or_null<ConstantInt>(idx_op)) {
					EEI.setOperand(1 /* idx op */, builder->getInt32((int32_t)const_idx_op->getValue().getZExtValue()));
				}
				// else: can't do anything, b/c other int type would still be used when casting
			}
		}
		
		// prefer i32 indices so that we don't need the Int64 capability
		void visitInsertElement(InsertElementInst& IEI) {
			const auto idx_op = IEI.llvm::User::getOperand(2);
			const auto idx_type = idx_op->getType();
			if(!idx_type->isIntegerTy(32)) {
				if(const auto const_idx_op = dyn_cast_or_null<ConstantInt>(idx_op)) {
					IEI.setOperand(2 /* idx op */, builder->getInt32((int32_t)const_idx_op->getValue().getZExtValue()));
				}
				// else: can't do anything, b/c other int type would still be used when casting
			}
		}
		
		void visitAllocaInst(AllocaInst &AI) {
			vulkan_function_scope_type_replacement::handle_function_scope_variable(&AI, func);
		}
		
		void visitReturnInst(ReturnInst &RI) {
			if(!is_vertex_func && !is_fragment_func) return;
			
			auto ret_val = RI.getReturnValue();
			if (!ret_val) return;
			const auto ret_type = ret_val->getType();
			if (!ret_type) return;
			
			DebugLoc DL;
			if(auto ret_instr = dyn_cast<Instruction>(ret_val)) {
				DL = ret_instr->getDebugLoc();
			}
			
			const auto get_output_var = [this](uint32_t& idx) -> GlobalValue* {
				const auto name = func->getName().str() + ".vulkan_output." + std::to_string(idx++);
				auto output_var = M->getNamedValue(name);
				if(output_var == nullptr) {
					errs() << "output variable \"" << name << "\" doesn't exist\n";
					return nullptr;
				}
				return output_var;
			};
			
			uint32_t ret_idx = 0;
			if(ret_type->isStructTy()) {
				// struct -> split up to individual field stores
				const auto st_type = cast<llvm::StructType>(ret_type);
				for(uint32_t i = 0; i < st_type->getNumElements(); ++i) {
					auto output_var = get_output_var(ret_idx);
					if(output_var == nullptr) return;
					
					SmallVector<uint32_t, 1> idx_list { ret_idx - 1 };
					auto st_extract = ExtractValueInst::Create(ret_val, idx_list, "ret.extract", &RI);
					auto st = new StoreInst(st_extract, output_var, false, &RI);
					if(DL) st->setDebugLoc(DL);
				}
			}
			else {
				// else: array, scalars, vectors -> direct store
				auto output_var = get_output_var(ret_idx);
				if(output_var == nullptr) return;
				auto st = new StoreInst(ret_val, output_var, false, &RI);
				if(DL) st->setDebugLoc(DL);
			}
			
			// clear return
			ReturnInst::Create(*ctx, RI.getParent());
			RI.eraseFromParent();
		}
	};
	
	// VulkanPreFinal
	struct VulkanPreFinal : public FunctionPass, InstVisitor<VulkanPreFinal> {
		friend class InstVisitor<VulkanPreFinal>;
		
		static char ID; // Pass identification, replacement for typeid
		
		Module* M { nullptr };
		LLVMContext* ctx { nullptr };
		Function* func { nullptr };
		bool is_kernel_func { false };
		bool is_vertex_func { false };
		bool is_fragment_func { false };
		bool is_task_func { false };
		bool is_mesh_func { false };
		bool was_modified { false };
		ConstantFolder folder;
		
		// function input/originating pointers (i.e. parameters + allocas)
		std::unordered_set<Value*> input_ptrs;
		std::vector<GetElementPtrInst*> gep_ptrs;
		std::vector<SelectInst*> select_ptrs;
		std::vector<PHINode*> phi_ptrs;
		
		// gathered memory intrinsics
		std::vector<MemIntrinsic*> mem_instrs;
		
		VulkanPreFinal() :
		FunctionPass(ID) {
			initializeVulkanPreFinalPass(*PassRegistry::getPassRegistry());
		}
		
		void getAnalysisUsage(AnalysisUsage &AU) const override {
			AU.addRequired<AAResultsWrapperPass>();
			AU.addRequired<GlobalsAAWrapperPass>();
			AU.addRequired<AssumptionCacheTracker>();
			AU.addRequired<TargetLibraryInfoWrapperPass>();
			AU.addRequired<AssumptionCacheTracker>();
			AU.addRequired<DominatorTreeWrapperPass>();
			AU.addRequired<TargetTransformInfoWrapperPass>();
		}
		
		bool runOnFunction(Function &F) override {
			is_kernel_func = F.getCallingConv() == CallingConv::FLOOR_KERNEL;
			is_vertex_func = F.getCallingConv() == CallingConv::FLOOR_VERTEX;
			is_fragment_func = F.getCallingConv() == CallingConv::FLOOR_FRAGMENT;
			is_task_func = F.getCallingConv() == CallingConv::FLOOR_TASK;
			is_mesh_func = F.getCallingConv() == CallingConv::FLOOR_MESH;
			if (!is_kernel_func &&
				!is_vertex_func &&
				!is_fragment_func &&
				!is_task_func &&
				!is_mesh_func) {
				return false;
			}
			
			//
			M = F.getParent();
			ctx = &M->getContext();
			func = &F;
			input_ptrs.clear();
			gep_ptrs.clear();
			select_ptrs.clear();
			phi_ptrs.clear();
			mem_instrs.clear();
			
			// store parameter pointers + globals used in this function
			for(auto& arg : F.args()) {
				if(arg.getType()->isPointerTy()) {
					input_ptrs.emplace(&arg);
				}
			}
			for (auto& GV : M->globals()) {
				if (!GV.getType()->isPointerTy()) {
					continue;
				}
				libfloor_utils::for_all_instruction_users(GV, [this, &GV](Instruction& instr) {
					input_ptrs.emplace(&GV);
				}, &F /* restrict to this function */);
			}
			
			// gather all stuff
			was_modified = false;
			visit(F);
			
			// NOTE: disabled for now as we don't need this any more (probably ...)
			// handle everything
			//handle_pointers();
			// enable specific pointer-select+store workarounds
			if (M->getNamedMetadata("floor.vulkan_ptr_workarounds")) {
				handle_select_ptrs();
			}
			
			// we can't use mem* instructions that handle more than one value in Vulkan/SPIR-V
			if (!mem_instrs.empty()) {
				lower_mem_instructions();
			}
			
			return was_modified;
		}
		
		// InstVisitor overrides...
		using InstVisitor<VulkanPreFinal>::visit;
		void visit(Instruction& I) {
			InstVisitor<VulkanPreFinal>::visit(I);
		}
		
		void visitMemIntrinsic(MemIntrinsic& I) {
			mem_instrs.push_back(&I);
		}
		
		void visitAllocaInst(AllocaInst& AI) {
			// store allocas
			input_ptrs.emplace(&AI);
		}
		
		// fuses "base_gep" and "gep" to a single GEP, possibly replacing "gep" with the new GEP
		// NOTE: for simple cases, the returned GEP can be the input "gep" (with indices replaced)
		std::pair<GetElementPtrInst*, bool /* abort */>
		fuse_geps(GetElementPtrInst* base_gep, GetElementPtrInst* gep, const bool replace_old_gep) {
			const auto fuse_index = [this](Value* lhs, Value* rhs, Instruction* insert_pos) -> Value* {
				ConstantInt* lhs_const = dyn_cast_or_null<ConstantInt>(lhs);
				ConstantInt* rhs_const = dyn_cast_or_null<ConstantInt>(rhs);
				if (lhs_const != nullptr && rhs_const != nullptr) {
					// both are constant -> simply fold them
					return folder.FoldAdd(lhs_const, rhs_const);
				} else if(lhs_const != nullptr && lhs_const->getZExtValue() == 0) {
					// lhs idx is 0
					return rhs;
				} else if(rhs_const != nullptr && rhs_const->getZExtValue() == 0) {
					// rhs idx is 0
					return lhs;
				} else {
					// need to create a proper addition
					std::string name = ".gep.idx.add";
					if (rhs->hasName()) name = rhs->getName().str() + name;
					if (lhs->hasName()) name = lhs->getName().str() + name;
					if (lhs->getType() != rhs->getType()) {
						// must cast upwards
						assert(lhs->getType()->isIntegerTy() && rhs->getType()->isIntegerTy());
						const auto lhs_int_width = lhs->getType()->getIntegerBitWidth();
						const auto rhs_int_width = rhs->getType()->getIntegerBitWidth();
						assert(lhs_int_width != rhs_int_width);
						if (lhs_int_width < rhs_int_width) {
							if (lhs_const) {
								lhs = ConstantInt::get(rhs->getType(), lhs_const->getZExtValue());
							} else {
								lhs = CastInst::CreateIntegerCast(lhs, rhs->getType(), false, "", insert_pos);
							}
						} else {
							if (rhs_const) {
								rhs = ConstantInt::get(lhs->getType(), rhs_const->getZExtValue());
							} else {
								rhs = CastInst::CreateIntegerCast(rhs, lhs->getType(), false, "", insert_pos);
							}
						}
					}
					return BinaryOperator::CreateAdd(lhs, rhs, name, insert_pos);
				}
			};
			
			const auto base_idx_count = base_gep->getNumIndices();
			const auto idx_count = gep->getNumIndices();
			
			DBG(errs() << ">>> fusing: " << *base_gep << " + " << *gep << "\n";)
			
			if(base_idx_count == 1 && replace_old_gep) {
				// -> base is just an offset pointer, can simply fuse both GEPs
				DBG(errs() << ">> fused (simple): " << *gep;)
				
				// replace pointer base
				gep->setOperand(0, base_gep->getPointerOperand());
				
				// fuse first index
				gep->setOperand(1, fuse_index(base_gep->getOperand(1), gep->getOperand(1), gep));
				
				DBG(errs() << ", with " << *gep << "\n";)
				return { gep, false };
			}
			else {
				// -> need to create a new GEP so that it can contain all indices from both GEPs
				// NOTE: last idx in base + first in current always matches -> -1
				const auto fused_idx_count = base_idx_count + idx_count - 1;
				
				// create fused idx list
				SmallVector<Value*, 4> idx_list;
				
				// add base indices
				for(uint32_t i = 1, count = base_gep->getNumIndices() + 1; i < count; ++i) {
					idx_list.push_back(base_gep->getOperand(i));
				}
				
				// fust last (base) + first (current) idx
				idx_list[idx_list.size() - 1] = fuse_index(idx_list.back(), gep->getOperand(1), gep);
				
				// add current indices
				for(uint32_t i = 2, count = gep->getNumIndices() + 1; i < count; ++i) {
					idx_list.push_back(gep->getOperand(i));
				}
				assert(idx_list.size() == fused_idx_count && "invalid idx list size");
				
				// create the new GEP
				auto gep_ptr = base_gep->getPointerOperand();
				auto gep_ptr_type = cast<PointerType>(gep_ptr->getType()->getScalarType())->getPointerElementType();
				if (const auto idx_type = GetElementPtrInst::getIndexedType(gep_ptr_type, idx_list); !idx_type) {
					// can't handle it -> ignore it
					// NOTE: since variable pointers are now used by default, PtrAccessChain is possible -> we don't necessarily
					// need fused GEPs (but it's still a good thing that enables other transformations)
					return { gep, true };
				}
				auto fused_gep = GetElementPtrInst::CreateInBounds(gep_ptr_type, gep_ptr, idx_list,
																   gep->getName() + ".gep.fused", gep);
				fused_gep->setDebugLoc(gep->getDebugLoc());
				
				// replace this with new fused one
				DBG(errs() << ">> fused: replacing " << *gep << ", with " << *fused_gep << "\n";)
				if(replace_old_gep) {
					gep->replaceAllUsesWith(fused_gep);
					gep->eraseFromParent();
				}
				return { fused_gep, false };
			}
		}
		
		void visitGetElementPtrInst(GetElementPtrInst &I) {
			// prefer i32 indices where we can
			libfloor_utils::simplify_gep_indices(*ctx, I);
			
			// GEP fusion: we can't have GEPs into GEPs, so fuse them
			// NOTE: this has to be done recursively of course
			GetElementPtrInst* output_gep = &I;
			bool did_fuse = false;
			do {
				did_fuse = false;
				if(GetElementPtrInst* GEP = dyn_cast_or_null<GetElementPtrInst>(output_gep->getPointerOperand())) {
					bool abort = false;
					std::tie(output_gep, abort) = fuse_geps(GEP, output_gep, true /* replace_old_gep */);
					if (abort) {
						break;
					}
					did_fuse = true;
				}
			} while(did_fuse);
			
			gep_ptrs.push_back(output_gep);
		}
		
		void visitSelectInst(SelectInst& SI) {
			// we're only interested in pointer selects, as these aren't supported in vulkan
			if(!SI.getType()->isPointerTy()) {
				return;
			}
			select_ptrs.emplace_back(&SI);
		}
		
		void visitPHINode(PHINode& PHI) {
			// we're only interested in pointer selects, as these aren't supported in vulkan
			if(!PHI.getType()->isPointerTy()) {
				return;
			}
			phi_ptrs.emplace_back(&PHI);
		}
		
		void handle_select_ptrs() {
			DBG(errs() << "####################\n## in " << func->getName() << "\n";
				for(const auto& sel : select_ptrs) {
					errs() << "select: " << *sel << "\n";
				}
			)
			was_modified = true; // TODO: do this properly
			
			//
			std::vector<GetElementPtrInst*> problem_geps;
			for(const auto& gep : gep_ptrs) {
				if(input_ptrs.count(gep->getPointerOperand()) == 0) {
					problem_geps.emplace_back(gep);
					continue;
				}
			}
			DBG(errs() << "\n";
				for(const auto& gep : problem_geps) {
					errs() << "PROBLEM GEP: " << *gep << "\n";
				}
			)
			
			// needed for finding the origin pointers
			auto DT = &getAnalysis<DominatorTreeWrapperPass>().getDomTree();
			DT->recalculate(*func);
			LoopInfo LI(*DT);
			
			// find set of problem instructions and their pointer origin(s)
			std::deque<Instruction*> problem_instrs;
			problem_instrs.insert(begin(problem_instrs), begin(select_ptrs), end(select_ptrs));
			problem_instrs.insert(begin(problem_instrs), begin(problem_geps), end(problem_geps));
			
			struct ptr_set {
				// pointer origin(s)
				std::unordered_set<const Value*> src;
				// pointer producers (PHIs, selects, GEPs)
				// TODO: pointer bitcasts should be handled previously
				std::unordered_set<Instruction*> producers;
				// pointer consumers (loads, stores)
				std::unordered_set<Instruction*> consumers;
			};
			std::vector<std::shared_ptr<ptr_set>> ptr_sets;
			std::unordered_map<Instruction*, ptr_set*> ptr_set_map;
			std::unordered_multimap<const Value*, ptr_set*> origin_map;
			
			// create initial ptr_sets from our "problem" instructions
			for(; !problem_instrs.empty(); problem_instrs.pop_front()) {
				const auto& instr = problem_instrs[0];
				
				// already handled elsewhere?
				if(ptr_set_map.count(instr) > 0) continue;
				
				// figure out which origin pointer(s) this comes from
				SmallVector<const Value*, 3> origins;
				DBG(errs() << "\n-> " << *instr << "\n";)
				getUnderlyingObjects(instr, origins, &LI, 0);
				DBG(for(const auto& orig : origins) {
					errs() << "     origin: " << *orig << "\n";
				})
				if(origins.empty()) {
					ctx->emitError(instr, "instruction has no origin");
					return;
				}
				
				// check if we have a ptr_set for this origin combination yet
				ptr_set* pset = nullptr;
				auto orig_range = origin_map.equal_range(origins[0]);
				for(auto oiter = orig_range.first; oiter != orig_range.second; ++oiter) {
					// same size?
					if(oiter->second->src.size() != origins.size()) {
						continue;
					}
					
					// same content?
					bool equal_set = true;
					for(const auto& origin : origins) {
						if(oiter->second->src.count(origin) == 0) {
							equal_set = false;
							break;
						}
					}
					if(!equal_set) continue;
					
					// found a ptr_set that matches
					pset = oiter->second;
					break;
				}
				
				// create a ptr_set if none exists yet
				if(pset == nullptr) {
					auto new_ptr_set = std::make_shared<ptr_set>();
					ptr_sets.push_back(new_ptr_set);
					pset = new_ptr_set.get();
					
					pset->src.insert(origins.begin(), origins.end());
					for(const auto& origin : origins) {
						origin_map.emplace(origin, pset);
					}
				}
				
				// add this instruction
				pset->producers.emplace(instr);
				ptr_set_map.emplace(instr, pset);
				
				// we must also handle the pointers contained within instructions
				// -> this makes sure that we recursively get the complete net/graph of pointers
				if(PHINode* phi = dyn_cast_or_null<PHINode>(instr)) {
					for(const auto& phi_inc_val : phi->incoming_values()) {
						// only add the value/instruction if we haven't handled it yet + if it isn't a input ptr
						const auto phi_inc_instr = dyn_cast_or_null<Instruction>(&phi_inc_val);
						if(phi_inc_instr != nullptr &&
						   input_ptrs.count(phi_inc_instr) == 0 &&
						   ptr_set_map.count(phi_inc_instr) == 0) {
							problem_instrs.emplace_back(phi_inc_instr);
						}
					}
				}
				else if(SelectInst* sel = dyn_cast_or_null<SelectInst>(instr)) {
					const auto true_instr = dyn_cast_or_null<Instruction>(sel->getTrueValue());
					const auto false_instr = dyn_cast_or_null<Instruction>(sel->getFalseValue());
					if(true_instr != nullptr &&
					   input_ptrs.count(true_instr) == 0 &&
					   ptr_set_map.count(true_instr) == 0) {
						problem_instrs.emplace_back(true_instr);
					}
					if(false_instr != nullptr &&
					   input_ptrs.count(false_instr) == 0 &&
					   ptr_set_map.count(false_instr) == 0) {
						problem_instrs.emplace_back(false_instr);
					}
				}
				else if(GetElementPtrInst* GEP = dyn_cast_or_null<GetElementPtrInst>(instr)) {
					const auto gep_instr = dyn_cast_or_null<Instruction>(GEP->getPointerOperand());
					if(gep_instr != nullptr &&
					   input_ptrs.count(gep_instr) == 0 &&
					   ptr_set_map.count(gep_instr) == 0) {
						problem_instrs.emplace_back(gep_instr);
					}
				}
			}
			
			// find all consumers of our producers
			for (const auto& instr_pset : ptr_set_map) {
				const auto& instr = instr_pset.first;
				auto& pset = instr_pset.second;
				
				// only need to consider direct users this time
				libfloor_utils::for_all_instruction_users(*instr, [&pset](Instruction& user_instr) {
					if (isa<GetElementPtrInst>(user_instr) ||
						isa<SelectInst>(user_instr) ||
						isa<PHINode>(user_instr) ||
						isa<LoadInst>(user_instr)) {
						// not interested in these, should already be handled elsewhere
						return;
					}
					// NOTE: we don't care about what kind of instructions consumers are, because
					// we can simply iterate over all operands of the instruction and replace the
					// producer(s) accordingly
					pset->consumers.emplace(&user_instr);
				});
			}
			
			// debug output
			DBG(errs() << "\n\n## ptr sets:\n";
				for(const auto& pset : ptr_sets) {
					errs() << "# set\n";
					errs() << "origins:\n";
					for(const auto& origin : pset->src) {
						errs() << "\t" << *origin << "\n";
					}
					errs() << "producers:\n";
					for(const auto& prod : pset->producers) {
						errs() << "\t" << *prod << "\n";
					}
					errs() << "consumers:\n";
					for(const auto& cons : pset->consumers) {
						errs() << "\t" << *cons << "\n";
					}
					
					errs() << "\n";
				}
			)
			
			for (const auto& pset : ptr_sets) {
				if (pset->src.size() < 2) {
					// ignore trivial cases
				}
				
				bool has_store_inst = false;
				for (const auto& cons : pset->consumers) {
					if (isa<StoreInst>(cons)) {
						has_store_inst = true;
						break;
					}
				}
				if (!has_store_inst) {
					DBG(errs() << "!! ignoring non-store-only consumers\n";)
				}
				
				// go down the condition tree until we hit a leaf / origin pointer, in between:
				//  * for GEPs: store them in a chain (in the same order as they were encountered) and pass/copy
				//              them through to each tree branch (so that each branch has its individual chain)
				//  * for SELs: create if/else branches for each SEL (using the same condition as the SEL),
				//              then recursively go down each branch, meeting up again after the SEL instruction,
				//              also insert a PHI if the consumer produces a value (replace consumer with that at the end)
				//  * for origin ptrs: fuse the GEP chain up to that point, replace pointer with the origin pointer (now
				//                     unambiguous), replace use of producer pointer in the consumer with the new GEP
				
				// (upside-down) condition tree
				struct condition_tree {
					enum class TYPE : uint32_t {
						COMPARE_BOOL,
						COMPARE_PHI,
						ORIGIN_PTR,
						GEP,
					};
					TYPE type;
					
					// if COMPARE_BOOL: i1 compare value
					// if COMPARE_PHI: i32 compare value
					// if ORIGIN_PTR: actual origin ptr (+this is a leaf node)
					// if GEP: intermediate GEP
					Value* val;
					
					// pointers to the next condition node(s) or actual origin ptrs (if they are leaf nodes)
					std::vector<condition_tree> nodes;
				};
				uint32_t leaf_count = 0;
				const std::function<void(Value*, condition_tree&)> create_condition_tree =
				[&create_condition_tree, &pset, &leaf_count, this](Value* prod, condition_tree& node) {
					// origin ptr: attach as leaf node
					if(pset->src.count(prod) > 0) {
						node.type = condition_tree::TYPE::ORIGIN_PTR;
						node.val = prod;
						++leaf_count;
						return;
					}
					
					// intermediate GEP, continue with pointer op
					if(GetElementPtrInst* GEP = dyn_cast_or_null<GetElementPtrInst>(prod)) {
						node.type = condition_tree::TYPE::GEP;
						node.val = GEP;
						
						condition_tree child;
						create_condition_tree(GEP->getPointerOperand(), child);
						node.nodes.push_back(child);
					}
					// select, store condition + continue traversal with true and false value
					else if(SelectInst* sel = dyn_cast_or_null<SelectInst>(prod)) {
						node.type = condition_tree::TYPE::COMPARE_BOOL;
						node.val = sel->getCondition();
						
						// NOTE: must be in this order
						condition_tree true_child, false_child;
						create_condition_tree(sel->getTrueValue(), true_child);
						create_condition_tree(sel->getFalseValue(), false_child);
						node.nodes.push_back(true_child);
						node.nodes.push_back(false_child);
					}
					else if(isa<PHINode>(prod)) {
						// TODO: handle PHIs!
						node.type = condition_tree::TYPE::COMPARE_PHI;
						return;
					}
					else {
						if(Instruction* instr = dyn_cast_or_null<Instruction>(prod)) {
							ctx->emitError(instr, "unhandled producer while creating condition tree");
						}
						else {
							ctx->emitError("unhandled non-instruction producer value while creating condition tree");
						}
					}
				};
				
				// for debugging purposes
				const std::function<void(const condition_tree&, const uint32_t)> dump_condition_tree =
				[&dump_condition_tree](const condition_tree& node, const uint32_t level) {
					errs() << std::string(level, '-') << "> ";
					switch(node.type) {
						case condition_tree::TYPE::COMPARE_BOOL:
							errs() << "SELECT: " << *node.val << "\n";
							dump_condition_tree(node.nodes[0], level + 1);
							dump_condition_tree(node.nodes[1], level + 1);
							break;
						case condition_tree::TYPE::COMPARE_PHI:
							errs() << "PHI:\n";
							break;
						case condition_tree::TYPE::GEP:
							errs() << "GEP: " << *node.val << "\n";
							dump_condition_tree(node.nodes[0], level + 1);
							break;
						case condition_tree::TYPE::ORIGIN_PTR:
							errs() << "LEAF: " << *node.val << "\n";
							break;
					}
				};
				
				// this determines if replacing the condition is viable/possible, i.e. there are no PHIs in it
				const std::function<bool(const condition_tree&, bool)> is_viable_condition_tree =
				[&is_viable_condition_tree](const condition_tree& node, const bool has_branch_term) {
					bool viable = true;
					switch (node.type) {
						case condition_tree::TYPE::COMPARE_BOOL:
							viable &= is_viable_condition_tree(node.nodes[0], true);
							viable &= is_viable_condition_tree(node.nodes[1], true);
							break;
						case condition_tree::TYPE::COMPARE_PHI:
							viable = false;
							break;
						case condition_tree::TYPE::GEP:
							viable &= is_viable_condition_tree(node.nodes[0], has_branch_term);
							break;
						case condition_tree::TYPE::ORIGIN_PTR:
							if (!has_branch_term) {
								return false;
							}
							break;
					}
					return viable;
				};
				
				for (Instruction* cons : pset->consumers) {
					// find producer
					Instruction* producer = nullptr;
					for(uint32_t i = 0, count = cons->getNumOperands(); i < count; ++i) {
						const auto op = cons->getOperand(i);
						const auto piter = pset->producers.find((Instruction*)op);
						if(piter != pset->producers.end()) {
							producer = *piter;
							break;
						}
					}
					assert(producer != nullptr && "consumer has no producer");
					DBG(errs() << "-> cons/prod: " << *cons << " -> " << *producer << "\n";)
					
					// find the condition(s) on which we need to branch
					condition_tree cond_tree;
					create_condition_tree(producer, cond_tree);
					DBG(dump_condition_tree(cond_tree, 0); errs() << "\n";)
					if (!is_viable_condition_tree(cond_tree, false)) {
						DBG(errs() << "!! ignoring non-viable condition tree\n";)
						continue;
					}
					
					// TODO: once switches are supported, combine all conditions to a single large switch,
					// instead of creating a condition tree
					// TODO: try to merge multiple users to a single block per origin instead of per GEP/user
					
					BasicBlock* continue_block = nullptr; // code/block _after_ the consumer instruction
					PHINode* continue_phi = nullptr;
					const std::function<void(const condition_tree&, std::vector<GetElementPtrInst*>, Instruction*)>
					handle_condition_tree = [this, &handle_condition_tree, &continue_block, &continue_phi, &leaf_count,
											 &cons, &producer](const condition_tree& node,
															   std::vector<GetElementPtrInst*> gep_chain,
															   Instruction* branch_term)
					{
						switch(node.type) {
							case condition_tree::TYPE::COMPARE_BOOL: {
								DBG(errs() << "@SEL: " << *node.val << "\n";)
								// select: create an if-else branch
								Value* cond = node.val;
								
								Instruction* true_term = nullptr;
								Instruction* false_term = nullptr;
								if(continue_block == nullptr) {
									// create the first split and final continue block
									SplitBlockAndInsertIfThenElse(cond, cons, &true_term, &false_term);
									assert(true_term != nullptr && "failed to create true branch/term");
									assert(false_term != nullptr && "failed to create false branch/term");
									continue_block = cons->getParent();
									continue_block->setName("continue.block");
									
									// insert continue phi if the consumer produces a value
									if(!cons->getType()->isVoidTy()) {
										continue_phi = PHINode::Create(cons->getType(), leaf_count, "continue.phi",
																	   cons->getNextNode());
									}
								}
								else {
									// TODO: implement this
									assert(false && "not implemented yet");
									ctx->emitError("multi-select not implemented yet");
									return;
								}
								
								true_term->getParent()->setName("sel.true");
								false_term->getParent()->setName("sel.false");
								
								// copy GEP chain into both branches
								handle_condition_tree(node.nodes[0], gep_chain, true_term);
								handle_condition_tree(node.nodes[1], gep_chain, false_term);
								break;
							}
							case condition_tree::TYPE::COMPARE_PHI: {
								DBG(errs() << "@PHI: " << *node.val << "\n";)
								ctx->emitError("PHI nodes are not handled yet");
								break;
							}
							case condition_tree::TYPE::GEP: {
								DBG(errs() << "@GEP: " << *node.val << "\n";)
								// add GEP to chain and continue
								gep_chain.emplace_back(dyn_cast<GetElementPtrInst>(node.val));
								handle_condition_tree(node.nodes[0], std::move(gep_chain), branch_term);
								break;
							}
							case condition_tree::TYPE::ORIGIN_PTR: {
								DBG(errs() << "@PTR: " << *node.val << "\n";)
								// leaf: finally reached an origin pointer
								assert(branch_term != nullptr && "must have a branch terminator");
								
								// fuse GEPs with origin ptr
								GetElementPtrInst* mat_gep = (GetElementPtrInst*)gep_chain[0]->clone();
								mat_gep->setOperand(0, node.val);
								mat_gep->setName(gep_chain.back()->getName());
								mat_gep->setDebugLoc(gep_chain.back()->getDebugLoc());
								for(size_t i = 1, count = gep_chain.size(); i < count; ++i) {
									bool abort = false;
									std::tie(mat_gep, abort) = fuse_geps(mat_gep, gep_chain[i], false); // TODO
									if (abort) {
										break;
									}
								}
								mat_gep->insertBefore(branch_term);
								DBG(errs() << ">> mat_gep: " << *mat_gep << "\n";)
								
								// copy consumer
								auto cons_clone = cons->clone();
								if(cons->hasName()) {
									cons_clone->setName(cons->getName());
								}
								cons_clone->setDebugLoc(cons->getDebugLoc());
								cons_clone->insertBefore(branch_term);
								
								// replace consumer ptr
								DBG(errs() << ">> cons before: " << *cons_clone << "\n";)
								for(uint32_t i = 0, count = cons_clone->getNumOperands(); i < count; ++i) {
									const auto op = cons_clone->getOperand(i);
									if(op == producer) {
										cons_clone->setOperand(i, mat_gep);
									}
								}
								DBG(errs() << ">> cons after: " << *cons_clone << "\n";)
								
								// branch to continue block
								auto br_inst = dyn_cast_or_null<BranchInst>(branch_term);
								assert(br_inst != nullptr && "invalid terminator");
								assert(br_inst->isUnconditional() && "branch must be unconditional");
								br_inst->setSuccessor(0, continue_block);
								
								// update continue phi (if there is one)
								if(continue_phi != nullptr) {
									continue_phi->addIncoming(cons_clone, cons_clone->getParent());
									DBG(errs() << ">> updated phi: " << *continue_phi << "\n";)
								}
								break;
							}
						}
					};
					handle_condition_tree(cond_tree, {}, nullptr);
					
					if(continue_phi != nullptr) {
						cons->replaceAllUsesWith(continue_phi);
					}
					cons->eraseFromParent();
				}
			}
		}
		
		void handle_pointers() {
			DBG(errs() << "####################\n## in " << func->getName() << "\n";
				for(const auto& iptr : input_ptrs) {
					errs() << "input: " << *iptr << "\n";
				}
				for(const auto& gep : gep_ptrs) {
					errs() << "GEP: " << *gep << "\n";
				}
				for(const auto& sel : select_ptrs) {
					errs() << "select: " << *sel << "\n";
				}
				for(const auto& phi : phi_ptrs) {
					errs() << "phi: " << *phi << "\n";
				}
			)
			was_modified = true; // TODO: do this properly
			
			//
			std::vector<GetElementPtrInst*> problem_geps;
			for(const auto& gep : gep_ptrs) {
				if(input_ptrs.count(gep->getPointerOperand()) == 0) {
					problem_geps.emplace_back(gep);
					continue;
				}
			}
			DBG(errs() << "\n";
				for(const auto& gep : problem_geps) {
					errs() << "PROBLEM GEP: " << *gep << "\n";
				}
			)
			
			// needed for finding the origin pointers
			auto DT = &getAnalysis<DominatorTreeWrapperPass>().getDomTree();
			DT->recalculate(*func);
			LoopInfo LI(*DT);
			
			// find set of problem instructions and their pointer origin(s)
			std::deque<Instruction*> problem_instrs;
			problem_instrs.insert(begin(problem_instrs), begin(select_ptrs), end(select_ptrs));
			problem_instrs.insert(begin(problem_instrs), begin(phi_ptrs), end(phi_ptrs));
			problem_instrs.insert(begin(problem_instrs), begin(problem_geps), end(problem_geps));
			
			struct ptr_set {
				// pointer origin(s)
				std::unordered_set<const Value*> src;
				// pointer producers (PHIs, selects, GEPs)
				// TODO: pointer bitcasts should be handled previously
				std::unordered_set<Instruction*> producers;
				// pointer consumers (loads, stores)
				std::unordered_set<Instruction*> consumers;
			};
			std::vector<std::shared_ptr<ptr_set>> ptr_sets;
			std::unordered_map<Instruction*, ptr_set*> ptr_set_map;
			std::unordered_multimap<const Value*, ptr_set*> origin_map;
			
			// create initial ptr_sets from our "problem" instructions
			for(; !problem_instrs.empty(); problem_instrs.pop_front()) {
				const auto& instr = problem_instrs[0];
				
				// already handled elsewhere?
				if(ptr_set_map.count(instr) > 0) continue;
				
				// figure out which origin pointer(s) this comes from
				SmallVector<const Value*, 3> origins;
				DBG(errs() << "\n-> " << *instr << "\n";)
				getUnderlyingObjects(instr, origins, &LI, 0);
				DBG(for(const auto& orig : origins) {
					errs() << "     origin: " << *orig << "\n";
				})
				if(origins.empty()) {
					ctx->emitError(instr, "instruction has no origin");
					return;
				}
				
				// check if we have a ptr_set for this origin combination yet
				ptr_set* pset = nullptr;
				auto orig_range = origin_map.equal_range(origins[0]);
				for(auto oiter = orig_range.first; oiter != orig_range.second; ++oiter) {
					// same size?
					if(oiter->second->src.size() != origins.size()) {
						continue;
					}
					
					// same content?
					bool equal_set = true;
					for(const auto& origin : origins) {
						if(oiter->second->src.count(origin) == 0) {
							equal_set = false;
							break;
						}
					}
					if(!equal_set) continue;
					
					// found a ptr_set that matches
					pset = oiter->second;
					break;
				}
				
				// create a ptr_set if none exists yet
				if(pset == nullptr) {
					auto new_ptr_set = std::make_shared<ptr_set>();
					ptr_sets.push_back(new_ptr_set);
					pset = new_ptr_set.get();
					
					pset->src.insert(origins.begin(), origins.end());
					for(const auto& origin : origins) {
						origin_map.emplace(origin, pset);
					}
				}
				
				// add this instruction
				pset->producers.emplace(instr);
				ptr_set_map.emplace(instr, pset);
				
				// we must also handle the pointers contained within instructions
				// -> this makes sure that we recursively get the complete net/graph of pointers
				if(PHINode* phi = dyn_cast_or_null<PHINode>(instr)) {
					for(const auto& phi_inc_val : phi->incoming_values()) {
						// only add the value/instruction if we haven't handled it yet + if it isn't a input ptr
						const auto phi_inc_instr = dyn_cast_or_null<Instruction>(&phi_inc_val);
						if(phi_inc_instr != nullptr &&
						   input_ptrs.count(phi_inc_instr) == 0 &&
						   ptr_set_map.count(phi_inc_instr) == 0) {
							problem_instrs.emplace_back(phi_inc_instr);
						}
					}
				}
				else if(SelectInst* sel = dyn_cast_or_null<SelectInst>(instr)) {
					const auto true_instr = dyn_cast_or_null<Instruction>(sel->getTrueValue());
					const auto false_instr = dyn_cast_or_null<Instruction>(sel->getFalseValue());
					if(true_instr != nullptr &&
					   input_ptrs.count(true_instr) == 0 &&
					   ptr_set_map.count(true_instr) == 0) {
						problem_instrs.emplace_back(true_instr);
					}
					if(false_instr != nullptr &&
					   input_ptrs.count(false_instr) == 0 &&
					   ptr_set_map.count(false_instr) == 0) {
						problem_instrs.emplace_back(false_instr);
					}
				}
				else if(GetElementPtrInst* GEP = dyn_cast_or_null<GetElementPtrInst>(instr)) {
					const auto gep_instr = dyn_cast_or_null<Instruction>(GEP->getPointerOperand());
					if(gep_instr != nullptr &&
					   input_ptrs.count(gep_instr) == 0 &&
					   ptr_set_map.count(gep_instr) == 0) {
						problem_instrs.emplace_back(gep_instr);
					}
				}
			}
			
			// find all consumers of our producers
			for (const auto& instr_pset : ptr_set_map) {
				const auto& instr = instr_pset.first;
				auto& pset = instr_pset.second;
				
				// only need to consider direct users this time
				libfloor_utils::for_all_instruction_users(*instr, [&ptr_set_map, &pset](Instruction& user_instr) {
					if (isa<GetElementPtrInst>(user_instr) ||
						isa<SelectInst>(user_instr) ||
						isa<PHINode>(user_instr)) {
						// not interested in these, should already be handled elsewhere
						const auto pset_iter = ptr_set_map.find(&user_instr);
						assert(pset_iter != ptr_set_map.end() && "unknown producer");
						assert(pset_iter->second->producers.count(&user_instr) > 0 && "unhandled producer");
						return;
					}
					// NOTE: we don't care about what kind of instructions consumers are, because
					// we can simply iterate over all operands of the instruction and replace the
					// producer(s) accordingly
					pset->consumers.emplace(&user_instr);
				});
			}
			
			// debug output
			DBG(errs() << "\n\n## ptr sets:\n";
				for(const auto& pset : ptr_sets) {
					errs() << "# set\n";
					errs() << "origins:\n";
					for(const auto& origin : pset->src) {
						errs() << "\t" << *origin << "\n";
					}
					errs() << "producers:\n";
					for(const auto& prod : pset->producers) {
						errs() << "\t" << *prod << "\n";
					}
					errs() << "consumers:\n";
					for(const auto& cons : pset->consumers) {
						errs() << "\t" << *cons << "\n";
					}
					
					errs() << "\n";
				}
			)
			
			//
			const auto idx_type = Type::getInt32Ty(*ctx);
			const auto idx_zero = ConstantInt::get(idx_type, 0);
			for(const auto& pset : ptr_sets) {
				// ignore set if there are no consumers (that we need to handle)
				if(pset->consumers.empty()) continue;
				
				if(pset->src.size() == 1) {
					// simple case with a single origin ptr
					// -> can replace everything with indices
					
					// ptr -> index map
					std::unordered_map<Instruction*, Instruction*> index_map;
					
					// TODO: handle necessary index count (> 1)
					// * check which op#s are constant (throughout?) -> no need to handle them
					// * for non-constant -> use array? n-vector? multiple scalar indices?
					
					// create indices / replace pointers with integers - due to chain dependencies this is a bit tricky
					const auto replace_with_idx = [&pset, &index_map, &idx_zero](Value* val) -> Value* {
						// is this an origin pointer - if so, index is 0
						if(pset->src.count(val) > 0) {
							return idx_zero;
						}
						
						// otherwise it must be an instruction
						const auto instr = dyn_cast_or_null<Instruction>(val);
						assert(instr != nullptr && "must be an instruction");
						
						const auto idx_iter = index_map.find(instr);
						assert(idx_iter != index_map.end() && "no index for this instruction");
						
						return idx_iter->second;
					};
					
					bool all_indices_created = false;
					while(!all_indices_created) {
						DBG(errs() << "... index round ...\n";)
						all_indices_created = true;
						for(const auto& prod : pset->producers) {
							if(index_map.count(prod) > 0) continue;
							
							Instruction* idx = nullptr;
							if(GetElementPtrInst* GEP = dyn_cast_or_null<GetElementPtrInst>(prod)) {
								const auto base_ptr = GEP->getPointerOperand();
								const auto base_ptr_instr = dyn_cast_or_null<Instruction>(base_ptr);
								if(pset->src.count(base_ptr) > 0 ||
								   (base_ptr_instr != nullptr && index_map.count(base_ptr_instr) > 0)) {
									idx = BinaryOperator::CreateAdd(replace_with_idx(base_ptr), GEP->getOperand(1),
																	GEP->getName() + ".idx", GEP);
								}
								else {
									// can't handle this yet - do another round
									DBG(errs() << " -- can't handle yet: " << *GEP << "\n";)
									all_indices_created = false;
									continue;
								}
							}
							else if(PHINode* phi = dyn_cast_or_null<PHINode>(prod)) {
								// NOTE: we will make sure this contains the proper incoming values later
								idx = PHINode::Create(idx_type, phi->getNumIncomingValues(),
													  phi->getName() + ".idx", phi);
							}
							else if(SelectInst* sel = dyn_cast_or_null<SelectInst>(prod)) {
								// NOTE: we will make sure this contains the proper true/false values later
								idx = SelectInst::Create(sel->getCondition(), idx_zero, idx_zero);
							}
							
							if (auto dbg_loc = prod->getDebugLoc(); dbg_loc) {
								idx->setDebugLoc(dbg_loc);
							}
							index_map.emplace(prod, idx);
							DBG(errs() << "idx: " << *prod << " -> " << *idx << "\n";)
						}
					}
					
					// phi/select index fixup now that we have created all indices
					for(const auto& prod : pset->producers) {
						if(PHINode* phi = dyn_cast_or_null<PHINode>(prod)) {
							auto idx_phi = dyn_cast<PHINode>(index_map[prod]);
							for(uint32_t i = 0, count = phi->getNumIncomingValues(); i < count; ++i) {
								idx_phi->addIncoming(replace_with_idx(phi->getIncomingValue(i)),
													 phi->getIncomingBlock(i));
							}
							DBG(errs() << "updated phi: " << *idx_phi << "\n";)
						}
						else if(SelectInst* sel = dyn_cast_or_null<SelectInst>(prod)) {
							auto idx_sel = dyn_cast<SelectInst>(index_map[prod]);
							idx_sel->setTrueValue(replace_with_idx(sel->getTrueValue()));
							idx_sel->setFalseValue(replace_with_idx(sel->getFalseValue()));
							DBG(errs() << "updated sel: " << *idx_sel << "\n";)
						}
					}
					
					// materialize pointers/GEPs for users (from indices)
					auto base_ptr = *pset->src.begin();
					for(Instruction* cons : pset->consumers) {
						DBG(errs() << ">> cons before: " << *cons << "\n";)
						for(uint32_t i = 0, count = cons->getNumOperands(); i < count; ++i) {
							const auto op = cons->getOperand(i);
							const auto idx_iter = index_map.find((Instruction*)op);
							if(idx_iter != index_map.end()) {
								// replace with new materialized GEP
								SmallVector<Value*, 3> idx_list;
								idx_list.push_back(idx_zero);
								idx_list.push_back(idx_iter->second);
								auto val_ptr = const_cast<Value*>(base_ptr);
								auto mat_gep = GetElementPtrInst::CreateInBounds(val_ptr->getType()->getScalarType()->getPointerElementType(),
																				 val_ptr, idx_list,
																				 op->getName() + ".idx.mat.gep", cons);
								if(Instruction* op_instr = dyn_cast_or_null<Instruction>(op)) {
									mat_gep->setDebugLoc(op_instr->getDebugLoc());
								}
								DBG(errs() << "> consumer op replace: " << *op << " -> " << *mat_gep << "\n";)
								cons->setOperand(i, mat_gep);
							}
						}
						DBG(errs() << ">> cons after: " << *cons << "\n";)
					}
				}
				else if(pset->src.size() > 1) {
					// complex case with 2 or more origin ptrs
					
					// go down the condition tree until we hit a leaf / origin pointer, in between:
					//  * for GEPs: store them in a chain (in the same order as they were encountered) and pass/copy
					//              them through to each tree branch (so that each branch has its individual chain)
					//  * for SELs: create if/else branches for each SEL (using the same condition as the SEL),
					//              then recursively go down each branch, meeting up again after the SEL instruction,
					//              also insert a PHI if the consumer produces a value (replace consumer with that at the end)
					//  * for PHIs: not handled yet - need to create a new integer condition, then use that for branching?
					//  * for origin ptrs: fuse the GEP chain up to that point, replace pointer with the origin pointer (now
					//                     unambiguous), replace use of producer pointer in the consumer with the new GEP
					
					// (upside-down) condition tree
					struct condition_tree {
						enum class TYPE : uint32_t {
							COMPARE_BOOL,
							COMPARE_PHI,
							ORIGIN_PTR,
							GEP,
						};
						TYPE type;
						
						// if COMPARE_BOOL: i1 compare value
						// if COMPARE_PHI: i32 compare value
						// if ORIGIN_PTR: actual origin ptr (+this is a leaf node)
						// if GEP: intermediate GEP
						Value* val;
						
						// pointers to the next condition node(s) or actual origin ptrs (if they are leaf nodes)
						std::vector<condition_tree> nodes;
					};
					uint32_t leaf_count = 0;
					const std::function<void(Value*, condition_tree&)> create_condition_tree =
					[&create_condition_tree, &pset, &leaf_count, this](Value* prod, condition_tree& node) {
						// origin ptr: attach as leaf node
						if(pset->src.count(prod) > 0) {
							node.type = condition_tree::TYPE::ORIGIN_PTR;
							node.val = prod;
							++leaf_count;
							return;
						}
						
						// intermediate GEP, continue with pointer op
						if(GetElementPtrInst* GEP = dyn_cast_or_null<GetElementPtrInst>(prod)) {
							node.type = condition_tree::TYPE::GEP;
							node.val = GEP;
							
							condition_tree child;
							create_condition_tree(GEP->getPointerOperand(), child);
							node.nodes.push_back(child);
						}
						// select, store condition + continue traversal with true and false value
						else if(SelectInst* sel = dyn_cast_or_null<SelectInst>(prod)) {
							node.type = condition_tree::TYPE::COMPARE_BOOL;
							node.val = sel->getCondition();
							
							// NOTE: must be in this order
							condition_tree true_child, false_child;
							create_condition_tree(sel->getTrueValue(), true_child);
							create_condition_tree(sel->getFalseValue(), false_child);
							node.nodes.push_back(true_child);
							node.nodes.push_back(false_child);
						}
						else if(PHINode* phi = dyn_cast_or_null<PHINode>(prod)) {
							// TODO: handle PHIs!
							node.type = condition_tree::TYPE::COMPARE_PHI;
							ctx->emitError(phi, "PHI nodes are not handled yet");
							return;
						}
						else {
							if(Instruction* instr = dyn_cast_or_null<Instruction>(prod)) {
								ctx->emitError(instr, "unhandled producer while creating condition tree");
							}
							else {
								ctx->emitError("unhandled non-instruction producer value while creating condition tree");
							}
						}
					};
					
					// for debugging purposes
					const std::function<void(const condition_tree&, const uint32_t)> dump_condition_tree =
					[&dump_condition_tree](const condition_tree& node, const uint32_t level) {
						errs() << std::string(level, '-') << "> ";
						switch(node.type) {
							case condition_tree::TYPE::COMPARE_BOOL:
								errs() << "SELECT: " << *node.val << "\n";
								dump_condition_tree(node.nodes[0], level + 1);
								dump_condition_tree(node.nodes[1], level + 1);
								break;
							case condition_tree::TYPE::COMPARE_PHI:
								errs() << "PHI: " << *node.val << "\n";
								for(const auto& child : node.nodes) {
									dump_condition_tree(child, level + 1);
								}
								break;
							case condition_tree::TYPE::GEP:
								errs() << "GEP: " << *node.val << "\n";
								dump_condition_tree(node.nodes[0], level + 1);
								break;
							case condition_tree::TYPE::ORIGIN_PTR:
								errs() << "LEAF: " << *node.val << "\n";
								break;
						}
					};
					
					for(Instruction* cons : pset->consumers) {
						// find producer
						Instruction* producer = nullptr;
						for(uint32_t i = 0, count = cons->getNumOperands(); i < count; ++i) {
							const auto op = cons->getOperand(i);
							const auto piter = pset->producers.find((Instruction*)op);
							if(piter != pset->producers.end()) {
								producer = *piter;
								break;
							}
						}
						assert(producer != nullptr && "consumer has no producer");
						DBG(errs() << "-> cons/prod: " << *cons << " -> " << *producer << "\n";)
						
						// find the condition(s) on which we need to branch
						condition_tree cond_tree;
						create_condition_tree(producer, cond_tree);
						DBG(dump_condition_tree(cond_tree, 0); errs() << "\n";)
						
						// TODO: once switches are supported, combine all conditions to a single large switch,
						// instead of creating a condition tree
						// TODO: try to merge multiple users to a single block per origin instead of per GEP/user
						
						BasicBlock* continue_block = nullptr; // code/block _after_ the consumer instruction
						PHINode* continue_phi = nullptr;
						const std::function<void(const condition_tree&, std::vector<GetElementPtrInst*>, Instruction*)>
						handle_condition_tree = [this, &handle_condition_tree, &continue_block, &continue_phi, &leaf_count,
												 &cons, &producer](const condition_tree& node,
																   std::vector<GetElementPtrInst*> gep_chain,
																   Instruction* branch_term) {
							switch(node.type) {
								case condition_tree::TYPE::COMPARE_BOOL: {
									DBG(errs() << "@SEL: " << *node.val << "\n";)
									// select: create an if-else branch
									Value* cond = node.val;
									
									Instruction* true_term = nullptr;
									Instruction* false_term = nullptr;
									if(continue_block == nullptr) {
										// create the first split and final continue block
										SplitBlockAndInsertIfThenElse(cond, cons, &true_term, &false_term);
										assert(true_term != nullptr && "failed to create true branch/term");
										assert(false_term != nullptr && "failed to create false branch/term");
										continue_block = cons->getParent();
										continue_block->setName("continue.block");
										
										// insert continue phi if the consumer produces a value
										if(!cons->getType()->isVoidTy()) {
											continue_phi = PHINode::Create(cons->getType(), leaf_count, "continue.phi",
																		   cons->getNextNode());
										}
									}
									else {
										// TODO: implement this
										assert(false && "not implemented yet");
										ctx->emitError("multi-select not implemented yet");
										return;
									}
									
									true_term->getParent()->setName("sel.true");
									false_term->getParent()->setName("sel.false");
									
									// copy GEP chain into both branches
									handle_condition_tree(node.nodes[0], gep_chain, true_term);
									handle_condition_tree(node.nodes[1], gep_chain, false_term);
									break;
								}
								case condition_tree::TYPE::COMPARE_PHI: {
									DBG(errs() << "@PHI: " << *node.val << "\n";)
									ctx->emitError("PHI nodes are not handled yet");
									break;
								}
								case condition_tree::TYPE::GEP: {
									DBG(errs() << "@GEP: " << *node.val << "\n";)
									// add GEP to chain and continue
									gep_chain.emplace_back(dyn_cast<GetElementPtrInst>(node.val));
									handle_condition_tree(node.nodes[0], std::move(gep_chain), branch_term);
									break;
								}
								case condition_tree::TYPE::ORIGIN_PTR: {
									DBG(errs() << "@PTR: " << *node.val << "\n";)
									// leaf: finally reached an origin pointer
									assert(branch_term != nullptr && "must have a branch terminator");
									
									// fuse GEPs with origin ptr
									GetElementPtrInst* mat_gep = (GetElementPtrInst*)gep_chain[0]->clone();
									mat_gep->setOperand(0, node.val);
									mat_gep->setName(gep_chain.back()->getName());
									mat_gep->setDebugLoc(gep_chain.back()->getDebugLoc());
									for(size_t i = 1, count = gep_chain.size(); i < count; ++i) {
										bool abort = false;
										std::tie(mat_gep, abort) = fuse_geps(mat_gep, gep_chain[i], false); // TODO
										if (abort) {
											break;
										}
									}
									mat_gep->insertBefore(branch_term);
									DBG(errs() << ">> mat_gep: " << *mat_gep << "\n";)
									
									// copy consumer
									auto cons_clone = cons->clone();
									if(cons->hasName()) {
										cons_clone->setName(cons->getName());
									}
									cons_clone->setDebugLoc(cons->getDebugLoc());
									cons_clone->insertBefore(branch_term);
									
									// replace consumer ptr
									DBG(errs() << ">> cons before: " << *cons_clone << "\n";)
									for(uint32_t i = 0, count = cons_clone->getNumOperands(); i < count; ++i) {
										const auto op = cons_clone->getOperand(i);
										if(op == producer) {
											cons_clone->setOperand(i, mat_gep);
										}
									}
									DBG(errs() << ">> cons after: " << *cons_clone << "\n";)
									
									// branch to continue block
									auto br_inst = dyn_cast_or_null<BranchInst>(branch_term);
									assert(br_inst != nullptr && "invalid terminator");
									assert(br_inst->isUnconditional() && "branch must be unconditional");
									br_inst->setSuccessor(0, continue_block);
									
									// update continue phi (if there is one)
									if(continue_phi != nullptr) {
										continue_phi->addIncoming(cons_clone, cons_clone->getParent());
										DBG(errs() << ">> updated phi: " << *continue_phi << "\n";)
									}
									break;
								}
							}
						};
						handle_condition_tree(cond_tree, {}, nullptr);
						
						if(continue_phi != nullptr) {
							cons->replaceAllUsesWith(continue_phi);
						}
						cons->eraseFromParent();
					}
				}
			}
			
			// TODO: cleanup / DCE (will be done at the end anyways, but might be better to do here already)
		}
		
		void lower_mem_instructions() {
			for (auto& mem_instr : mem_instrs) {
				if (auto memcpy_instr = dyn_cast_or_null<MemCpyInst>(mem_instr)) {
					was_modified |= lower_memcpy(*memcpy_instr);
				} else if (auto memmove_instr = dyn_cast_or_null<MemMoveInst>(mem_instr)) {
					llvm::errs() << "can't lower memmove yet: " << *memmove_instr << "\n";
				} else if (auto memset_instr = dyn_cast_or_null<MemSetInst>(mem_instr)) {
					was_modified |= lower_memset(*memset_instr);
				} else {
					llvm::errs() << "unknown/unhandled memory instruction: " << *mem_instr << "\n";
				}
			}
		}
		
		//! tries to lower "memcpy_instr" to LLVM instructions,
		//! returns true if the lowering happened
		bool lower_memcpy(MemCpyInst& memcpy_instr) {
			auto [src, dst, const_len_op, len_op, override_loop_op_type, _, __] = libfloor_utils::compute_memop_lower_info(*M, memcpy_instr);
			if (!src || !dst || !len_op) {
				return false;
			}
			
			// -> length is either constant > 1 or a dynamic length
			const TargetTransformInfo& TTI = getAnalysis<TargetTransformInfoWrapperPass>().getTTI(*func);
			if (const_len_op) {
				createMemCpyLoopKnownSize(&memcpy_instr, src, dst, const_len_op,
										  memcpy_instr.getSourceAlign().valueOrOne(), memcpy_instr.getDestAlign().valueOrOne(),
										  memcpy_instr.isVolatile(), memcpy_instr.isVolatile(), TTI, override_loop_op_type);
			} else {
				createMemCpyLoopUnknownSize(&memcpy_instr, src, dst, len_op,
											memcpy_instr.getSourceAlign().valueOrOne(), memcpy_instr.getDestAlign().valueOrOne(),
											memcpy_instr.isVolatile(), memcpy_instr.isVolatile(), TTI, override_loop_op_type);
			}
			memcpy_instr.eraseFromParent();
			return true;
		}
		
		//! tries to lower "memset_instr" to LLVM instructions,
		//! returns true if the lowering happened
		bool lower_memset(MemSetInst& memset_instr) {
			auto [src, dst, const_len_op, len_op, override_loop_op_type, _, __] = libfloor_utils::compute_memop_lower_info(*M, memset_instr);
			if (!src || !dst || !len_op) {
				return false;
			}
			
			// TODO: make use of VK_KHR_zero_initialize_workgroup_memory when memory is local/work-group and this is the first use of it
			
			if (const_len_op) {
				createMemSetLoopKnownSize(&memset_instr, dst, const_len_op, src,
										  memset_instr.getDestAlign().valueOrOne(),
										  memset_instr.isVolatile(), override_loop_op_type);
			} else {
				createMemSetLoop(&memset_instr, dst, len_op, src,
								 memset_instr.getDestAlign().valueOrOne(),
								 memset_instr.isVolatile(), override_loop_op_type);
			}
			memset_instr.eraseFromParent();
			return true;
		}
	};
	
	// VulkanFinalModuleCleanup:
	// * strip unused functions/prototypes/externs
	struct VulkanFinalModuleCleanup : public ModulePass {
		static char ID; // Pass identification, replacement for typeid
		
		Module* M { nullptr };
		LLVMContext* ctx { nullptr };
		
		VulkanFinalModuleCleanup() : ModulePass(ID) {
			initializeVulkanFinalModuleCleanupPass(*PassRegistry::getPassRegistry());
		}
		
		bool runOnModule(Module& Mod) override {
			M = &Mod;
			ctx = &M->getContext();
			
			// kill all functions named floor.builtin.* (we still need other floor.* functions)
			bool module_modified = false;
			for (auto func_iter = Mod.begin(); func_iter != Mod.end();) {
				auto& func = *func_iter;
				if (func.getName().startswith("floor.builtin.")) {
					if(func.getNumUses() != 0) {
						errs() << func.getName() << " should not have any uses at this point!\n";
					}
					++func_iter; // inc before erase
					func.eraseFromParent();
					module_modified = true;
					continue;
				}
				++func_iter;
			}
			
			// perform function scope type replacement for GVs with constant AS (later Function storage)
			for (auto& GV : Mod.globals()) {
				if (GV.getType()->getPointerAddressSpace() != SPIRAS_Constant) {
					continue;
				}
				module_modified |= vulkan_function_scope_type_replacement::handle_function_scope_variable(&GV);
			}
			
			return module_modified;
		}
		
	};
	
}

char VulkanFinal::ID = 0;
FunctionPass *llvm::createVulkanFinalPass() {
	return new VulkanFinal();
}
INITIALIZE_PASS_BEGIN(VulkanFinal, "VulkanFinal", "VulkanFinal Pass", false, false)
INITIALIZE_PASS_DEPENDENCY(AAResultsWrapperPass)
INITIALIZE_PASS_END(VulkanFinal, "VulkanFinal", "VulkanFinal Pass", false, false)

char VulkanBuiltinParamHandling::ID = 0;
FunctionPass *llvm::createVulkanBuiltinParamHandlingPass() {
	return new VulkanBuiltinParamHandling();
}
INITIALIZE_PASS_BEGIN(VulkanBuiltinParamHandling, "VulkanBuiltinParamHandling", "VulkanBuiltinParamHandling Pass", false, false)
INITIALIZE_PASS_DEPENDENCY(AAResultsWrapperPass)
INITIALIZE_PASS_END(VulkanBuiltinParamHandling, "VulkanBuiltinParamHandling", "VulkanBuiltinParamHandling Pass", false, false)

char VulkanPreFinal::ID = 0;
FunctionPass *llvm::createVulkanPreFinalPass() {
	return new VulkanPreFinal();
}
INITIALIZE_PASS_BEGIN(VulkanPreFinal, "VulkanPreFinal", "VulkanPreFinal Pass", false, false)
INITIALIZE_PASS_DEPENDENCY(AAResultsWrapperPass)
INITIALIZE_PASS_DEPENDENCY(GlobalsAAWrapperPass)
INITIALIZE_PASS_DEPENDENCY(AssumptionCacheTracker)
INITIALIZE_PASS_DEPENDENCY(CallGraphWrapperPass)
INITIALIZE_PASS_DEPENDENCY(TargetLibraryInfoWrapperPass)
INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
INITIALIZE_PASS_END(VulkanPreFinal, "VulkanPreFinal", "VulkanPreFinal Pass", false, false)

char VulkanFinalModuleCleanup::ID = 0;
ModulePass *llvm::createVulkanFinalModuleCleanupPass() {
	return new VulkanFinalModuleCleanup();
}
INITIALIZE_PASS_BEGIN(VulkanFinalModuleCleanup, "VulkanFinal module cleanup", "VulkanFinal module cleanup Pass", false, false)
INITIALIZE_PASS_END(VulkanFinalModuleCleanup, "VulkanFinal module cleanup", "VulkanFinal module cleanup Pass", false, false)
