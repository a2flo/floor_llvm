
#pragma once

#include <string>
#include <vector>
#include <optional>
#include <span>
#include <string_view>
#include <cstdint>

namespace metal::reflection {

struct table_vtable_t {
	uint16_t vtable_size;
	uint16_t inline_data_size;
};
static_assert(sizeof(table_vtable_t) == 4);

struct node_vtable_t : table_vtable_t {
	uint16_t node_type_offset;
	uint16_t node_indirection_offset;
};
static_assert(sizeof(node_vtable_t) == 8);

struct table_root_t {
	int32_t vtable_offset;
};
static_assert(sizeof(table_root_t) == 4);

struct node_root_t : table_root_t {
	NODE_TYPE type;
	int32_t indirection_offset;
};
static_assert(sizeof(node_root_t) == 12);

static inline std::unique_ptr<node_base_t> parse_node(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data);

template <NODE_TYPE node_type>
static std::unique_ptr<node_base_t> parse(const table_root_t& root, const table_vtable_t& vtable,
										  const std::span<const uint8_t> refl_data);

static inline void parse_string(const uint8_t* str_root_ptr, const void* refl_start_ptr, const void* refl_end_ptr, auto& str_dst) {
	const auto str_ptr = str_root_ptr + *(const uint32_t*)str_root_ptr;
	if (str_ptr >= refl_start_ptr && str_ptr + sizeof(uint32_t) <= refl_end_ptr) {
		const auto str_size = *(const uint32_t*)str_ptr;
		const auto str_end_ptr = str_ptr + sizeof(uint32_t) + str_size;
		if (str_end_ptr >= refl_start_ptr && str_end_ptr <= refl_end_ptr) {
			str_dst = std::string_view { (const char*)(str_ptr + sizeof(uint32_t)), str_size };
		}
	}
}

template <typename elem_type>
static inline void parse_vector(const uint8_t* vec_root_ptr, const void* refl_start_ptr, const void* refl_end_ptr, auto& vec_dst) {
	const auto vec_ptr = vec_root_ptr + *(const uint32_t*)vec_root_ptr;
	if (vec_ptr >= refl_start_ptr && vec_ptr + sizeof(uint32_t) <= refl_end_ptr) {
		const auto elem_count = *(const uint32_t*)vec_ptr;
		const auto vec_end_ptr = vec_ptr + sizeof(uint32_t) + elem_count * sizeof(elem_type);
		if (vec_end_ptr >= refl_start_ptr && vec_end_ptr <= refl_end_ptr) {
			const std::span<const elem_type> elements { (const elem_type*)(vec_ptr + sizeof(uint32_t)), elem_count };
			vec_dst = std::vector<elem_type>(elements.begin(), elements.end());
		}
	}
}

static inline local_allocation_t parse_local_allocation(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	local_allocation_t obj {};

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		obj.size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		obj.alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}

	return obj;
}

static inline reflection_t parse_reflection(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	reflection_t obj {};

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(version_t) <= vtable.inline_data_size) {
		obj.version = *(const version_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		obj.nodes = std::vector<std::unique_ptr<node_base_t>> {};
		std::vector<uint32_t> offsets;
		const auto vec_base_ptr = &root_data[vtable_fields[3]];
		parse_vector<uint32_t>(vec_base_ptr, refl_start_ptr, refl_end_ptr, offsets);
		for (uint32_t i = 0, count = uint32_t(offsets.size()); i < count; ++i) {
			const auto root_ptr = vec_base_ptr + *(const uint32_t*)vec_base_ptr + sizeof(uint32_t) * (i + 1) + offsets[i];
			if (root_ptr < refl_start_ptr && root_ptr + sizeof(table_root_t) > refl_end_ptr) {
				continue;
			}
			const auto table_root = (const table_root_t*)root_ptr;
			const auto vtable_ptr = root_ptr - table_root->vtable_offset;
			if (vtable_ptr < refl_start_ptr && vtable_ptr + sizeof(table_vtable_t) > refl_end_ptr) {
				continue;
			}
			const auto table_vtable = (const table_vtable_t*)vtable_ptr;
			obj.nodes->emplace_back(parse_node(*table_root, *table_vtable, refl_data));
		}
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, obj.fragment_functions);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[5]], refl_start_ptr, refl_end_ptr, obj.intersection_functions);
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[6]], refl_start_ptr, refl_end_ptr, obj.kernel_functions);
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[7]], refl_start_ptr, refl_end_ptr, obj.vertex_functions);
	}
	if (vtable.vtable_size >= 18 && vtable_fields[8] != 0u &&
		vtable_fields[8] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[8]], refl_start_ptr, refl_end_ptr, obj.visible_functions);
	}
	if (vtable.vtable_size >= 20 && vtable_fields[9] != 0u &&
		vtable_fields[9] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[9]], refl_start_ptr, refl_end_ptr, obj.mesh_functions);
	}
	if (vtable.vtable_size >= 22 && vtable_fields[10] != 0u &&
		vtable_fields[10] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[10]], refl_start_ptr, refl_end_ptr, obj.object_functions);
	}
	if (vtable.vtable_size >= 24 && vtable_fields[11] != 0u &&
		vtable_fields[11] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[11]], refl_start_ptr, refl_end_ptr, obj.function_constants);
	}
	if (vtable.vtable_size >= 26 && vtable_fields[12] != 0u &&
		vtable_fields[12] + sizeof(uint32_t) <= vtable.inline_data_size) {
		obj.static_local_allocations = std::vector<local_allocation_t> {};
		std::vector<uint32_t> offsets;
		const auto vec_base_ptr = &root_data[vtable_fields[12]];
		parse_vector<uint32_t>(vec_base_ptr, refl_start_ptr, refl_end_ptr, offsets);
		for (uint32_t i = 0, count = uint32_t(offsets.size()); i < count; ++i) {
			const auto root_ptr = vec_base_ptr + *(const uint32_t*)vec_base_ptr + sizeof(uint32_t) * (i + 1) + offsets[i];
			if (root_ptr < refl_start_ptr && root_ptr + sizeof(table_root_t) > refl_end_ptr) {
				continue;
			}
			const auto table_root = (const table_root_t*)root_ptr;
			const auto vtable_ptr = root_ptr - table_root->vtable_offset;
			if (vtable_ptr < refl_start_ptr && vtable_ptr + sizeof(table_vtable_t) > refl_end_ptr) {
				continue;
			}
			const auto table_vtable = (const table_vtable_t*)vtable_ptr;
			obj.static_local_allocations->emplace_back(parse_local_allocation(*table_root, *table_vtable, refl_data));
		}
	}
	if (vtable.vtable_size >= 28 && vtable_fields[13] != 0u &&
		vtable_fields[13] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[13]], refl_start_ptr, refl_end_ptr, obj.emulations);
	}
	if (vtable.vtable_size >= 30 && vtable_fields[14] != 0u &&
		vtable_fields[14] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[14]], refl_start_ptr, refl_end_ptr, obj.global_bindings);
	}
	if (vtable.vtable_size >= 32 && vtable_fields[15] != 0u &&
		vtable_fields[15] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[15]], refl_start_ptr, refl_end_ptr, obj.visible_function_references);
	}
	if (vtable.vtable_size >= 34 && vtable_fields[16] != 0u &&
		vtable_fields[16] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[16]], refl_start_ptr, refl_end_ptr, obj.ci_functions);
	}

	return obj;
}

static inline stitching_info_t parse_stitching_info(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	stitching_info_t obj {};

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(node_id_t) <= vtable.inline_data_size) {
		obj.return_type = *(const node_id_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, obj.arguments);
	}

	return obj;
}

static inline struct_type_info_field_t parse_struct_type_info_field(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	struct_type_info_field_t obj {};

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(node_id_t) <= vtable.inline_data_size) {
		obj.struct_type_info = *(const node_id_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		obj.offset = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		obj.size = *(const uint32_t*)&root_data[vtable_fields[4]];
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(uint32_t) <= vtable.inline_data_size) {
		obj.array_entries = *(const uint32_t*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[6]], refl_start_ptr, refl_end_ptr, obj.type_name);
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[7]], refl_start_ptr, refl_end_ptr, obj.field_name);
	}
	if (vtable.vtable_size >= 18 && vtable_fields[8] != 0u &&
		vtable_fields[8] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[8]], refl_start_ptr, refl_end_ptr, obj.attribute_name);
	}
	if (vtable.vtable_size >= 20 && vtable_fields[9] != 0u &&
		vtable_fields[9] + sizeof(node_id_t) <= vtable.inline_data_size) {
		obj.indirect_argument = *(const node_id_t*)&root_data[vtable_fields[9]];
	}
	if (vtable.vtable_size >= 22 && vtable_fields[10] != 0u &&
		vtable_fields[10] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		obj.indirect_location = *(const uint_value_t*)&root_data[vtable_fields[10]];
	}
	if (vtable.vtable_size >= 24 && vtable_fields[11] != 0u &&
		vtable_fields[11] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		obj.raster_order_group = *(const uint_value_t*)&root_data[vtable_fields[11]];
	}
	if (vtable.vtable_size >= 26 && vtable_fields[12] != 0u &&
		vtable_fields[12] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		obj.render_target_index = *(const uint_value_t*)&root_data[vtable_fields[12]];
	}
	if (vtable.vtable_size >= 28 && vtable_fields[13] != 0u &&
		vtable_fields[13] + sizeof(node_id_t) <= vtable.inline_data_size) {
		obj.inline_type_info = *(const node_id_t*)&root_data[vtable_fields[13]];
	}

	return obj;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::ACCELERATION_STRUCTURE_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_acceleration_structure_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->instancing = *(const bool*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(bool) <= vtable.inline_data_size) {
		node->primitive_motion = *(const bool*)&root_data[vtable_fields[6]];
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(bool) <= vtable.inline_data_size) {
		node->instance_motion = *(const bool*)&root_data[vtable_fields[7]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::ACCEPT_INTERSECTION_RET>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_accept_intersection_ret_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::ADDRESS_SPACE_TYPE_QUAL>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_address_space_type_qual_t>();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(ADDRESS_SPACE) <= vtable.inline_data_size) {
		node->address_space = *(const ADDRESS_SPACE*)&root_data[vtable_fields[2]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::AMPLIFICATION_COUNT_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_amplification_count_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::AMPLIFICATION_ID_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_amplification_id_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::ARRAY_OF_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_array_of_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->element_type = *(const node_id_t*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->num_elements = *(const uint32_t*)&root_data[vtable_fields[6]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::ARRAY_REF_OF_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_array_ref_of_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->element_type = *(const node_id_t*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::ARRAY_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_array_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->element_type = *(const node_id_t*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->num_elements = *(const uint32_t*)&root_data[vtable_fields[6]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::BFLOAT_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_bfloat_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::BARYCENTRIC_COORD_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_barycentric_coord_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(SAMPLING_QUALIFIER) <= vtable.inline_data_size) {
		node->sampling_qualifier = *(const SAMPLING_QUALIFIER*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(INTERPOLATION_QUALIFIER) <= vtable.inline_data_size) {
		node->interpolation_qualifier = *(const INTERPOLATION_QUALIFIER*)&root_data[vtable_fields[4]];
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[5]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[6]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[7]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::BASE_INSTANCE_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_base_instance_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::BASE_VERTEX_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_base_vertex_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::BOOL_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_bool_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::BUFFER_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_buffer_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->buffer_size = *(const uint_value_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->location_index = *(const uint_value_t*)&root_data[vtable_fields[4]];
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->location_count = *(const uint_value_t*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(ACCESS_QUALIFIER) <= vtable.inline_data_size) {
		node->access_qualifier = *(const ACCESS_QUALIFIER*)&root_data[vtable_fields[6]];
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(ADDRESS_SPACE) <= vtable.inline_data_size) {
		node->address_space = *(const ADDRESS_SPACE*)&root_data[vtable_fields[7]];
	}
	if (vtable.vtable_size >= 18 && vtable_fields[8] != 0u &&
		vtable_fields[8] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->struct_type_info = *(const node_id_t*)&root_data[vtable_fields[8]];
	}
	if (vtable.vtable_size >= 20 && vtable_fields[9] != 0u &&
		vtable_fields[9] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->raster_order_group = *(const uint_value_t*)&root_data[vtable_fields[9]];
	}
	if (vtable.vtable_size >= 22 && vtable_fields[10] != 0u &&
		vtable_fields[10] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->type_size = *(const uint_value_t*)&root_data[vtable_fields[10]];
	}
	if (vtable.vtable_size >= 24 && vtable_fields[11] != 0u &&
		vtable_fields[11] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->type_align = *(const uint_value_t*)&root_data[vtable_fields[11]];
	}
	if (vtable.vtable_size >= 26 && vtable_fields[12] != 0u &&
		vtable_fields[12] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[12]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 28 && vtable_fields[13] != 0u &&
		vtable_fields[13] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[13]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 30 && vtable_fields[14] != 0u &&
		vtable_fields[14] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[14]];
	}
	if (vtable.vtable_size >= 32 && vtable_fields[15] != 0u &&
		vtable_fields[15] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->inline_type_info = *(const node_id_t*)&root_data[vtable_fields[15]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::BUFFER_STRIDE_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_buffer_stride_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->location_index = *(const uint_value_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->location_count = *(const uint_value_t*)&root_data[vtable_fields[4]];
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[5]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[6]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[7]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::CIARRAY_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_ciarray_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->inline_type_info = *(const node_id_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->struct_type_info = *(const node_id_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->type_size = *(const uint_value_t*)&root_data[vtable_fields[4]];
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->type_align = *(const uint_value_t*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[6]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[7]], refl_start_ptr, refl_end_ptr, node->name);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::CIBUILTIN_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_cibuiltin_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[2]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->name);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::CIBUILTIN_RET>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_cibuiltin_ret_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[2]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->name);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::CI_FUNCTION>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_ci_function_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[2]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->return_types);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->arguments);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->user_annotation = *(const node_id_t*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::CIIMAGEBLOCK_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_ciimageblock_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[2]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->name);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::CIIMAGEBLOCK_RET>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_ciimageblock_ret_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[2]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->name);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::CIMATRIX_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_cimatrix_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[2]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->name);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::CIMATRIX_RET>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_cimatrix_ret_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[2]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->name);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::CIPADDING_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_cipadding_arg_t>();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };


	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::CIPOINTER_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_cipointer_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(ACCESS_QUALIFIER) <= vtable.inline_data_size) {
		node->access_qualifier = *(const ACCESS_QUALIFIER*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(ADDRESS_SPACE) <= vtable.inline_data_size) {
		node->address_space = *(const ADDRESS_SPACE*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->inline_type_info = *(const node_id_t*)&root_data[vtable_fields[4]];
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->struct_type_info = *(const node_id_t*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->type_size = *(const uint_value_t*)&root_data[vtable_fields[6]];
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->type_align = *(const uint_value_t*)&root_data[vtable_fields[7]];
	}
	if (vtable.vtable_size >= 18 && vtable_fields[8] != 0u &&
		vtable_fields[8] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[8]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 20 && vtable_fields[9] != 0u &&
		vtable_fields[9] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[9]], refl_start_ptr, refl_end_ptr, node->name);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::CIPOINTER_RET>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_cipointer_ret_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(ACCESS_QUALIFIER) <= vtable.inline_data_size) {
		node->access_qualifier = *(const ACCESS_QUALIFIER*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(ADDRESS_SPACE) <= vtable.inline_data_size) {
		node->address_space = *(const ADDRESS_SPACE*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->inline_type_info = *(const node_id_t*)&root_data[vtable_fields[4]];
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->struct_type_info = *(const node_id_t*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->type_size = *(const uint_value_t*)&root_data[vtable_fields[6]];
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->type_align = *(const uint_value_t*)&root_data[vtable_fields[7]];
	}
	if (vtable.vtable_size >= 18 && vtable_fields[8] != 0u &&
		vtable_fields[8] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[8]], refl_start_ptr, refl_end_ptr, node->type_name);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::CISAMPLER_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_cisampler_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[2]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->name);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::CISAMPLER_RET>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_cisampler_ret_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[2]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->name);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::CISTRUCT_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_cistruct_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->struct_type_info = *(const node_id_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::CISTRUCT_RET>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_cistruct_ret_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->struct_type_info = *(const node_id_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::CITEXTURE_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_citexture_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->location_count = *(const uint_value_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(ACCESS_QUALIFIER) <= vtable.inline_data_size) {
		node->access_qualifier = *(const ACCESS_QUALIFIER*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[5]], refl_start_ptr, refl_end_ptr, node->name);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::CITEXTURE_RET>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_citexture_ret_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->location_count = *(const uint_value_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(ACCESS_QUALIFIER) <= vtable.inline_data_size) {
		node->access_qualifier = *(const ACCESS_QUALIFIER*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->type_name);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::CHAR_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_char_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::CLIP_DISTANCE_ATTR>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_clip_distance_attr_t>();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };


	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::CLIP_DISTANCE_RET>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_clip_distance_ret_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->array_size = *(const uint_value_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(bool) <= vtable.inline_data_size) {
		node->shared = *(const bool*)&root_data[vtable_fields[4]];
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[5]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[6]], refl_start_ptr, refl_end_ptr, node->name);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::COMMAND_BUFFER_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_command_buffer_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->location_index = *(const uint_value_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->location_count = *(const uint_value_t*)&root_data[vtable_fields[4]];
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[5]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[6]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[7]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::COMMAND_BUFFER_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_command_buffer_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::COMPUTE_PIPELINE_STATE_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_compute_pipeline_state_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->location_index = *(const uint_value_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->location_count = *(const uint_value_t*)&root_data[vtable_fields[4]];
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[5]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[6]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[7]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::COMPUTE_PIPELINE_STATE_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_compute_pipeline_state_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::CONSTANT_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_constant_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->location_index = *(const uint_value_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->location_count = *(const uint_value_t*)&root_data[vtable_fields[4]];
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->struct_type_info = *(const node_id_t*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->type_size = *(const uint_value_t*)&root_data[vtable_fields[6]];
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->type_align = *(const uint_value_t*)&root_data[vtable_fields[7]];
	}
	if (vtable.vtable_size >= 18 && vtable_fields[8] != 0u &&
		vtable_fields[8] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[8]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 20 && vtable_fields[9] != 0u &&
		vtable_fields[9] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[9]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 22 && vtable_fields[10] != 0u &&
		vtable_fields[10] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[10]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::CONTINUE_SEARCH_RET>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_continue_search_ret_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::CONTROL_POINT_FIELD>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_control_point_field_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->location_index = *(const uint_value_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->location_count = *(const uint_value_t*)&root_data[vtable_fields[4]];
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[5]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[6]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[7]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::CONTROL_POINT_INDEX_BUFFER_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_control_point_index_buffer_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::CONTROL_POINT_INPUT_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_control_point_input_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->fields);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[4]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::CURVE_PARAMETER_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_curve_parameter_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::DEPTH2D_ARRAY_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_depth2d_array_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->channel_type = *(const node_id_t*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(ACCESS_QUALIFIER) <= vtable.inline_data_size) {
		node->access_qualifier = *(const ACCESS_QUALIFIER*)&root_data[vtable_fields[6]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::DEPTH2D_MS_ARRAY_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_depth2d_ms_array_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->channel_type = *(const node_id_t*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(ACCESS_QUALIFIER) <= vtable.inline_data_size) {
		node->access_qualifier = *(const ACCESS_QUALIFIER*)&root_data[vtable_fields[6]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::DEPTH2D_MS_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_depth2d_ms_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->channel_type = *(const node_id_t*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(ACCESS_QUALIFIER) <= vtable.inline_data_size) {
		node->access_qualifier = *(const ACCESS_QUALIFIER*)&root_data[vtable_fields[6]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::DEPTH2D_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_depth2d_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->channel_type = *(const node_id_t*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(ACCESS_QUALIFIER) <= vtable.inline_data_size) {
		node->access_qualifier = *(const ACCESS_QUALIFIER*)&root_data[vtable_fields[6]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::DEPTH_CUBE_ARRAY_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_depth_cube_array_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->channel_type = *(const node_id_t*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(ACCESS_QUALIFIER) <= vtable.inline_data_size) {
		node->access_qualifier = *(const ACCESS_QUALIFIER*)&root_data[vtable_fields[6]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::DEPTH_CUBE_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_depth_cube_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->channel_type = *(const node_id_t*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(ACCESS_QUALIFIER) <= vtable.inline_data_size) {
		node->access_qualifier = *(const ACCESS_QUALIFIER*)&root_data[vtable_fields[6]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::DEPTH_RET>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_depth_ret_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(DEPTH_QUALIFIER) <= vtable.inline_data_size) {
		node->depth_qualifier = *(const DEPTH_QUALIFIER*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[5]], refl_start_ptr, refl_end_ptr, node->name);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::DEPTH_STENCIL_STATE_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_depth_stencil_state_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->location_index = *(const uint_value_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->location_count = *(const uint_value_t*)&root_data[vtable_fields[4]];
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[5]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[6]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[7]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::DEPTH_STENCIL_STATE_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_depth_stencil_state_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::DIRECTION_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_direction_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::DISPATCH_QUADGROUPS_PER_THREADGROUP_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_dispatch_quadgroups_per_threadgroup_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::DISPATCH_SIMDGROUPS_PER_THREADGROUP_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_dispatch_simdgroups_per_threadgroup_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::DISPATCH_THREADS_PER_THREADGROUP_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_dispatch_threads_per_threadgroup_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::DISTANCE_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_distance_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::DISTANCE_RET>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_distance_ret_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::DOUBLE_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_double_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::ENUM_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_enum_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[5]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->underlying_type = *(const node_id_t*)&root_data[vtable_fields[6]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::EXTENTS_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_extents_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->index_type = *(const node_id_t*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<uint64_t>(&root_data[vtable_fields[6]], refl_start_ptr, refl_end_ptr, node->extents);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::FLOAT_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_float_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::FRAGMENT_FUNCTION>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_fragment_function_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[2]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->return_type);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->arguments);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->early_fragment_tests = *(const bool*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->user_annotation = *(const node_id_t*)&root_data[vtable_fields[6]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::FRAGMENT_INPUT_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_fragment_input_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->attribute_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->location = *(const uint_value_t*)&root_data[vtable_fields[4]];
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(SAMPLING_QUALIFIER) <= vtable.inline_data_size) {
		node->sampling_qualifier = *(const SAMPLING_QUALIFIER*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(INTERPOLATION_QUALIFIER) <= vtable.inline_data_size) {
		node->interpolation_qualifier = *(const INTERPOLATION_QUALIFIER*)&root_data[vtable_fields[6]];
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[7]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 18 && vtable_fields[8] != 0u &&
		vtable_fields[8] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[8]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 20 && vtable_fields[9] != 0u &&
		vtable_fields[9] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[9]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::FRONT_FACING_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_front_facing_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::FUNCTION_CONSTANT>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_function_constant_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[2]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->index = *(const uint32_t*)&root_data[vtable_fields[4]];
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->required = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::FUNCTION_CONSTANT_PREDICATE_ATTR>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_function_constant_predicate_attr_t>();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool_value_t) <= vtable.inline_data_size) {
		node->predicate = *(const bool_value_t*)&root_data[vtable_fields[2]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::FUNCTION_HANDLE_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_function_handle_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->location_index = *(const uint_value_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->location_count = *(const uint_value_t*)&root_data[vtable_fields[4]];
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[5]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[6]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[7]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::FUNCTION_HANDLE_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_function_handle_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::FUNCTION_ID_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_function_id_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::FUNCTION_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_function_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->return_type = *(const node_id_t*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[6]], refl_start_ptr, refl_end_ptr, node->param_types);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::GEOMETRY_ID_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_geometry_id_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::GEOMETRY_INTERSECTION_FUNCTION_TABLE_OFFSET_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_geometry_intersection_function_table_offset_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::GLOBAL_BINDING>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_global_binding_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[2]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->argument = *(const node_id_t*)&root_data[vtable_fields[3]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::HALF_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_half_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::IMAGEBLOCK_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_imageblock_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->data_size = *(const uint_value_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->struct_type_info = *(const node_id_t*)&root_data[vtable_fields[4]];
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->alias_all_render_targets = *(const bool*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->alias_render_target_index = *(const uint_value_t*)&root_data[vtable_fields[6]];
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->type_align = *(const uint_value_t*)&root_data[vtable_fields[7]];
	}
	if (vtable.vtable_size >= 18 && vtable_fields[8] != 0u &&
		vtable_fields[8] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[8]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 20 && vtable_fields[9] != 0u &&
		vtable_fields[9] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[9]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 22 && vtable_fields[10] != 0u &&
		vtable_fields[10] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[10]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::IMAGEBLOCK_DATA_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_imageblock_data_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->data_size = *(const uint_value_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->struct_type_info = *(const node_id_t*)&root_data[vtable_fields[4]];
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->master = *(const node_id_t*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(bool) <= vtable.inline_data_size) {
		node->alias_all_render_targets = *(const bool*)&root_data[vtable_fields[6]];
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->alias_render_target_index = *(const uint_value_t*)&root_data[vtable_fields[7]];
	}
	if (vtable.vtable_size >= 18 && vtable_fields[8] != 0u &&
		vtable_fields[8] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->type_align = *(const uint_value_t*)&root_data[vtable_fields[8]];
	}
	if (vtable.vtable_size >= 20 && vtable_fields[9] != 0u &&
		vtable_fields[9] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[9]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 22 && vtable_fields[10] != 0u &&
		vtable_fields[10] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[10]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 24 && vtable_fields[11] != 0u &&
		vtable_fields[11] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[11]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::IMAGEBLOCK_DATA_RET>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_imageblock_data_ret_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->data_size = *(const uint_value_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->struct_type_info = *(const node_id_t*)&root_data[vtable_fields[4]];
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->master = *(const node_id_t*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(bool) <= vtable.inline_data_size) {
		node->alias_all_render_targets = *(const bool*)&root_data[vtable_fields[6]];
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->alias_render_target_index = *(const uint_value_t*)&root_data[vtable_fields[7]];
	}
	if (vtable.vtable_size >= 18 && vtable_fields[8] != 0u &&
		vtable_fields[8] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->type_align = *(const uint_value_t*)&root_data[vtable_fields[8]];
	}
	if (vtable.vtable_size >= 20 && vtable_fields[9] != 0u &&
		vtable_fields[9] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[9]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 22 && vtable_fields[10] != 0u &&
		vtable_fields[10] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[10]], refl_start_ptr, refl_end_ptr, node->name);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::IMAGEBLOCK_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_imageblock_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(IMAGEBLOCK_LAYOUT) <= vtable.inline_data_size) {
		node->layout = *(const IMAGEBLOCK_LAYOUT*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->data_type = *(const node_id_t*)&root_data[vtable_fields[6]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::INDIRECT_BUFFER_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_indirect_buffer_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->buffer_size = *(const uint_value_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->location_index = *(const uint_value_t*)&root_data[vtable_fields[4]];
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->location_count = *(const uint_value_t*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(ACCESS_QUALIFIER) <= vtable.inline_data_size) {
		node->access_qualifier = *(const ACCESS_QUALIFIER*)&root_data[vtable_fields[6]];
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(ADDRESS_SPACE) <= vtable.inline_data_size) {
		node->address_space = *(const ADDRESS_SPACE*)&root_data[vtable_fields[7]];
	}
	if (vtable.vtable_size >= 18 && vtable_fields[8] != 0u &&
		vtable_fields[8] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->struct_type_info = *(const node_id_t*)&root_data[vtable_fields[8]];
	}
	if (vtable.vtable_size >= 20 && vtable_fields[9] != 0u &&
		vtable_fields[9] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->raster_order_group = *(const uint_value_t*)&root_data[vtable_fields[9]];
	}
	if (vtable.vtable_size >= 22 && vtable_fields[10] != 0u &&
		vtable_fields[10] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->type_size = *(const uint_value_t*)&root_data[vtable_fields[10]];
	}
	if (vtable.vtable_size >= 24 && vtable_fields[11] != 0u &&
		vtable_fields[11] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->type_align = *(const uint_value_t*)&root_data[vtable_fields[11]];
	}
	if (vtable.vtable_size >= 26 && vtable_fields[12] != 0u &&
		vtable_fields[12] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[12]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 28 && vtable_fields[13] != 0u &&
		vtable_fields[13] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[13]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 30 && vtable_fields[14] != 0u &&
		vtable_fields[14] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[14]];
	}
	if (vtable.vtable_size >= 32 && vtable_fields[15] != 0u &&
		vtable_fields[15] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->inline_type_info = *(const node_id_t*)&root_data[vtable_fields[15]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::INDIRECT_CONSTANT_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_indirect_constant_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->location_index = *(const uint_value_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->location_count = *(const uint_value_t*)&root_data[vtable_fields[4]];
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[5]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[6]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[7]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::INLINE_TYPE_INFO>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_inline_type_info_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(ADDRESS_SPACE) <= vtable.inline_data_size) {
		node->address_space = *(const ADDRESS_SPACE*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->inline_type_info = *(const node_id_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->struct_type_info = *(const node_id_t*)&root_data[vtable_fields[4]];
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[6]];
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->array_entries = *(const uint32_t*)&root_data[vtable_fields[7]];
	}
	if (vtable.vtable_size >= 18 && vtable_fields[8] != 0u &&
		vtable_fields[8] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[8]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 20 && vtable_fields[9] != 0u &&
		vtable_fields[9] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->indirect_argument = *(const node_id_t*)&root_data[vtable_fields[9]];
	}
	if (vtable.vtable_size >= 22 && vtable_fields[10] != 0u &&
		vtable_fields[10] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->indirect_location = *(const uint_value_t*)&root_data[vtable_fields[10]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::INSTANCE_ACCELERATION_STRUCTURE_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_instance_acceleration_structure_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->location_index = *(const uint_value_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->location_count = *(const uint_value_t*)&root_data[vtable_fields[4]];
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(ACCESS_QUALIFIER) <= vtable.inline_data_size) {
		node->access_qualifier = *(const ACCESS_QUALIFIER*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->raster_order_group = *(const uint_value_t*)&root_data[vtable_fields[6]];
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[7]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 18 && vtable_fields[8] != 0u &&
		vtable_fields[8] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[8]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 20 && vtable_fields[9] != 0u &&
		vtable_fields[9] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[9]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::INSTANCE_ID_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_instance_id_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::INSTANCE_ID_COUNT_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_instance_id_count_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::INSTANCE_INTERSECTION_FUNCTION_TABLE_OFFSET_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_instance_intersection_function_table_offset_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::INT_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_int_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::INTERPOLANT_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_interpolant_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->perspective = *(const bool*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->value_type = *(const node_id_t*)&root_data[vtable_fields[6]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::INTERSECTION_FUNCTION>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_intersection_function_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[2]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->return_types);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->arguments);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(PRIMITIVE_KIND) <= vtable.inline_data_size) {
		node->primitive_kind = *(const PRIMITIVE_KIND*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(bool) <= vtable.inline_data_size) {
		node->instancing = *(const bool*)&root_data[vtable_fields[6]];
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(bool) <= vtable.inline_data_size) {
		node->triangle_data = *(const bool*)&root_data[vtable_fields[7]];
	}
	if (vtable.vtable_size >= 18 && vtable_fields[8] != 0u &&
		vtable_fields[8] + sizeof(bool) <= vtable.inline_data_size) {
		node->world_space_data = *(const bool*)&root_data[vtable_fields[8]];
	}
	if (vtable.vtable_size >= 20 && vtable_fields[9] != 0u &&
		vtable_fields[9] + sizeof(bool) <= vtable.inline_data_size) {
		node->primitive_motion = *(const bool*)&root_data[vtable_fields[9]];
	}
	if (vtable.vtable_size >= 22 && vtable_fields[10] != 0u &&
		vtable_fields[10] + sizeof(bool) <= vtable.inline_data_size) {
		node->instance_motion = *(const bool*)&root_data[vtable_fields[10]];
	}
	if (vtable.vtable_size >= 24 && vtable_fields[11] != 0u &&
		vtable_fields[11] + sizeof(bool) <= vtable.inline_data_size) {
		node->extended_limits = *(const bool*)&root_data[vtable_fields[11]];
	}
	if (vtable.vtable_size >= 26 && vtable_fields[12] != 0u &&
		vtable_fields[12] + sizeof(bool) <= vtable.inline_data_size) {
		node->curve_data = *(const bool*)&root_data[vtable_fields[12]];
	}
	if (vtable.vtable_size >= 28 && vtable_fields[13] != 0u &&
		vtable_fields[13] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->multi_level_instancing = *(const uint32_t*)&root_data[vtable_fields[13]];
	}
	if (vtable.vtable_size >= 30 && vtable_fields[14] != 0u &&
		vtable_fields[14] + sizeof(bool) <= vtable.inline_data_size) {
		node->intersection_function_buffer = *(const bool*)&root_data[vtable_fields[14]];
	}
	if (vtable.vtable_size >= 32 && vtable_fields[15] != 0u &&
		vtable_fields[15] + sizeof(bool) <= vtable.inline_data_size) {
		node->user_data = *(const bool*)&root_data[vtable_fields[15]];
	}
	if (vtable.vtable_size >= 34 && vtable_fields[16] != 0u &&
		vtable_fields[16] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->user_annotation = *(const node_id_t*)&root_data[vtable_fields[16]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::INTERSECTION_FUNCTION_HANDLE_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_intersection_function_handle_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->intersection_function_buffer = *(const bool*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(bool) <= vtable.inline_data_size) {
		node->instancing = *(const bool*)&root_data[vtable_fields[6]];
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->multi_level_instancing = *(const uint32_t*)&root_data[vtable_fields[7]];
	}
	if (vtable.vtable_size >= 18 && vtable_fields[8] != 0u &&
		vtable_fields[8] + sizeof(bool) <= vtable.inline_data_size) {
		node->triangle_data = *(const bool*)&root_data[vtable_fields[8]];
	}
	if (vtable.vtable_size >= 20 && vtable_fields[9] != 0u &&
		vtable_fields[9] + sizeof(bool) <= vtable.inline_data_size) {
		node->curve_data = *(const bool*)&root_data[vtable_fields[9]];
	}
	if (vtable.vtable_size >= 22 && vtable_fields[10] != 0u &&
		vtable_fields[10] + sizeof(bool) <= vtable.inline_data_size) {
		node->world_space_data = *(const bool*)&root_data[vtable_fields[10]];
	}
	if (vtable.vtable_size >= 24 && vtable_fields[11] != 0u &&
		vtable_fields[11] + sizeof(bool) <= vtable.inline_data_size) {
		node->user_data = *(const bool*)&root_data[vtable_fields[11]];
	}
	if (vtable.vtable_size >= 26 && vtable_fields[12] != 0u &&
		vtable_fields[12] + sizeof(bool) <= vtable.inline_data_size) {
		node->primitive_motion = *(const bool*)&root_data[vtable_fields[12]];
	}
	if (vtable.vtable_size >= 28 && vtable_fields[13] != 0u &&
		vtable_fields[13] + sizeof(bool) <= vtable.inline_data_size) {
		node->instance_motion = *(const bool*)&root_data[vtable_fields[13]];
	}
	if (vtable.vtable_size >= 30 && vtable_fields[14] != 0u &&
		vtable_fields[14] + sizeof(bool) <= vtable.inline_data_size) {
		node->extended_limits = *(const bool*)&root_data[vtable_fields[14]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::INTERSECTION_FUNCTION_TABLE_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_intersection_function_table_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->location_index = *(const uint_value_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->location_count = *(const uint_value_t*)&root_data[vtable_fields[4]];
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(ACCESS_QUALIFIER) <= vtable.inline_data_size) {
		node->access_qualifier = *(const ACCESS_QUALIFIER*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->raster_order_group = *(const uint_value_t*)&root_data[vtable_fields[6]];
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[7]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 18 && vtable_fields[8] != 0u &&
		vtable_fields[8] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[8]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 20 && vtable_fields[9] != 0u &&
		vtable_fields[9] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[9]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::INTERSECTION_FUNCTION_TABLE_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_intersection_function_table_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->instancing = *(const bool*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(bool) <= vtable.inline_data_size) {
		node->triangle_data = *(const bool*)&root_data[vtable_fields[6]];
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(bool) <= vtable.inline_data_size) {
		node->world_space_data = *(const bool*)&root_data[vtable_fields[7]];
	}
	if (vtable.vtable_size >= 18 && vtable_fields[8] != 0u &&
		vtable_fields[8] + sizeof(bool) <= vtable.inline_data_size) {
		node->primitive_motion = *(const bool*)&root_data[vtable_fields[8]];
	}
	if (vtable.vtable_size >= 20 && vtable_fields[9] != 0u &&
		vtable_fields[9] + sizeof(bool) <= vtable.inline_data_size) {
		node->instance_motion = *(const bool*)&root_data[vtable_fields[9]];
	}
	if (vtable.vtable_size >= 22 && vtable_fields[10] != 0u &&
		vtable_fields[10] + sizeof(bool) <= vtable.inline_data_size) {
		node->extended_limits = *(const bool*)&root_data[vtable_fields[10]];
	}
	if (vtable.vtable_size >= 24 && vtable_fields[11] != 0u &&
		vtable_fields[11] + sizeof(bool) <= vtable.inline_data_size) {
		node->curve_data = *(const bool*)&root_data[vtable_fields[11]];
	}
	if (vtable.vtable_size >= 26 && vtable_fields[12] != 0u &&
		vtable_fields[12] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->multi_level_instancing = *(const uint32_t*)&root_data[vtable_fields[12]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::INVARIANT_ATTR>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_invariant_attr_t>();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };


	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::KERNEL_FUNCTION>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_kernel_function_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[2]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->return_types);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->arguments);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->vec_type_hint = *(const node_id_t*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->workgroup_size = *(const node_id_t*)&root_data[vtable_fields[6]];
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->workgroup_size_hint = *(const node_id_t*)&root_data[vtable_fields[7]];
	}
	if (vtable.vtable_size >= 18 && vtable_fields[8] != 0u &&
		vtable_fields[8] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->workgroup_max_size = *(const node_id_t*)&root_data[vtable_fields[8]];
	}
	if (vtable.vtable_size >= 20 && vtable_fields[9] != 0u &&
		vtable_fields[9] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->user_annotation = *(const node_id_t*)&root_data[vtable_fields[9]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::KEY_FRAME_COUNT_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_key_frame_count_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::LLONG_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_llong_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::LVALUE_REFERENCE_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_lvalue_reference_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->pointee_type = *(const node_id_t*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::LOCATION_INDEX_ATTR>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_location_index_attr_t>();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->index = *(const uint_value_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->count = *(const uint_value_t*)&root_data[vtable_fields[3]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::LONG_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_long_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::MATRIX_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_matrix_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->element_type = *(const node_id_t*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->num_columns = *(const uint32_t*)&root_data[vtable_fields[6]];
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->num_rows = *(const uint32_t*)&root_data[vtable_fields[7]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::MAX_DISTANCE_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_max_distance_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::MAX_MESH_WORKGROUPS_FN_ATTR>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_max_mesh_workgroups_fn_attr_t>();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->workgroups = *(const uint_value_t*)&root_data[vtable_fields[2]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::MESH_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_mesh_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->mesh_type_info = *(const node_id_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[5]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[6]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::MESH_EMULATION_BLOCK>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_mesh_emulation_block_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[2]], refl_start_ptr, refl_end_ptr, node->value_groups);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::MESH_EMULATION_FRAGMENT_ANALYSIS_RESULT>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_mesh_emulation_fragment_analysis_result_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[2]], refl_start_ptr, refl_end_ptr, node->function);
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->used_inputs = std::vector<std::string> {};
		std::vector<uint32_t> offsets;
		const auto vec_base_ptr = &root_data[vtable_fields[3]];
		parse_vector<uint32_t>(vec_base_ptr, refl_start_ptr, refl_end_ptr, offsets);
		for (uint32_t i = 0, count = uint32_t(offsets.size()); i < count; ++i) {
			const auto root_ptr = vec_base_ptr + *(const uint32_t*)vec_base_ptr + sizeof(uint32_t) * (i + 1) + offsets[i];
			if (root_ptr < refl_start_ptr && root_ptr + sizeof(uint32_t) > refl_end_ptr) {
				continue;
			}
			std::string tmp_str;
			parse_string(root_ptr, refl_start_ptr, refl_end_ptr, tmp_str);
			node->used_inputs->emplace_back(std::move(tmp_str));
		}
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::MESH_EMULATION_MESH_KERNEL>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_mesh_emulation_mesh_kernel_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[2]], refl_start_ptr, refl_end_ptr, node->function);
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->emulation_buffer_index = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->layout = *(const node_id_t*)&root_data[vtable_fields[4]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::MESH_EMULATION_MESH_LAYOUT>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_mesh_emulation_mesh_layout_t>();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->max_vertices = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->max_primitives = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->max_indices = *(const uint32_t*)&root_data[vtable_fields[4]];
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->max_indices_padding = *(const uint32_t*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->vertices_primitives_block = *(const node_id_t*)&root_data[vtable_fields[6]];
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->primitive_culled_block = *(const node_id_t*)&root_data[vtable_fields[7]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::MESH_EMULATION_MESH_VERTEX>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_mesh_emulation_mesh_vertex_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[2]], refl_start_ptr, refl_end_ptr, node->function);
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->emulation_buffer_index = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->layout = *(const node_id_t*)&root_data[vtable_fields[4]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::MESH_EMULATION_OBJECT_KERNEL>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_mesh_emulation_object_kernel_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[2]], refl_start_ptr, refl_end_ptr, node->function);
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->emulation_buffer_index = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->max_mesh_workgroups = *(const node_id_t*)&root_data[vtable_fields[4]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::MESH_EMULATION_VALUE_GROUP>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_mesh_emulation_value_group_t>();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->value_alignment = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->value_size = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->max_value_count = *(const uint32_t*)&root_data[vtable_fields[4]];
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->member_type = *(const node_id_t*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->member_index = *(const uint32_t*)&root_data[vtable_fields[6]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::MESH_FUNCTION>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_mesh_function_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[2]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->return_types);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->arguments);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->workgroup_max_size = *(const node_id_t*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->user_annotation = *(const node_id_t*)&root_data[vtable_fields[6]];
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->workgroup_size = *(const node_id_t*)&root_data[vtable_fields[7]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::MESH_GRID_PROPERTIES_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_mesh_grid_properties_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::MESH_GRID_PROPERTIES_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_mesh_grid_properties_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::MESH_PRIMITIVE_DATA_RET>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_mesh_primitive_data_ret_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->id = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->attribute_name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->shared = *(const bool*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[6]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[7]], refl_start_ptr, refl_end_ptr, node->name);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::MESH_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_mesh_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->vertex_type = *(const node_id_t*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->primitive_type = *(const node_id_t*)&root_data[vtable_fields[6]];
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->max_vertices = *(const uint32_t*)&root_data[vtable_fields[7]];
	}
	if (vtable.vtable_size >= 18 && vtable_fields[8] != 0u &&
		vtable_fields[8] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->max_primitives = *(const uint32_t*)&root_data[vtable_fields[8]];
	}
	if (vtable.vtable_size >= 20 && vtable_fields[9] != 0u &&
		vtable_fields[9] + sizeof(TOPOLOGY) <= vtable.inline_data_size) {
		node->topology = *(const TOPOLOGY*)&root_data[vtable_fields[9]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::MESH_TYPE_INFO>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_mesh_type_info_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[2]], refl_start_ptr, refl_end_ptr, node->vertex_types);
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->primitive_types);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->max_vertices = *(const uint32_t*)&root_data[vtable_fields[4]];
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->max_primitives = *(const uint32_t*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(TOPOLOGY) <= vtable.inline_data_size) {
		node->topology = *(const TOPOLOGY*)&root_data[vtable_fields[6]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::MESH_VERTEX_DATA_RET>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_mesh_vertex_data_ret_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->id = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->attribute_name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->shared = *(const bool*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[6]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[7]], refl_start_ptr, refl_end_ptr, node->name);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::MIN_DISTANCE_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_min_distance_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::MOTION_END_TIME_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_motion_end_time_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::MOTION_START_TIME_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_motion_start_time_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::OBJECT_FUNCTION>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_object_function_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[2]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->return_types);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->arguments);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->workgroup_max_size = *(const node_id_t*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->max_mesh_workgroups = *(const node_id_t*)&root_data[vtable_fields[6]];
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->user_annotation = *(const node_id_t*)&root_data[vtable_fields[7]];
	}
	if (vtable.vtable_size >= 18 && vtable_fields[8] != 0u &&
		vtable_fields[8] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->workgroup_size = *(const node_id_t*)&root_data[vtable_fields[8]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::OBJECT_TO_WORLD_TRANSFORM_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_object_to_world_transform_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::OPAQUE_PRIMITIVE_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_opaque_primitive_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::OPAQUE_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_opaque_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[5]], refl_start_ptr, refl_end_ptr, node->name);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::ORIGIN_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_origin_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::PACKED_VECTOR_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_packed_vector_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->element_type = *(const node_id_t*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->num_elements = *(const uint32_t*)&root_data[vtable_fields[6]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::PATCH_CONTROL_POINT_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_patch_control_point_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->control_point_type = *(const node_id_t*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::PATCH_FN_ATTR>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_patch_fn_attr_t>();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(PATCH_KIND) <= vtable.inline_data_size) {
		node->kind = *(const PATCH_KIND*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->control_points = *(const uint_value_t*)&root_data[vtable_fields[3]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::PATCH_ID_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_patch_id_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::PATCH_INPUT_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_patch_input_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->location_index = *(const uint_value_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->location_count = *(const uint_value_t*)&root_data[vtable_fields[4]];
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[5]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[6]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[7]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::PAYLOAD_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_payload_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->struct_type_info = *(const node_id_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->type_size = *(const uint_value_t*)&root_data[vtable_fields[4]];
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->type_align = *(const uint_value_t*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[6]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[7]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 18 && vtable_fields[8] != 0u &&
		vtable_fields[8] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[8]];
	}
	if (vtable.vtable_size >= 20 && vtable_fields[9] != 0u &&
		vtable_fields[9] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->inline_type_info = *(const node_id_t*)&root_data[vtable_fields[9]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::PIXEL_POSITION_IN_TILE_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_pixel_position_in_tile_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::PIXELS_PER_TILE_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_pixels_per_tile_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::POINT_COORD_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_point_coord_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::POINT_SIZE_ATTR>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_point_size_attr_t>();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };


	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::POINT_SIZE_RET>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_point_size_ret_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(bool) <= vtable.inline_data_size) {
		node->shared = *(const bool*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[5]], refl_start_ptr, refl_end_ptr, node->name);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::POINTER_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_pointer_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->pointee_type = *(const node_id_t*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::POSITION_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_position_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(SAMPLING_QUALIFIER) <= vtable.inline_data_size) {
		node->sampling_qualifier = *(const SAMPLING_QUALIFIER*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(INTERPOLATION_QUALIFIER) <= vtable.inline_data_size) {
		node->interpolation_qualifier = *(const INTERPOLATION_QUALIFIER*)&root_data[vtable_fields[4]];
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[5]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[6]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[7]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::POSITION_ATTR>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_position_attr_t>();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };


	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::POSITION_IN_PATCH_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_position_in_patch_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::POSITION_RET>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_position_ret_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(bool) <= vtable.inline_data_size) {
		node->invariant = *(const bool*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(bool) <= vtable.inline_data_size) {
		node->shared = *(const bool*)&root_data[vtable_fields[4]];
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[5]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[6]], refl_start_ptr, refl_end_ptr, node->name);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::PRIMITIVE_ACCELERATION_STRUCTURE_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_primitive_acceleration_structure_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->location_index = *(const uint_value_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->location_count = *(const uint_value_t*)&root_data[vtable_fields[4]];
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(ACCESS_QUALIFIER) <= vtable.inline_data_size) {
		node->access_qualifier = *(const ACCESS_QUALIFIER*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->raster_order_group = *(const uint_value_t*)&root_data[vtable_fields[6]];
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[7]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 18 && vtable_fields[8] != 0u &&
		vtable_fields[8] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[8]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 20 && vtable_fields[9] != 0u &&
		vtable_fields[9] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[9]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::PRIMITIVE_CULLED_ATTR>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_primitive_culled_attr_t>();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };


	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::PRIMITIVE_CULLED_RET>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_primitive_culled_ret_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(bool) <= vtable.inline_data_size) {
		node->shared = *(const bool*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[5]], refl_start_ptr, refl_end_ptr, node->name);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::PRIMITIVE_DATA_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_primitive_data_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::PRIMITIVE_ID_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_primitive_id_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::PRIMITIVE_ID_ATTR>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_primitive_id_attr_t>();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };


	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::PRIMITIVE_ID_RET>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_primitive_id_ret_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(bool) <= vtable.inline_data_size) {
		node->shared = *(const bool*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[5]], refl_start_ptr, refl_end_ptr, node->name);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::QUADGROUP_INDEX_IN_THREADGROUP_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_quadgroup_index_in_threadgroup_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::QUADGROUPS_PER_THREADGROUP_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_quadgroups_per_threadgroup_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::R16SNORM_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_r16snorm_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->alu_type = *(const node_id_t*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::R16UNORM_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_r16unorm_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->alu_type = *(const node_id_t*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::R8SNORM_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_r8snorm_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->alu_type = *(const node_id_t*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::R8UNORM_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_r8unorm_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->alu_type = *(const node_id_t*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::RG11B10F_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_rg11b10f_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->alu_type = *(const node_id_t*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::RG16SNORM_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_rg16snorm_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->alu_type = *(const node_id_t*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::RG16UNORM_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_rg16unorm_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->alu_type = *(const node_id_t*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::RG8SNORM_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_rg8snorm_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->alu_type = *(const node_id_t*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::RG8UNORM_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_rg8unorm_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->alu_type = *(const node_id_t*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::RGB10A2_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_rgb10a2_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->alu_type = *(const node_id_t*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::RGB9E5_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_rgb9e5_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->alu_type = *(const node_id_t*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::RGBA16SNORM_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_rgba16snorm_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->alu_type = *(const node_id_t*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::RGBA16UNORM_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_rgba16unorm_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->alu_type = *(const node_id_t*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::RGBA8SNORM_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_rgba8snorm_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->alu_type = *(const node_id_t*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::RGBA8UNORM_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_rgba8unorm_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->alu_type = *(const node_id_t*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::RVALUE_REFERENCE_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_rvalue_reference_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->pointee_type = *(const node_id_t*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::RECORD_BASE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_record_base_t>();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->offset = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->type = *(const node_id_t*)&root_data[vtable_fields[4]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::RECORD_FIELD>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_record_field_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->offset = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->type = *(const node_id_t*)&root_data[vtable_fields[4]];
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[5]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[6]], refl_start_ptr, refl_end_ptr, node->attributes);
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(bitfield_info_t) <= vtable.inline_data_size) {
		node->bitfield = *(const bitfield_info_t*)&root_data[vtable_fields[7]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::RENDER_PIPELINE_STATE_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_render_pipeline_state_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->location_index = *(const uint_value_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->location_count = *(const uint_value_t*)&root_data[vtable_fields[4]];
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[5]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[6]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[7]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::RENDER_PIPELINE_STATE_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_render_pipeline_state_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::RENDER_TARGET_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_render_target_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->render_target_index = *(const uint_value_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->raster_order_group = *(const uint_value_t*)&root_data[vtable_fields[4]];
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[5]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[6]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[7]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::RENDER_TARGET_ARRAY_INDEX_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_render_target_array_index_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::RENDER_TARGET_ARRAY_INDEX_ATTR>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_render_target_array_index_attr_t>();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };


	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::RENDER_TARGET_ARRAY_INDEX_RET>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_render_target_array_index_ret_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(bool) <= vtable.inline_data_size) {
		node->shared = *(const bool*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[5]], refl_start_ptr, refl_end_ptr, node->name);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::RENDER_TARGET_ATTR>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_render_target_attr_t>();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->index = *(const uint_value_t*)&root_data[vtable_fields[2]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::RENDER_TARGET_RET>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_render_target_ret_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->render_target_index = *(const uint_value_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->blend_source_index = *(const uint_value_t*)&root_data[vtable_fields[4]];
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->raster_order_group = *(const uint_value_t*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(ROUNDING_MODE) <= vtable.inline_data_size) {
		node->rounding_mode = *(const ROUNDING_MODE*)&root_data[vtable_fields[6]];
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[7]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 18 && vtable_fields[8] != 0u &&
		vtable_fields[8] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[8]], refl_start_ptr, refl_end_ptr, node->name);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::SRGBA8UNORM_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_srgba8unorm_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->alu_type = *(const node_id_t*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::SAMPLE_ID_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_sample_id_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::SAMPLE_MASK_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_sample_mask_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(bool) <= vtable.inline_data_size) {
		node->post_depth_coverage = *(const bool*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[5]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[6]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::SAMPLE_MASK_RET>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_sample_mask_ret_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::SAMPLER_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_sampler_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->location_index = *(const uint_value_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->location_count = *(const uint_value_t*)&root_data[vtable_fields[4]];
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[5]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[6]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[7]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::SAMPLER_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_sampler_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::SHARED_ATTR>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_shared_attr_t>();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };


	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::SHORT_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_short_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::SIMDGROUP_INDEX_IN_THREADGROUP_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_simdgroup_index_in_threadgroup_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::SIMDGROUPS_PER_THREADGROUP_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_simdgroups_per_threadgroup_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::STAGE_IN_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_stage_in_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->location_index = *(const uint_value_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->location_count = *(const uint_value_t*)&root_data[vtable_fields[4]];
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[5]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[6]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[7]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::STAGE_IN_GRID_ORIGIN_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_stage_in_grid_origin_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::STAGE_IN_GRID_SIZE_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_stage_in_grid_size_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::STENCIL_RET>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_stencil_ret_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::STITCHING_ARGUMENT>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_stitching_argument_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->type = *(const node_id_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->name);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::STRUCT_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_struct_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[5]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[6]], refl_start_ptr, refl_end_ptr, node->members);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::STRUCT_TYPE_INFO>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_struct_type_info_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->fields = std::vector<struct_type_info_field_t> {};
		std::vector<uint32_t> offsets;
		const auto vec_base_ptr = &root_data[vtable_fields[2]];
		parse_vector<uint32_t>(vec_base_ptr, refl_start_ptr, refl_end_ptr, offsets);
		for (uint32_t i = 0, count = uint32_t(offsets.size()); i < count; ++i) {
			const auto root_ptr = vec_base_ptr + *(const uint32_t*)vec_base_ptr + sizeof(uint32_t) * (i + 1) + offsets[i];
			if (root_ptr < refl_start_ptr && root_ptr + sizeof(table_root_t) > refl_end_ptr) {
				continue;
			}
			const auto table_root = (const table_root_t*)root_ptr;
			const auto vtable_ptr = root_ptr - table_root->vtable_offset;
			if (vtable_ptr < refl_start_ptr && vtable_ptr + sizeof(table_vtable_t) > refl_end_ptr) {
				continue;
			}
			const auto table_vtable = (const table_vtable_t*)vtable_ptr;
			node->fields->emplace_back(parse_struct_type_info_field(*table_root, *table_vtable, refl_data));
		}
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::TENSOR_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_tensor_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->location_index = *(const uint_value_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->location_count = *(const uint_value_t*)&root_data[vtable_fields[4]];
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(ACCESS_QUALIFIER) <= vtable.inline_data_size) {
		node->access_qualifier = *(const ACCESS_QUALIFIER*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->raster_order_group = *(const uint_value_t*)&root_data[vtable_fields[6]];
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[7]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 18 && vtable_fields[8] != 0u &&
		vtable_fields[8] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[8]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 20 && vtable_fields[9] != 0u &&
		vtable_fields[9] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[9]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::TENSOR_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_tensor_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->element_type = *(const node_id_t*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->extents_type = *(const node_id_t*)&root_data[vtable_fields[6]];
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(TENSOR_KIND) <= vtable.inline_data_size) {
		node->kind = *(const TENSOR_KIND*)&root_data[vtable_fields[7]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::TEXTURE1D_ARRAY_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_texture1d_array_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->channel_type = *(const node_id_t*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(ACCESS_QUALIFIER) <= vtable.inline_data_size) {
		node->access_qualifier = *(const ACCESS_QUALIFIER*)&root_data[vtable_fields[6]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::TEXTURE1D_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_texture1d_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->channel_type = *(const node_id_t*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(ACCESS_QUALIFIER) <= vtable.inline_data_size) {
		node->access_qualifier = *(const ACCESS_QUALIFIER*)&root_data[vtable_fields[6]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::TEXTURE2D_ARRAY_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_texture2d_array_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->channel_type = *(const node_id_t*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(ACCESS_QUALIFIER) <= vtable.inline_data_size) {
		node->access_qualifier = *(const ACCESS_QUALIFIER*)&root_data[vtable_fields[6]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::TEXTURE2D_MS_ARRAY_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_texture2d_ms_array_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->channel_type = *(const node_id_t*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(ACCESS_QUALIFIER) <= vtable.inline_data_size) {
		node->access_qualifier = *(const ACCESS_QUALIFIER*)&root_data[vtable_fields[6]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::TEXTURE2D_MS_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_texture2d_ms_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->channel_type = *(const node_id_t*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(ACCESS_QUALIFIER) <= vtable.inline_data_size) {
		node->access_qualifier = *(const ACCESS_QUALIFIER*)&root_data[vtable_fields[6]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::TEXTURE2D_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_texture2d_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->channel_type = *(const node_id_t*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(ACCESS_QUALIFIER) <= vtable.inline_data_size) {
		node->access_qualifier = *(const ACCESS_QUALIFIER*)&root_data[vtable_fields[6]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::TEXTURE3D_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_texture3d_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->channel_type = *(const node_id_t*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(ACCESS_QUALIFIER) <= vtable.inline_data_size) {
		node->access_qualifier = *(const ACCESS_QUALIFIER*)&root_data[vtable_fields[6]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::TEXTURE_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_texture_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->location_index = *(const uint_value_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->location_count = *(const uint_value_t*)&root_data[vtable_fields[4]];
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(ACCESS_QUALIFIER) <= vtable.inline_data_size) {
		node->access_qualifier = *(const ACCESS_QUALIFIER*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->raster_order_group = *(const uint_value_t*)&root_data[vtable_fields[6]];
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[7]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 18 && vtable_fields[8] != 0u &&
		vtable_fields[8] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[8]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 20 && vtable_fields[9] != 0u &&
		vtable_fields[9] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[9]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::TEXTURE_BUFFER1D_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_texture_buffer1d_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->channel_type = *(const node_id_t*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(ACCESS_QUALIFIER) <= vtable.inline_data_size) {
		node->access_qualifier = *(const ACCESS_QUALIFIER*)&root_data[vtable_fields[6]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::TEXTURE_CUBE_ARRAY_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_texture_cube_array_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->channel_type = *(const node_id_t*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(ACCESS_QUALIFIER) <= vtable.inline_data_size) {
		node->access_qualifier = *(const ACCESS_QUALIFIER*)&root_data[vtable_fields[6]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::TEXTURE_CUBE_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_texture_cube_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->channel_type = *(const node_id_t*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(ACCESS_QUALIFIER) <= vtable.inline_data_size) {
		node->access_qualifier = *(const ACCESS_QUALIFIER*)&root_data[vtable_fields[6]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::THREAD_EXECUTION_WIDTH_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_thread_execution_width_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::THREAD_INDEX_IN_QUADGROUP_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_thread_index_in_quadgroup_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::THREAD_INDEX_IN_SIMDGROUP_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_thread_index_in_simdgroup_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::THREAD_INDEX_IN_THREADGROUP_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_thread_index_in_threadgroup_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::THREAD_POSITION_IN_GRID_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_thread_position_in_grid_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::THREAD_POSITION_IN_THREADGROUP_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_thread_position_in_threadgroup_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::THREADGROUP_POSITION_IN_GRID_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_threadgroup_position_in_grid_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::THREADGROUPS_PER_GRID_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_threadgroups_per_grid_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::THREADS_PER_GRID_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_threads_per_grid_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::THREADS_PER_SIMDGROUP_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_threads_per_simdgroup_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::THREADS_PER_THREADGROUP_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_threads_per_threadgroup_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::TILE_INDEX_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_tile_index_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::TIME_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_time_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::UCHAR_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_uchar_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::UINT_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_uint_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::ULLONG_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_ullong_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::ULONG_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_ulong_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::USHORT_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_ushort_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::UNION_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_union_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[5]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[6]], refl_start_ptr, refl_end_ptr, node->members);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::USER_ANNOTATION_FN_ATTR>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_user_annotation_fn_attr_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[2]], refl_start_ptr, refl_end_ptr, node->annotation);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::USER_ATTR>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_user_attr_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[2]], refl_start_ptr, refl_end_ptr, node->name);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::USER_DATA_BUFFER_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_user_data_buffer_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(ACCESS_QUALIFIER) <= vtable.inline_data_size) {
		node->access_qualifier = *(const ACCESS_QUALIFIER*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->inline_type_info = *(const node_id_t*)&root_data[vtable_fields[4]];
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->struct_type_info = *(const node_id_t*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->type_size = *(const uint_value_t*)&root_data[vtable_fields[6]];
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->type_align = *(const uint_value_t*)&root_data[vtable_fields[7]];
	}
	if (vtable.vtable_size >= 18 && vtable_fields[8] != 0u &&
		vtable_fields[8] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[8]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 20 && vtable_fields[9] != 0u &&
		vtable_fields[9] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[9]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 22 && vtable_fields[10] != 0u &&
		vtable_fields[10] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[10]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::USER_INSTANCE_ID_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_user_instance_id_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::USER_INSTANCE_ID_COUNT_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_user_instance_id_count_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::VEC_TYPE_HINT_FN_ATTR>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_vec_type_hint_fn_attr_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[2]], refl_start_ptr, refl_end_ptr, node->type_name);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::VECTOR_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_vector_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->element_type = *(const node_id_t*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->num_elements = *(const uint32_t*)&root_data[vtable_fields[6]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::VERTEX_FUNCTION>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_vertex_function_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[2]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->return_types);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->arguments);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->patch = *(const node_id_t*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->user_annotation = *(const node_id_t*)&root_data[vtable_fields[6]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::VERTEX_ID_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_vertex_id_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::VERTEX_INPUT_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_vertex_input_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->location_index = *(const uint_value_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->location_count = *(const uint_value_t*)&root_data[vtable_fields[4]];
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[5]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[6]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[7]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::VERTEX_OUTPUT_RET>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_vertex_output_ret_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->attribute_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->location = *(const uint_value_t*)&root_data[vtable_fields[4]];
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->shared = *(const bool*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[6]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[7]], refl_start_ptr, refl_end_ptr, node->name);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::VERTEX_VALUE_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_vertex_value_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->underlying_type = *(const node_id_t*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::VIEWPORT_ARRAY_INDEX_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_viewport_array_index_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::VIEWPORT_ARRAY_INDEX_ATTR>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_viewport_array_index_attr_t>();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };


	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::VIEWPORT_ARRAY_INDEX_RET>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_viewport_array_index_ret_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(bool) <= vtable.inline_data_size) {
		node->shared = *(const bool*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[5]], refl_start_ptr, refl_end_ptr, node->name);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::VISIBLE_FUNCTION>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_visible_function_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[2]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(stitching_info_t) <= vtable.inline_data_size) {
		node->stitching_info = *(const stitching_info_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->user_annotation = *(const node_id_t*)&root_data[vtable_fields[4]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::VISIBLE_FUNCTION_REFERENCE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_visible_function_reference_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[2]], refl_start_ptr, refl_end_ptr, node->function_name);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::VISIBLE_FUNCTION_TABLE_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_visible_function_table_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->location_index = *(const uint_value_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->location_count = *(const uint_value_t*)&root_data[vtable_fields[4]];
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(ACCESS_QUALIFIER) <= vtable.inline_data_size) {
		node->access_qualifier = *(const ACCESS_QUALIFIER*)&root_data[vtable_fields[5]];
	}
	if (vtable.vtable_size >= 14 && vtable_fields[6] != 0u &&
		vtable_fields[6] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->raster_order_group = *(const uint_value_t*)&root_data[vtable_fields[6]];
	}
	if (vtable.vtable_size >= 16 && vtable_fields[7] != 0u &&
		vtable_fields[7] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[7]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 18 && vtable_fields[8] != 0u &&
		vtable_fields[8] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[8]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 20 && vtable_fields[9] != 0u &&
		vtable_fields[9] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[9]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::VISIBLE_FUNCTION_TABLE_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_visible_function_table_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(node_id_t) <= vtable.inline_data_size) {
		node->function_type = *(const node_id_t*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::VOID_TYPE>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_void_type_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->size = *(const uint32_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		node->alignment = *(const uint32_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_vector<node_id_t>(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->qualifiers);
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::WORKGROUP_MAX_SIZE_FN_ATTR>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_workgroup_max_size_fn_attr_t>();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->size = *(const uint_value_t*)&root_data[vtable_fields[2]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::WORKGROUP_SIZE_FN_ATTR>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_workgroup_size_fn_attr_t>();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->width = *(const uint_value_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->height = *(const uint_value_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->depth = *(const uint_value_t*)&root_data[vtable_fields[4]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::WORKGROUP_SIZE_HINT_FN_ATTR>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_workgroup_size_hint_fn_attr_t>();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->width = *(const uint_value_t*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->height = *(const uint_value_t*)&root_data[vtable_fields[3]];
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint_value_t) <= vtable.inline_data_size) {
		node->depth = *(const uint_value_t*)&root_data[vtable_fields[4]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::WORLD_SPACE_DIRECTION_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_world_space_direction_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::WORLD_SPACE_ORIGIN_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_world_space_origin_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}

template<> inline std::unique_ptr<node_base_t>
parse<NODE_TYPE::WORLD_TO_OBJECT_TRANSFORM_ARG>(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	auto node = std::make_unique<node_world_to_object_transform_arg_t>();

	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();

	const std::span<const uint8_t> root_data { (const uint8_t*)&root, size_t(vtable.inline_data_size) };
	assert((vtable.vtable_size % 2u) == 0u);
	const std::span<const uint16_t> vtable_fields { (const uint16_t*)&vtable, size_t(vtable.vtable_size / 2u) };

	if (vtable.vtable_size >= 6 && vtable_fields[2] != 0u &&
		vtable_fields[2] + sizeof(bool) <= vtable.inline_data_size) {
		node->function_constant = *(const bool*)&root_data[vtable_fields[2]];
	}
	if (vtable.vtable_size >= 8 && vtable_fields[3] != 0u &&
		vtable_fields[3] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[3]], refl_start_ptr, refl_end_ptr, node->type_name);
	}
	if (vtable.vtable_size >= 10 && vtable_fields[4] != 0u &&
		vtable_fields[4] + sizeof(uint32_t) <= vtable.inline_data_size) {
		parse_string(&root_data[vtable_fields[4]], refl_start_ptr, refl_end_ptr, node->name);
	}
	if (vtable.vtable_size >= 12 && vtable_fields[5] != 0u &&
		vtable_fields[5] + sizeof(bool) <= vtable.inline_data_size) {
		node->unused = *(const bool*)&root_data[vtable_fields[5]];
	}

	return node;
}


static inline std::unique_ptr<node_base_t> parse_node(const table_root_t& root, const table_vtable_t& vtable, const std::span<const uint8_t> refl_data) {
	// node layout: while close to the layout of a flatbuffers table, it is not exactly the same (and not covered by any other standard
	// flatbuffers type), essentially being some kind of indirect table (or double/nested table), thus requiring some special handling:
	//  * the initial node pointer we get points to the table-like root table (-ish)
	//    * this root table generally contains three 32-bit values: the vtable offset, the node type and an indirection offset
	//    * while *generally* stored in this order, this technically isn't required and the layout *may* be variable
	//      -> for simplicity, we will however consider this fixed and just bail out if this is not the case
	//  * as with a standard table, the first 32-bit of the node contain a signed offset to the "vtable"
	//  * the "vtable" is similar to a table vtable:
	//    * the vtable size is identical
	//    * the "inline data size" specifies the size inside the node root (generally 12 bytes, but may be padded with an additional +2)
	//    * this is followed by two offsets, which could also be understood as table fields
	//  * from the first offset we get the node type enum
	//  * from the second offset we get the table indirection offset
	//    * adding this to the node root pointer gives us the start of the indirect table root
	//    * this indirect table root then contains a standard table (thus also ref'ing a vtable of its own)
	
	const auto refl_start_ptr = refl_data.data();
	const auto refl_end_ptr = refl_data.data() + refl_data.size_bytes();
	
	const auto root_ptr = (const uint8_t*)&root;
	if (root_ptr + sizeof(node_root_t) > refl_end_ptr) {
		llvm::errs() << "out-of-bounds node\n";
		return {};
	}
	const auto& node_root = (const node_root_t&)root;
	
	const auto vtable_ptr = (const uint8_t*)&vtable;
	if (vtable_ptr < refl_start_ptr || vtable_ptr + sizeof(node_vtable_t) > refl_end_ptr) {
		llvm::errs() << "out-of-bounds node vtable\n";
		return {};
	}
	const auto& node_vtable = (const node_vtable_t&)vtable;
	
	// check assumptions
	if (node_vtable.inline_data_size < sizeof(node_root_t)) {
		llvm::errs() << "unexpected node inline data size: " << std::to_string(node_vtable.inline_data_size) << "\n";
		return {};
	}
	if (node_vtable.node_type_offset != 4u) {
		llvm::errs() << "unexpected node type offset: " << node_vtable.node_type_offset << "\n";
		return {};
	}
	if (node_vtable.node_indirection_offset != 8u) {
		llvm::errs() << "unexpected node indirection offset: " << node_vtable.node_type_offset << "\n";
		return {};
	}
	
	// now we can get to the indirect table / actual node data
	const auto indirect_node_ptr = root_ptr + node_vtable.node_indirection_offset + node_root.indirection_offset;
	if (indirect_node_ptr < refl_start_ptr || indirect_node_ptr + sizeof(table_root_t) > refl_end_ptr) {
		llvm::errs() << "out-of-bounds indirect node offset\n";
		return {};
	}
	const auto& indirect_root = *(const table_root_t*)indirect_node_ptr;
	
	const auto indirect_vtable_ptr = indirect_node_ptr - indirect_root.vtable_offset;
	if (indirect_vtable_ptr < refl_start_ptr || indirect_vtable_ptr + sizeof(table_vtable_t) > refl_end_ptr) {
		llvm::errs() << "out-of-bounds indirect node vtable offset\n";
		return {};
	}
	const auto& indirect_node_vtable = *(const table_vtable_t*)indirect_vtable_ptr;
	
	std::unique_ptr<node_base_t> node {};
	switch (node_root.type) {
		default:
			break;
		case NODE_TYPE::ACCELERATION_STRUCTURE_TYPE:
			node = parse<NODE_TYPE::ACCELERATION_STRUCTURE_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::ACCEPT_INTERSECTION_RET:
			node = parse<NODE_TYPE::ACCEPT_INTERSECTION_RET>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::ADDRESS_SPACE_TYPE_QUAL:
			node = parse<NODE_TYPE::ADDRESS_SPACE_TYPE_QUAL>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::AMPLIFICATION_COUNT_ARG:
			node = parse<NODE_TYPE::AMPLIFICATION_COUNT_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::AMPLIFICATION_ID_ARG:
			node = parse<NODE_TYPE::AMPLIFICATION_ID_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::ARRAY_OF_TYPE:
			node = parse<NODE_TYPE::ARRAY_OF_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::ARRAY_REF_OF_TYPE:
			node = parse<NODE_TYPE::ARRAY_REF_OF_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::ARRAY_TYPE:
			node = parse<NODE_TYPE::ARRAY_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::BFLOAT_TYPE:
			node = parse<NODE_TYPE::BFLOAT_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::BARYCENTRIC_COORD_ARG:
			node = parse<NODE_TYPE::BARYCENTRIC_COORD_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::BASE_INSTANCE_ARG:
			node = parse<NODE_TYPE::BASE_INSTANCE_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::BASE_VERTEX_ARG:
			node = parse<NODE_TYPE::BASE_VERTEX_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::BOOL_TYPE:
			node = parse<NODE_TYPE::BOOL_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::BUFFER_ARG:
			node = parse<NODE_TYPE::BUFFER_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::BUFFER_STRIDE_ARG:
			node = parse<NODE_TYPE::BUFFER_STRIDE_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::CIARRAY_ARG:
			node = parse<NODE_TYPE::CIARRAY_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::CIBUILTIN_ARG:
			node = parse<NODE_TYPE::CIBUILTIN_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::CIBUILTIN_RET:
			node = parse<NODE_TYPE::CIBUILTIN_RET>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::CI_FUNCTION:
			node = parse<NODE_TYPE::CI_FUNCTION>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::CIIMAGEBLOCK_ARG:
			node = parse<NODE_TYPE::CIIMAGEBLOCK_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::CIIMAGEBLOCK_RET:
			node = parse<NODE_TYPE::CIIMAGEBLOCK_RET>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::CIMATRIX_ARG:
			node = parse<NODE_TYPE::CIMATRIX_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::CIMATRIX_RET:
			node = parse<NODE_TYPE::CIMATRIX_RET>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::CIPADDING_ARG:
			node = parse<NODE_TYPE::CIPADDING_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::CIPOINTER_ARG:
			node = parse<NODE_TYPE::CIPOINTER_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::CIPOINTER_RET:
			node = parse<NODE_TYPE::CIPOINTER_RET>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::CISAMPLER_ARG:
			node = parse<NODE_TYPE::CISAMPLER_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::CISAMPLER_RET:
			node = parse<NODE_TYPE::CISAMPLER_RET>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::CISTRUCT_ARG:
			node = parse<NODE_TYPE::CISTRUCT_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::CISTRUCT_RET:
			node = parse<NODE_TYPE::CISTRUCT_RET>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::CITEXTURE_ARG:
			node = parse<NODE_TYPE::CITEXTURE_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::CITEXTURE_RET:
			node = parse<NODE_TYPE::CITEXTURE_RET>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::CHAR_TYPE:
			node = parse<NODE_TYPE::CHAR_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::CLIP_DISTANCE_ATTR:
			node = parse<NODE_TYPE::CLIP_DISTANCE_ATTR>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::CLIP_DISTANCE_RET:
			node = parse<NODE_TYPE::CLIP_DISTANCE_RET>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::COMMAND_BUFFER_ARG:
			node = parse<NODE_TYPE::COMMAND_BUFFER_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::COMMAND_BUFFER_TYPE:
			node = parse<NODE_TYPE::COMMAND_BUFFER_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::COMPUTE_PIPELINE_STATE_ARG:
			node = parse<NODE_TYPE::COMPUTE_PIPELINE_STATE_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::COMPUTE_PIPELINE_STATE_TYPE:
			node = parse<NODE_TYPE::COMPUTE_PIPELINE_STATE_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::CONSTANT_ARG:
			node = parse<NODE_TYPE::CONSTANT_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::CONTINUE_SEARCH_RET:
			node = parse<NODE_TYPE::CONTINUE_SEARCH_RET>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::CONTROL_POINT_FIELD:
			node = parse<NODE_TYPE::CONTROL_POINT_FIELD>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::CONTROL_POINT_INDEX_BUFFER_ARG:
			node = parse<NODE_TYPE::CONTROL_POINT_INDEX_BUFFER_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::CONTROL_POINT_INPUT_ARG:
			node = parse<NODE_TYPE::CONTROL_POINT_INPUT_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::CURVE_PARAMETER_ARG:
			node = parse<NODE_TYPE::CURVE_PARAMETER_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::DEPTH2D_ARRAY_TYPE:
			node = parse<NODE_TYPE::DEPTH2D_ARRAY_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::DEPTH2D_MS_ARRAY_TYPE:
			node = parse<NODE_TYPE::DEPTH2D_MS_ARRAY_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::DEPTH2D_MS_TYPE:
			node = parse<NODE_TYPE::DEPTH2D_MS_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::DEPTH2D_TYPE:
			node = parse<NODE_TYPE::DEPTH2D_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::DEPTH_CUBE_ARRAY_TYPE:
			node = parse<NODE_TYPE::DEPTH_CUBE_ARRAY_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::DEPTH_CUBE_TYPE:
			node = parse<NODE_TYPE::DEPTH_CUBE_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::DEPTH_RET:
			node = parse<NODE_TYPE::DEPTH_RET>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::DEPTH_STENCIL_STATE_ARG:
			node = parse<NODE_TYPE::DEPTH_STENCIL_STATE_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::DEPTH_STENCIL_STATE_TYPE:
			node = parse<NODE_TYPE::DEPTH_STENCIL_STATE_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::DIRECTION_ARG:
			node = parse<NODE_TYPE::DIRECTION_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::DISPATCH_QUADGROUPS_PER_THREADGROUP_ARG:
			node = parse<NODE_TYPE::DISPATCH_QUADGROUPS_PER_THREADGROUP_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::DISPATCH_SIMDGROUPS_PER_THREADGROUP_ARG:
			node = parse<NODE_TYPE::DISPATCH_SIMDGROUPS_PER_THREADGROUP_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::DISPATCH_THREADS_PER_THREADGROUP_ARG:
			node = parse<NODE_TYPE::DISPATCH_THREADS_PER_THREADGROUP_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::DISTANCE_ARG:
			node = parse<NODE_TYPE::DISTANCE_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::DISTANCE_RET:
			node = parse<NODE_TYPE::DISTANCE_RET>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::DOUBLE_TYPE:
			node = parse<NODE_TYPE::DOUBLE_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::ENUM_TYPE:
			node = parse<NODE_TYPE::ENUM_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::EXTENTS_TYPE:
			node = parse<NODE_TYPE::EXTENTS_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::FLOAT_TYPE:
			node = parse<NODE_TYPE::FLOAT_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::FRAGMENT_FUNCTION:
			node = parse<NODE_TYPE::FRAGMENT_FUNCTION>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::FRAGMENT_INPUT_ARG:
			node = parse<NODE_TYPE::FRAGMENT_INPUT_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::FRONT_FACING_ARG:
			node = parse<NODE_TYPE::FRONT_FACING_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::FUNCTION_CONSTANT:
			node = parse<NODE_TYPE::FUNCTION_CONSTANT>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::FUNCTION_CONSTANT_PREDICATE_ATTR:
			node = parse<NODE_TYPE::FUNCTION_CONSTANT_PREDICATE_ATTR>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::FUNCTION_HANDLE_ARG:
			node = parse<NODE_TYPE::FUNCTION_HANDLE_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::FUNCTION_HANDLE_TYPE:
			node = parse<NODE_TYPE::FUNCTION_HANDLE_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::FUNCTION_ID_ARG:
			node = parse<NODE_TYPE::FUNCTION_ID_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::FUNCTION_TYPE:
			node = parse<NODE_TYPE::FUNCTION_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::GEOMETRY_ID_ARG:
			node = parse<NODE_TYPE::GEOMETRY_ID_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::GEOMETRY_INTERSECTION_FUNCTION_TABLE_OFFSET_ARG:
			node = parse<NODE_TYPE::GEOMETRY_INTERSECTION_FUNCTION_TABLE_OFFSET_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::GLOBAL_BINDING:
			node = parse<NODE_TYPE::GLOBAL_BINDING>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::HALF_TYPE:
			node = parse<NODE_TYPE::HALF_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::IMAGEBLOCK_ARG:
			node = parse<NODE_TYPE::IMAGEBLOCK_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::IMAGEBLOCK_DATA_ARG:
			node = parse<NODE_TYPE::IMAGEBLOCK_DATA_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::IMAGEBLOCK_DATA_RET:
			node = parse<NODE_TYPE::IMAGEBLOCK_DATA_RET>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::IMAGEBLOCK_TYPE:
			node = parse<NODE_TYPE::IMAGEBLOCK_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::INDIRECT_BUFFER_ARG:
			node = parse<NODE_TYPE::INDIRECT_BUFFER_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::INDIRECT_CONSTANT_ARG:
			node = parse<NODE_TYPE::INDIRECT_CONSTANT_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::INLINE_TYPE_INFO:
			node = parse<NODE_TYPE::INLINE_TYPE_INFO>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::INSTANCE_ACCELERATION_STRUCTURE_ARG:
			node = parse<NODE_TYPE::INSTANCE_ACCELERATION_STRUCTURE_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::INSTANCE_ID_ARG:
			node = parse<NODE_TYPE::INSTANCE_ID_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::INSTANCE_ID_COUNT_ARG:
			node = parse<NODE_TYPE::INSTANCE_ID_COUNT_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::INSTANCE_INTERSECTION_FUNCTION_TABLE_OFFSET_ARG:
			node = parse<NODE_TYPE::INSTANCE_INTERSECTION_FUNCTION_TABLE_OFFSET_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::INT_TYPE:
			node = parse<NODE_TYPE::INT_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::INTERPOLANT_TYPE:
			node = parse<NODE_TYPE::INTERPOLANT_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::INTERSECTION_FUNCTION:
			node = parse<NODE_TYPE::INTERSECTION_FUNCTION>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::INTERSECTION_FUNCTION_HANDLE_TYPE:
			node = parse<NODE_TYPE::INTERSECTION_FUNCTION_HANDLE_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::INTERSECTION_FUNCTION_TABLE_ARG:
			node = parse<NODE_TYPE::INTERSECTION_FUNCTION_TABLE_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::INTERSECTION_FUNCTION_TABLE_TYPE:
			node = parse<NODE_TYPE::INTERSECTION_FUNCTION_TABLE_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::INVARIANT_ATTR:
			node = parse<NODE_TYPE::INVARIANT_ATTR>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::KERNEL_FUNCTION:
			node = parse<NODE_TYPE::KERNEL_FUNCTION>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::KEY_FRAME_COUNT_ARG:
			node = parse<NODE_TYPE::KEY_FRAME_COUNT_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::LLONG_TYPE:
			node = parse<NODE_TYPE::LLONG_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::LVALUE_REFERENCE_TYPE:
			node = parse<NODE_TYPE::LVALUE_REFERENCE_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::LOCATION_INDEX_ATTR:
			node = parse<NODE_TYPE::LOCATION_INDEX_ATTR>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::LONG_TYPE:
			node = parse<NODE_TYPE::LONG_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::MATRIX_TYPE:
			node = parse<NODE_TYPE::MATRIX_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::MAX_DISTANCE_ARG:
			node = parse<NODE_TYPE::MAX_DISTANCE_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::MAX_MESH_WORKGROUPS_FN_ATTR:
			node = parse<NODE_TYPE::MAX_MESH_WORKGROUPS_FN_ATTR>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::MESH_ARG:
			node = parse<NODE_TYPE::MESH_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::MESH_EMULATION_BLOCK:
			node = parse<NODE_TYPE::MESH_EMULATION_BLOCK>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::MESH_EMULATION_FRAGMENT_ANALYSIS_RESULT:
			node = parse<NODE_TYPE::MESH_EMULATION_FRAGMENT_ANALYSIS_RESULT>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::MESH_EMULATION_MESH_KERNEL:
			node = parse<NODE_TYPE::MESH_EMULATION_MESH_KERNEL>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::MESH_EMULATION_MESH_LAYOUT:
			node = parse<NODE_TYPE::MESH_EMULATION_MESH_LAYOUT>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::MESH_EMULATION_MESH_VERTEX:
			node = parse<NODE_TYPE::MESH_EMULATION_MESH_VERTEX>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::MESH_EMULATION_OBJECT_KERNEL:
			node = parse<NODE_TYPE::MESH_EMULATION_OBJECT_KERNEL>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::MESH_EMULATION_VALUE_GROUP:
			node = parse<NODE_TYPE::MESH_EMULATION_VALUE_GROUP>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::MESH_FUNCTION:
			node = parse<NODE_TYPE::MESH_FUNCTION>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::MESH_GRID_PROPERTIES_ARG:
			node = parse<NODE_TYPE::MESH_GRID_PROPERTIES_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::MESH_GRID_PROPERTIES_TYPE:
			node = parse<NODE_TYPE::MESH_GRID_PROPERTIES_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::MESH_PRIMITIVE_DATA_RET:
			node = parse<NODE_TYPE::MESH_PRIMITIVE_DATA_RET>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::MESH_TYPE:
			node = parse<NODE_TYPE::MESH_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::MESH_TYPE_INFO:
			node = parse<NODE_TYPE::MESH_TYPE_INFO>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::MESH_VERTEX_DATA_RET:
			node = parse<NODE_TYPE::MESH_VERTEX_DATA_RET>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::MIN_DISTANCE_ARG:
			node = parse<NODE_TYPE::MIN_DISTANCE_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::MOTION_END_TIME_ARG:
			node = parse<NODE_TYPE::MOTION_END_TIME_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::MOTION_START_TIME_ARG:
			node = parse<NODE_TYPE::MOTION_START_TIME_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::OBJECT_FUNCTION:
			node = parse<NODE_TYPE::OBJECT_FUNCTION>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::OBJECT_TO_WORLD_TRANSFORM_ARG:
			node = parse<NODE_TYPE::OBJECT_TO_WORLD_TRANSFORM_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::OPAQUE_PRIMITIVE_ARG:
			node = parse<NODE_TYPE::OPAQUE_PRIMITIVE_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::OPAQUE_TYPE:
			node = parse<NODE_TYPE::OPAQUE_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::ORIGIN_ARG:
			node = parse<NODE_TYPE::ORIGIN_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::PACKED_VECTOR_TYPE:
			node = parse<NODE_TYPE::PACKED_VECTOR_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::PATCH_CONTROL_POINT_TYPE:
			node = parse<NODE_TYPE::PATCH_CONTROL_POINT_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::PATCH_FN_ATTR:
			node = parse<NODE_TYPE::PATCH_FN_ATTR>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::PATCH_ID_ARG:
			node = parse<NODE_TYPE::PATCH_ID_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::PATCH_INPUT_ARG:
			node = parse<NODE_TYPE::PATCH_INPUT_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::PAYLOAD_ARG:
			node = parse<NODE_TYPE::PAYLOAD_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::PIXEL_POSITION_IN_TILE_ARG:
			node = parse<NODE_TYPE::PIXEL_POSITION_IN_TILE_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::PIXELS_PER_TILE_ARG:
			node = parse<NODE_TYPE::PIXELS_PER_TILE_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::POINT_COORD_ARG:
			node = parse<NODE_TYPE::POINT_COORD_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::POINT_SIZE_ATTR:
			node = parse<NODE_TYPE::POINT_SIZE_ATTR>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::POINT_SIZE_RET:
			node = parse<NODE_TYPE::POINT_SIZE_RET>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::POINTER_TYPE:
			node = parse<NODE_TYPE::POINTER_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::POSITION_ARG:
			node = parse<NODE_TYPE::POSITION_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::POSITION_ATTR:
			node = parse<NODE_TYPE::POSITION_ATTR>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::POSITION_IN_PATCH_ARG:
			node = parse<NODE_TYPE::POSITION_IN_PATCH_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::POSITION_RET:
			node = parse<NODE_TYPE::POSITION_RET>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::PRIMITIVE_ACCELERATION_STRUCTURE_ARG:
			node = parse<NODE_TYPE::PRIMITIVE_ACCELERATION_STRUCTURE_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::PRIMITIVE_CULLED_ATTR:
			node = parse<NODE_TYPE::PRIMITIVE_CULLED_ATTR>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::PRIMITIVE_CULLED_RET:
			node = parse<NODE_TYPE::PRIMITIVE_CULLED_RET>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::PRIMITIVE_DATA_ARG:
			node = parse<NODE_TYPE::PRIMITIVE_DATA_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::PRIMITIVE_ID_ARG:
			node = parse<NODE_TYPE::PRIMITIVE_ID_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::PRIMITIVE_ID_ATTR:
			node = parse<NODE_TYPE::PRIMITIVE_ID_ATTR>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::PRIMITIVE_ID_RET:
			node = parse<NODE_TYPE::PRIMITIVE_ID_RET>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::QUADGROUP_INDEX_IN_THREADGROUP_ARG:
			node = parse<NODE_TYPE::QUADGROUP_INDEX_IN_THREADGROUP_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::QUADGROUPS_PER_THREADGROUP_ARG:
			node = parse<NODE_TYPE::QUADGROUPS_PER_THREADGROUP_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::R16SNORM_TYPE:
			node = parse<NODE_TYPE::R16SNORM_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::R16UNORM_TYPE:
			node = parse<NODE_TYPE::R16UNORM_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::R8SNORM_TYPE:
			node = parse<NODE_TYPE::R8SNORM_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::R8UNORM_TYPE:
			node = parse<NODE_TYPE::R8UNORM_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::RG11B10F_TYPE:
			node = parse<NODE_TYPE::RG11B10F_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::RG16SNORM_TYPE:
			node = parse<NODE_TYPE::RG16SNORM_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::RG16UNORM_TYPE:
			node = parse<NODE_TYPE::RG16UNORM_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::RG8SNORM_TYPE:
			node = parse<NODE_TYPE::RG8SNORM_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::RG8UNORM_TYPE:
			node = parse<NODE_TYPE::RG8UNORM_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::RGB10A2_TYPE:
			node = parse<NODE_TYPE::RGB10A2_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::RGB9E5_TYPE:
			node = parse<NODE_TYPE::RGB9E5_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::RGBA16SNORM_TYPE:
			node = parse<NODE_TYPE::RGBA16SNORM_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::RGBA16UNORM_TYPE:
			node = parse<NODE_TYPE::RGBA16UNORM_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::RGBA8SNORM_TYPE:
			node = parse<NODE_TYPE::RGBA8SNORM_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::RGBA8UNORM_TYPE:
			node = parse<NODE_TYPE::RGBA8UNORM_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::RVALUE_REFERENCE_TYPE:
			node = parse<NODE_TYPE::RVALUE_REFERENCE_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::RECORD_BASE:
			node = parse<NODE_TYPE::RECORD_BASE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::RECORD_FIELD:
			node = parse<NODE_TYPE::RECORD_FIELD>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::RENDER_PIPELINE_STATE_ARG:
			node = parse<NODE_TYPE::RENDER_PIPELINE_STATE_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::RENDER_PIPELINE_STATE_TYPE:
			node = parse<NODE_TYPE::RENDER_PIPELINE_STATE_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::RENDER_TARGET_ARG:
			node = parse<NODE_TYPE::RENDER_TARGET_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::RENDER_TARGET_ARRAY_INDEX_ARG:
			node = parse<NODE_TYPE::RENDER_TARGET_ARRAY_INDEX_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::RENDER_TARGET_ARRAY_INDEX_ATTR:
			node = parse<NODE_TYPE::RENDER_TARGET_ARRAY_INDEX_ATTR>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::RENDER_TARGET_ARRAY_INDEX_RET:
			node = parse<NODE_TYPE::RENDER_TARGET_ARRAY_INDEX_RET>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::RENDER_TARGET_ATTR:
			node = parse<NODE_TYPE::RENDER_TARGET_ATTR>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::RENDER_TARGET_RET:
			node = parse<NODE_TYPE::RENDER_TARGET_RET>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::SRGBA8UNORM_TYPE:
			node = parse<NODE_TYPE::SRGBA8UNORM_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::SAMPLE_ID_ARG:
			node = parse<NODE_TYPE::SAMPLE_ID_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::SAMPLE_MASK_ARG:
			node = parse<NODE_TYPE::SAMPLE_MASK_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::SAMPLE_MASK_RET:
			node = parse<NODE_TYPE::SAMPLE_MASK_RET>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::SAMPLER_ARG:
			node = parse<NODE_TYPE::SAMPLER_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::SAMPLER_TYPE:
			node = parse<NODE_TYPE::SAMPLER_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::SHARED_ATTR:
			node = parse<NODE_TYPE::SHARED_ATTR>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::SHORT_TYPE:
			node = parse<NODE_TYPE::SHORT_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::SIMDGROUP_INDEX_IN_THREADGROUP_ARG:
			node = parse<NODE_TYPE::SIMDGROUP_INDEX_IN_THREADGROUP_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::SIMDGROUPS_PER_THREADGROUP_ARG:
			node = parse<NODE_TYPE::SIMDGROUPS_PER_THREADGROUP_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::STAGE_IN_ARG:
			node = parse<NODE_TYPE::STAGE_IN_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::STAGE_IN_GRID_ORIGIN_ARG:
			node = parse<NODE_TYPE::STAGE_IN_GRID_ORIGIN_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::STAGE_IN_GRID_SIZE_ARG:
			node = parse<NODE_TYPE::STAGE_IN_GRID_SIZE_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::STENCIL_RET:
			node = parse<NODE_TYPE::STENCIL_RET>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::STITCHING_ARGUMENT:
			node = parse<NODE_TYPE::STITCHING_ARGUMENT>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::STRUCT_TYPE:
			node = parse<NODE_TYPE::STRUCT_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::STRUCT_TYPE_INFO:
			node = parse<NODE_TYPE::STRUCT_TYPE_INFO>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::TENSOR_ARG:
			node = parse<NODE_TYPE::TENSOR_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::TENSOR_TYPE:
			node = parse<NODE_TYPE::TENSOR_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::TEXTURE1D_ARRAY_TYPE:
			node = parse<NODE_TYPE::TEXTURE1D_ARRAY_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::TEXTURE1D_TYPE:
			node = parse<NODE_TYPE::TEXTURE1D_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::TEXTURE2D_ARRAY_TYPE:
			node = parse<NODE_TYPE::TEXTURE2D_ARRAY_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::TEXTURE2D_MS_ARRAY_TYPE:
			node = parse<NODE_TYPE::TEXTURE2D_MS_ARRAY_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::TEXTURE2D_MS_TYPE:
			node = parse<NODE_TYPE::TEXTURE2D_MS_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::TEXTURE2D_TYPE:
			node = parse<NODE_TYPE::TEXTURE2D_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::TEXTURE3D_TYPE:
			node = parse<NODE_TYPE::TEXTURE3D_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::TEXTURE_ARG:
			node = parse<NODE_TYPE::TEXTURE_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::TEXTURE_BUFFER1D_TYPE:
			node = parse<NODE_TYPE::TEXTURE_BUFFER1D_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::TEXTURE_CUBE_ARRAY_TYPE:
			node = parse<NODE_TYPE::TEXTURE_CUBE_ARRAY_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::TEXTURE_CUBE_TYPE:
			node = parse<NODE_TYPE::TEXTURE_CUBE_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::THREAD_EXECUTION_WIDTH_ARG:
			node = parse<NODE_TYPE::THREAD_EXECUTION_WIDTH_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::THREAD_INDEX_IN_QUADGROUP_ARG:
			node = parse<NODE_TYPE::THREAD_INDEX_IN_QUADGROUP_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::THREAD_INDEX_IN_SIMDGROUP_ARG:
			node = parse<NODE_TYPE::THREAD_INDEX_IN_SIMDGROUP_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::THREAD_INDEX_IN_THREADGROUP_ARG:
			node = parse<NODE_TYPE::THREAD_INDEX_IN_THREADGROUP_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::THREAD_POSITION_IN_GRID_ARG:
			node = parse<NODE_TYPE::THREAD_POSITION_IN_GRID_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::THREAD_POSITION_IN_THREADGROUP_ARG:
			node = parse<NODE_TYPE::THREAD_POSITION_IN_THREADGROUP_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::THREADGROUP_POSITION_IN_GRID_ARG:
			node = parse<NODE_TYPE::THREADGROUP_POSITION_IN_GRID_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::THREADGROUPS_PER_GRID_ARG:
			node = parse<NODE_TYPE::THREADGROUPS_PER_GRID_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::THREADS_PER_GRID_ARG:
			node = parse<NODE_TYPE::THREADS_PER_GRID_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::THREADS_PER_SIMDGROUP_ARG:
			node = parse<NODE_TYPE::THREADS_PER_SIMDGROUP_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::THREADS_PER_THREADGROUP_ARG:
			node = parse<NODE_TYPE::THREADS_PER_THREADGROUP_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::TILE_INDEX_ARG:
			node = parse<NODE_TYPE::TILE_INDEX_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::TIME_ARG:
			node = parse<NODE_TYPE::TIME_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::UCHAR_TYPE:
			node = parse<NODE_TYPE::UCHAR_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::UINT_TYPE:
			node = parse<NODE_TYPE::UINT_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::ULLONG_TYPE:
			node = parse<NODE_TYPE::ULLONG_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::ULONG_TYPE:
			node = parse<NODE_TYPE::ULONG_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::USHORT_TYPE:
			node = parse<NODE_TYPE::USHORT_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::UNION_TYPE:
			node = parse<NODE_TYPE::UNION_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::USER_ANNOTATION_FN_ATTR:
			node = parse<NODE_TYPE::USER_ANNOTATION_FN_ATTR>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::USER_ATTR:
			node = parse<NODE_TYPE::USER_ATTR>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::USER_DATA_BUFFER_ARG:
			node = parse<NODE_TYPE::USER_DATA_BUFFER_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::USER_INSTANCE_ID_ARG:
			node = parse<NODE_TYPE::USER_INSTANCE_ID_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::USER_INSTANCE_ID_COUNT_ARG:
			node = parse<NODE_TYPE::USER_INSTANCE_ID_COUNT_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::VEC_TYPE_HINT_FN_ATTR:
			node = parse<NODE_TYPE::VEC_TYPE_HINT_FN_ATTR>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::VECTOR_TYPE:
			node = parse<NODE_TYPE::VECTOR_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::VERTEX_FUNCTION:
			node = parse<NODE_TYPE::VERTEX_FUNCTION>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::VERTEX_ID_ARG:
			node = parse<NODE_TYPE::VERTEX_ID_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::VERTEX_INPUT_ARG:
			node = parse<NODE_TYPE::VERTEX_INPUT_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::VERTEX_OUTPUT_RET:
			node = parse<NODE_TYPE::VERTEX_OUTPUT_RET>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::VERTEX_VALUE_TYPE:
			node = parse<NODE_TYPE::VERTEX_VALUE_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::VIEWPORT_ARRAY_INDEX_ARG:
			node = parse<NODE_TYPE::VIEWPORT_ARRAY_INDEX_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::VIEWPORT_ARRAY_INDEX_ATTR:
			node = parse<NODE_TYPE::VIEWPORT_ARRAY_INDEX_ATTR>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::VIEWPORT_ARRAY_INDEX_RET:
			node = parse<NODE_TYPE::VIEWPORT_ARRAY_INDEX_RET>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::VISIBLE_FUNCTION:
			node = parse<NODE_TYPE::VISIBLE_FUNCTION>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::VISIBLE_FUNCTION_REFERENCE:
			node = parse<NODE_TYPE::VISIBLE_FUNCTION_REFERENCE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::VISIBLE_FUNCTION_TABLE_ARG:
			node = parse<NODE_TYPE::VISIBLE_FUNCTION_TABLE_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::VISIBLE_FUNCTION_TABLE_TYPE:
			node = parse<NODE_TYPE::VISIBLE_FUNCTION_TABLE_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::VOID_TYPE:
			node = parse<NODE_TYPE::VOID_TYPE>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::WORKGROUP_MAX_SIZE_FN_ATTR:
			node = parse<NODE_TYPE::WORKGROUP_MAX_SIZE_FN_ATTR>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::WORKGROUP_SIZE_FN_ATTR:
			node = parse<NODE_TYPE::WORKGROUP_SIZE_FN_ATTR>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::WORKGROUP_SIZE_HINT_FN_ATTR:
			node = parse<NODE_TYPE::WORKGROUP_SIZE_HINT_FN_ATTR>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::WORLD_SPACE_DIRECTION_ARG:
			node = parse<NODE_TYPE::WORLD_SPACE_DIRECTION_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::WORLD_SPACE_ORIGIN_ARG:
			node = parse<NODE_TYPE::WORLD_SPACE_ORIGIN_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
		case NODE_TYPE::WORLD_TO_OBJECT_TRANSFORM_ARG:
			node = parse<NODE_TYPE::WORLD_TO_OBJECT_TRANSFORM_ARG>(indirect_root, indirect_node_vtable, refl_data);
			break;
	}
	return node;
}

} // namespace metal::reflection
