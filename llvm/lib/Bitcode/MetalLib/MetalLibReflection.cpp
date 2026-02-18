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

#include "MetalLibReflection.hpp"
#include <optional>
#include <vector>
#include <array>
#include "llvm/Transforms/LibFloor/MetalTypes.h"
#include "llvm/Transforms/LibFloor/metal_reflection.hpp"
#include "llvm/Transforms/LibFloor/metal_reflection_writing.hpp"

#define METAL_REFLECTION_DUMP_STATE 0
#if METAL_REFLECTION_DUMP_STATE
#include "llvm/Transforms/LibFloor/metal_reflection_dumping.hpp"
#endif

namespace metal::reflection {

static constexpr const node_id_t invalid_node_id { .id = ~0u };

struct reflection_state_t {
	reflection_t& refl;
	
	node_id_t add_node(std::unique_ptr<node_base_t>&& node) {
		refl.nodes->emplace_back(std::move(node));
		return node_id_t { .id = uint32_t(refl.nodes->size() - 1u) };
	}
	
	template <typename node_type> requires (std::is_base_of_v<node_base_t, node_type>)
	std::pair<node_type&, node_id_t> create_node() {
		auto node = std::make_unique<node_type>();
		auto node_ptr = node.get();
		return { *node_ptr, add_node(std::move(node)) };
	}
	
	// these MDNodes may be referenced more than once, but we only want to emit them once
	// -> remember which ones we've already emitted + their reflection node ID
	std::unordered_map<const llvm::MDNode*, node_id_t> struct_type_info_lut;
};

static inline node_id_t create_node(const llvm::MDNode& md_node, const std::string node_type_name, reflection_state_t& state,
									const bool is_return_type = false);

static bool md_node_iterate(const llvm::MDNode& md_node, std::function<bool(llvm::StringRef, llvm::MDNode::op_iterator&)> cb,
							const uint32_t start_idx = 2u /* for most direct nodes */) {
	assert(cb);
	if (md_node.getNumOperands() <= start_idx + 1u) {
		llvm::errs() << "reflection: start index " << start_idx << " too high for MDNode: " << md_node << "\n";
		return false;
	}
	auto iter = std::next(md_node.op_begin(), start_idx);
	for (; iter != md_node.op_end(); ++iter) {
		const auto type = llvm::dyn_cast_or_null<llvm::MDString>(*iter);
		if (!type) {
			llvm::errs() << "reflection: unexpected metadata type in: " << md_node << "\n";
			return false;
		}
		if (!cb(type->getString(), iter)) {
			llvm::errs() << "reflection: error during handling of \"" << type->getString() << "\" in: " << md_node << "\n";
			return false;
		}
	}
	return true;
}

static const llvm::MDNode* md_get_mdnode(const llvm::MDNode& md_node, llvm::MDNode::op_iterator& iter) {
	if (iter == md_node.op_end()) {
		llvm::errs() << "reflection: premature end in: " << md_node << "\n";
		return nullptr;
	}
	const auto ret_md_mode = llvm::dyn_cast_or_null<llvm::MDNode>(*iter);
	if (!ret_md_mode) {
		llvm::errs() << "reflection: expected MDNode in: " << md_node << "\n";
		return nullptr;
	}
	return ret_md_mode;
}
static const llvm::MDNode* md_get_next_mdnode(const llvm::MDNode& md_node, llvm::MDNode::op_iterator& iter) {
	return md_get_mdnode(md_node, ++iter);
}

static std::optional<llvm::StringRef> md_get_string(const llvm::MDNode& md_node, llvm::MDNode::op_iterator& iter) {
	if (iter == md_node.op_end()) {
		llvm::errs() << "reflection: premature end in: " << md_node << "\n";
		return {};
	}
	const auto str = llvm::dyn_cast_or_null<llvm::MDString>(*iter);
	if (!str) {
		llvm::errs() << "reflection: expected string in: " << md_node << "\n";
		return {};
	}
	return str->getString();
}
static std::optional<llvm::StringRef> md_get_next_string(const llvm::MDNode& md_node, llvm::MDNode::op_iterator& iter) {
	return md_get_string(md_node, ++iter);
}

static std::optional<uint32_t> md_get_uint(const llvm::MDNode& md_node, llvm::MDNode::op_iterator& iter) {
	if (iter == md_node.op_end()) {
		llvm::errs() << "reflection: premature end in: " << md_node << "\n";
		return {};
	}
	const auto cnst = llvm::dyn_cast_or_null<llvm::ConstantAsMetadata>(*iter);
	if (!cnst) {
		llvm::errs() << "reflection: expected constant in: " << md_node << "\n";
		return {};
	}
	const auto cnst_int = llvm::dyn_cast_or_null<llvm::ConstantInt>(cnst->getValue());
	if (!cnst_int) {
		llvm::errs() << "reflection: expected constant integer: " << md_node << "\n";
		return {};
	}
	const auto val = cnst_int->getZExtValue();
	if (val > 0xFFFF'FFFFull) {
		llvm::errs() << "reflection: expected constant 32-bit unsigned integer: " << md_node << " -> " << val << "\n";
		return {};
	}
	return uint32_t(val);
}
static std::optional<uint32_t> md_get_next_uint(const llvm::MDNode& md_node, llvm::MDNode::op_iterator& iter) {
	return md_get_uint(md_node, ++iter);
}

template <typename node_type>
static inline bool generic_create_type_name_and_name(node_type& node, const llvm::MDNode& md_node, reflection_state_t& state) {
	if (!md_node_iterate(md_node, [&node, &md_node](llvm::StringRef type_str, llvm::MDNode::op_iterator& iter) {
		if (type_str == "air.arg_type_name") {
			if (const auto str = md_get_next_string(md_node, iter); str) {
				node.type_name = str->str();
				return true;
			}
		} else if (type_str == "air.arg_name") {
			if (const auto str = md_get_next_string(md_node, iter); str) {
				node.name = str->str();
				return true;
			}
		}
		return false;
	})) {
		return false;
	}
	return true;
}

static inline node_id_t create_barycentric(const llvm::MDNode& md_node, reflection_state_t& state) {
	auto [node, node_id] = state.create_node<node_barycentric_coord_arg_t>();
	if (!md_node_iterate(md_node, [&node, &md_node](llvm::StringRef type_str, llvm::MDNode::op_iterator& iter) {
		if (type_str == "air.center") {
			node.sampling_qualifier = SAMPLING_QUALIFIER::CENTER;
			return true;
		} else if (type_str == "air.centroid") {
			node.sampling_qualifier = SAMPLING_QUALIFIER::CENTROID;
			return true;
		} else if (type_str == "air.perspective") {
			node.interpolation_qualifier = INTERPOLATION_QUALIFIER::PERSPECTIVE;
			return true;
		} else if (type_str == "air.flat") {
			node.interpolation_qualifier = INTERPOLATION_QUALIFIER::FLAT;
			return true;
		} else if (type_str == "air.no_perspective") {
			node.interpolation_qualifier = INTERPOLATION_QUALIFIER::NO_PERSPECTIVE;
			return true;
		} else if (type_str == "air.arg_type_name") {
			if (const auto str = md_get_next_string(md_node, iter); str) {
				node.type_name = str->str();
				return true;
			}
		} else if (type_str == "air.arg_name") {
			if (const auto str = md_get_next_string(md_node, iter); str) {
				node.name = str->str();
				return true;
			}
		}
		return false;
	})) {
		return invalid_node_id;
	}
	return node_id;
}

static inline node_id_t create_base_instance(const llvm::MDNode& md_node, reflection_state_t& state) {
	auto [node, node_id] = state.create_node<node_base_instance_arg_t>();
	return generic_create_type_name_and_name(node, md_node, state) ? node_id : invalid_node_id;
}

static inline node_id_t create_base_vertex(const llvm::MDNode& md_node, reflection_state_t& state) {
	auto [node, node_id] = state.create_node<node_base_vertex_arg_t>();
	return generic_create_type_name_and_name(node, md_node, state) ? node_id : invalid_node_id;
}

static inline node_id_t create_struct_type_info(const llvm::MDNode& md_node, reflection_state_t& state) {
	// only emit this once
	if (const auto lut_iter = state.struct_type_info_lut.find(&md_node); lut_iter != state.struct_type_info_lut.end()) {
		return lut_iter->second;
	}
	
	auto [node, node_id] = state.create_node<node_struct_type_info_t>();
	state.struct_type_info_lut.emplace(&md_node, node_id);
	
	auto md_iter = md_node.op_begin();
	while (md_iter != md_node.op_end()) {
		struct_type_info_field_t field {};
		
		// optional air.struct_type_info:
		if (const auto str = llvm::dyn_cast_or_null<llvm::MDString>(*md_iter); str) {
			if (str->getString() != "air.struct_type_info") {
				llvm::errs() << "reflection: expected air.struct_type_info in: " << md_node << "\n";
				return invalid_node_id;
			}
			
			const auto st_type_info_md_node = md_get_next_mdnode(md_node, md_iter);
			if (!st_type_info_md_node) {
				llvm::errs() << "reflection: expected MDNode after air.struct_type_info in: " << md_node << "\n";
				return invalid_node_id;
			}
			const auto st_type_info_node_id = create_struct_type_info(*st_type_info_md_node, state);
			if (st_type_info_node_id.id == invalid_node_id.id) {
				llvm::errs() << "reflection: invalid air.struct_type_info in: " << md_node << "\n";
				return invalid_node_id;
			}
			field.struct_type_info = st_type_info_node_id;
			++md_iter;
		}
		
		const auto offset = md_get_uint(md_node, md_iter);
		if (!offset) {
			return invalid_node_id;
		}
		field.offset = *offset;
		
		const auto size = md_get_next_uint(md_node, md_iter);
		if (!size) {
			return invalid_node_id;
		}
		field.size = *size;
		
		const auto array_entries = md_get_next_uint(md_node, md_iter);
		if (!array_entries) {
			return invalid_node_id;
		}
		field.array_entries = *array_entries;
		
		const auto type_name = md_get_next_string(md_node, md_iter);
		if (!type_name) {
			return invalid_node_id;
		}
		field.type_name = *type_name;
		
		const auto field_name = md_get_next_string(md_node, md_iter);
		if (!field_name) {
			return invalid_node_id;
		}
		field.field_name = *field_name;
		
		// optional indirect_argument handling
		++md_iter;
		if (md_iter != md_node.op_end()) {
			const auto indirect_arg = llvm::dyn_cast_or_null<llvm::MDString>(*md_iter);
			if (indirect_arg && indirect_arg->getString() != "air.struct_type_info" /* part of the next field */) {
				if (indirect_arg->getString() != "air.indirect_argument") {
					llvm::errs() << "reflection: got " << *indirect_arg << ", expected air.indirect_argument in: " << md_node << "\n";
					return invalid_node_id;
				}
				
				++md_iter;
				if (md_iter == md_node.op_end()) {
					llvm::errs() << "reflection: expected argument after air.indirect_argument in: " << md_node << "\n";
					return invalid_node_id;
				}
				
				if (const auto ind_arg_md_node = llvm::dyn_cast_or_null<llvm::MDNode>(*md_iter); ind_arg_md_node) {
					if (ind_arg_md_node->getNumOperands() < 2) {
						llvm::errs() << "reflection: unexpected operand count in air.indirect_argument in: " << md_node << "\n";
						return invalid_node_id;
					}
					
					const auto arg_idx = dyn_cast_or_null<llvm::ConstantAsMetadata>(ind_arg_md_node->getOperand(0));
					const auto arg_type = dyn_cast_or_null<llvm::MDString>(ind_arg_md_node->getOperand(1));
					if (!arg_idx || !arg_type) {
						llvm::errs() << "reflection: invalid air.indirect_argument argument index/type\n";
						return invalid_node_id;
					}
					
					const auto ind_node_id = create_node(*ind_arg_md_node, arg_type->getString().data(), state);
					if (ind_node_id.id == invalid_node_id.id) {
						llvm::errs() << "reflection: failed to handle air.indirect_argument argument in: " << md_node << "\n";
						return invalid_node_id;
					}
					field.indirect_argument = ind_node_id;
				} else if (const auto ind_arg_int_node = llvm::dyn_cast_or_null<llvm::ConstantAsMetadata>(*md_iter); ind_arg_int_node) {
					const auto cnst_int = llvm::dyn_cast_or_null<llvm::ConstantInt>(ind_arg_int_node->getValue());
					if (!cnst_int) {
						llvm::errs() << "reflection: expected constant integer: " << ind_arg_int_node << "\n";
						return invalid_node_id;
					}
					const auto val = cnst_int->getZExtValue();
					if (val > 0xFFFF'FFFFull) {
						llvm::errs() << "reflection: expected constant 32-bit unsigned integer: " << ind_arg_int_node << " -> " << val << "\n";
						return invalid_node_id;
					}
					field.indirect_location = { uint32_t(val) };
				} else {
					llvm::errs() << "reflection: unexpected air.indirect_argument argument in: " << md_node << "\n";
					return invalid_node_id;
				}
				
				++md_iter;
			}
			// else: normal field continuation
		}
		
		if (!node.fields) {
			node.fields = std::vector<struct_type_info_field_t> {};
		}
		node.fields->emplace_back(field);
	}
	
	return node_id;
}

template <NODE_TYPE node_type_enum, typename node_type>
requires (node_type_enum == NODE_TYPE::BUFFER_ARG || node_type_enum == NODE_TYPE::INDIRECT_BUFFER_ARG)
static inline bool generic_create_buffer(node_type& node, const llvm::MDNode& md_node, reflection_state_t& state) {
	if (!md_node_iterate(md_node, [&node, &md_node, &state](llvm::StringRef type_str, llvm::MDNode::op_iterator& iter) {
		if (type_str == "air.buffer_size") {
			if (const auto val = md_get_next_uint(md_node, iter); val) {
				node.buffer_size = { *val };
				return true;
			}
		} else if (type_str == "air.location_index") {
			const auto location_index = md_get_next_uint(md_node, iter);
			if (!location_index) {
				return false;
			}
			const auto location_count = md_get_next_uint(md_node, iter);
			if (!location_count) {
				return false;
			}
			node.location_index = { *location_index };
			node.location_count = { *location_count };
			return true;
		} else if (type_str == "air.read") {
			node.access_qualifier = ACCESS_QUALIFIER::READ;
			return true;
		} else if (type_str == "air.read_write") {
			node.access_qualifier = ACCESS_QUALIFIER::READ_WRITE;
			return true;
		} else if (type_str == "air.write") {
			node.access_qualifier = ACCESS_QUALIFIER::WRITE;
			return true;
		} else if (type_str == "air.address_space") {
			if (const auto val = md_get_next_uint(md_node, iter); val) {
				node.address_space = (ADDRESS_SPACE)*val;
				return true;
			}
		} else if (type_str == "air.struct_type_info") {
			const auto st_info_md_node = md_get_next_mdnode(md_node, iter);
			if (!st_info_md_node) {
				return false;
			}
			if (const auto st_info_node_id = create_struct_type_info(*st_info_md_node, state);
				st_info_node_id.id != invalid_node_id.id) {
				node.struct_type_info = st_info_node_id;
				return true;
			}
		} else if (type_str == "air.arg_type_size") {
			if (const auto val = md_get_next_uint(md_node, iter); val) {
				node.type_size = { *val };
				return true;
			}
		} else if (type_str == "air.arg_type_align_size") {
			if (const auto val = md_get_next_uint(md_node, iter); val) {
				node.type_align = { *val };
				return true;
			}
		} else if (type_str == "air.arg_type_name") {
			if (const auto str = md_get_next_string(md_node, iter); str) {
				node.type_name = str->str();
				return true;
			}
		} else if (type_str == "air.arg_name") {
			if (const auto str = md_get_next_string(md_node, iter); str) {
				node.name = str->str();
				return true;
			}
		}
		return false;
	})) {
		return false;
	}
	return true;
}

static inline node_id_t create_buffer(const llvm::MDNode& md_node, reflection_state_t& state) {
	auto [node, node_id] = state.create_node<node_buffer_arg_t>();
	return generic_create_buffer<NODE_TYPE::BUFFER_ARG>(node, md_node, state) ? node_id : invalid_node_id;
}

static inline node_id_t create_indirect_buffer(const llvm::MDNode& md_node, reflection_state_t& state) {
	auto [node, node_id] = state.create_node<node_indirect_buffer_arg_t>();
	return generic_create_buffer<NODE_TYPE::INDIRECT_BUFFER_ARG>(node, md_node, state) ? node_id : invalid_node_id;
}

static inline node_id_t create_control_point_field(const llvm::MDNode& md_node, reflection_state_t& state) {
	auto [node, node_id] = state.create_node<node_control_point_field_t>();
	if (!md_node_iterate(md_node, [&node, &md_node](llvm::StringRef type_str, llvm::MDNode::op_iterator& iter) {
		if (type_str == "air.location_index") {
			const auto location_index = md_get_next_uint(md_node, iter);
			if (!location_index) {
				return false;
			}
			const auto location_count = md_get_next_uint(md_node, iter);
			if (!location_count) {
				return false;
			}
			node.location_index = { *location_index };
			node.location_count = { *location_count };
			return true;
		} else if (type_str == "air.arg_type_name") {
			if (const auto str = md_get_next_string(md_node, iter); str) {
				node.type_name = str->str();
				return true;
			}
		} else if (type_str == "air.arg_name") {
			if (const auto str = md_get_next_string(md_node, iter); str) {
				node.name = str->str();
				return true;
			}
		}
		return false;
	}, 0u /* must start at #0 for these */)) {
		return invalid_node_id;
	}
	return node_id;
}

static inline node_id_t create_control_point_input(const llvm::MDNode& md_node, reflection_state_t& state) {
	auto [node, node_id] = state.create_node<node_control_point_input_arg_t>();
	
	auto md_iter = std::next(md_node.op_begin(), 1);
	
	const auto func_md_node = md_get_next_mdnode(md_node, md_iter);
	if (!func_md_node) {
		llvm::errs() << "reflection: expected function in: " << md_node << "\n";
		return invalid_node_id;
	}
	
	// rest is MDNodes
	++md_iter;
	if (md_iter != md_node.op_end()) {
		node.fields = std::vector<node_id_t> {};
		for (; md_iter != md_node.op_end(); ++md_iter) {
			const auto field_md_node = md_get_mdnode(md_node, md_iter);
			if (!field_md_node) {
				llvm::errs() << "reflection: expected field MDNode in: " << md_node << "\n";
				return invalid_node_id;
			}
			const auto field_id = create_control_point_field(*field_md_node, state);
			if (field_id.id == invalid_node_id.id) {
				return invalid_node_id;
			}
			node.fields->emplace_back(field_id);
		}
	}
	
	return node_id;
}

static inline node_id_t create_fragment_input(const llvm::MDNode& md_node, reflection_state_t& state) {
	auto [node, node_id] = state.create_node<node_fragment_input_arg_t>();
	if (!md_node_iterate(md_node, [&node, &md_node](llvm::StringRef type_str, llvm::MDNode::op_iterator& iter) {
		if (type_str == "air.fragment_input") {
			if (const auto str = md_get_next_string(md_node, iter); str) {
				node.attribute_name = str->str();
				return true;
			}
		} else if (type_str == "air.center") {
			node.sampling_qualifier = SAMPLING_QUALIFIER::CENTER;
			return true;
		} else if (type_str == "air.centroid") {
			node.sampling_qualifier = SAMPLING_QUALIFIER::CENTROID;
			return true;
		} else if (type_str == "air.perspective") {
			node.interpolation_qualifier = INTERPOLATION_QUALIFIER::PERSPECTIVE;
			return true;
		} else if (type_str == "air.flat") {
			node.interpolation_qualifier = INTERPOLATION_QUALIFIER::FLAT;
			return true;
		} else if (type_str == "air.no_perspective") {
			node.interpolation_qualifier = INTERPOLATION_QUALIFIER::NO_PERSPECTIVE;
			return true;
		} else if (type_str == "air.arg_type_name") {
			if (const auto str = md_get_next_string(md_node, iter); str) {
				node.type_name = str->str();
				return true;
			}
		} else if (type_str == "air.arg_name") {
			if (const auto str = md_get_next_string(md_node, iter); str) {
				node.name = str->str();
				return true;
			}
		}
		return false;
	}, 1u /* start one earlier so that we get air.fragment_input */)) {
		return invalid_node_id;
	}
	return node_id;
}

static inline node_id_t create_indirect_constant(const llvm::MDNode& md_node, reflection_state_t& state) {
	auto [node, node_id] = state.create_node<node_indirect_constant_arg_t>();
	if (!md_node_iterate(md_node, [&node, &md_node](llvm::StringRef type_str, llvm::MDNode::op_iterator& iter) {
		if (type_str == "air.location_index") {
			const auto location_index = md_get_next_uint(md_node, iter);
			if (!location_index) {
				return false;
			}
			const auto location_count = md_get_next_uint(md_node, iter);
			if (!location_count) {
				return false;
			}
			node.location_index = { *location_index };
			node.location_count = { *location_count };
			return true;
		} else if (type_str == "air.arg_type_name") {
			if (const auto str = md_get_next_string(md_node, iter); str) {
				node.type_name = str->str();
				return true;
			}
		} else if (type_str == "air.arg_name") {
			if (const auto str = md_get_next_string(md_node, iter); str) {
				node.name = str->str();
				return true;
			}
		}
		return false;
	})) {
		return invalid_node_id;
	}
	return node_id;
}

static inline node_id_t create_instance_id(const llvm::MDNode& md_node, reflection_state_t& state) {
	auto [node, node_id] = state.create_node<node_instance_id_arg_t>();
	return generic_create_type_name_and_name(node, md_node, state) ? node_id : invalid_node_id;
}

static inline node_id_t create_patch_id(const llvm::MDNode& md_node, reflection_state_t& state) {
	auto [node, node_id] = state.create_node<node_patch_id_arg_t>();
	return generic_create_type_name_and_name(node, md_node, state) ? node_id : invalid_node_id;
}

static inline node_id_t create_point_coord(const llvm::MDNode& md_node, reflection_state_t& state) {
	auto [node, node_id] = state.create_node<node_point_coord_arg_t>();
	return generic_create_type_name_and_name(node, md_node, state) ? node_id : invalid_node_id;
}

static inline node_id_t create_position(const llvm::MDNode& md_node, reflection_state_t& state) {
	auto [node, node_id] = state.create_node<node_position_arg_t>();
	if (!md_node_iterate(md_node, [&node, &md_node](llvm::StringRef type_str, llvm::MDNode::op_iterator& iter) {
		if (type_str == "air.center") {
			node.sampling_qualifier = SAMPLING_QUALIFIER::CENTER;
			return true;
		} else if (type_str == "air.centroid") {
			node.sampling_qualifier = SAMPLING_QUALIFIER::CENTROID;
			return true;
		} else if (type_str == "air.perspective") {
			node.interpolation_qualifier = INTERPOLATION_QUALIFIER::PERSPECTIVE;
			return true;
		} else if (type_str == "air.flat") {
			node.interpolation_qualifier = INTERPOLATION_QUALIFIER::FLAT;
			return true;
		} else if (type_str == "air.no_perspective") {
			node.interpolation_qualifier = INTERPOLATION_QUALIFIER::NO_PERSPECTIVE;
			return true;
		} else if (type_str == "air.arg_type_name") {
			if (const auto str = md_get_next_string(md_node, iter); str) {
				node.type_name = str->str();
				return true;
			}
		} else if (type_str == "air.arg_name") {
			if (const auto str = md_get_next_string(md_node, iter); str) {
				node.name = str->str();
				return true;
			}
		}
		return false;
	})) {
		return invalid_node_id;
	}
	return node_id;
}

static inline node_id_t create_position_in_patch(const llvm::MDNode& md_node, reflection_state_t& state) {
	auto [node, node_id] = state.create_node<node_position_in_patch_arg_t>();
	return generic_create_type_name_and_name(node, md_node, state) ? node_id : invalid_node_id;
}

static inline node_id_t create_primitive_id(const llvm::MDNode& md_node, reflection_state_t& state) {
	auto [node, node_id] = state.create_node<node_primitive_id_arg_t>();
	return generic_create_type_name_and_name(node, md_node, state) ? node_id : invalid_node_id;
}

static inline node_id_t create_simdgroup_index_in_threadgroup(const llvm::MDNode& md_node, reflection_state_t& state) {
	auto [node, node_id] = state.create_node<node_simdgroup_index_in_threadgroup_arg_t>();
	return generic_create_type_name_and_name(node, md_node, state) ? node_id : invalid_node_id;
}

static inline node_id_t create_simdgroups_per_threadgroup(const llvm::MDNode& md_node, reflection_state_t& state) {
	auto [node, node_id] = state.create_node<node_simdgroups_per_threadgroup_arg_t>();
	return generic_create_type_name_and_name(node, md_node, state) ? node_id : invalid_node_id;
}

static inline node_id_t create_texture(const llvm::MDNode& md_node, reflection_state_t& state) {
	auto [node, node_id] = state.create_node<node_texture_arg_t>();
	if (!md_node_iterate(md_node, [&node, &md_node](llvm::StringRef type_str, llvm::MDNode::op_iterator& iter) {
		if (type_str == "air.location_index") {
			const auto location_index = md_get_next_uint(md_node, iter);
			if (!location_index) {
				return false;
			}
			const auto location_count = md_get_next_uint(md_node, iter);
			if (!location_count) {
				return false;
			}
			node.location_index = { *location_index };
			node.location_count = { *location_count };
			return true;
		} else if (type_str == "air.sample") {
			node.access_qualifier = ACCESS_QUALIFIER::SAMPLE;
			return true;
		} else if (type_str == "air.read") {
			node.access_qualifier = ACCESS_QUALIFIER::READ;
			return true;
		} else if (type_str == "air.read_write") {
			node.access_qualifier = ACCESS_QUALIFIER::READ_WRITE;
			return true;
		} else if (type_str == "air.write") {
			node.access_qualifier = ACCESS_QUALIFIER::WRITE;
			return true;
		} else if (type_str == "air.arg_type_name") {
			if (const auto str = md_get_next_string(md_node, iter); str) {
				node.type_name = str->str();
				return true;
			}
		} else if (type_str == "air.arg_name") {
			if (const auto str = md_get_next_string(md_node, iter); str) {
				node.name = str->str();
				return true;
			}
		}
		return false;
	})) {
		return invalid_node_id;
	}
	return node_id;
}

static inline node_id_t create_thread_index_in_simdgroup(const llvm::MDNode& md_node, reflection_state_t& state) {
	auto [node, node_id] = state.create_node<node_thread_index_in_simdgroup_arg_t>();
	return generic_create_type_name_and_name(node, md_node, state) ? node_id : invalid_node_id;
}

static inline node_id_t create_thread_position_in_grid(const llvm::MDNode& md_node, reflection_state_t& state) {
	auto [node, node_id] = state.create_node<node_thread_position_in_grid_arg_t>();
	return generic_create_type_name_and_name(node, md_node, state) ? node_id : invalid_node_id;
}

static inline node_id_t create_thread_position_in_threadgroup(const llvm::MDNode& md_node, reflection_state_t& state) {
	auto [node, node_id] = state.create_node<node_thread_position_in_threadgroup_arg_t>();
	return generic_create_type_name_and_name(node, md_node, state) ? node_id : invalid_node_id;
}

static inline node_id_t create_threadgroup_position_in_grid(const llvm::MDNode& md_node, reflection_state_t& state) {
	auto [node, node_id] = state.create_node<node_threadgroup_position_in_grid_arg_t>();
	return generic_create_type_name_and_name(node, md_node, state) ? node_id : invalid_node_id;
}

static inline node_id_t create_threadgroups_per_grid(const llvm::MDNode& md_node, reflection_state_t& state) {
	auto [node, node_id] = state.create_node<node_threadgroups_per_grid_arg_t>();
	return generic_create_type_name_and_name(node, md_node, state) ? node_id : invalid_node_id;
}

static inline node_id_t create_threads_per_grid(const llvm::MDNode& md_node, reflection_state_t& state) {
	auto [node, node_id] = state.create_node<node_threads_per_grid_arg_t>();
	return generic_create_type_name_and_name(node, md_node, state) ? node_id : invalid_node_id;
}

static inline node_id_t create_threads_per_simdgroup(const llvm::MDNode& md_node, reflection_state_t& state) {
	auto [node, node_id] = state.create_node<node_threads_per_simdgroup_arg_t>();
	return generic_create_type_name_and_name(node, md_node, state) ? node_id : invalid_node_id;
}

static inline node_id_t create_threads_per_threadgroup(const llvm::MDNode& md_node, reflection_state_t& state) {
	auto [node, node_id] = state.create_node<node_threads_per_threadgroup_arg_t>();
	return generic_create_type_name_and_name(node, md_node, state) ? node_id : invalid_node_id;
}

static inline node_id_t create_vertex_id(const llvm::MDNode& md_node, reflection_state_t& state) {
	auto [node, node_id] = state.create_node<node_vertex_id_arg_t>();
	return generic_create_type_name_and_name(node, md_node, state) ? node_id : invalid_node_id;
}

static inline node_id_t create_depth_ret(const llvm::MDNode& md_node, reflection_state_t& state) {
	auto [node, node_id] = state.create_node<node_depth_ret_t>();
	if (!md_node_iterate(md_node, [&node, &md_node](llvm::StringRef type_str, llvm::MDNode::op_iterator& iter) {
		if (type_str == "air.depth_qualifier") {
			if (const auto str = md_get_next_string(md_node, iter); str) {
				const auto qualifier_str = str->str();
				if (qualifier_str == "air.any") {
					node.depth_qualifier = DEPTH_QUALIFIER::ANY;
				} else if (qualifier_str == "air.less") {
					node.depth_qualifier = DEPTH_QUALIFIER::LESS;
				} else if (qualifier_str == "air.greater") {
					node.depth_qualifier = DEPTH_QUALIFIER::GREATER;
				} else {
					llvm::errs() << "reflection: invalid depth qualifier in air.depth_qualifier: " << md_node << "\n";
					return false;
				}
				return true;
			}
		} else if (type_str == "air.arg_type_name") {
			if (const auto str = md_get_next_string(md_node, iter); str) {
				node.type_name = str->str();
				return true;
			}
		} else if (type_str == "air.arg_name") {
			if (const auto str = md_get_next_string(md_node, iter); str) {
				node.name = str->str();
				return true;
			}
		}
		return false;
	}, 1u /* start at #1 for depth return type */)) {
		return invalid_node_id;
	}
	return node_id;
}

static inline node_id_t create_point_size_ret(const llvm::MDNode& md_node, reflection_state_t& state) {
	auto [node, node_id] = state.create_node<node_point_size_ret_t>();
	if (!md_node_iterate(md_node, [&node, &md_node](llvm::StringRef type_str, llvm::MDNode::op_iterator& iter) {
		if (type_str == "air.arg_type_name") {
			if (const auto str = md_get_next_string(md_node, iter); str) {
				node.type_name = str->str();
				return true;
			}
		} else if (type_str == "air.arg_name") {
			if (const auto str = md_get_next_string(md_node, iter); str) {
				node.name = str->str();
				return true;
			}
		}
		return false;
	}, 1u /* start at #1 for return type */)) {
		return invalid_node_id;
	}
	return node_id;
}

static inline node_id_t create_position_ret(const llvm::MDNode& md_node, reflection_state_t& state) {
	auto [node, node_id] = state.create_node<node_position_ret_t>();
	if (!md_node_iterate(md_node, [&node, &md_node](llvm::StringRef type_str, llvm::MDNode::op_iterator& iter) {
		if (type_str == "air.arg_type_name") {
			if (const auto str = md_get_next_string(md_node, iter); str) {
				node.type_name = str->str();
				return true;
			}
		} else if (type_str == "air.arg_name") {
			if (const auto str = md_get_next_string(md_node, iter); str) {
				node.name = str->str();
				return true;
			}
		}
		return false;
	}, 1u /* start at #1 for return type */)) {
		return invalid_node_id;
	}
	return node_id;
}

static inline node_id_t create_vertex_output_ret(const llvm::MDNode& md_node, reflection_state_t& state) {
	auto [node, node_id] = state.create_node<node_vertex_output_ret_t>();
	if (!md_node_iterate(md_node, [&node, &md_node](llvm::StringRef type_str, llvm::MDNode::op_iterator& iter) {
		if (type_str == "air.vertex_output") {
			if (const auto str = md_get_next_string(md_node, iter); str) {
				node.attribute_name = str->str();
				return true;
			}
		} else if (type_str == "air.arg_type_name") {
			if (const auto str = md_get_next_string(md_node, iter); str) {
				node.type_name = str->str();
				return true;
			}
		} else if (type_str == "air.arg_name") {
			if (const auto str = md_get_next_string(md_node, iter); str) {
				node.name = str->str();
				return true;
			}
		}
		return false;
	}, 0u /* start at #0 for vertex output */)) {
		return invalid_node_id;
	}
	return node_id;
}

static inline node_id_t create_render_target_ret(const llvm::MDNode& md_node, reflection_state_t& state) {
	auto [node, node_id] = state.create_node<node_render_target_ret_t>();
	if (!md_node_iterate(md_node, [&node, &md_node](llvm::StringRef type_str, llvm::MDNode::op_iterator& iter) {
		if (type_str == "air.render_target") {
			const auto render_target_index = md_get_next_uint(md_node, iter);
			if (!render_target_index) {
				return false;
			}
			const auto raster_order_group = md_get_next_uint(md_node, iter);
			if (!raster_order_group) {
				return false;
			}
			node.render_target_index = { *render_target_index };
			node.raster_order_group = { *raster_order_group };
			return true;
		} else if (type_str == "air.arg_type_name") {
			if (const auto str = md_get_next_string(md_node, iter); str) {
				node.type_name = str->str();
				return true;
			}
		} else if (type_str == "air.arg_name") {
			if (const auto str = md_get_next_string(md_node, iter); str) {
				node.name = str->str();
				return true;
			}
		}
		return false;
	}, 0u /* start at #0 for return type */)) {
		return invalid_node_id;
	}
	return node_id;
}

static inline node_id_t create_workgroup_max_size_fn_attr(const llvm::MDNode& md_node, reflection_state_t& state) {
	auto [node, node_id] = state.create_node<node_workgroup_max_size_fn_attr_t>();
	if (!md_node_iterate(md_node, [&node, &md_node](llvm::StringRef type_str, llvm::MDNode::op_iterator& iter) {
		if (type_str == "air.max_work_group_size") {
			if (const auto val = md_get_next_uint(md_node, iter); val) {
				node.size = { *val };
				return true;
			}
		}
		return false;
	}, 0u /* start at #0 for function attribute */)) {
		return invalid_node_id;
	}
	return node_id;
}

static inline node_id_t create_patch_fn_attr(const llvm::MDNode& md_node, reflection_state_t& state) {
	auto [node, node_id] = state.create_node<node_patch_fn_attr_t>();
	if (!md_node_iterate(md_node, [&node, &md_node](llvm::StringRef type_str, llvm::MDNode::op_iterator& iter) {
		if (type_str == "air.patch") {
			if (const auto str = md_get_next_string(md_node, iter); str) {
				const auto kind_str = str->str();
				if (kind_str == "triangle") {
					node.kind = PATCH_KIND::TRIANGLE;
				} else if (kind_str == "quad") {
					node.kind = PATCH_KIND::QUAD;
				} else {
					llvm::errs() << "reflection: invalid patch kind in air.patch: " << md_node << "\n";
					return false;
				}
				return true;
			}
		} else if (type_str == "air.patch_control_point") {
			if (const auto val = md_get_next_uint(md_node, iter); val) {
				node.control_points = { *val };
				return true;
			}
		}
		return false;
	}, 0u /* start at #0 for function attribute */)) {
		return invalid_node_id;
	}
	return node_id;
}

static inline node_id_t create_node(const llvm::MDNode& md_node, const std::string node_type_name, reflection_state_t& state,
									const bool is_return_type) {
	using create_func_t = node_id_t (*)(const llvm::MDNode&, reflection_state_t&);
	static const std::unordered_map<std::string, create_func_t> arg_attr_handlers {
		// argument nodes
		{ "air.barycentric_coord", &create_barycentric },
		{ "air.base_instance", &create_base_instance },
		{ "air.base_vertex", &create_base_vertex },
		{ "air.buffer", &create_buffer },
		{ "air.patch_control_point_input", &create_control_point_input },
		{ "air.fragment_input", &create_fragment_input },
		{ "air.indirect_buffer", &create_indirect_buffer },
		{ "air.indirect_constant", &create_indirect_constant },
		{ "air.instance_id", &create_instance_id },
		{ "air.patch_id", &create_patch_id },
		{ "air.point_coord", &create_point_coord },
		{ "air.position", &create_position },
		{ "air.position_in_patch", &create_position_in_patch },
		{ "air.primitive_id", &create_primitive_id },
		{ "air.simdgroup_index_in_threadgroup", &create_simdgroup_index_in_threadgroup },
		{ "air.simdgroups_per_threadgroup", &create_simdgroups_per_threadgroup },
		{ "air.texture", &create_texture },
		{ "air.thread_index_in_simdgroup", &create_thread_index_in_simdgroup },
		{ "air.thread_position_in_grid", &create_thread_position_in_grid },
		{ "air.thread_position_in_threadgroup", &create_thread_position_in_threadgroup },
		{ "air.threadgroup_position_in_grid", &create_threadgroup_position_in_grid },
		{ "air.threadgroups_per_grid", &create_threadgroups_per_grid },
		{ "air.threads_per_grid", &create_threads_per_grid },
		{ "air.threads_per_simdgroup", &create_threads_per_simdgroup },
		{ "air.threads_per_threadgroup", &create_threads_per_threadgroup },
		{ "air.vertex_id", &create_vertex_id },
		
		// attribute nodes
		{ "air.max_work_group_size", &create_workgroup_max_size_fn_attr },
		{ "air.patch", &create_patch_fn_attr },
	};
	static const std::unordered_map<std::string, create_func_t> ret_handlers {
		// return nodes
		{ "air.depth", &create_depth_ret },
		{ "air.point_size", &create_point_size_ret },
		{ "air.position", &create_position_ret },
		{ "air.render_target", &create_render_target_ret },
		{ "air.vertex_output", &create_vertex_output_ret },
	};
	
	const auto& handlers = (!is_return_type ? arg_attr_handlers : ret_handlers);
	const auto handler = handlers.find(node_type_name);
	if (handler == handlers.end()) {
		llvm::errs() << "reflection: no handler for \"" << node_type_name << "\"\n";
		return invalid_node_id;
	}
	return handler->second(md_node, state);
}

template <FUNCTION_TYPE func_type>
static inline bool handle_return_types(const llvm::MDNode& rets, reflection_state_t& state,
									   std::optional<std::vector<node_id_t>>& func_node_ret) {
	if (rets.getNumOperands() > 0) {
		func_node_ret = std::vector<node_id_t> {};
	}
	for (const auto& op : rets.operands()) {
		const auto md_node = dyn_cast_or_null<llvm::MDNode>(op);
		if (!md_node || md_node->getNumOperands() < 1) {
			llvm::errs() << "reflection: invalid function return type metadata\n";
			return false;
		}
		
		const auto arg_type = dyn_cast_or_null<llvm::MDString>(md_node->getOperand(0));
		if (!arg_type) {
			llvm::errs() << "reflection: invalid function return type\n";
			return false;
		}
		
		const auto node_id = create_node(*md_node, arg_type->getString().data(), state, true /* handle as return type */);
		if (node_id.id == invalid_node_id.id) {
			return false;
		}
		func_node_ret->emplace_back(node_id);
	}
	return true;
}

template <FUNCTION_TYPE func_type, typename func_node_type>
static inline bool handle_arguments(const llvm::MDNode& args, reflection_state_t& state, func_node_type& func_node) {
	if (args.getNumOperands() > 0) {
		func_node.arguments = std::vector<node_id_t> {};
	}
	for (const auto& op : args.operands()) {
		const auto md_node = dyn_cast_or_null<llvm::MDNode>(op);
		if (!md_node || md_node->getNumOperands() < 2) {
			llvm::errs() << "reflection: invalid function argument metadata\n";
			return false;
		}
		
		const auto arg_idx = dyn_cast_or_null<llvm::ConstantAsMetadata>(md_node->getOperand(0));
		const auto arg_type = dyn_cast_or_null<llvm::MDString>(md_node->getOperand(1));
		if (!arg_idx || !arg_type) {
			llvm::errs() << "reflection: invalid function argument index/type\n";
			return false;
		}
		
		const auto node_id = create_node(*md_node, arg_type->getString().data(), state);
		if (node_id.id == invalid_node_id.id) {
			return false;
		}
		func_node.arguments->emplace_back(node_id);
	}
	return true;
}

template <FUNCTION_TYPE func_type, typename func_node_type>
static inline bool handle_func_attribute(const llvm::MDNode& md_node, reflection_state_t& state, func_node_type& func_node) {
	auto md_iter = md_node.op_begin();
	const auto attr_type_md_str = md_get_string(md_node, md_iter);
	if (!attr_type_md_str) {
		return false;
	}
	
	const auto attr_type_str = attr_type_md_str->str();
	if (attr_type_str == "air.max_work_group_size") {
		if constexpr (func_type == FUNCTION_TYPE::KERNEL) {
			const auto node_id = create_node(md_node, attr_type_str, state);
			if (node_id.id == invalid_node_id.id) {
				return false;
			}
			func_node.workgroup_max_size = node_id;
		} else {
			llvm::errs() << "reflection: invalid function type for air.max_work_group_size\n";
			return false;
		}
	} else if (attr_type_str == "air.patch") {
		if constexpr (func_type == FUNCTION_TYPE::VERTEX) {
			const auto node_id = create_node(md_node, attr_type_str, state);
			if (node_id.id == invalid_node_id.id) {
				return false;
			}
			func_node.patch = node_id;
		} else {
			llvm::errs() << "reflection: invalid function type for air.patch\n";
			return false;
		}
	} else if (attr_type_str == "early_fragment_tests") {
		if constexpr (func_type == FUNCTION_TYPE::FRAGMENT) {
			func_node.early_fragment_tests = true;
		} else {
			llvm::errs() << "reflection: invalid function type for early_fragment_tests\n";
			return false;
		}
	} else {
		llvm::errs() << "reflection: unhandled function attribute: " << attr_type_str << "\n";
		return false;
	}
	
	return true;
}

template <FUNCTION_TYPE func_type, typename func_node_type>
static inline bool handle_function_reflection(const llvm::MDNode& func_md, reflection_state_t& state,
											  std::vector<node_id_t>& refl_functions,
											  std::unique_ptr<func_node_type>&& func_node_obj) {
	if (func_md.getNumOperands() < 3) {
		llvm::errs() << "reflection: invalid function metadata count\n";
		return false;
	}
	
	auto& func_node = *func_node_obj;
	
	// function metadata
	const auto func_md_const = dyn_cast_or_null<llvm::ConstantAsMetadata>(func_md.getOperand(0));
	if (!func_md_const) {
		llvm::errs() << "reflection: invalid function metadata\n";
		return false;
	}
	const auto func = dyn_cast_or_null<llvm::Function>(func_md_const->getValue());
	if (!func) {
		llvm::errs() << "reflection: invalid function metadata\n";
		return false;
	}
	refl_functions.emplace_back(state.add_node(std::move(func_node_obj)));
	func_node.name = func->getName().str();
	
	// return type metadata
	const auto ret = dyn_cast_or_null<llvm::MDNode>(func_md.getOperand(1));
	if (ret) {
		if constexpr (func_type == FUNCTION_TYPE::FRAGMENT) {
			if (!handle_return_types<func_type>(*ret, state, func_node.return_type)) {
				llvm::errs() << "reflection: invalid function return type metadata\n";
				return false;
			}
		} else {
			if (!handle_return_types<func_type>(*ret, state, func_node.return_types)) {
				llvm::errs() << "reflection: invalid function return type metadata\n";
				return false;
			}
		}
	} else {
		llvm::errs() << "reflection: invalid function return types\n";
	}
	
	// arguments metadata
	const auto args = dyn_cast_or_null<llvm::MDNode>(func_md.getOperand(2));
	if (args) {
		if (!handle_arguments<func_type>(*args, state, func_node)) {
			llvm::errs() << "reflection: invalid function arguments metadata\n";
			return false;
		}
	} else {
		llvm::errs() << "reflection: invalid function arguments\n";
	}
	
	// optional attributes metadata
	if (func_md.getNumOperands() > 3) {
		for (auto op = func_md.op_begin() + 3; op != func_md.op_end(); ++op) {
			const auto attr_md_node = dyn_cast_or_null<const llvm::MDNode>(*op);
			if (!attr_md_node || !handle_func_attribute<func_type>(*attr_md_node, state, func_node)) {
				llvm::errs() << "reflection: invalid function attribute metadata\n";
				return false;
			}
		}
	}
	
	return true;
}

std::vector<uint8_t> create_reflection(llvm::Module& M) {
	reflection_t refl {
		.version = version_t { .major = 0, .minor = 4, .sub_minor = 0 },
	};
	reflection_state_t state { .refl = refl };
	
	// handle functions
	const std::array<std::tuple<const char*, std::optional<std::vector<node_id_t>>&, FUNCTION_TYPE>, 3> function_types {{
		{ "air.fragment", refl.fragment_functions, FUNCTION_TYPE::FRAGMENT },
		{ "air.kernel", refl.kernel_functions, FUNCTION_TYPE::KERNEL },
		{ "air.vertex", refl.vertex_functions, FUNCTION_TYPE::VERTEX },
	}};
	bool has_any_function = false;
	for (const auto& func_type : function_types) {
		auto func_md = M.getNamedMetadata(get<0>(func_type));
		if (!func_md || func_md->getNumOperands() == 0) {
			continue;
		}
		
		if (!get<1>(func_type)) {
			get<1>(func_type) = std::vector<node_id_t> {};
		}
		if (!has_any_function) {
			has_any_function = true;
			refl.nodes = std::vector<std::unique_ptr<node_base_t>> {};
		}
		for (const auto& func : func_md->operands()) {
			bool success = true;
			switch (get<2>(func_type)) {
				case FUNCTION_TYPE::VERTEX: {
					success = handle_function_reflection<FUNCTION_TYPE::VERTEX>(*func, state, *get<1>(func_type),
																				std::make_unique<node_vertex_function_t>());
					break;
				}
				case FUNCTION_TYPE::FRAGMENT: {
					success = handle_function_reflection<FUNCTION_TYPE::FRAGMENT>(*func, state, *get<1>(func_type),
																				  std::make_unique<node_fragment_function_t>());
					break;
				}
				case FUNCTION_TYPE::KERNEL: {
					success = handle_function_reflection<FUNCTION_TYPE::KERNEL>(*func, state, *get<1>(func_type),
																				std::make_unique<node_kernel_function_t>());
					break;
				}
				case FUNCTION_TYPE::UNQUALIFIED:
				case FUNCTION_TYPE::VISIBLE:
				case FUNCTION_TYPE::EXTERN:
				case FUNCTION_TYPE::INTERSECTION:
				case FUNCTION_TYPE::MESH:
				case FUNCTION_TYPE::OBJECT:
				case FUNCTION_TYPE::NONE:
					llvm_unreachable("unhandled function type");
			}
			if (!success) {
				return {};
			}
		}
	}
	
	// handle local memory allocations
	for (const auto& GV : M.globals()) {
		if (GV.getType()->getAddressSpace() != uint32_t(ADDRESS_SPACE::LOCAL)) {
			continue;
		}
		local_allocation_t alloc {
			.size = M.getDataLayout().getTypeStoreSize(GV.getValueType()).getFixedValue(),
		};
		if (GV.getAlign()) {
			alloc.alignment = GV.getAlign().getValue().value();
		}
		if (!refl.static_local_allocations) {
			refl.static_local_allocations = std::vector<local_allocation_t> {};
		}
		refl.static_local_allocations->emplace_back(alloc);
	}
	
#if METAL_REFLECTION_DUMP_STATE
	llvm::outs() << "reflection:\n";
	dump(refl, llvm::outs(), 1u);
#endif
	
	// convert to binary
	return write(refl);
}

} // namespace metal::reflection
