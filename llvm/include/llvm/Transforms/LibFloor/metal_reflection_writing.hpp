
#pragma once

#include <string>
#include <vector>
#include <optional>
#include <span>
#include <string_view>
#include <cstdint>

namespace metal::reflection {

struct writer_state_t {
	std::vector<uint8_t> data;
	bool is_error { false };

	uint32_t size() const {
		return uint32_t(data.size());
	}

	void pad() {
		if (const auto remainder = size() % 4u; remainder > 0u) {
			data.insert(data.end(), 4u - remainder, 0u);
		}
	}

	void write(const uint8_t val) {
		data.emplace_back(val);
	}

	void write(const char ch) {
		write(std::bit_cast<uint8_t>(ch));
	}

	void write(const bool flag) {
		data.emplace_back(flag ? 1u : 0u);
	}

	void write(const uint16_t val) {
		assert((size() % 2u) == 0u);
		data.insert(data.end(), (const uint8_t*)&val, ((const uint8_t*)&val) + sizeof(uint16_t));
	}

	void write(const uint32_t val) {
		assert((size() % 4u) == 0u);
		data.insert(data.end(), (const uint8_t*)&val, ((const uint8_t*)&val) + sizeof(uint32_t));
	}

	void write(const uint64_t val) {
		assert((size() % 8u) == 0u);
		data.insert(data.end(), (const uint8_t*)&val, ((const uint8_t*)&val) + sizeof(uint64_t));
	}

	void write(const std::string& str) {
		write(uint32_t(str.size()));
		data.insert(data.end(), (const uint8_t*)str.data(), (const uint8_t*)str.data() + str.size());
		write('\0');
		pad();
	}

	void patch(const uint32_t offset, const uint32_t value) {
		assert((offset % 4u) == 0u);
		assert(offset + sizeof(value) <= size());
		memcpy(data.data() + offset, &value, sizeof(value));
	}
};

static inline uint32_t write_node(const node_base_t& node, writer_state_t& state);

static inline void write(const version_t& obj, writer_state_t& state) {
	state.write(obj.major);
	state.write(obj.minor);
	state.write(obj.sub_minor);
}

static inline void write(const bitfield_info_t& obj, writer_state_t& state) {
	state.write(obj.bit_offset);
	state.write(obj.bit_size);
	state.write(obj.storage_size);
}

static inline void write(const bool_value_t& obj, writer_state_t& state) {
	state.write(obj.value);
	state.write(uint8_t(0));
	state.write(uint8_t(0));
	state.write(uint8_t(0));
}

static inline void write(const node_id_t& obj, writer_state_t& state) {
	state.write(obj.id);
}

static inline void write(const uint_value_t& obj, writer_state_t& state) {
	state.write(obj.value);
}

static inline uint32_t write(const local_allocation_t& obj, writer_state_t& state) {
	uint32_t field_count = 0u;
	if (obj.alignment) {
		field_count = 2u;
	} else if (obj.size) {
		field_count = 1u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (obj.size) { inline_data_size += 4; }
	if (obj.alignment) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (obj.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 0) {
		state.write(uint16_t(0u));
	}
	if (obj.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 1) {
		state.write(uint16_t(0u));
	}
	const auto root_table_offset = state.size();
	state.write(uint32_t(state.size() - vtable_start_offset));
	if (obj.size) {
		state.write(*obj.size);
	}
	if (obj.alignment) {
		state.write(*obj.alignment);
	}
	state.pad();
	return root_table_offset;
}

static inline uint32_t write(const reflection_t& obj, writer_state_t& state) {
	uint32_t field_count = 0u;
	if (obj.ci_functions) {
		field_count = 15u;
	} else if (obj.visible_function_references) {
		field_count = 14u;
	} else if (obj.global_bindings) {
		field_count = 13u;
	} else if (obj.emulations) {
		field_count = 12u;
	} else if (obj.static_local_allocations) {
		field_count = 11u;
	} else if (obj.function_constants) {
		field_count = 10u;
	} else if (obj.object_functions) {
		field_count = 9u;
	} else if (obj.mesh_functions) {
		field_count = 8u;
	} else if (obj.visible_functions) {
		field_count = 7u;
	} else if (obj.vertex_functions) {
		field_count = 6u;
	} else if (obj.kernel_functions) {
		field_count = 5u;
	} else if (obj.intersection_functions) {
		field_count = 4u;
	} else if (obj.fragment_functions) {
		field_count = 3u;
	} else if (obj.nodes) {
		field_count = 2u;
	} else if (obj.version) {
		field_count = 1u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (obj.version) { inline_data_size += 12; }
	if (obj.nodes) { inline_data_size += 4; }
	if (obj.fragment_functions) { inline_data_size += 4; }
	if (obj.intersection_functions) { inline_data_size += 4; }
	if (obj.kernel_functions) { inline_data_size += 4; }
	if (obj.vertex_functions) { inline_data_size += 4; }
	if (obj.visible_functions) { inline_data_size += 4; }
	if (obj.mesh_functions) { inline_data_size += 4; }
	if (obj.object_functions) { inline_data_size += 4; }
	if (obj.function_constants) { inline_data_size += 4; }
	if (obj.static_local_allocations) { inline_data_size += 4; }
	if (obj.emulations) { inline_data_size += 4; }
	if (obj.global_bindings) { inline_data_size += 4; }
	if (obj.visible_function_references) { inline_data_size += 4; }
	if (obj.ci_functions) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (obj.version) {
		state.write(inline_offset);
		inline_offset += 12;
	} else if (field_count > 0) {
		state.write(uint16_t(0u));
	}
	if (obj.nodes) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 1) {
		state.write(uint16_t(0u));
	}
	if (obj.fragment_functions) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (obj.intersection_functions) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	if (obj.kernel_functions) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	if (obj.vertex_functions) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 5) {
		state.write(uint16_t(0u));
	}
	if (obj.visible_functions) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 6) {
		state.write(uint16_t(0u));
	}
	if (obj.mesh_functions) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 7) {
		state.write(uint16_t(0u));
	}
	if (obj.object_functions) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 8) {
		state.write(uint16_t(0u));
	}
	if (obj.function_constants) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 9) {
		state.write(uint16_t(0u));
	}
	if (obj.static_local_allocations) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 10) {
		state.write(uint16_t(0u));
	}
	if (obj.emulations) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 11) {
		state.write(uint16_t(0u));
	}
	if (obj.global_bindings) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 12) {
		state.write(uint16_t(0u));
	}
	if (obj.visible_function_references) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 13) {
		state.write(uint16_t(0u));
	}
	if (obj.ci_functions) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 14) {
		state.write(uint16_t(0u));
	}
	const auto root_table_offset = state.size();
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 14> patch_offsets {};
	if (obj.version) {
		write(*obj.version, state);
	}
	if (obj.nodes) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	if (obj.fragment_functions) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (obj.intersection_functions) {
		patch_offsets[2] = state.size();
		state.write(0u);
	}
	if (obj.kernel_functions) {
		patch_offsets[3] = state.size();
		state.write(0u);
	}
	if (obj.vertex_functions) {
		patch_offsets[4] = state.size();
		state.write(0u);
	}
	if (obj.visible_functions) {
		patch_offsets[5] = state.size();
		state.write(0u);
	}
	if (obj.mesh_functions) {
		patch_offsets[6] = state.size();
		state.write(0u);
	}
	if (obj.object_functions) {
		patch_offsets[7] = state.size();
		state.write(0u);
	}
	if (obj.function_constants) {
		patch_offsets[8] = state.size();
		state.write(0u);
	}
	if (obj.static_local_allocations) {
		patch_offsets[9] = state.size();
		state.write(0u);
	}
	if (obj.emulations) {
		patch_offsets[10] = state.size();
		state.write(0u);
	}
	if (obj.global_bindings) {
		patch_offsets[11] = state.size();
		state.write(0u);
	}
	if (obj.visible_function_references) {
		patch_offsets[12] = state.size();
		state.write(0u);
	}
	if (obj.ci_functions) {
		patch_offsets[13] = state.size();
		state.write(0u);
	}
	state.pad();
	if (obj.nodes) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(obj.nodes->size()));
		std::vector<uint32_t> vec_patch_offset;
		vec_patch_offset.reserve(obj.nodes->size());
		for (uint32_t i = 0, count = obj.nodes->size(); i < count; ++i) {
			vec_patch_offset.emplace_back(state.size());
			state.write(0u);
		}
		for (uint32_t vec_patch_idx = 0u; const auto& elem : *obj.nodes) {
			const auto patch_offset = write_node(*elem, state);
			state.patch(vec_patch_offset[vec_patch_idx], patch_offset - vec_patch_offset[vec_patch_idx]);
			++vec_patch_idx;
		}
	}
	if (obj.fragment_functions) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(uint32_t(obj.fragment_functions->size()));
		for (const auto& elem : *obj.fragment_functions) {
			write(elem, state);
		}
	}
	if (obj.intersection_functions) {
		state.patch(patch_offsets[2], state.size() - patch_offsets[2]);
		state.write(uint32_t(obj.intersection_functions->size()));
		for (const auto& elem : *obj.intersection_functions) {
			write(elem, state);
		}
	}
	if (obj.kernel_functions) {
		state.patch(patch_offsets[3], state.size() - patch_offsets[3]);
		state.write(uint32_t(obj.kernel_functions->size()));
		for (const auto& elem : *obj.kernel_functions) {
			write(elem, state);
		}
	}
	if (obj.vertex_functions) {
		state.patch(patch_offsets[4], state.size() - patch_offsets[4]);
		state.write(uint32_t(obj.vertex_functions->size()));
		for (const auto& elem : *obj.vertex_functions) {
			write(elem, state);
		}
	}
	if (obj.visible_functions) {
		state.patch(patch_offsets[5], state.size() - patch_offsets[5]);
		state.write(uint32_t(obj.visible_functions->size()));
		for (const auto& elem : *obj.visible_functions) {
			write(elem, state);
		}
	}
	if (obj.mesh_functions) {
		state.patch(patch_offsets[6], state.size() - patch_offsets[6]);
		state.write(uint32_t(obj.mesh_functions->size()));
		for (const auto& elem : *obj.mesh_functions) {
			write(elem, state);
		}
	}
	if (obj.object_functions) {
		state.patch(patch_offsets[7], state.size() - patch_offsets[7]);
		state.write(uint32_t(obj.object_functions->size()));
		for (const auto& elem : *obj.object_functions) {
			write(elem, state);
		}
	}
	if (obj.function_constants) {
		state.patch(patch_offsets[8], state.size() - patch_offsets[8]);
		state.write(uint32_t(obj.function_constants->size()));
		for (const auto& elem : *obj.function_constants) {
			write(elem, state);
		}
	}
	if (obj.static_local_allocations) {
		state.patch(patch_offsets[9], state.size() - patch_offsets[9]);
		state.write(uint32_t(obj.static_local_allocations->size()));
		std::vector<uint32_t> vec_patch_offset;
		vec_patch_offset.reserve(obj.static_local_allocations->size());
		for (uint32_t i = 0, count = obj.static_local_allocations->size(); i < count; ++i) {
			vec_patch_offset.emplace_back(state.size());
			state.write(0u);
		}
		for (uint32_t vec_patch_idx = 0u; const auto& elem : *obj.static_local_allocations) {
			const auto patch_offset = write(elem, state);
			state.patch(vec_patch_offset[vec_patch_idx], patch_offset - vec_patch_offset[vec_patch_idx]);
			++vec_patch_idx;
		}
	}
	if (obj.emulations) {
		state.patch(patch_offsets[10], state.size() - patch_offsets[10]);
		state.write(uint32_t(obj.emulations->size()));
		for (const auto& elem : *obj.emulations) {
			write(elem, state);
		}
	}
	if (obj.global_bindings) {
		state.patch(patch_offsets[11], state.size() - patch_offsets[11]);
		state.write(uint32_t(obj.global_bindings->size()));
		for (const auto& elem : *obj.global_bindings) {
			write(elem, state);
		}
	}
	if (obj.visible_function_references) {
		state.patch(patch_offsets[12], state.size() - patch_offsets[12]);
		state.write(uint32_t(obj.visible_function_references->size()));
		for (const auto& elem : *obj.visible_function_references) {
			write(elem, state);
		}
	}
	if (obj.ci_functions) {
		state.patch(patch_offsets[13], state.size() - patch_offsets[13]);
		state.write(uint32_t(obj.ci_functions->size()));
		for (const auto& elem : *obj.ci_functions) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write(const stitching_info_t& obj, writer_state_t& state) {
	uint32_t field_count = 0u;
	if (obj.arguments) {
		field_count = 2u;
	} else if (obj.return_type) {
		field_count = 1u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (obj.return_type) { inline_data_size += 4; }
	if (obj.arguments) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (obj.return_type) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 0) {
		state.write(uint16_t(0u));
	}
	if (obj.arguments) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 1) {
		state.write(uint16_t(0u));
	}
	const auto root_table_offset = state.size();
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (obj.return_type) {
		write(*obj.return_type, state);
	}
	if (obj.arguments) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	state.pad();
	if (obj.arguments) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(obj.arguments->size()));
		for (const auto& elem : *obj.arguments) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write(const struct_type_info_field_t& obj, writer_state_t& state) {
	uint32_t field_count = 0u;
	if (obj.inline_type_info) {
		field_count = 12u;
	} else if (obj.render_target_index) {
		field_count = 11u;
	} else if (obj.raster_order_group) {
		field_count = 10u;
	} else if (obj.indirect_location) {
		field_count = 9u;
	} else if (obj.indirect_argument) {
		field_count = 8u;
	} else if (obj.attribute_name) {
		field_count = 7u;
	} else if (obj.field_name) {
		field_count = 6u;
	} else if (obj.type_name) {
		field_count = 5u;
	} else if (obj.array_entries) {
		field_count = 4u;
	} else if (obj.size) {
		field_count = 3u;
	} else if (obj.offset) {
		field_count = 2u;
	} else if (obj.struct_type_info) {
		field_count = 1u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (obj.struct_type_info) { inline_data_size += 4; }
	if (obj.offset) { inline_data_size += 4; }
	if (obj.size) { inline_data_size += 4; }
	if (obj.array_entries) { inline_data_size += 4; }
	if (obj.type_name) { inline_data_size += 4; }
	if (obj.field_name) { inline_data_size += 4; }
	if (obj.attribute_name) { inline_data_size += 4; }
	if (obj.indirect_argument) { inline_data_size += 4; }
	if (obj.indirect_location) { inline_data_size += 4; }
	if (obj.raster_order_group) { inline_data_size += 4; }
	if (obj.render_target_index) { inline_data_size += 4; }
	if (obj.inline_type_info) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (obj.struct_type_info) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 0) {
		state.write(uint16_t(0u));
	}
	if (obj.offset) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 1) {
		state.write(uint16_t(0u));
	}
	if (obj.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (obj.array_entries) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	if (obj.type_name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	if (obj.field_name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 5) {
		state.write(uint16_t(0u));
	}
	if (obj.attribute_name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 6) {
		state.write(uint16_t(0u));
	}
	if (obj.indirect_argument) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 7) {
		state.write(uint16_t(0u));
	}
	if (obj.indirect_location) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 8) {
		state.write(uint16_t(0u));
	}
	if (obj.raster_order_group) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 9) {
		state.write(uint16_t(0u));
	}
	if (obj.render_target_index) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 10) {
		state.write(uint16_t(0u));
	}
	if (obj.inline_type_info) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 11) {
		state.write(uint16_t(0u));
	}
	const auto root_table_offset = state.size();
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 3> patch_offsets {};
	if (obj.struct_type_info) {
		write(*obj.struct_type_info, state);
	}
	if (obj.offset) {
		state.write(*obj.offset);
	}
	if (obj.size) {
		state.write(*obj.size);
	}
	if (obj.array_entries) {
		state.write(*obj.array_entries);
	}
	if (obj.type_name) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	if (obj.field_name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (obj.attribute_name) {
		patch_offsets[2] = state.size();
		state.write(0u);
	}
	if (obj.indirect_argument) {
		write(*obj.indirect_argument, state);
	}
	if (obj.indirect_location) {
		write(*obj.indirect_location, state);
	}
	if (obj.raster_order_group) {
		write(*obj.raster_order_group, state);
	}
	if (obj.render_target_index) {
		write(*obj.render_target_index, state);
	}
	if (obj.inline_type_info) {
		write(*obj.inline_type_info, state);
	}
	state.pad();
	if (obj.type_name) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(*obj.type_name);
	}
	if (obj.field_name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*obj.field_name);
	}
	if (obj.attribute_name) {
		state.patch(patch_offsets[2], state.size() - patch_offsets[2]);
		state.write(*obj.attribute_name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_acceleration_structure_type_t& node, writer_state_t& state) {
	uint32_t field_count = 0u;
	if (node.instance_motion) {
		field_count = 6u;
	} else if (node.primitive_motion) {
		field_count = 5u;
	} else if (node.instancing) {
		field_count = 4u;
	} else if (node.qualifiers) {
		field_count = 3u;
	} else if (node.alignment) {
		field_count = 2u;
	} else if (node.size) {
		field_count = 1u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	if (node.instancing) { inline_data_size += 4; }
	if (node.primitive_motion) { inline_data_size += 4; }
	if (node.instance_motion) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::ACCELERATION_STRUCTURE_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 0) {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 1) {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.instancing) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	if (node.primitive_motion) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	if (node.instance_motion) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 5) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	if (node.instancing) {
		state.write(*node.instancing);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.primitive_motion) {
		state.write(*node.primitive_motion);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.instance_motion) {
		state.write(*node.instance_motion);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_accept_intersection_ret_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::ACCEPT_INTERSECTION_RET));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_address_space_type_qual_t& node, writer_state_t& state) {
	uint32_t field_count = 0u;
	if (node.address_space) {
		field_count = 1u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.address_space) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::ADDRESS_SPACE_TYPE_QUAL));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.address_space) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 0) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	if (node.address_space) {
		state.write(std::underlying_type_t<ADDRESS_SPACE>(*node.address_space));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	return root_table_offset;
}

static inline uint32_t write_node(const node_amplification_count_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::AMPLIFICATION_COUNT_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_amplification_id_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::AMPLIFICATION_ID_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_array_of_type_t& node, writer_state_t& state) {
	uint32_t field_count = 4u;
	if (node.num_elements) {
		field_count = 5u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.num_elements) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::ARRAY_OF_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.num_elements) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	write(node.element_type, state);
	if (node.num_elements) {
		state.write(*node.num_elements);
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_array_ref_of_type_t& node, writer_state_t& state) {
	const uint32_t field_count = 4u;
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	inline_data_size += 4;
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::ARRAY_REF_OF_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	write(node.element_type, state);
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_array_type_t& node, writer_state_t& state) {
	uint32_t field_count = 4u;
	if (node.num_elements) {
		field_count = 5u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.num_elements) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::ARRAY_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.num_elements) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	write(node.element_type, state);
	if (node.num_elements) {
		state.write(*node.num_elements);
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_bfloat_type_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.qualifiers) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	inline_data_size += 4;
	inline_data_size += 4;
	if (node.qualifiers) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::BFLOAT_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	state.write(inline_offset);
	inline_offset += 4;
	state.write(inline_offset);
	inline_offset += 4;
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	state.write(node.size);
	state.write(node.alignment);
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_barycentric_coord_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 4u;
	if (node.unused) {
		field_count = 6u;
	} else if (node.name) {
		field_count = 5u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	if (node.sampling_qualifier) { inline_data_size += 4; }
	if (node.interpolation_qualifier) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::BARYCENTRIC_COORD_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.sampling_qualifier) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.interpolation_qualifier) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 5) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.sampling_qualifier) {
		state.write(std::underlying_type_t<SAMPLING_QUALIFIER>(*node.sampling_qualifier));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.interpolation_qualifier) {
		state.write(std::underlying_type_t<INTERPOLATION_QUALIFIER>(*node.interpolation_qualifier));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_base_instance_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::BASE_INSTANCE_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_base_vertex_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::BASE_VERTEX_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_bool_type_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.qualifiers) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	inline_data_size += 4;
	inline_data_size += 4;
	if (node.qualifiers) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::BOOL_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	state.write(inline_offset);
	inline_offset += 4;
	state.write(inline_offset);
	inline_offset += 4;
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	state.write(node.size);
	state.write(node.alignment);
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_buffer_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 6u;
	if (node.inline_type_info) {
		field_count = 14u;
	} else if (node.unused) {
		field_count = 13u;
	} else if (node.name) {
		field_count = 12u;
	} else if (node.type_name) {
		field_count = 11u;
	} else if (node.type_align) {
		field_count = 10u;
	} else if (node.type_size) {
		field_count = 9u;
	} else if (node.raster_order_group) {
		field_count = 8u;
	} else if (node.struct_type_info) {
		field_count = 7u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	if (node.buffer_size) { inline_data_size += 4; }
	if (node.location_index) { inline_data_size += 4; }
	if (node.location_count) { inline_data_size += 4; }
	if (node.access_qualifier) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.struct_type_info) { inline_data_size += 4; }
	if (node.raster_order_group) { inline_data_size += 4; }
	if (node.type_size) { inline_data_size += 4; }
	if (node.type_align) { inline_data_size += 4; }
	if (node.type_name) { inline_data_size += 4; }
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	if (node.inline_type_info) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::BUFFER_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.buffer_size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.location_index) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.location_count) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.access_qualifier) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.struct_type_info) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 6) {
		state.write(uint16_t(0u));
	}
	if (node.raster_order_group) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 7) {
		state.write(uint16_t(0u));
	}
	if (node.type_size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 8) {
		state.write(uint16_t(0u));
	}
	if (node.type_align) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 9) {
		state.write(uint16_t(0u));
	}
	if (node.type_name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 10) {
		state.write(uint16_t(0u));
	}
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 11) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 12) {
		state.write(uint16_t(0u));
	}
	if (node.inline_type_info) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 13) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.buffer_size) {
		write(*node.buffer_size, state);
	}
	if (node.location_index) {
		write(*node.location_index, state);
	}
	if (node.location_count) {
		write(*node.location_count, state);
	}
	if (node.access_qualifier) {
		state.write(std::underlying_type_t<ACCESS_QUALIFIER>(*node.access_qualifier));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.write(std::underlying_type_t<ADDRESS_SPACE>(node.address_space));
	state.write(uint8_t(0));
	state.write(uint8_t(0));
	state.write(uint8_t(0));
	if (node.struct_type_info) {
		write(*node.struct_type_info, state);
	}
	if (node.raster_order_group) {
		write(*node.raster_order_group, state);
	}
	if (node.type_size) {
		write(*node.type_size, state);
	}
	if (node.type_align) {
		write(*node.type_align, state);
	}
	if (node.type_name) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.inline_type_info) {
		write(*node.inline_type_info, state);
	}
	state.pad();
	if (node.type_name) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(*node.type_name);
	}
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_buffer_stride_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 0u;
	if (node.unused) {
		field_count = 6u;
	} else if (node.name) {
		field_count = 5u;
	} else if (node.type_name) {
		field_count = 4u;
	} else if (node.location_count) {
		field_count = 3u;
	} else if (node.location_index) {
		field_count = 2u;
	} else if (node.function_constant) {
		field_count = 1u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	if (node.location_index) { inline_data_size += 4; }
	if (node.location_count) { inline_data_size += 4; }
	if (node.type_name) { inline_data_size += 4; }
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::BUFFER_STRIDE_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 0) {
		state.write(uint16_t(0u));
	}
	if (node.location_index) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 1) {
		state.write(uint16_t(0u));
	}
	if (node.location_count) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.type_name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 5) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.location_index) {
		write(*node.location_index, state);
	}
	if (node.location_count) {
		write(*node.location_count, state);
	}
	if (node.type_name) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	if (node.type_name) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(*node.type_name);
	}
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_ciarray_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 5u;
	if (node.name) {
		field_count = 6u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.inline_type_info) { inline_data_size += 4; }
	if (node.struct_type_info) { inline_data_size += 4; }
	inline_data_size += 4;
	inline_data_size += 4;
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::CIARRAY_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.inline_type_info) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.struct_type_info) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	state.write(inline_offset);
	inline_offset += 4;
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 5) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.inline_type_info) {
		write(*node.inline_type_info, state);
	}
	if (node.struct_type_info) {
		write(*node.struct_type_info, state);
	}
	write(node.type_size, state);
	write(node.type_align, state);
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_cibuiltin_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 1u;
	if (node.name) {
		field_count = 2u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::CIBUILTIN_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 1) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_cibuiltin_ret_t& node, writer_state_t& state) {
	uint32_t field_count = 1u;
	if (node.name) {
		field_count = 2u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::CIBUILTIN_RET));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 1) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_ci_function_t& node, writer_state_t& state) {
	uint32_t field_count = 1u;
	if (node.user_annotation) {
		field_count = 4u;
	} else if (node.arguments) {
		field_count = 3u;
	} else if (node.return_types) {
		field_count = 2u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	inline_data_size += 4;
	if (node.return_types) { inline_data_size += 4; }
	if (node.arguments) { inline_data_size += 4; }
	if (node.user_annotation) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::CI_FUNCTION));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	state.write(inline_offset);
	inline_offset += 4;
	if (node.return_types) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 1) {
		state.write(uint16_t(0u));
	}
	if (node.arguments) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.user_annotation) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 3> patch_offsets {};
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.return_types) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.arguments) {
		patch_offsets[2] = state.size();
		state.write(0u);
	}
	if (node.user_annotation) {
		write(*node.user_annotation, state);
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.name);
	if (node.return_types) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(uint32_t(node.return_types->size()));
		for (const auto& elem : *node.return_types) {
			write(elem, state);
		}
	}
	if (node.arguments) {
		state.patch(patch_offsets[2], state.size() - patch_offsets[2]);
		state.write(uint32_t(node.arguments->size()));
		for (const auto& elem : *node.arguments) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_ciimageblock_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 1u;
	if (node.name) {
		field_count = 2u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::CIIMAGEBLOCK_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 1) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_ciimageblock_ret_t& node, writer_state_t& state) {
	uint32_t field_count = 1u;
	if (node.name) {
		field_count = 2u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::CIIMAGEBLOCK_RET));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 1) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_cimatrix_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 1u;
	if (node.name) {
		field_count = 2u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::CIMATRIX_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 1) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_cimatrix_ret_t& node, writer_state_t& state) {
	uint32_t field_count = 1u;
	if (node.name) {
		field_count = 2u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::CIMATRIX_RET));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 1) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_cipadding_arg_t& node, writer_state_t& state) {
	const uint32_t field_count = 0u;
	uint32_t inline_data_size = sizeof(table_root_t);
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::CIPADDING_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	state.write(uint32_t(state.size() - vtable_start_offset));
	state.pad();
	return root_table_offset;
}

static inline uint32_t write_node(const node_cipointer_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 7u;
	if (node.name) {
		field_count = 8u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.access_qualifier) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.inline_type_info) { inline_data_size += 4; }
	if (node.struct_type_info) { inline_data_size += 4; }
	inline_data_size += 4;
	inline_data_size += 4;
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::CIPOINTER_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.access_qualifier) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.inline_type_info) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.struct_type_info) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	state.write(inline_offset);
	inline_offset += 4;
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 7) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.access_qualifier) {
		state.write(std::underlying_type_t<ACCESS_QUALIFIER>(*node.access_qualifier));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.write(std::underlying_type_t<ADDRESS_SPACE>(node.address_space));
	state.write(uint8_t(0));
	state.write(uint8_t(0));
	state.write(uint8_t(0));
	if (node.inline_type_info) {
		write(*node.inline_type_info, state);
	}
	if (node.struct_type_info) {
		write(*node.struct_type_info, state);
	}
	write(node.type_size, state);
	write(node.type_align, state);
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_cipointer_ret_t& node, writer_state_t& state) {
	const uint32_t field_count = 7u;
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.access_qualifier) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.inline_type_info) { inline_data_size += 4; }
	if (node.struct_type_info) { inline_data_size += 4; }
	inline_data_size += 4;
	inline_data_size += 4;
	inline_data_size += 4;
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::CIPOINTER_RET));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.access_qualifier) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.inline_type_info) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.struct_type_info) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	state.write(inline_offset);
	inline_offset += 4;
	state.write(inline_offset);
	inline_offset += 4;
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.access_qualifier) {
		state.write(std::underlying_type_t<ACCESS_QUALIFIER>(*node.access_qualifier));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.write(std::underlying_type_t<ADDRESS_SPACE>(node.address_space));
	state.write(uint8_t(0));
	state.write(uint8_t(0));
	state.write(uint8_t(0));
	if (node.inline_type_info) {
		write(*node.inline_type_info, state);
	}
	if (node.struct_type_info) {
		write(*node.struct_type_info, state);
	}
	write(node.type_size, state);
	write(node.type_align, state);
	patch_offsets[0] = state.size();
	state.write(0u);
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	return root_table_offset;
}

static inline uint32_t write_node(const node_cisampler_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 1u;
	if (node.name) {
		field_count = 2u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::CISAMPLER_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 1) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_cisampler_ret_t& node, writer_state_t& state) {
	uint32_t field_count = 1u;
	if (node.name) {
		field_count = 2u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::CISAMPLER_RET));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 1) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_cistruct_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.struct_type_info) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::CISTRUCT_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.struct_type_info) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.struct_type_info) {
		write(*node.struct_type_info, state);
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_cistruct_ret_t& node, writer_state_t& state) {
	const uint32_t field_count = 2u;
	uint32_t inline_data_size = sizeof(table_root_t);
	inline_data_size += 4;
	inline_data_size += 4;
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::CISTRUCT_RET));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	state.write(inline_offset);
	inline_offset += 4;
	state.write(inline_offset);
	inline_offset += 4;
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	write(node.struct_type_info, state);
	patch_offsets[0] = state.size();
	state.write(0u);
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	return root_table_offset;
}

static inline uint32_t write_node(const node_citexture_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 3u;
	if (node.name) {
		field_count = 4u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	inline_data_size += 4;
	if (node.access_qualifier) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::CITEXTURE_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	state.write(inline_offset);
	inline_offset += 4;
	if (node.access_qualifier) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	write(node.location_count, state);
	if (node.access_qualifier) {
		state.write(std::underlying_type_t<ACCESS_QUALIFIER>(*node.access_qualifier));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_citexture_ret_t& node, writer_state_t& state) {
	const uint32_t field_count = 3u;
	uint32_t inline_data_size = sizeof(table_root_t);
	inline_data_size += 4;
	if (node.access_qualifier) { inline_data_size += 4; }
	inline_data_size += 4;
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::CITEXTURE_RET));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	state.write(inline_offset);
	inline_offset += 4;
	if (node.access_qualifier) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	write(node.location_count, state);
	if (node.access_qualifier) {
		state.write(std::underlying_type_t<ACCESS_QUALIFIER>(*node.access_qualifier));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	return root_table_offset;
}

static inline uint32_t write_node(const node_char_type_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.qualifiers) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	inline_data_size += 4;
	inline_data_size += 4;
	if (node.qualifiers) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::CHAR_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	state.write(inline_offset);
	inline_offset += 4;
	state.write(inline_offset);
	inline_offset += 4;
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	state.write(node.size);
	state.write(node.alignment);
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_clip_distance_attr_t& node, writer_state_t& state) {
	const uint32_t field_count = 0u;
	uint32_t inline_data_size = sizeof(table_root_t);
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::CLIP_DISTANCE_ATTR));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	state.write(uint32_t(state.size() - vtable_start_offset));
	state.pad();
	return root_table_offset;
}

static inline uint32_t write_node(const node_clip_distance_ret_t& node, writer_state_t& state) {
	uint32_t field_count = 4u;
	if (node.name) {
		field_count = 5u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	if (node.array_size) { inline_data_size += 4; }
	if (node.shared) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::CLIP_DISTANCE_RET));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.array_size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.shared) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.array_size) {
		write(*node.array_size, state);
	}
	if (node.shared) {
		state.write(*node.shared);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_command_buffer_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 4u;
	if (node.unused) {
		field_count = 6u;
	} else if (node.name) {
		field_count = 5u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	if (node.location_index) { inline_data_size += 4; }
	if (node.location_count) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::COMMAND_BUFFER_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.location_index) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.location_count) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 5) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.location_index) {
		write(*node.location_index, state);
	}
	if (node.location_count) {
		write(*node.location_count, state);
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_command_buffer_type_t& node, writer_state_t& state) {
	uint32_t field_count = 0u;
	if (node.qualifiers) {
		field_count = 3u;
	} else if (node.alignment) {
		field_count = 2u;
	} else if (node.size) {
		field_count = 1u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::COMMAND_BUFFER_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 0) {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 1) {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_compute_pipeline_state_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 4u;
	if (node.unused) {
		field_count = 6u;
	} else if (node.name) {
		field_count = 5u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	if (node.location_index) { inline_data_size += 4; }
	if (node.location_count) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::COMPUTE_PIPELINE_STATE_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.location_index) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.location_count) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 5) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.location_index) {
		write(*node.location_index, state);
	}
	if (node.location_count) {
		write(*node.location_count, state);
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_compute_pipeline_state_type_t& node, writer_state_t& state) {
	uint32_t field_count = 0u;
	if (node.qualifiers) {
		field_count = 3u;
	} else if (node.alignment) {
		field_count = 2u;
	} else if (node.size) {
		field_count = 1u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::COMPUTE_PIPELINE_STATE_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 0) {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 1) {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_constant_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 6u;
	if (node.unused) {
		field_count = 9u;
	} else if (node.name) {
		field_count = 8u;
	} else if (node.type_name) {
		field_count = 7u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	if (node.location_index) { inline_data_size += 4; }
	if (node.location_count) { inline_data_size += 4; }
	if (node.struct_type_info) { inline_data_size += 4; }
	inline_data_size += 4;
	inline_data_size += 4;
	if (node.type_name) { inline_data_size += 4; }
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::CONSTANT_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.location_index) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.location_count) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.struct_type_info) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	state.write(inline_offset);
	inline_offset += 4;
	if (node.type_name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 6) {
		state.write(uint16_t(0u));
	}
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 7) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 8) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.location_index) {
		write(*node.location_index, state);
	}
	if (node.location_count) {
		write(*node.location_count, state);
	}
	if (node.struct_type_info) {
		write(*node.struct_type_info, state);
	}
	write(node.type_size, state);
	write(node.type_align, state);
	if (node.type_name) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	if (node.type_name) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(*node.type_name);
	}
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_continue_search_ret_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::CONTINUE_SEARCH_RET));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_control_point_field_t& node, writer_state_t& state) {
	uint32_t field_count = 4u;
	if (node.unused) {
		field_count = 6u;
	} else if (node.name) {
		field_count = 5u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	if (node.location_index) { inline_data_size += 4; }
	if (node.location_count) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::CONTROL_POINT_FIELD));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.location_index) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.location_count) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 5) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.location_index) {
		write(*node.location_index, state);
	}
	if (node.location_count) {
		write(*node.location_count, state);
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_control_point_index_buffer_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::CONTROL_POINT_INDEX_BUFFER_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_control_point_input_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 0u;
	if (node.unused) {
		field_count = 3u;
	} else if (node.fields) {
		field_count = 2u;
	} else if (node.function_constant) {
		field_count = 1u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	if (node.fields) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::CONTROL_POINT_INPUT_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 0) {
		state.write(uint16_t(0u));
	}
	if (node.fields) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 1) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.fields) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	if (node.fields) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.fields->size()));
		for (const auto& elem : *node.fields) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_curve_parameter_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::CURVE_PARAMETER_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_depth2d_array_type_t& node, writer_state_t& state) {
	uint32_t field_count = 4u;
	if (node.access_qualifier) {
		field_count = 5u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.access_qualifier) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::DEPTH2D_ARRAY_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.access_qualifier) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	write(node.channel_type, state);
	if (node.access_qualifier) {
		state.write(std::underlying_type_t<ACCESS_QUALIFIER>(*node.access_qualifier));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_depth2d_ms_array_type_t& node, writer_state_t& state) {
	uint32_t field_count = 4u;
	if (node.access_qualifier) {
		field_count = 5u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.access_qualifier) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::DEPTH2D_MS_ARRAY_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.access_qualifier) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	write(node.channel_type, state);
	if (node.access_qualifier) {
		state.write(std::underlying_type_t<ACCESS_QUALIFIER>(*node.access_qualifier));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_depth2d_ms_type_t& node, writer_state_t& state) {
	uint32_t field_count = 4u;
	if (node.access_qualifier) {
		field_count = 5u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.access_qualifier) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::DEPTH2D_MS_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.access_qualifier) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	write(node.channel_type, state);
	if (node.access_qualifier) {
		state.write(std::underlying_type_t<ACCESS_QUALIFIER>(*node.access_qualifier));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_depth2d_type_t& node, writer_state_t& state) {
	uint32_t field_count = 4u;
	if (node.access_qualifier) {
		field_count = 5u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.access_qualifier) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::DEPTH2D_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.access_qualifier) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	write(node.channel_type, state);
	if (node.access_qualifier) {
		state.write(std::underlying_type_t<ACCESS_QUALIFIER>(*node.access_qualifier));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_depth_cube_array_type_t& node, writer_state_t& state) {
	uint32_t field_count = 4u;
	if (node.access_qualifier) {
		field_count = 5u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.access_qualifier) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::DEPTH_CUBE_ARRAY_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.access_qualifier) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	write(node.channel_type, state);
	if (node.access_qualifier) {
		state.write(std::underlying_type_t<ACCESS_QUALIFIER>(*node.access_qualifier));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_depth_cube_type_t& node, writer_state_t& state) {
	uint32_t field_count = 4u;
	if (node.access_qualifier) {
		field_count = 5u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.access_qualifier) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::DEPTH_CUBE_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.access_qualifier) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	write(node.channel_type, state);
	if (node.access_qualifier) {
		state.write(std::underlying_type_t<ACCESS_QUALIFIER>(*node.access_qualifier));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_depth_ret_t& node, writer_state_t& state) {
	uint32_t field_count = 3u;
	if (node.name) {
		field_count = 4u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	if (node.depth_qualifier) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::DEPTH_RET));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.depth_qualifier) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.depth_qualifier) {
		state.write(std::underlying_type_t<DEPTH_QUALIFIER>(*node.depth_qualifier));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_depth_stencil_state_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 4u;
	if (node.unused) {
		field_count = 6u;
	} else if (node.name) {
		field_count = 5u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	if (node.location_index) { inline_data_size += 4; }
	if (node.location_count) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::DEPTH_STENCIL_STATE_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.location_index) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.location_count) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 5) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.location_index) {
		write(*node.location_index, state);
	}
	if (node.location_count) {
		write(*node.location_count, state);
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_depth_stencil_state_type_t& node, writer_state_t& state) {
	uint32_t field_count = 0u;
	if (node.qualifiers) {
		field_count = 3u;
	} else if (node.alignment) {
		field_count = 2u;
	} else if (node.size) {
		field_count = 1u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::DEPTH_STENCIL_STATE_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 0) {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 1) {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_direction_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::DIRECTION_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_dispatch_quadgroups_per_threadgroup_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::DISPATCH_QUADGROUPS_PER_THREADGROUP_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_dispatch_simdgroups_per_threadgroup_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::DISPATCH_SIMDGROUPS_PER_THREADGROUP_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_dispatch_threads_per_threadgroup_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::DISPATCH_THREADS_PER_THREADGROUP_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_distance_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::DISTANCE_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_distance_ret_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::DISTANCE_RET));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_double_type_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.qualifiers) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	inline_data_size += 4;
	inline_data_size += 4;
	if (node.qualifiers) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::DOUBLE_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	state.write(inline_offset);
	inline_offset += 4;
	state.write(inline_offset);
	inline_offset += 4;
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	state.write(node.size);
	state.write(node.alignment);
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_enum_type_t& node, writer_state_t& state) {
	uint32_t field_count = 0u;
	if (node.underlying_type) {
		field_count = 5u;
	} else if (node.name) {
		field_count = 4u;
	} else if (node.qualifiers) {
		field_count = 3u;
	} else if (node.alignment) {
		field_count = 2u;
	} else if (node.size) {
		field_count = 1u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	if (node.name) { inline_data_size += 4; }
	if (node.underlying_type) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::ENUM_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 0) {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 1) {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	if (node.underlying_type) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.underlying_type) {
		write(*node.underlying_type, state);
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_extents_type_t& node, writer_state_t& state) {
	const uint32_t field_count = 5u;
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	inline_data_size += 4;
	inline_data_size += 4;
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::EXTENTS_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	state.write(inline_offset);
	inline_offset += 4;
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	write(node.index_type, state);
	patch_offsets[1] = state.size();
	state.write(0u);
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
	state.write(uint32_t(node.extents.size()));
	for (const auto& elem : node.extents) {
		state.write(elem);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_float_type_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.qualifiers) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	inline_data_size += 4;
	inline_data_size += 4;
	if (node.qualifiers) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::FLOAT_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	state.write(inline_offset);
	inline_offset += 4;
	state.write(inline_offset);
	inline_offset += 4;
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	state.write(node.size);
	state.write(node.alignment);
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_fragment_function_t& node, writer_state_t& state) {
	uint32_t field_count = 1u;
	if (node.user_annotation) {
		field_count = 5u;
	} else if (node.early_fragment_tests) {
		field_count = 4u;
	} else if (node.arguments) {
		field_count = 3u;
	} else if (node.return_type) {
		field_count = 2u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	inline_data_size += 4;
	if (node.return_type) { inline_data_size += 4; }
	if (node.arguments) { inline_data_size += 4; }
	if (node.early_fragment_tests) { inline_data_size += 4; }
	if (node.user_annotation) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::FRAGMENT_FUNCTION));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	state.write(inline_offset);
	inline_offset += 4;
	if (node.return_type) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 1) {
		state.write(uint16_t(0u));
	}
	if (node.arguments) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.early_fragment_tests) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	if (node.user_annotation) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 3> patch_offsets {};
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.return_type) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.arguments) {
		patch_offsets[2] = state.size();
		state.write(0u);
	}
	if (node.early_fragment_tests) {
		state.write(*node.early_fragment_tests);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.user_annotation) {
		write(*node.user_annotation, state);
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.name);
	if (node.return_type) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(uint32_t(node.return_type->size()));
		for (const auto& elem : *node.return_type) {
			write(elem, state);
		}
	}
	if (node.arguments) {
		state.patch(patch_offsets[2], state.size() - patch_offsets[2]);
		state.write(uint32_t(node.arguments->size()));
		for (const auto& elem : *node.arguments) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_fragment_input_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 6u;
	if (node.unused) {
		field_count = 8u;
	} else if (node.name) {
		field_count = 7u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.location) { inline_data_size += 4; }
	if (node.sampling_qualifier) { inline_data_size += 4; }
	if (node.interpolation_qualifier) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::FRAGMENT_INPUT_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.location) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.sampling_qualifier) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.interpolation_qualifier) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 6) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 7) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 3> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.location) {
		write(*node.location, state);
	}
	if (node.sampling_qualifier) {
		state.write(std::underlying_type_t<SAMPLING_QUALIFIER>(*node.sampling_qualifier));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.interpolation_qualifier) {
		state.write(std::underlying_type_t<INTERPOLATION_QUALIFIER>(*node.interpolation_qualifier));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[1] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[2] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.attribute_name);
	state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[2], state.size() - patch_offsets[2]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_front_facing_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::FRONT_FACING_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_function_constant_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.required) {
		field_count = 4u;
	} else if (node.index) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	inline_data_size += 4;
	inline_data_size += 4;
	if (node.index) { inline_data_size += 4; }
	if (node.required) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::FUNCTION_CONSTANT));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	state.write(inline_offset);
	inline_offset += 4;
	state.write(inline_offset);
	inline_offset += 4;
	if (node.index) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.required) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	patch_offsets[0] = state.size();
	state.write(0u);
	patch_offsets[1] = state.size();
	state.write(0u);
	if (node.index) {
		state.write(*node.index);
	}
	if (node.required) {
		state.write(*node.required);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
	state.write(node.name);
	return root_table_offset;
}

static inline uint32_t write_node(const node_function_constant_predicate_attr_t& node, writer_state_t& state) {
	uint32_t field_count = 0u;
	if (node.predicate) {
		field_count = 1u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.predicate) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::FUNCTION_CONSTANT_PREDICATE_ATTR));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.predicate) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 0) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	if (node.predicate) {
		write(*node.predicate, state);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	return root_table_offset;
}

static inline uint32_t write_node(const node_function_handle_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 4u;
	if (node.unused) {
		field_count = 6u;
	} else if (node.name) {
		field_count = 5u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	if (node.location_index) { inline_data_size += 4; }
	if (node.location_count) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::FUNCTION_HANDLE_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.location_index) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.location_count) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 5) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.location_index) {
		write(*node.location_index, state);
	}
	if (node.location_count) {
		write(*node.location_count, state);
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_function_handle_type_t& node, writer_state_t& state) {
	uint32_t field_count = 0u;
	if (node.qualifiers) {
		field_count = 3u;
	} else if (node.alignment) {
		field_count = 2u;
	} else if (node.size) {
		field_count = 1u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::FUNCTION_HANDLE_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 0) {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 1) {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_function_id_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::FUNCTION_ID_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_function_type_t& node, writer_state_t& state) {
	uint32_t field_count = 4u;
	if (node.param_types) {
		field_count = 5u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.param_types) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::FUNCTION_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.param_types) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	write(node.return_type, state);
	if (node.param_types) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	if (node.param_types) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(uint32_t(node.param_types->size()));
		for (const auto& elem : *node.param_types) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_geometry_id_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::GEOMETRY_ID_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_geometry_intersection_function_table_offset_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::GEOMETRY_INTERSECTION_FUNCTION_TABLE_OFFSET_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_global_binding_t& node, writer_state_t& state) {
	const uint32_t field_count = 2u;
	uint32_t inline_data_size = sizeof(table_root_t);
	inline_data_size += 4;
	inline_data_size += 4;
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::GLOBAL_BINDING));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	state.write(inline_offset);
	inline_offset += 4;
	state.write(inline_offset);
	inline_offset += 4;
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	patch_offsets[0] = state.size();
	state.write(0u);
	write(node.argument, state);
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.name);
	return root_table_offset;
}

static inline uint32_t write_node(const node_half_type_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.qualifiers) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	inline_data_size += 4;
	inline_data_size += 4;
	if (node.qualifiers) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::HALF_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	state.write(inline_offset);
	inline_offset += 4;
	state.write(inline_offset);
	inline_offset += 4;
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	state.write(node.size);
	state.write(node.alignment);
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_imageblock_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 6u;
	if (node.unused) {
		field_count = 9u;
	} else if (node.name) {
		field_count = 8u;
	} else if (node.type_name) {
		field_count = 7u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	if (node.data_size) { inline_data_size += 4; }
	if (node.struct_type_info) { inline_data_size += 4; }
	if (node.alias_all_render_targets) { inline_data_size += 4; }
	if (node.alias_render_target_index) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.type_name) { inline_data_size += 4; }
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::IMAGEBLOCK_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.data_size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.struct_type_info) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alias_all_render_targets) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alias_render_target_index) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.type_name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 6) {
		state.write(uint16_t(0u));
	}
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 7) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 8) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.data_size) {
		write(*node.data_size, state);
	}
	if (node.struct_type_info) {
		write(*node.struct_type_info, state);
	}
	if (node.alias_all_render_targets) {
		state.write(*node.alias_all_render_targets);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.alias_render_target_index) {
		write(*node.alias_render_target_index, state);
	}
	write(node.type_align, state);
	if (node.type_name) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	if (node.type_name) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(*node.type_name);
	}
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_imageblock_data_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 7u;
	if (node.unused) {
		field_count = 10u;
	} else if (node.name) {
		field_count = 9u;
	} else if (node.type_name) {
		field_count = 8u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.struct_type_info) { inline_data_size += 4; }
	if (node.master) { inline_data_size += 4; }
	if (node.alias_all_render_targets) { inline_data_size += 4; }
	if (node.alias_render_target_index) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.type_name) { inline_data_size += 4; }
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::IMAGEBLOCK_DATA_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.struct_type_info) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.master) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alias_all_render_targets) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alias_render_target_index) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.type_name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 7) {
		state.write(uint16_t(0u));
	}
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 8) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 9) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	write(node.data_size, state);
	if (node.struct_type_info) {
		write(*node.struct_type_info, state);
	}
	if (node.master) {
		write(*node.master, state);
	}
	if (node.alias_all_render_targets) {
		state.write(*node.alias_all_render_targets);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.alias_render_target_index) {
		write(*node.alias_render_target_index, state);
	}
	write(node.type_align, state);
	if (node.type_name) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	if (node.type_name) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(*node.type_name);
	}
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_imageblock_data_ret_t& node, writer_state_t& state) {
	uint32_t field_count = 7u;
	if (node.name) {
		field_count = 9u;
	} else if (node.type_name) {
		field_count = 8u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	if (node.data_size) { inline_data_size += 4; }
	if (node.struct_type_info) { inline_data_size += 4; }
	if (node.master) { inline_data_size += 4; }
	if (node.alias_all_render_targets) { inline_data_size += 4; }
	if (node.alias_render_target_index) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.type_name) { inline_data_size += 4; }
	if (node.name) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::IMAGEBLOCK_DATA_RET));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.data_size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.struct_type_info) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.master) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alias_all_render_targets) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alias_render_target_index) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.type_name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 7) {
		state.write(uint16_t(0u));
	}
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 8) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.data_size) {
		write(*node.data_size, state);
	}
	if (node.struct_type_info) {
		write(*node.struct_type_info, state);
	}
	if (node.master) {
		write(*node.master, state);
	}
	if (node.alias_all_render_targets) {
		state.write(*node.alias_all_render_targets);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.alias_render_target_index) {
		write(*node.alias_render_target_index, state);
	}
	write(node.type_align, state);
	if (node.type_name) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	state.pad();
	if (node.type_name) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(*node.type_name);
	}
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_imageblock_type_t& node, writer_state_t& state) {
	const uint32_t field_count = 5u;
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	if (node.layout) { inline_data_size += 4; }
	inline_data_size += 4;
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::IMAGEBLOCK_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.layout) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	if (node.layout) {
		state.write(std::underlying_type_t<IMAGEBLOCK_LAYOUT>(*node.layout));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	write(node.data_type, state);
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_indirect_buffer_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 6u;
	if (node.inline_type_info) {
		field_count = 14u;
	} else if (node.unused) {
		field_count = 13u;
	} else if (node.name) {
		field_count = 12u;
	} else if (node.type_name) {
		field_count = 11u;
	} else if (node.type_align) {
		field_count = 10u;
	} else if (node.type_size) {
		field_count = 9u;
	} else if (node.raster_order_group) {
		field_count = 8u;
	} else if (node.struct_type_info) {
		field_count = 7u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	if (node.buffer_size) { inline_data_size += 4; }
	if (node.location_index) { inline_data_size += 4; }
	if (node.location_count) { inline_data_size += 4; }
	if (node.access_qualifier) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.struct_type_info) { inline_data_size += 4; }
	if (node.raster_order_group) { inline_data_size += 4; }
	if (node.type_size) { inline_data_size += 4; }
	if (node.type_align) { inline_data_size += 4; }
	if (node.type_name) { inline_data_size += 4; }
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	if (node.inline_type_info) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::INDIRECT_BUFFER_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.buffer_size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.location_index) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.location_count) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.access_qualifier) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.struct_type_info) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 6) {
		state.write(uint16_t(0u));
	}
	if (node.raster_order_group) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 7) {
		state.write(uint16_t(0u));
	}
	if (node.type_size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 8) {
		state.write(uint16_t(0u));
	}
	if (node.type_align) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 9) {
		state.write(uint16_t(0u));
	}
	if (node.type_name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 10) {
		state.write(uint16_t(0u));
	}
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 11) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 12) {
		state.write(uint16_t(0u));
	}
	if (node.inline_type_info) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 13) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.buffer_size) {
		write(*node.buffer_size, state);
	}
	if (node.location_index) {
		write(*node.location_index, state);
	}
	if (node.location_count) {
		write(*node.location_count, state);
	}
	if (node.access_qualifier) {
		state.write(std::underlying_type_t<ACCESS_QUALIFIER>(*node.access_qualifier));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.write(std::underlying_type_t<ADDRESS_SPACE>(node.address_space));
	state.write(uint8_t(0));
	state.write(uint8_t(0));
	state.write(uint8_t(0));
	if (node.struct_type_info) {
		write(*node.struct_type_info, state);
	}
	if (node.raster_order_group) {
		write(*node.raster_order_group, state);
	}
	if (node.type_size) {
		write(*node.type_size, state);
	}
	if (node.type_align) {
		write(*node.type_align, state);
	}
	if (node.type_name) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.inline_type_info) {
		write(*node.inline_type_info, state);
	}
	state.pad();
	if (node.type_name) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(*node.type_name);
	}
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_indirect_constant_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 0u;
	if (node.unused) {
		field_count = 6u;
	} else if (node.name) {
		field_count = 5u;
	} else if (node.type_name) {
		field_count = 4u;
	} else if (node.location_count) {
		field_count = 3u;
	} else if (node.location_index) {
		field_count = 2u;
	} else if (node.function_constant) {
		field_count = 1u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	if (node.location_index) { inline_data_size += 4; }
	if (node.location_count) { inline_data_size += 4; }
	if (node.type_name) { inline_data_size += 4; }
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::INDIRECT_CONSTANT_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 0) {
		state.write(uint16_t(0u));
	}
	if (node.location_index) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 1) {
		state.write(uint16_t(0u));
	}
	if (node.location_count) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.type_name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 5) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.location_index) {
		write(*node.location_index, state);
	}
	if (node.location_count) {
		write(*node.location_count, state);
	}
	if (node.type_name) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	if (node.type_name) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(*node.type_name);
	}
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_inline_type_info_t& node, writer_state_t& state) {
	uint32_t field_count = 1u;
	if (node.indirect_location) {
		field_count = 9u;
	} else if (node.indirect_argument) {
		field_count = 8u;
	} else if (node.type_name) {
		field_count = 7u;
	} else if (node.array_entries) {
		field_count = 6u;
	} else if (node.alignment) {
		field_count = 5u;
	} else if (node.size) {
		field_count = 4u;
	} else if (node.struct_type_info) {
		field_count = 3u;
	} else if (node.inline_type_info) {
		field_count = 2u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	inline_data_size += 4;
	if (node.inline_type_info) { inline_data_size += 4; }
	if (node.struct_type_info) { inline_data_size += 4; }
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.array_entries) { inline_data_size += 4; }
	if (node.type_name) { inline_data_size += 4; }
	if (node.indirect_argument) { inline_data_size += 4; }
	if (node.indirect_location) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::INLINE_TYPE_INFO));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	state.write(inline_offset);
	inline_offset += 4;
	if (node.inline_type_info) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 1) {
		state.write(uint16_t(0u));
	}
	if (node.struct_type_info) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	if (node.array_entries) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 5) {
		state.write(uint16_t(0u));
	}
	if (node.type_name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 6) {
		state.write(uint16_t(0u));
	}
	if (node.indirect_argument) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 7) {
		state.write(uint16_t(0u));
	}
	if (node.indirect_location) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 8) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	state.write(std::underlying_type_t<ADDRESS_SPACE>(node.address_space));
	state.write(uint8_t(0));
	state.write(uint8_t(0));
	state.write(uint8_t(0));
	if (node.inline_type_info) {
		write(*node.inline_type_info, state);
	}
	if (node.struct_type_info) {
		write(*node.struct_type_info, state);
	}
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.array_entries) {
		state.write(*node.array_entries);
	}
	if (node.type_name) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	if (node.indirect_argument) {
		write(*node.indirect_argument, state);
	}
	if (node.indirect_location) {
		write(*node.indirect_location, state);
	}
	state.pad();
	if (node.type_name) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(*node.type_name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_instance_acceleration_structure_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 6u;
	if (node.unused) {
		field_count = 8u;
	} else if (node.name) {
		field_count = 7u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	if (node.location_index) { inline_data_size += 4; }
	if (node.location_count) { inline_data_size += 4; }
	if (node.access_qualifier) { inline_data_size += 4; }
	if (node.raster_order_group) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::INSTANCE_ACCELERATION_STRUCTURE_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.location_index) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.location_count) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.access_qualifier) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.raster_order_group) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 6) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 7) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.location_index) {
		write(*node.location_index, state);
	}
	if (node.location_count) {
		write(*node.location_count, state);
	}
	if (node.access_qualifier) {
		state.write(std::underlying_type_t<ACCESS_QUALIFIER>(*node.access_qualifier));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.raster_order_group) {
		write(*node.raster_order_group, state);
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_instance_id_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::INSTANCE_ID_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_instance_id_count_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::INSTANCE_ID_COUNT_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_instance_intersection_function_table_offset_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::INSTANCE_INTERSECTION_FUNCTION_TABLE_OFFSET_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_int_type_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.qualifiers) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	inline_data_size += 4;
	inline_data_size += 4;
	if (node.qualifiers) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::INT_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	state.write(inline_offset);
	inline_offset += 4;
	state.write(inline_offset);
	inline_offset += 4;
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	state.write(node.size);
	state.write(node.alignment);
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_interpolant_type_t& node, writer_state_t& state) {
	const uint32_t field_count = 5u;
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	if (node.perspective) { inline_data_size += 4; }
	inline_data_size += 4;
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::INTERPOLANT_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.perspective) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	if (node.perspective) {
		state.write(*node.perspective);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	write(node.value_type, state);
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_intersection_function_t& node, writer_state_t& state) {
	uint32_t field_count = 1u;
	if (node.user_annotation) {
		field_count = 15u;
	} else if (node.user_data) {
		field_count = 14u;
	} else if (node.intersection_function_buffer) {
		field_count = 13u;
	} else if (node.multi_level_instancing) {
		field_count = 12u;
	} else if (node.curve_data) {
		field_count = 11u;
	} else if (node.extended_limits) {
		field_count = 10u;
	} else if (node.instance_motion) {
		field_count = 9u;
	} else if (node.primitive_motion) {
		field_count = 8u;
	} else if (node.world_space_data) {
		field_count = 7u;
	} else if (node.triangle_data) {
		field_count = 6u;
	} else if (node.instancing) {
		field_count = 5u;
	} else if (node.primitive_kind) {
		field_count = 4u;
	} else if (node.arguments) {
		field_count = 3u;
	} else if (node.return_types) {
		field_count = 2u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	inline_data_size += 4;
	if (node.return_types) { inline_data_size += 4; }
	if (node.arguments) { inline_data_size += 4; }
	if (node.primitive_kind) { inline_data_size += 4; }
	if (node.instancing) { inline_data_size += 4; }
	if (node.triangle_data) { inline_data_size += 4; }
	if (node.world_space_data) { inline_data_size += 4; }
	if (node.primitive_motion) { inline_data_size += 4; }
	if (node.instance_motion) { inline_data_size += 4; }
	if (node.extended_limits) { inline_data_size += 4; }
	if (node.curve_data) { inline_data_size += 4; }
	if (node.multi_level_instancing) { inline_data_size += 4; }
	if (node.intersection_function_buffer) { inline_data_size += 4; }
	if (node.user_data) { inline_data_size += 4; }
	if (node.user_annotation) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::INTERSECTION_FUNCTION));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	state.write(inline_offset);
	inline_offset += 4;
	if (node.return_types) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 1) {
		state.write(uint16_t(0u));
	}
	if (node.arguments) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.primitive_kind) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	if (node.instancing) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	if (node.triangle_data) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 5) {
		state.write(uint16_t(0u));
	}
	if (node.world_space_data) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 6) {
		state.write(uint16_t(0u));
	}
	if (node.primitive_motion) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 7) {
		state.write(uint16_t(0u));
	}
	if (node.instance_motion) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 8) {
		state.write(uint16_t(0u));
	}
	if (node.extended_limits) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 9) {
		state.write(uint16_t(0u));
	}
	if (node.curve_data) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 10) {
		state.write(uint16_t(0u));
	}
	if (node.multi_level_instancing) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 11) {
		state.write(uint16_t(0u));
	}
	if (node.intersection_function_buffer) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 12) {
		state.write(uint16_t(0u));
	}
	if (node.user_data) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 13) {
		state.write(uint16_t(0u));
	}
	if (node.user_annotation) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 14) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 3> patch_offsets {};
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.return_types) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.arguments) {
		patch_offsets[2] = state.size();
		state.write(0u);
	}
	if (node.primitive_kind) {
		state.write(std::underlying_type_t<PRIMITIVE_KIND>(*node.primitive_kind));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.instancing) {
		state.write(*node.instancing);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.triangle_data) {
		state.write(*node.triangle_data);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.world_space_data) {
		state.write(*node.world_space_data);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.primitive_motion) {
		state.write(*node.primitive_motion);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.instance_motion) {
		state.write(*node.instance_motion);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.extended_limits) {
		state.write(*node.extended_limits);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.curve_data) {
		state.write(*node.curve_data);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.multi_level_instancing) {
		state.write(*node.multi_level_instancing);
	}
	if (node.intersection_function_buffer) {
		state.write(*node.intersection_function_buffer);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.user_data) {
		state.write(*node.user_data);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.user_annotation) {
		write(*node.user_annotation, state);
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.name);
	if (node.return_types) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(uint32_t(node.return_types->size()));
		for (const auto& elem : *node.return_types) {
			write(elem, state);
		}
	}
	if (node.arguments) {
		state.patch(patch_offsets[2], state.size() - patch_offsets[2]);
		state.write(uint32_t(node.arguments->size()));
		for (const auto& elem : *node.arguments) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_intersection_function_handle_type_t& node, writer_state_t& state) {
	uint32_t field_count = 0u;
	if (node.extended_limits) {
		field_count = 13u;
	} else if (node.instance_motion) {
		field_count = 12u;
	} else if (node.primitive_motion) {
		field_count = 11u;
	} else if (node.user_data) {
		field_count = 10u;
	} else if (node.world_space_data) {
		field_count = 9u;
	} else if (node.curve_data) {
		field_count = 8u;
	} else if (node.triangle_data) {
		field_count = 7u;
	} else if (node.multi_level_instancing) {
		field_count = 6u;
	} else if (node.instancing) {
		field_count = 5u;
	} else if (node.intersection_function_buffer) {
		field_count = 4u;
	} else if (node.qualifiers) {
		field_count = 3u;
	} else if (node.alignment) {
		field_count = 2u;
	} else if (node.size) {
		field_count = 1u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	if (node.intersection_function_buffer) { inline_data_size += 4; }
	if (node.instancing) { inline_data_size += 4; }
	if (node.multi_level_instancing) { inline_data_size += 4; }
	if (node.triangle_data) { inline_data_size += 4; }
	if (node.curve_data) { inline_data_size += 4; }
	if (node.world_space_data) { inline_data_size += 4; }
	if (node.user_data) { inline_data_size += 4; }
	if (node.primitive_motion) { inline_data_size += 4; }
	if (node.instance_motion) { inline_data_size += 4; }
	if (node.extended_limits) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::INTERSECTION_FUNCTION_HANDLE_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 0) {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 1) {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.intersection_function_buffer) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	if (node.instancing) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	if (node.multi_level_instancing) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 5) {
		state.write(uint16_t(0u));
	}
	if (node.triangle_data) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 6) {
		state.write(uint16_t(0u));
	}
	if (node.curve_data) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 7) {
		state.write(uint16_t(0u));
	}
	if (node.world_space_data) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 8) {
		state.write(uint16_t(0u));
	}
	if (node.user_data) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 9) {
		state.write(uint16_t(0u));
	}
	if (node.primitive_motion) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 10) {
		state.write(uint16_t(0u));
	}
	if (node.instance_motion) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 11) {
		state.write(uint16_t(0u));
	}
	if (node.extended_limits) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 12) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	if (node.intersection_function_buffer) {
		state.write(*node.intersection_function_buffer);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.instancing) {
		state.write(*node.instancing);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.multi_level_instancing) {
		state.write(*node.multi_level_instancing);
	}
	if (node.triangle_data) {
		state.write(*node.triangle_data);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.curve_data) {
		state.write(*node.curve_data);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.world_space_data) {
		state.write(*node.world_space_data);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.user_data) {
		state.write(*node.user_data);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.primitive_motion) {
		state.write(*node.primitive_motion);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.instance_motion) {
		state.write(*node.instance_motion);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.extended_limits) {
		state.write(*node.extended_limits);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_intersection_function_table_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 6u;
	if (node.unused) {
		field_count = 8u;
	} else if (node.name) {
		field_count = 7u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	if (node.location_index) { inline_data_size += 4; }
	if (node.location_count) { inline_data_size += 4; }
	if (node.access_qualifier) { inline_data_size += 4; }
	if (node.raster_order_group) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::INTERSECTION_FUNCTION_TABLE_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.location_index) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.location_count) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.access_qualifier) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.raster_order_group) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 6) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 7) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.location_index) {
		write(*node.location_index, state);
	}
	if (node.location_count) {
		write(*node.location_count, state);
	}
	if (node.access_qualifier) {
		state.write(std::underlying_type_t<ACCESS_QUALIFIER>(*node.access_qualifier));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.raster_order_group) {
		write(*node.raster_order_group, state);
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_intersection_function_table_type_t& node, writer_state_t& state) {
	uint32_t field_count = 0u;
	if (node.multi_level_instancing) {
		field_count = 11u;
	} else if (node.curve_data) {
		field_count = 10u;
	} else if (node.extended_limits) {
		field_count = 9u;
	} else if (node.instance_motion) {
		field_count = 8u;
	} else if (node.primitive_motion) {
		field_count = 7u;
	} else if (node.world_space_data) {
		field_count = 6u;
	} else if (node.triangle_data) {
		field_count = 5u;
	} else if (node.instancing) {
		field_count = 4u;
	} else if (node.qualifiers) {
		field_count = 3u;
	} else if (node.alignment) {
		field_count = 2u;
	} else if (node.size) {
		field_count = 1u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	if (node.instancing) { inline_data_size += 4; }
	if (node.triangle_data) { inline_data_size += 4; }
	if (node.world_space_data) { inline_data_size += 4; }
	if (node.primitive_motion) { inline_data_size += 4; }
	if (node.instance_motion) { inline_data_size += 4; }
	if (node.extended_limits) { inline_data_size += 4; }
	if (node.curve_data) { inline_data_size += 4; }
	if (node.multi_level_instancing) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::INTERSECTION_FUNCTION_TABLE_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 0) {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 1) {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.instancing) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	if (node.triangle_data) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	if (node.world_space_data) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 5) {
		state.write(uint16_t(0u));
	}
	if (node.primitive_motion) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 6) {
		state.write(uint16_t(0u));
	}
	if (node.instance_motion) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 7) {
		state.write(uint16_t(0u));
	}
	if (node.extended_limits) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 8) {
		state.write(uint16_t(0u));
	}
	if (node.curve_data) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 9) {
		state.write(uint16_t(0u));
	}
	if (node.multi_level_instancing) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 10) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	if (node.instancing) {
		state.write(*node.instancing);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.triangle_data) {
		state.write(*node.triangle_data);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.world_space_data) {
		state.write(*node.world_space_data);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.primitive_motion) {
		state.write(*node.primitive_motion);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.instance_motion) {
		state.write(*node.instance_motion);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.extended_limits) {
		state.write(*node.extended_limits);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.curve_data) {
		state.write(*node.curve_data);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.multi_level_instancing) {
		state.write(*node.multi_level_instancing);
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_invariant_attr_t& node, writer_state_t& state) {
	const uint32_t field_count = 0u;
	uint32_t inline_data_size = sizeof(table_root_t);
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::INVARIANT_ATTR));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	state.write(uint32_t(state.size() - vtable_start_offset));
	state.pad();
	return root_table_offset;
}

static inline uint32_t write_node(const node_kernel_function_t& node, writer_state_t& state) {
	uint32_t field_count = 1u;
	if (node.user_annotation) {
		field_count = 8u;
	} else if (node.workgroup_max_size) {
		field_count = 7u;
	} else if (node.workgroup_size_hint) {
		field_count = 6u;
	} else if (node.workgroup_size) {
		field_count = 5u;
	} else if (node.vec_type_hint) {
		field_count = 4u;
	} else if (node.arguments) {
		field_count = 3u;
	} else if (node.return_types) {
		field_count = 2u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	inline_data_size += 4;
	if (node.return_types) { inline_data_size += 4; }
	if (node.arguments) { inline_data_size += 4; }
	if (node.vec_type_hint) { inline_data_size += 4; }
	if (node.workgroup_size) { inline_data_size += 4; }
	if (node.workgroup_size_hint) { inline_data_size += 4; }
	if (node.workgroup_max_size) { inline_data_size += 4; }
	if (node.user_annotation) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::KERNEL_FUNCTION));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	state.write(inline_offset);
	inline_offset += 4;
	if (node.return_types) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 1) {
		state.write(uint16_t(0u));
	}
	if (node.arguments) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.vec_type_hint) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	if (node.workgroup_size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	if (node.workgroup_size_hint) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 5) {
		state.write(uint16_t(0u));
	}
	if (node.workgroup_max_size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 6) {
		state.write(uint16_t(0u));
	}
	if (node.user_annotation) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 7) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 3> patch_offsets {};
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.return_types) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.arguments) {
		patch_offsets[2] = state.size();
		state.write(0u);
	}
	if (node.vec_type_hint) {
		write(*node.vec_type_hint, state);
	}
	if (node.workgroup_size) {
		write(*node.workgroup_size, state);
	}
	if (node.workgroup_size_hint) {
		write(*node.workgroup_size_hint, state);
	}
	if (node.workgroup_max_size) {
		write(*node.workgroup_max_size, state);
	}
	if (node.user_annotation) {
		write(*node.user_annotation, state);
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.name);
	if (node.return_types) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(uint32_t(node.return_types->size()));
		for (const auto& elem : *node.return_types) {
			write(elem, state);
		}
	}
	if (node.arguments) {
		state.patch(patch_offsets[2], state.size() - patch_offsets[2]);
		state.write(uint32_t(node.arguments->size()));
		for (const auto& elem : *node.arguments) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_key_frame_count_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::KEY_FRAME_COUNT_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_llong_type_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.qualifiers) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	inline_data_size += 4;
	inline_data_size += 4;
	if (node.qualifiers) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::LLONG_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	state.write(inline_offset);
	inline_offset += 4;
	state.write(inline_offset);
	inline_offset += 4;
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	state.write(node.size);
	state.write(node.alignment);
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_lvalue_reference_type_t& node, writer_state_t& state) {
	const uint32_t field_count = 4u;
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	inline_data_size += 4;
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::LVALUE_REFERENCE_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	write(node.pointee_type, state);
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_location_index_attr_t& node, writer_state_t& state) {
	uint32_t field_count = 0u;
	if (node.count) {
		field_count = 2u;
	} else if (node.index) {
		field_count = 1u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.index) { inline_data_size += 4; }
	if (node.count) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::LOCATION_INDEX_ATTR));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.index) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 0) {
		state.write(uint16_t(0u));
	}
	if (node.count) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 1) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	if (node.index) {
		write(*node.index, state);
	}
	if (node.count) {
		write(*node.count, state);
	}
	state.pad();
	return root_table_offset;
}

static inline uint32_t write_node(const node_long_type_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.qualifiers) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	inline_data_size += 4;
	inline_data_size += 4;
	if (node.qualifiers) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::LONG_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	state.write(inline_offset);
	inline_offset += 4;
	state.write(inline_offset);
	inline_offset += 4;
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	state.write(node.size);
	state.write(node.alignment);
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_matrix_type_t& node, writer_state_t& state) {
	uint32_t field_count = 4u;
	if (node.num_rows) {
		field_count = 6u;
	} else if (node.num_columns) {
		field_count = 5u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.num_columns) { inline_data_size += 4; }
	if (node.num_rows) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::MATRIX_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.num_columns) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	if (node.num_rows) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 5) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	write(node.element_type, state);
	if (node.num_columns) {
		state.write(*node.num_columns);
	}
	if (node.num_rows) {
		state.write(*node.num_rows);
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_max_distance_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::MAX_DISTANCE_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_max_mesh_workgroups_fn_attr_t& node, writer_state_t& state) {
	uint32_t field_count = 0u;
	if (node.workgroups) {
		field_count = 1u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.workgroups) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::MAX_MESH_WORKGROUPS_FN_ATTR));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.workgroups) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 0) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	if (node.workgroups) {
		write(*node.workgroups, state);
	}
	state.pad();
	return root_table_offset;
}

static inline uint32_t write_node(const node_mesh_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 0u;
	if (node.unused) {
		field_count = 5u;
	} else if (node.name) {
		field_count = 4u;
	} else if (node.type_name) {
		field_count = 3u;
	} else if (node.mesh_type_info) {
		field_count = 2u;
	} else if (node.function_constant) {
		field_count = 1u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	if (node.mesh_type_info) { inline_data_size += 4; }
	if (node.type_name) { inline_data_size += 4; }
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::MESH_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 0) {
		state.write(uint16_t(0u));
	}
	if (node.mesh_type_info) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 1) {
		state.write(uint16_t(0u));
	}
	if (node.type_name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.mesh_type_info) {
		write(*node.mesh_type_info, state);
	}
	if (node.type_name) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	if (node.type_name) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(*node.type_name);
	}
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_mesh_emulation_block_t& node, writer_state_t& state) {
	uint32_t field_count = 0u;
	if (node.value_groups) {
		field_count = 1u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.value_groups) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::MESH_EMULATION_BLOCK));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.value_groups) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 0) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.value_groups) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	state.pad();
	if (node.value_groups) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.value_groups->size()));
		for (const auto& elem : *node.value_groups) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_mesh_emulation_fragment_analysis_result_t& node, writer_state_t& state) {
	uint32_t field_count = 1u;
	if (node.used_inputs) {
		field_count = 2u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	inline_data_size += 4;
	if (node.used_inputs) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::MESH_EMULATION_FRAGMENT_ANALYSIS_RESULT));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	state.write(inline_offset);
	inline_offset += 4;
	if (node.used_inputs) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 1) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.used_inputs) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.function);
	if (node.used_inputs) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(uint32_t(node.used_inputs->size()));
		std::vector<uint32_t> vec_patch_offset;
		vec_patch_offset.reserve(node.used_inputs->size());
		for (uint32_t i = 0, count = node.used_inputs->size(); i < count; ++i) {
			vec_patch_offset.emplace_back(state.size());
			state.write(0u);
		}
		for (uint32_t vec_patch_idx = 0u; const auto& elem : *node.used_inputs) {
			state.patch(vec_patch_offset[vec_patch_idx], state.size() - vec_patch_offset[vec_patch_idx]);
			++vec_patch_idx;
			state.write(elem);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_mesh_emulation_mesh_kernel_t& node, writer_state_t& state) {
	uint32_t field_count = 1u;
	if (node.layout) {
		field_count = 3u;
	} else if (node.emulation_buffer_index) {
		field_count = 2u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	inline_data_size += 4;
	if (node.emulation_buffer_index) { inline_data_size += 4; }
	if (node.layout) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::MESH_EMULATION_MESH_KERNEL));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	state.write(inline_offset);
	inline_offset += 4;
	if (node.emulation_buffer_index) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 1) {
		state.write(uint16_t(0u));
	}
	if (node.layout) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.emulation_buffer_index) {
		state.write(*node.emulation_buffer_index);
	}
	if (node.layout) {
		write(*node.layout, state);
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.function);
	return root_table_offset;
}

static inline uint32_t write_node(const node_mesh_emulation_mesh_layout_t& node, writer_state_t& state) {
	uint32_t field_count = 5u;
	if (node.primitive_culled_block) {
		field_count = 6u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.max_vertices) { inline_data_size += 4; }
	if (node.max_primitives) { inline_data_size += 4; }
	if (node.max_indices) { inline_data_size += 4; }
	if (node.max_indices_padding) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.primitive_culled_block) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::MESH_EMULATION_MESH_LAYOUT));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.max_vertices) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.max_primitives) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.max_indices) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.max_indices_padding) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.primitive_culled_block) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 5) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	if (node.max_vertices) {
		state.write(*node.max_vertices);
	}
	if (node.max_primitives) {
		state.write(*node.max_primitives);
	}
	if (node.max_indices) {
		state.write(*node.max_indices);
	}
	if (node.max_indices_padding) {
		state.write(*node.max_indices_padding);
	}
	write(node.vertices_primitives_block, state);
	if (node.primitive_culled_block) {
		write(*node.primitive_culled_block, state);
	}
	state.pad();
	return root_table_offset;
}

static inline uint32_t write_node(const node_mesh_emulation_mesh_vertex_t& node, writer_state_t& state) {
	uint32_t field_count = 1u;
	if (node.layout) {
		field_count = 3u;
	} else if (node.emulation_buffer_index) {
		field_count = 2u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	inline_data_size += 4;
	if (node.emulation_buffer_index) { inline_data_size += 4; }
	if (node.layout) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::MESH_EMULATION_MESH_VERTEX));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	state.write(inline_offset);
	inline_offset += 4;
	if (node.emulation_buffer_index) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 1) {
		state.write(uint16_t(0u));
	}
	if (node.layout) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.emulation_buffer_index) {
		state.write(*node.emulation_buffer_index);
	}
	if (node.layout) {
		write(*node.layout, state);
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.function);
	return root_table_offset;
}

static inline uint32_t write_node(const node_mesh_emulation_object_kernel_t& node, writer_state_t& state) {
	uint32_t field_count = 1u;
	if (node.max_mesh_workgroups) {
		field_count = 3u;
	} else if (node.emulation_buffer_index) {
		field_count = 2u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	inline_data_size += 4;
	if (node.emulation_buffer_index) { inline_data_size += 4; }
	if (node.max_mesh_workgroups) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::MESH_EMULATION_OBJECT_KERNEL));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	state.write(inline_offset);
	inline_offset += 4;
	if (node.emulation_buffer_index) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 1) {
		state.write(uint16_t(0u));
	}
	if (node.max_mesh_workgroups) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.emulation_buffer_index) {
		state.write(*node.emulation_buffer_index);
	}
	if (node.max_mesh_workgroups) {
		write(*node.max_mesh_workgroups, state);
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.function);
	return root_table_offset;
}

static inline uint32_t write_node(const node_mesh_emulation_value_group_t& node, writer_state_t& state) {
	uint32_t field_count = 0u;
	if (node.member_index) {
		field_count = 5u;
	} else if (node.member_type) {
		field_count = 4u;
	} else if (node.max_value_count) {
		field_count = 3u;
	} else if (node.value_size) {
		field_count = 2u;
	} else if (node.value_alignment) {
		field_count = 1u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.value_alignment) { inline_data_size += 4; }
	if (node.value_size) { inline_data_size += 4; }
	if (node.max_value_count) { inline_data_size += 4; }
	if (node.member_type) { inline_data_size += 4; }
	if (node.member_index) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::MESH_EMULATION_VALUE_GROUP));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.value_alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 0) {
		state.write(uint16_t(0u));
	}
	if (node.value_size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 1) {
		state.write(uint16_t(0u));
	}
	if (node.max_value_count) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.member_type) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	if (node.member_index) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	if (node.value_alignment) {
		state.write(*node.value_alignment);
	}
	if (node.value_size) {
		state.write(*node.value_size);
	}
	if (node.max_value_count) {
		state.write(*node.max_value_count);
	}
	if (node.member_type) {
		write(*node.member_type, state);
	}
	if (node.member_index) {
		state.write(*node.member_index);
	}
	state.pad();
	return root_table_offset;
}

static inline uint32_t write_node(const node_mesh_function_t& node, writer_state_t& state) {
	uint32_t field_count = 1u;
	if (node.workgroup_size) {
		field_count = 6u;
	} else if (node.user_annotation) {
		field_count = 5u;
	} else if (node.workgroup_max_size) {
		field_count = 4u;
	} else if (node.arguments) {
		field_count = 3u;
	} else if (node.return_types) {
		field_count = 2u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	inline_data_size += 4;
	if (node.return_types) { inline_data_size += 4; }
	if (node.arguments) { inline_data_size += 4; }
	if (node.workgroup_max_size) { inline_data_size += 4; }
	if (node.user_annotation) { inline_data_size += 4; }
	if (node.workgroup_size) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::MESH_FUNCTION));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	state.write(inline_offset);
	inline_offset += 4;
	if (node.return_types) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 1) {
		state.write(uint16_t(0u));
	}
	if (node.arguments) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.workgroup_max_size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	if (node.user_annotation) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	if (node.workgroup_size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 5) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 3> patch_offsets {};
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.return_types) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.arguments) {
		patch_offsets[2] = state.size();
		state.write(0u);
	}
	if (node.workgroup_max_size) {
		write(*node.workgroup_max_size, state);
	}
	if (node.user_annotation) {
		write(*node.user_annotation, state);
	}
	if (node.workgroup_size) {
		write(*node.workgroup_size, state);
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.name);
	if (node.return_types) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(uint32_t(node.return_types->size()));
		for (const auto& elem : *node.return_types) {
			write(elem, state);
		}
	}
	if (node.arguments) {
		state.patch(patch_offsets[2], state.size() - patch_offsets[2]);
		state.write(uint32_t(node.arguments->size()));
		for (const auto& elem : *node.arguments) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_mesh_grid_properties_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::MESH_GRID_PROPERTIES_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_mesh_grid_properties_type_t& node, writer_state_t& state) {
	uint32_t field_count = 0u;
	if (node.qualifiers) {
		field_count = 3u;
	} else if (node.alignment) {
		field_count = 2u;
	} else if (node.size) {
		field_count = 1u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::MESH_GRID_PROPERTIES_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 0) {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 1) {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_mesh_primitive_data_ret_t& node, writer_state_t& state) {
	uint32_t field_count = 5u;
	if (node.name) {
		field_count = 6u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	if (node.id) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.shared) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::MESH_PRIMITIVE_DATA_RET));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.id) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.shared) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 5) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 3> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.id) {
		state.write(*node.id);
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.shared) {
		state.write(*node.shared);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[1] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[2] = state.size();
		state.write(0u);
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.attribute_name);
	state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[2], state.size() - patch_offsets[2]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_mesh_type_t& node, writer_state_t& state) {
	uint32_t field_count = 5u;
	if (node.topology) {
		field_count = 8u;
	} else if (node.max_primitives) {
		field_count = 7u;
	} else if (node.max_vertices) {
		field_count = 6u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	inline_data_size += 4;
	inline_data_size += 4;
	if (node.max_vertices) { inline_data_size += 4; }
	if (node.max_primitives) { inline_data_size += 4; }
	if (node.topology) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::MESH_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	state.write(inline_offset);
	inline_offset += 4;
	if (node.max_vertices) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 5) {
		state.write(uint16_t(0u));
	}
	if (node.max_primitives) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 6) {
		state.write(uint16_t(0u));
	}
	if (node.topology) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 7) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	write(node.vertex_type, state);
	write(node.primitive_type, state);
	if (node.max_vertices) {
		state.write(*node.max_vertices);
	}
	if (node.max_primitives) {
		state.write(*node.max_primitives);
	}
	if (node.topology) {
		state.write(std::underlying_type_t<TOPOLOGY>(*node.topology));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_mesh_type_info_t& node, writer_state_t& state) {
	const uint32_t field_count = 5u;
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.vertex_types) { inline_data_size += 4; }
	if (node.primitive_types) { inline_data_size += 4; }
	if (node.max_vertices) { inline_data_size += 4; }
	if (node.max_primitives) { inline_data_size += 4; }
	inline_data_size += 4;
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::MESH_TYPE_INFO));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.vertex_types) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.primitive_types) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.max_vertices) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.max_primitives) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.vertex_types) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	if (node.primitive_types) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.max_vertices) {
		state.write(*node.max_vertices);
	}
	if (node.max_primitives) {
		state.write(*node.max_primitives);
	}
	state.write(std::underlying_type_t<TOPOLOGY>(node.topology));
	state.write(uint8_t(0));
	state.write(uint8_t(0));
	state.write(uint8_t(0));
	state.pad();
	if (node.vertex_types) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.vertex_types->size()));
		for (const auto& elem : *node.vertex_types) {
			write(elem, state);
		}
	}
	if (node.primitive_types) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(uint32_t(node.primitive_types->size()));
		for (const auto& elem : *node.primitive_types) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_mesh_vertex_data_ret_t& node, writer_state_t& state) {
	uint32_t field_count = 5u;
	if (node.name) {
		field_count = 6u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	if (node.id) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.shared) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::MESH_VERTEX_DATA_RET));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.id) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.shared) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 5) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 3> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.id) {
		state.write(*node.id);
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.shared) {
		state.write(*node.shared);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[1] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[2] = state.size();
		state.write(0u);
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.attribute_name);
	state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[2], state.size() - patch_offsets[2]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_min_distance_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::MIN_DISTANCE_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_motion_end_time_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::MOTION_END_TIME_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_motion_start_time_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::MOTION_START_TIME_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_object_function_t& node, writer_state_t& state) {
	uint32_t field_count = 1u;
	if (node.workgroup_size) {
		field_count = 7u;
	} else if (node.user_annotation) {
		field_count = 6u;
	} else if (node.max_mesh_workgroups) {
		field_count = 5u;
	} else if (node.workgroup_max_size) {
		field_count = 4u;
	} else if (node.arguments) {
		field_count = 3u;
	} else if (node.return_types) {
		field_count = 2u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	inline_data_size += 4;
	if (node.return_types) { inline_data_size += 4; }
	if (node.arguments) { inline_data_size += 4; }
	if (node.workgroup_max_size) { inline_data_size += 4; }
	if (node.max_mesh_workgroups) { inline_data_size += 4; }
	if (node.user_annotation) { inline_data_size += 4; }
	if (node.workgroup_size) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::OBJECT_FUNCTION));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	state.write(inline_offset);
	inline_offset += 4;
	if (node.return_types) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 1) {
		state.write(uint16_t(0u));
	}
	if (node.arguments) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.workgroup_max_size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	if (node.max_mesh_workgroups) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	if (node.user_annotation) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 5) {
		state.write(uint16_t(0u));
	}
	if (node.workgroup_size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 6) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 3> patch_offsets {};
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.return_types) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.arguments) {
		patch_offsets[2] = state.size();
		state.write(0u);
	}
	if (node.workgroup_max_size) {
		write(*node.workgroup_max_size, state);
	}
	if (node.max_mesh_workgroups) {
		write(*node.max_mesh_workgroups, state);
	}
	if (node.user_annotation) {
		write(*node.user_annotation, state);
	}
	if (node.workgroup_size) {
		write(*node.workgroup_size, state);
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.name);
	if (node.return_types) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(uint32_t(node.return_types->size()));
		for (const auto& elem : *node.return_types) {
			write(elem, state);
		}
	}
	if (node.arguments) {
		state.patch(patch_offsets[2], state.size() - patch_offsets[2]);
		state.write(uint32_t(node.arguments->size()));
		for (const auto& elem : *node.arguments) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_object_to_world_transform_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::OBJECT_TO_WORLD_TRANSFORM_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_opaque_primitive_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::OPAQUE_PRIMITIVE_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_opaque_type_t& node, writer_state_t& state) {
	const uint32_t field_count = 4u;
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	inline_data_size += 4;
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::OPAQUE_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	patch_offsets[1] = state.size();
	state.write(0u);
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
	state.write(node.name);
	return root_table_offset;
}

static inline uint32_t write_node(const node_origin_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::ORIGIN_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_packed_vector_type_t& node, writer_state_t& state) {
	uint32_t field_count = 4u;
	if (node.num_elements) {
		field_count = 5u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.num_elements) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::PACKED_VECTOR_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.num_elements) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	write(node.element_type, state);
	if (node.num_elements) {
		state.write(*node.num_elements);
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_patch_control_point_type_t& node, writer_state_t& state) {
	const uint32_t field_count = 4u;
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	inline_data_size += 4;
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::PATCH_CONTROL_POINT_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	write(node.control_point_type, state);
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_patch_fn_attr_t& node, writer_state_t& state) {
	uint32_t field_count = 0u;
	if (node.control_points) {
		field_count = 2u;
	} else if (node.kind) {
		field_count = 1u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.kind) { inline_data_size += 4; }
	if (node.control_points) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::PATCH_FN_ATTR));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.kind) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 0) {
		state.write(uint16_t(0u));
	}
	if (node.control_points) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 1) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	if (node.kind) {
		state.write(std::underlying_type_t<PATCH_KIND>(*node.kind));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.control_points) {
		write(*node.control_points, state);
	}
	state.pad();
	return root_table_offset;
}

static inline uint32_t write_node(const node_patch_id_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::PATCH_ID_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_patch_input_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 4u;
	if (node.unused) {
		field_count = 6u;
	} else if (node.name) {
		field_count = 5u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	if (node.location_index) { inline_data_size += 4; }
	if (node.location_count) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::PATCH_INPUT_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.location_index) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.location_count) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 5) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.location_index) {
		write(*node.location_index, state);
	}
	if (node.location_count) {
		write(*node.location_count, state);
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_payload_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 4u;
	if (node.inline_type_info) {
		field_count = 8u;
	} else if (node.unused) {
		field_count = 7u;
	} else if (node.name) {
		field_count = 6u;
	} else if (node.type_name) {
		field_count = 5u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	if (node.struct_type_info) { inline_data_size += 4; }
	inline_data_size += 4;
	inline_data_size += 4;
	if (node.type_name) { inline_data_size += 4; }
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	if (node.inline_type_info) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::PAYLOAD_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.struct_type_info) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	state.write(inline_offset);
	inline_offset += 4;
	if (node.type_name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 5) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 6) {
		state.write(uint16_t(0u));
	}
	if (node.inline_type_info) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 7) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.struct_type_info) {
		write(*node.struct_type_info, state);
	}
	write(node.type_size, state);
	write(node.type_align, state);
	if (node.type_name) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.inline_type_info) {
		write(*node.inline_type_info, state);
	}
	state.pad();
	if (node.type_name) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(*node.type_name);
	}
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_pixel_position_in_tile_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::PIXEL_POSITION_IN_TILE_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_pixels_per_tile_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::PIXELS_PER_TILE_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_point_coord_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::POINT_COORD_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_point_size_attr_t& node, writer_state_t& state) {
	const uint32_t field_count = 0u;
	uint32_t inline_data_size = sizeof(table_root_t);
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::POINT_SIZE_ATTR));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	state.write(uint32_t(state.size() - vtable_start_offset));
	state.pad();
	return root_table_offset;
}

static inline uint32_t write_node(const node_point_size_ret_t& node, writer_state_t& state) {
	uint32_t field_count = 3u;
	if (node.name) {
		field_count = 4u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	if (node.shared) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::POINT_SIZE_RET));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.shared) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.shared) {
		state.write(*node.shared);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_pointer_type_t& node, writer_state_t& state) {
	const uint32_t field_count = 4u;
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	inline_data_size += 4;
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::POINTER_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	write(node.pointee_type, state);
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_position_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 4u;
	if (node.unused) {
		field_count = 6u;
	} else if (node.name) {
		field_count = 5u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	inline_data_size += 4;
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::POSITION_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	state.write(inline_offset);
	inline_offset += 4;
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 5) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.write(std::underlying_type_t<SAMPLING_QUALIFIER>(node.sampling_qualifier));
	state.write(uint8_t(0));
	state.write(uint8_t(0));
	state.write(uint8_t(0));
	state.write(std::underlying_type_t<INTERPOLATION_QUALIFIER>(node.interpolation_qualifier));
	state.write(uint8_t(0));
	state.write(uint8_t(0));
	state.write(uint8_t(0));
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_position_attr_t& node, writer_state_t& state) {
	const uint32_t field_count = 0u;
	uint32_t inline_data_size = sizeof(table_root_t);
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::POSITION_ATTR));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	state.write(uint32_t(state.size() - vtable_start_offset));
	state.pad();
	return root_table_offset;
}

static inline uint32_t write_node(const node_position_in_patch_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::POSITION_IN_PATCH_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_position_ret_t& node, writer_state_t& state) {
	uint32_t field_count = 4u;
	if (node.name) {
		field_count = 5u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	if (node.invariant) { inline_data_size += 4; }
	if (node.shared) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::POSITION_RET));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.invariant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.shared) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.invariant) {
		state.write(*node.invariant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.shared) {
		state.write(*node.shared);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_primitive_acceleration_structure_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 6u;
	if (node.unused) {
		field_count = 8u;
	} else if (node.name) {
		field_count = 7u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	if (node.location_index) { inline_data_size += 4; }
	if (node.location_count) { inline_data_size += 4; }
	if (node.access_qualifier) { inline_data_size += 4; }
	if (node.raster_order_group) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::PRIMITIVE_ACCELERATION_STRUCTURE_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.location_index) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.location_count) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.access_qualifier) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.raster_order_group) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 6) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 7) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.location_index) {
		write(*node.location_index, state);
	}
	if (node.location_count) {
		write(*node.location_count, state);
	}
	if (node.access_qualifier) {
		state.write(std::underlying_type_t<ACCESS_QUALIFIER>(*node.access_qualifier));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.raster_order_group) {
		write(*node.raster_order_group, state);
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_primitive_culled_attr_t& node, writer_state_t& state) {
	const uint32_t field_count = 0u;
	uint32_t inline_data_size = sizeof(table_root_t);
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::PRIMITIVE_CULLED_ATTR));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	state.write(uint32_t(state.size() - vtable_start_offset));
	state.pad();
	return root_table_offset;
}

static inline uint32_t write_node(const node_primitive_culled_ret_t& node, writer_state_t& state) {
	uint32_t field_count = 3u;
	if (node.name) {
		field_count = 4u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	if (node.shared) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::PRIMITIVE_CULLED_RET));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.shared) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.shared) {
		state.write(*node.shared);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_primitive_data_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::PRIMITIVE_DATA_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_primitive_id_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::PRIMITIVE_ID_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_primitive_id_attr_t& node, writer_state_t& state) {
	const uint32_t field_count = 0u;
	uint32_t inline_data_size = sizeof(table_root_t);
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::PRIMITIVE_ID_ATTR));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	state.write(uint32_t(state.size() - vtable_start_offset));
	state.pad();
	return root_table_offset;
}

static inline uint32_t write_node(const node_primitive_id_ret_t& node, writer_state_t& state) {
	uint32_t field_count = 3u;
	if (node.name) {
		field_count = 4u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	if (node.shared) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::PRIMITIVE_ID_RET));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.shared) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.shared) {
		state.write(*node.shared);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_quadgroup_index_in_threadgroup_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::QUADGROUP_INDEX_IN_THREADGROUP_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_quadgroups_per_threadgroup_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::QUADGROUPS_PER_THREADGROUP_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_r16snorm_type_t& node, writer_state_t& state) {
	const uint32_t field_count = 4u;
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	inline_data_size += 4;
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::R16SNORM_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	write(node.alu_type, state);
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_r16unorm_type_t& node, writer_state_t& state) {
	const uint32_t field_count = 4u;
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	inline_data_size += 4;
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::R16UNORM_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	write(node.alu_type, state);
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_r8snorm_type_t& node, writer_state_t& state) {
	const uint32_t field_count = 4u;
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	inline_data_size += 4;
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::R8SNORM_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	write(node.alu_type, state);
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_r8unorm_type_t& node, writer_state_t& state) {
	const uint32_t field_count = 4u;
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	inline_data_size += 4;
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::R8UNORM_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	write(node.alu_type, state);
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_rg11b10f_type_t& node, writer_state_t& state) {
	const uint32_t field_count = 4u;
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	inline_data_size += 4;
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::RG11B10F_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	write(node.alu_type, state);
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_rg16snorm_type_t& node, writer_state_t& state) {
	const uint32_t field_count = 4u;
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	inline_data_size += 4;
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::RG16SNORM_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	write(node.alu_type, state);
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_rg16unorm_type_t& node, writer_state_t& state) {
	const uint32_t field_count = 4u;
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	inline_data_size += 4;
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::RG16UNORM_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	write(node.alu_type, state);
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_rg8snorm_type_t& node, writer_state_t& state) {
	const uint32_t field_count = 4u;
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	inline_data_size += 4;
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::RG8SNORM_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	write(node.alu_type, state);
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_rg8unorm_type_t& node, writer_state_t& state) {
	const uint32_t field_count = 4u;
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	inline_data_size += 4;
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::RG8UNORM_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	write(node.alu_type, state);
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_rgb10a2_type_t& node, writer_state_t& state) {
	const uint32_t field_count = 4u;
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	inline_data_size += 4;
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::RGB10A2_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	write(node.alu_type, state);
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_rgb9e5_type_t& node, writer_state_t& state) {
	const uint32_t field_count = 4u;
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	inline_data_size += 4;
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::RGB9E5_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	write(node.alu_type, state);
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_rgba16snorm_type_t& node, writer_state_t& state) {
	const uint32_t field_count = 4u;
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	inline_data_size += 4;
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::RGBA16SNORM_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	write(node.alu_type, state);
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_rgba16unorm_type_t& node, writer_state_t& state) {
	const uint32_t field_count = 4u;
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	inline_data_size += 4;
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::RGBA16UNORM_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	write(node.alu_type, state);
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_rgba8snorm_type_t& node, writer_state_t& state) {
	const uint32_t field_count = 4u;
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	inline_data_size += 4;
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::RGBA8SNORM_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	write(node.alu_type, state);
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_rgba8unorm_type_t& node, writer_state_t& state) {
	const uint32_t field_count = 4u;
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	inline_data_size += 4;
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::RGBA8UNORM_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	write(node.alu_type, state);
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_rvalue_reference_type_t& node, writer_state_t& state) {
	const uint32_t field_count = 4u;
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	inline_data_size += 4;
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::RVALUE_REFERENCE_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	write(node.pointee_type, state);
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_record_base_t& node, writer_state_t& state) {
	const uint32_t field_count = 3u;
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.offset) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	inline_data_size += 4;
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::RECORD_BASE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.offset) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	state.write(uint32_t(state.size() - vtable_start_offset));
	if (node.offset) {
		state.write(*node.offset);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	write(node.type, state);
	state.pad();
	return root_table_offset;
}

static inline uint32_t write_node(const node_record_field_t& node, writer_state_t& state) {
	uint32_t field_count = 3u;
	if (node.bitfield) {
		field_count = 6u;
	} else if (node.attributes) {
		field_count = 5u;
	} else if (node.name) {
		field_count = 4u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.offset) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.attributes) { inline_data_size += 4; }
	if (node.bitfield) { inline_data_size += 12; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::RECORD_FIELD));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.offset) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	if (node.attributes) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	if (node.bitfield) {
		state.write(inline_offset);
		inline_offset += 12;
	} else if (field_count > 5) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.offset) {
		state.write(*node.offset);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	write(node.type, state);
	if (node.name) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	if (node.attributes) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.bitfield) {
		write(*node.bitfield, state);
	}
	state.pad();
	if (node.name) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(*node.name);
	}
	if (node.attributes) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(uint32_t(node.attributes->size()));
		for (const auto& elem : *node.attributes) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_render_pipeline_state_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 4u;
	if (node.unused) {
		field_count = 6u;
	} else if (node.name) {
		field_count = 5u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	if (node.location_index) { inline_data_size += 4; }
	if (node.location_count) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::RENDER_PIPELINE_STATE_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.location_index) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.location_count) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 5) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.location_index) {
		write(*node.location_index, state);
	}
	if (node.location_count) {
		write(*node.location_count, state);
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_render_pipeline_state_type_t& node, writer_state_t& state) {
	uint32_t field_count = 0u;
	if (node.qualifiers) {
		field_count = 3u;
	} else if (node.alignment) {
		field_count = 2u;
	} else if (node.size) {
		field_count = 1u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::RENDER_PIPELINE_STATE_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 0) {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 1) {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_render_target_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 4u;
	if (node.unused) {
		field_count = 6u;
	} else if (node.name) {
		field_count = 5u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	if (node.render_target_index) { inline_data_size += 4; }
	if (node.raster_order_group) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::RENDER_TARGET_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.render_target_index) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.raster_order_group) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 5) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.render_target_index) {
		write(*node.render_target_index, state);
	}
	if (node.raster_order_group) {
		write(*node.raster_order_group, state);
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_render_target_array_index_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::RENDER_TARGET_ARRAY_INDEX_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_render_target_array_index_attr_t& node, writer_state_t& state) {
	const uint32_t field_count = 0u;
	uint32_t inline_data_size = sizeof(table_root_t);
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::RENDER_TARGET_ARRAY_INDEX_ATTR));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	state.write(uint32_t(state.size() - vtable_start_offset));
	state.pad();
	return root_table_offset;
}

static inline uint32_t write_node(const node_render_target_array_index_ret_t& node, writer_state_t& state) {
	uint32_t field_count = 3u;
	if (node.name) {
		field_count = 4u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	if (node.shared) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::RENDER_TARGET_ARRAY_INDEX_RET));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.shared) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.shared) {
		state.write(*node.shared);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_render_target_attr_t& node, writer_state_t& state) {
	uint32_t field_count = 0u;
	if (node.index) {
		field_count = 1u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.index) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::RENDER_TARGET_ATTR));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.index) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 0) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	if (node.index) {
		write(*node.index, state);
	}
	state.pad();
	return root_table_offset;
}

static inline uint32_t write_node(const node_render_target_ret_t& node, writer_state_t& state) {
	uint32_t field_count = 6u;
	if (node.name) {
		field_count = 7u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	if (node.render_target_index) { inline_data_size += 4; }
	if (node.blend_source_index) { inline_data_size += 4; }
	if (node.raster_order_group) { inline_data_size += 4; }
	if (node.rounding_mode) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::RENDER_TARGET_RET));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.render_target_index) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.blend_source_index) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.raster_order_group) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.rounding_mode) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 6) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.render_target_index) {
		write(*node.render_target_index, state);
	}
	if (node.blend_source_index) {
		write(*node.blend_source_index, state);
	}
	if (node.raster_order_group) {
		write(*node.raster_order_group, state);
	}
	if (node.rounding_mode) {
		state.write(std::underlying_type_t<ROUNDING_MODE>(*node.rounding_mode));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_srgba8unorm_type_t& node, writer_state_t& state) {
	const uint32_t field_count = 4u;
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	inline_data_size += 4;
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::SRGBA8UNORM_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	write(node.alu_type, state);
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_sample_id_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::SAMPLE_ID_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_sample_mask_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 3u;
	if (node.unused) {
		field_count = 5u;
	} else if (node.name) {
		field_count = 4u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	if (node.post_depth_coverage) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::SAMPLE_MASK_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.post_depth_coverage) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.post_depth_coverage) {
		state.write(*node.post_depth_coverage);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_sample_mask_ret_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::SAMPLE_MASK_RET));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_sampler_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 4u;
	if (node.unused) {
		field_count = 6u;
	} else if (node.name) {
		field_count = 5u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	if (node.location_index) { inline_data_size += 4; }
	if (node.location_count) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::SAMPLER_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.location_index) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.location_count) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 5) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.location_index) {
		write(*node.location_index, state);
	}
	if (node.location_count) {
		write(*node.location_count, state);
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_sampler_type_t& node, writer_state_t& state) {
	uint32_t field_count = 0u;
	if (node.qualifiers) {
		field_count = 3u;
	} else if (node.alignment) {
		field_count = 2u;
	} else if (node.size) {
		field_count = 1u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::SAMPLER_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 0) {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 1) {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_shared_attr_t& node, writer_state_t& state) {
	const uint32_t field_count = 0u;
	uint32_t inline_data_size = sizeof(table_root_t);
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::SHARED_ATTR));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	state.write(uint32_t(state.size() - vtable_start_offset));
	state.pad();
	return root_table_offset;
}

static inline uint32_t write_node(const node_short_type_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.qualifiers) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	inline_data_size += 4;
	inline_data_size += 4;
	if (node.qualifiers) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::SHORT_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	state.write(inline_offset);
	inline_offset += 4;
	state.write(inline_offset);
	inline_offset += 4;
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	state.write(node.size);
	state.write(node.alignment);
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_simdgroup_index_in_threadgroup_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::SIMDGROUP_INDEX_IN_THREADGROUP_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_simdgroups_per_threadgroup_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::SIMDGROUPS_PER_THREADGROUP_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_stage_in_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 4u;
	if (node.unused) {
		field_count = 6u;
	} else if (node.name) {
		field_count = 5u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	if (node.location_index) { inline_data_size += 4; }
	if (node.location_count) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::STAGE_IN_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.location_index) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.location_count) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 5) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.location_index) {
		write(*node.location_index, state);
	}
	if (node.location_count) {
		write(*node.location_count, state);
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_stage_in_grid_origin_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::STAGE_IN_GRID_ORIGIN_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_stage_in_grid_size_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::STAGE_IN_GRID_SIZE_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_stencil_ret_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::STENCIL_RET));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_stitching_argument_t& node, writer_state_t& state) {
	uint32_t field_count = 1u;
	if (node.name) {
		field_count = 2u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::STITCHING_ARGUMENT));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 1) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	write(node.type, state);
	if (node.name) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	state.pad();
	if (node.name) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_struct_type_t& node, writer_state_t& state) {
	uint32_t field_count = 0u;
	if (node.members) {
		field_count = 5u;
	} else if (node.name) {
		field_count = 4u;
	} else if (node.qualifiers) {
		field_count = 3u;
	} else if (node.alignment) {
		field_count = 2u;
	} else if (node.size) {
		field_count = 1u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	if (node.name) { inline_data_size += 4; }
	if (node.members) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::STRUCT_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 0) {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 1) {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	if (node.members) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 3> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.members) {
		patch_offsets[2] = state.size();
		state.write(0u);
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	if (node.members) {
		state.patch(patch_offsets[2], state.size() - patch_offsets[2]);
		state.write(uint32_t(node.members->size()));
		for (const auto& elem : *node.members) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_struct_type_info_t& node, writer_state_t& state) {
	uint32_t field_count = 0u;
	if (node.fields) {
		field_count = 1u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.fields) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::STRUCT_TYPE_INFO));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.fields) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 0) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.fields) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	state.pad();
	if (node.fields) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.fields->size()));
		std::vector<uint32_t> vec_patch_offset;
		vec_patch_offset.reserve(node.fields->size());
		for (uint32_t i = 0, count = node.fields->size(); i < count; ++i) {
			vec_patch_offset.emplace_back(state.size());
			state.write(0u);
		}
		for (uint32_t vec_patch_idx = 0u; const auto& elem : *node.fields) {
			const auto patch_offset = write(elem, state);
			state.patch(vec_patch_offset[vec_patch_idx], patch_offset - vec_patch_offset[vec_patch_idx]);
			++vec_patch_idx;
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_tensor_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 6u;
	if (node.unused) {
		field_count = 8u;
	} else if (node.name) {
		field_count = 7u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	if (node.location_index) { inline_data_size += 4; }
	if (node.location_count) { inline_data_size += 4; }
	if (node.access_qualifier) { inline_data_size += 4; }
	if (node.raster_order_group) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::TENSOR_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.location_index) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.location_count) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.access_qualifier) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.raster_order_group) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 6) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 7) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.location_index) {
		write(*node.location_index, state);
	}
	if (node.location_count) {
		write(*node.location_count, state);
	}
	if (node.access_qualifier) {
		state.write(std::underlying_type_t<ACCESS_QUALIFIER>(*node.access_qualifier));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.raster_order_group) {
		write(*node.raster_order_group, state);
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_tensor_type_t& node, writer_state_t& state) {
	uint32_t field_count = 5u;
	if (node.kind) {
		field_count = 6u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	inline_data_size += 4;
	inline_data_size += 4;
	if (node.kind) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::TENSOR_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	state.write(inline_offset);
	inline_offset += 4;
	if (node.kind) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 5) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	write(node.element_type, state);
	write(node.extents_type, state);
	if (node.kind) {
		state.write(std::underlying_type_t<TENSOR_KIND>(*node.kind));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_texture1d_array_type_t& node, writer_state_t& state) {
	uint32_t field_count = 4u;
	if (node.access_qualifier) {
		field_count = 5u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.access_qualifier) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::TEXTURE1D_ARRAY_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.access_qualifier) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	write(node.channel_type, state);
	if (node.access_qualifier) {
		state.write(std::underlying_type_t<ACCESS_QUALIFIER>(*node.access_qualifier));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_texture1d_type_t& node, writer_state_t& state) {
	uint32_t field_count = 4u;
	if (node.access_qualifier) {
		field_count = 5u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.access_qualifier) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::TEXTURE1D_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.access_qualifier) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	write(node.channel_type, state);
	if (node.access_qualifier) {
		state.write(std::underlying_type_t<ACCESS_QUALIFIER>(*node.access_qualifier));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_texture2d_array_type_t& node, writer_state_t& state) {
	uint32_t field_count = 4u;
	if (node.access_qualifier) {
		field_count = 5u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.access_qualifier) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::TEXTURE2D_ARRAY_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.access_qualifier) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	write(node.channel_type, state);
	if (node.access_qualifier) {
		state.write(std::underlying_type_t<ACCESS_QUALIFIER>(*node.access_qualifier));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_texture2d_ms_array_type_t& node, writer_state_t& state) {
	uint32_t field_count = 4u;
	if (node.access_qualifier) {
		field_count = 5u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.access_qualifier) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::TEXTURE2D_MS_ARRAY_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.access_qualifier) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	write(node.channel_type, state);
	if (node.access_qualifier) {
		state.write(std::underlying_type_t<ACCESS_QUALIFIER>(*node.access_qualifier));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_texture2d_ms_type_t& node, writer_state_t& state) {
	uint32_t field_count = 4u;
	if (node.access_qualifier) {
		field_count = 5u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.access_qualifier) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::TEXTURE2D_MS_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.access_qualifier) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	write(node.channel_type, state);
	if (node.access_qualifier) {
		state.write(std::underlying_type_t<ACCESS_QUALIFIER>(*node.access_qualifier));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_texture2d_type_t& node, writer_state_t& state) {
	uint32_t field_count = 4u;
	if (node.access_qualifier) {
		field_count = 5u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.access_qualifier) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::TEXTURE2D_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.access_qualifier) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	write(node.channel_type, state);
	if (node.access_qualifier) {
		state.write(std::underlying_type_t<ACCESS_QUALIFIER>(*node.access_qualifier));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_texture3d_type_t& node, writer_state_t& state) {
	uint32_t field_count = 4u;
	if (node.access_qualifier) {
		field_count = 5u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.access_qualifier) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::TEXTURE3D_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.access_qualifier) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	write(node.channel_type, state);
	if (node.access_qualifier) {
		state.write(std::underlying_type_t<ACCESS_QUALIFIER>(*node.access_qualifier));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_texture_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 6u;
	if (node.unused) {
		field_count = 8u;
	} else if (node.name) {
		field_count = 7u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	if (node.location_index) { inline_data_size += 4; }
	if (node.location_count) { inline_data_size += 4; }
	if (node.access_qualifier) { inline_data_size += 4; }
	if (node.raster_order_group) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::TEXTURE_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.location_index) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.location_count) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.access_qualifier) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.raster_order_group) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 6) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 7) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.location_index) {
		write(*node.location_index, state);
	}
	if (node.location_count) {
		write(*node.location_count, state);
	}
	if (node.access_qualifier) {
		state.write(std::underlying_type_t<ACCESS_QUALIFIER>(*node.access_qualifier));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.raster_order_group) {
		write(*node.raster_order_group, state);
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_texture_buffer1d_type_t& node, writer_state_t& state) {
	uint32_t field_count = 4u;
	if (node.access_qualifier) {
		field_count = 5u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.access_qualifier) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::TEXTURE_BUFFER1D_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.access_qualifier) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	write(node.channel_type, state);
	if (node.access_qualifier) {
		state.write(std::underlying_type_t<ACCESS_QUALIFIER>(*node.access_qualifier));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_texture_cube_array_type_t& node, writer_state_t& state) {
	uint32_t field_count = 4u;
	if (node.access_qualifier) {
		field_count = 5u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.access_qualifier) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::TEXTURE_CUBE_ARRAY_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.access_qualifier) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	write(node.channel_type, state);
	if (node.access_qualifier) {
		state.write(std::underlying_type_t<ACCESS_QUALIFIER>(*node.access_qualifier));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_texture_cube_type_t& node, writer_state_t& state) {
	uint32_t field_count = 4u;
	if (node.access_qualifier) {
		field_count = 5u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.access_qualifier) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::TEXTURE_CUBE_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.access_qualifier) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	write(node.channel_type, state);
	if (node.access_qualifier) {
		state.write(std::underlying_type_t<ACCESS_QUALIFIER>(*node.access_qualifier));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_thread_execution_width_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::THREAD_EXECUTION_WIDTH_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_thread_index_in_quadgroup_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::THREAD_INDEX_IN_QUADGROUP_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_thread_index_in_simdgroup_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::THREAD_INDEX_IN_SIMDGROUP_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_thread_index_in_threadgroup_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::THREAD_INDEX_IN_THREADGROUP_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_thread_position_in_grid_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::THREAD_POSITION_IN_GRID_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_thread_position_in_threadgroup_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::THREAD_POSITION_IN_THREADGROUP_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_threadgroup_position_in_grid_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::THREADGROUP_POSITION_IN_GRID_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_threadgroups_per_grid_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::THREADGROUPS_PER_GRID_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_threads_per_grid_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::THREADS_PER_GRID_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_threads_per_simdgroup_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::THREADS_PER_SIMDGROUP_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_threads_per_threadgroup_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::THREADS_PER_THREADGROUP_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_tile_index_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::TILE_INDEX_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_time_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::TIME_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_uchar_type_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.qualifiers) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	inline_data_size += 4;
	inline_data_size += 4;
	if (node.qualifiers) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::UCHAR_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	state.write(inline_offset);
	inline_offset += 4;
	state.write(inline_offset);
	inline_offset += 4;
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	state.write(node.size);
	state.write(node.alignment);
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_uint_type_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.qualifiers) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	inline_data_size += 4;
	inline_data_size += 4;
	if (node.qualifiers) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::UINT_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	state.write(inline_offset);
	inline_offset += 4;
	state.write(inline_offset);
	inline_offset += 4;
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	state.write(node.size);
	state.write(node.alignment);
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_ullong_type_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.qualifiers) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	inline_data_size += 4;
	inline_data_size += 4;
	if (node.qualifiers) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::ULLONG_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	state.write(inline_offset);
	inline_offset += 4;
	state.write(inline_offset);
	inline_offset += 4;
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	state.write(node.size);
	state.write(node.alignment);
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_ulong_type_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.qualifiers) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	inline_data_size += 4;
	inline_data_size += 4;
	if (node.qualifiers) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::ULONG_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	state.write(inline_offset);
	inline_offset += 4;
	state.write(inline_offset);
	inline_offset += 4;
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	state.write(node.size);
	state.write(node.alignment);
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_ushort_type_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.qualifiers) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	inline_data_size += 4;
	inline_data_size += 4;
	if (node.qualifiers) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::USHORT_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	state.write(inline_offset);
	inline_offset += 4;
	state.write(inline_offset);
	inline_offset += 4;
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	state.write(node.size);
	state.write(node.alignment);
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_union_type_t& node, writer_state_t& state) {
	uint32_t field_count = 0u;
	if (node.members) {
		field_count = 5u;
	} else if (node.name) {
		field_count = 4u;
	} else if (node.qualifiers) {
		field_count = 3u;
	} else if (node.alignment) {
		field_count = 2u;
	} else if (node.size) {
		field_count = 1u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	if (node.name) { inline_data_size += 4; }
	if (node.members) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::UNION_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 0) {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 1) {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	if (node.members) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 3> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.members) {
		patch_offsets[2] = state.size();
		state.write(0u);
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	if (node.members) {
		state.patch(patch_offsets[2], state.size() - patch_offsets[2]);
		state.write(uint32_t(node.members->size()));
		for (const auto& elem : *node.members) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_user_annotation_fn_attr_t& node, writer_state_t& state) {
	uint32_t field_count = 0u;
	if (node.annotation) {
		field_count = 1u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.annotation) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::USER_ANNOTATION_FN_ATTR));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.annotation) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 0) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.annotation) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	state.pad();
	if (node.annotation) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(*node.annotation);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_user_attr_t& node, writer_state_t& state) {
	const uint32_t field_count = 1u;
	uint32_t inline_data_size = sizeof(table_root_t);
	inline_data_size += 4;
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::USER_ATTR));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	state.write(inline_offset);
	inline_offset += 4;
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	patch_offsets[0] = state.size();
	state.write(0u);
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.name);
	return root_table_offset;
}

static inline uint32_t write_node(const node_user_data_buffer_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 0u;
	if (node.unused) {
		field_count = 9u;
	} else if (node.name) {
		field_count = 8u;
	} else if (node.type_name) {
		field_count = 7u;
	} else if (node.type_align) {
		field_count = 6u;
	} else if (node.type_size) {
		field_count = 5u;
	} else if (node.struct_type_info) {
		field_count = 4u;
	} else if (node.inline_type_info) {
		field_count = 3u;
	} else if (node.access_qualifier) {
		field_count = 2u;
	} else if (node.function_constant) {
		field_count = 1u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	if (node.access_qualifier) { inline_data_size += 4; }
	if (node.inline_type_info) { inline_data_size += 4; }
	if (node.struct_type_info) { inline_data_size += 4; }
	if (node.type_size) { inline_data_size += 4; }
	if (node.type_align) { inline_data_size += 4; }
	if (node.type_name) { inline_data_size += 4; }
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::USER_DATA_BUFFER_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 0) {
		state.write(uint16_t(0u));
	}
	if (node.access_qualifier) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 1) {
		state.write(uint16_t(0u));
	}
	if (node.inline_type_info) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.struct_type_info) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	if (node.type_size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	if (node.type_align) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 5) {
		state.write(uint16_t(0u));
	}
	if (node.type_name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 6) {
		state.write(uint16_t(0u));
	}
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 7) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 8) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.access_qualifier) {
		state.write(std::underlying_type_t<ACCESS_QUALIFIER>(*node.access_qualifier));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.inline_type_info) {
		write(*node.inline_type_info, state);
	}
	if (node.struct_type_info) {
		write(*node.struct_type_info, state);
	}
	if (node.type_size) {
		write(*node.type_size, state);
	}
	if (node.type_align) {
		write(*node.type_align, state);
	}
	if (node.type_name) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	if (node.type_name) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(*node.type_name);
	}
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_user_instance_id_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::USER_INSTANCE_ID_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_user_instance_id_count_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::USER_INSTANCE_ID_COUNT_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_vec_type_hint_fn_attr_t& node, writer_state_t& state) {
	uint32_t field_count = 0u;
	if (node.type_name) {
		field_count = 1u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.type_name) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::VEC_TYPE_HINT_FN_ATTR));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.type_name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 0) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.type_name) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	state.pad();
	if (node.type_name) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(*node.type_name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_vector_type_t& node, writer_state_t& state) {
	uint32_t field_count = 4u;
	if (node.num_elements) {
		field_count = 5u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.num_elements) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::VECTOR_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.num_elements) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	write(node.element_type, state);
	if (node.num_elements) {
		state.write(*node.num_elements);
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_vertex_function_t& node, writer_state_t& state) {
	uint32_t field_count = 1u;
	if (node.user_annotation) {
		field_count = 5u;
	} else if (node.patch) {
		field_count = 4u;
	} else if (node.arguments) {
		field_count = 3u;
	} else if (node.return_types) {
		field_count = 2u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	inline_data_size += 4;
	if (node.return_types) { inline_data_size += 4; }
	if (node.arguments) { inline_data_size += 4; }
	if (node.patch) { inline_data_size += 4; }
	if (node.user_annotation) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::VERTEX_FUNCTION));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	state.write(inline_offset);
	inline_offset += 4;
	if (node.return_types) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 1) {
		state.write(uint16_t(0u));
	}
	if (node.arguments) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.patch) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	if (node.user_annotation) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 3> patch_offsets {};
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.return_types) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.arguments) {
		patch_offsets[2] = state.size();
		state.write(0u);
	}
	if (node.patch) {
		write(*node.patch, state);
	}
	if (node.user_annotation) {
		write(*node.user_annotation, state);
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.name);
	if (node.return_types) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(uint32_t(node.return_types->size()));
		for (const auto& elem : *node.return_types) {
			write(elem, state);
		}
	}
	if (node.arguments) {
		state.patch(patch_offsets[2], state.size() - patch_offsets[2]);
		state.write(uint32_t(node.arguments->size()));
		for (const auto& elem : *node.arguments) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_vertex_id_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::VERTEX_ID_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_vertex_input_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 4u;
	if (node.unused) {
		field_count = 6u;
	} else if (node.name) {
		field_count = 5u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	if (node.location_index) { inline_data_size += 4; }
	if (node.location_count) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::VERTEX_INPUT_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.location_index) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.location_count) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 4) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 5) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.location_index) {
		write(*node.location_index, state);
	}
	if (node.location_count) {
		write(*node.location_count, state);
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_vertex_output_ret_t& node, writer_state_t& state) {
	uint32_t field_count = 5u;
	if (node.name) {
		field_count = 6u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.location) { inline_data_size += 4; }
	if (node.shared) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::VERTEX_OUTPUT_RET));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.location) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.shared) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 5) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 3> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.location) {
		write(*node.location, state);
	}
	if (node.shared) {
		state.write(*node.shared);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[1] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[2] = state.size();
		state.write(0u);
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.attribute_name);
	state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[2], state.size() - patch_offsets[2]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_vertex_value_type_t& node, writer_state_t& state) {
	const uint32_t field_count = 4u;
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	inline_data_size += 4;
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::VERTEX_VALUE_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	write(node.underlying_type, state);
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_viewport_array_index_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::VIEWPORT_ARRAY_INDEX_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_viewport_array_index_attr_t& node, writer_state_t& state) {
	const uint32_t field_count = 0u;
	uint32_t inline_data_size = sizeof(table_root_t);
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::VIEWPORT_ARRAY_INDEX_ATTR));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	state.write(uint32_t(state.size() - vtable_start_offset));
	state.pad();
	return root_table_offset;
}

static inline uint32_t write_node(const node_viewport_array_index_ret_t& node, writer_state_t& state) {
	uint32_t field_count = 3u;
	if (node.name) {
		field_count = 4u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	if (node.shared) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::VIEWPORT_ARRAY_INDEX_RET));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.shared) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.shared) {
		state.write(*node.shared);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_visible_function_t& node, writer_state_t& state) {
	uint32_t field_count = 1u;
	if (node.user_annotation) {
		field_count = 3u;
	} else if (node.stitching_info) {
		field_count = 2u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	inline_data_size += 4;
	if (node.stitching_info) { inline_data_size += 4; }
	if (node.user_annotation) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::VISIBLE_FUNCTION));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	state.write(inline_offset);
	inline_offset += 4;
	if (node.stitching_info) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 1) {
		state.write(uint16_t(0u));
	}
	if (node.user_annotation) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.stitching_info) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.user_annotation) {
		write(*node.user_annotation, state);
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.name);
	if (node.stitching_info) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		write(*node.stitching_info, state);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_visible_function_reference_t& node, writer_state_t& state) {
	const uint32_t field_count = 1u;
	uint32_t inline_data_size = sizeof(table_root_t);
	inline_data_size += 4;
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::VISIBLE_FUNCTION_REFERENCE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	state.write(inline_offset);
	inline_offset += 4;
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	patch_offsets[0] = state.size();
	state.write(0u);
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.function_name);
	return root_table_offset;
}

static inline uint32_t write_node(const node_visible_function_table_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 6u;
	if (node.unused) {
		field_count = 8u;
	} else if (node.name) {
		field_count = 7u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	if (node.location_index) { inline_data_size += 4; }
	if (node.location_count) { inline_data_size += 4; }
	if (node.access_qualifier) { inline_data_size += 4; }
	if (node.raster_order_group) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::VISIBLE_FUNCTION_TABLE_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.location_index) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.location_count) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.access_qualifier) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.raster_order_group) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 6) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 7) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.location_index) {
		write(*node.location_index, state);
	}
	if (node.location_count) {
		write(*node.location_count, state);
	}
	if (node.access_qualifier) {
		state.write(std::underlying_type_t<ACCESS_QUALIFIER>(*node.access_qualifier));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	if (node.raster_order_group) {
		write(*node.raster_order_group, state);
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_visible_function_table_type_t& node, writer_state_t& state) {
	const uint32_t field_count = 4u;
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	inline_data_size += 4;
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::VISIBLE_FUNCTION_TABLE_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	write(node.function_type, state);
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_void_type_t& node, writer_state_t& state) {
	uint32_t field_count = 0u;
	if (node.qualifiers) {
		field_count = 3u;
	} else if (node.alignment) {
		field_count = 2u;
	} else if (node.size) {
		field_count = 1u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	if (node.alignment) { inline_data_size += 4; }
	if (node.qualifiers) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::VOID_TYPE));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 0) {
		state.write(uint16_t(0u));
	}
	if (node.alignment) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 1) {
		state.write(uint16_t(0u));
	}
	if (node.qualifiers) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 1> patch_offsets {};
	if (node.size) {
		state.write(*node.size);
	}
	if (node.alignment) {
		state.write(*node.alignment);
	}
	if (node.qualifiers) {
		patch_offsets[0] = state.size();
		state.write(0u);
	}
	state.pad();
	if (node.qualifiers) {
		state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
		state.write(uint32_t(node.qualifiers->size()));
		for (const auto& elem : *node.qualifiers) {
			write(elem, state);
		}
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_workgroup_max_size_fn_attr_t& node, writer_state_t& state) {
	uint32_t field_count = 0u;
	if (node.size) {
		field_count = 1u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.size) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::WORKGROUP_MAX_SIZE_FN_ATTR));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.size) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 0) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	if (node.size) {
		write(*node.size, state);
	}
	state.pad();
	return root_table_offset;
}

static inline uint32_t write_node(const node_workgroup_size_fn_attr_t& node, writer_state_t& state) {
	uint32_t field_count = 0u;
	if (node.depth) {
		field_count = 3u;
	} else if (node.height) {
		field_count = 2u;
	} else if (node.width) {
		field_count = 1u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.width) { inline_data_size += 4; }
	if (node.height) { inline_data_size += 4; }
	if (node.depth) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::WORKGROUP_SIZE_FN_ATTR));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.width) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 0) {
		state.write(uint16_t(0u));
	}
	if (node.height) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 1) {
		state.write(uint16_t(0u));
	}
	if (node.depth) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	if (node.width) {
		write(*node.width, state);
	}
	if (node.height) {
		write(*node.height, state);
	}
	if (node.depth) {
		write(*node.depth, state);
	}
	state.pad();
	return root_table_offset;
}

static inline uint32_t write_node(const node_workgroup_size_hint_fn_attr_t& node, writer_state_t& state) {
	uint32_t field_count = 0u;
	if (node.depth) {
		field_count = 3u;
	} else if (node.height) {
		field_count = 2u;
	} else if (node.width) {
		field_count = 1u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.width) { inline_data_size += 4; }
	if (node.height) { inline_data_size += 4; }
	if (node.depth) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::WORKGROUP_SIZE_HINT_FN_ATTR));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.width) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 0) {
		state.write(uint16_t(0u));
	}
	if (node.height) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 1) {
		state.write(uint16_t(0u));
	}
	if (node.depth) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	if (node.width) {
		write(*node.width, state);
	}
	if (node.height) {
		write(*node.height, state);
	}
	if (node.depth) {
		write(*node.depth, state);
	}
	state.pad();
	return root_table_offset;
}

static inline uint32_t write_node(const node_world_space_direction_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::WORLD_SPACE_DIRECTION_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_world_space_origin_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::WORLD_SPACE_ORIGIN_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

static inline uint32_t write_node(const node_world_to_object_transform_arg_t& node, writer_state_t& state) {
	uint32_t field_count = 2u;
	if (node.unused) {
		field_count = 4u;
	} else if (node.name) {
		field_count = 3u;
	}
	uint32_t inline_data_size = sizeof(table_root_t);
	if (node.function_constant) { inline_data_size += 4; }
	inline_data_size += 4;
	if (node.name) { inline_data_size += 4; }
	if (node.unused) { inline_data_size += 4; }
	const auto vtable_size = sizeof(table_vtable_t) + field_count * sizeof(uint16_t);
	const auto vtable_size_aligned = vtable_size + ((vtable_size % 4u) != 0 ? sizeof(uint16_t) : 0u);
	state.write(0xC0008u);
	state.write(0x80004u);
	const auto root_table_offset = state.size();
	state.write(0x8u);
	state.write(uint32_t(NODE_TYPE::WORLD_TO_OBJECT_TRANSFORM_ARG));
	state.write(uint32_t(vtable_size_aligned + sizeof(uint32_t)));
	if (vtable_size != vtable_size_aligned /* pad? */) { state.write(uint16_t(0)); }
	const auto vtable_start_offset = state.size();
	state.write(uint16_t(vtable_size));
	state.write(uint16_t(inline_data_size));
	uint16_t inline_offset = sizeof(table_root_t);
	if (node.function_constant) {
		state.write(inline_offset);
		inline_offset += 4;
	} else {
		state.write(uint16_t(0u));
	}
	state.write(inline_offset);
	inline_offset += 4;
	if (node.name) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 2) {
		state.write(uint16_t(0u));
	}
	if (node.unused) {
		state.write(inline_offset);
		inline_offset += 4;
	} else if (field_count > 3) {
		state.write(uint16_t(0u));
	}
	state.write(uint32_t(state.size() - vtable_start_offset));
	std::array<uint32_t, 2> patch_offsets {};
	if (node.function_constant) {
		state.write(*node.function_constant);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	patch_offsets[0] = state.size();
	state.write(0u);
	if (node.name) {
		patch_offsets[1] = state.size();
		state.write(0u);
	}
	if (node.unused) {
		state.write(*node.unused);
		state.write(uint8_t(0));
		state.write(uint8_t(0));
		state.write(uint8_t(0));
	}
	state.pad();
	state.patch(patch_offsets[0], state.size() - patch_offsets[0]);
	state.write(node.type_name);
	if (node.name) {
		state.patch(patch_offsets[1], state.size() - patch_offsets[1]);
		state.write(*node.name);
	}
	return root_table_offset;
}

struct write_dummy_node_t : node_base_t { const NODE_TYPE node_type { NODE_TYPE::NONE }; };

static inline uint32_t write_node(const node_base_t& node, writer_state_t& state) {
	switch (((const write_dummy_node_t*)&node)->node_type) {
		default:
			return 0u;
		case NODE_TYPE::ACCELERATION_STRUCTURE_TYPE:
			return write_node((const node_acceleration_structure_type_t&)node, state);
		case NODE_TYPE::ACCEPT_INTERSECTION_RET:
			return write_node((const node_accept_intersection_ret_t&)node, state);
		case NODE_TYPE::ADDRESS_SPACE_TYPE_QUAL:
			return write_node((const node_address_space_type_qual_t&)node, state);
		case NODE_TYPE::AMPLIFICATION_COUNT_ARG:
			return write_node((const node_amplification_count_arg_t&)node, state);
		case NODE_TYPE::AMPLIFICATION_ID_ARG:
			return write_node((const node_amplification_id_arg_t&)node, state);
		case NODE_TYPE::ARRAY_OF_TYPE:
			return write_node((const node_array_of_type_t&)node, state);
		case NODE_TYPE::ARRAY_REF_OF_TYPE:
			return write_node((const node_array_ref_of_type_t&)node, state);
		case NODE_TYPE::ARRAY_TYPE:
			return write_node((const node_array_type_t&)node, state);
		case NODE_TYPE::BFLOAT_TYPE:
			return write_node((const node_bfloat_type_t&)node, state);
		case NODE_TYPE::BARYCENTRIC_COORD_ARG:
			return write_node((const node_barycentric_coord_arg_t&)node, state);
		case NODE_TYPE::BASE_INSTANCE_ARG:
			return write_node((const node_base_instance_arg_t&)node, state);
		case NODE_TYPE::BASE_VERTEX_ARG:
			return write_node((const node_base_vertex_arg_t&)node, state);
		case NODE_TYPE::BOOL_TYPE:
			return write_node((const node_bool_type_t&)node, state);
		case NODE_TYPE::BUFFER_ARG:
			return write_node((const node_buffer_arg_t&)node, state);
		case NODE_TYPE::BUFFER_STRIDE_ARG:
			return write_node((const node_buffer_stride_arg_t&)node, state);
		case NODE_TYPE::CIARRAY_ARG:
			return write_node((const node_ciarray_arg_t&)node, state);
		case NODE_TYPE::CIBUILTIN_ARG:
			return write_node((const node_cibuiltin_arg_t&)node, state);
		case NODE_TYPE::CIBUILTIN_RET:
			return write_node((const node_cibuiltin_ret_t&)node, state);
		case NODE_TYPE::CI_FUNCTION:
			return write_node((const node_ci_function_t&)node, state);
		case NODE_TYPE::CIIMAGEBLOCK_ARG:
			return write_node((const node_ciimageblock_arg_t&)node, state);
		case NODE_TYPE::CIIMAGEBLOCK_RET:
			return write_node((const node_ciimageblock_ret_t&)node, state);
		case NODE_TYPE::CIMATRIX_ARG:
			return write_node((const node_cimatrix_arg_t&)node, state);
		case NODE_TYPE::CIMATRIX_RET:
			return write_node((const node_cimatrix_ret_t&)node, state);
		case NODE_TYPE::CIPADDING_ARG:
			return write_node((const node_cipadding_arg_t&)node, state);
		case NODE_TYPE::CIPOINTER_ARG:
			return write_node((const node_cipointer_arg_t&)node, state);
		case NODE_TYPE::CIPOINTER_RET:
			return write_node((const node_cipointer_ret_t&)node, state);
		case NODE_TYPE::CISAMPLER_ARG:
			return write_node((const node_cisampler_arg_t&)node, state);
		case NODE_TYPE::CISAMPLER_RET:
			return write_node((const node_cisampler_ret_t&)node, state);
		case NODE_TYPE::CISTRUCT_ARG:
			return write_node((const node_cistruct_arg_t&)node, state);
		case NODE_TYPE::CISTRUCT_RET:
			return write_node((const node_cistruct_ret_t&)node, state);
		case NODE_TYPE::CITEXTURE_ARG:
			return write_node((const node_citexture_arg_t&)node, state);
		case NODE_TYPE::CITEXTURE_RET:
			return write_node((const node_citexture_ret_t&)node, state);
		case NODE_TYPE::CHAR_TYPE:
			return write_node((const node_char_type_t&)node, state);
		case NODE_TYPE::CLIP_DISTANCE_ATTR:
			return write_node((const node_clip_distance_attr_t&)node, state);
		case NODE_TYPE::CLIP_DISTANCE_RET:
			return write_node((const node_clip_distance_ret_t&)node, state);
		case NODE_TYPE::COMMAND_BUFFER_ARG:
			return write_node((const node_command_buffer_arg_t&)node, state);
		case NODE_TYPE::COMMAND_BUFFER_TYPE:
			return write_node((const node_command_buffer_type_t&)node, state);
		case NODE_TYPE::COMPUTE_PIPELINE_STATE_ARG:
			return write_node((const node_compute_pipeline_state_arg_t&)node, state);
		case NODE_TYPE::COMPUTE_PIPELINE_STATE_TYPE:
			return write_node((const node_compute_pipeline_state_type_t&)node, state);
		case NODE_TYPE::CONSTANT_ARG:
			return write_node((const node_constant_arg_t&)node, state);
		case NODE_TYPE::CONTINUE_SEARCH_RET:
			return write_node((const node_continue_search_ret_t&)node, state);
		case NODE_TYPE::CONTROL_POINT_FIELD:
			return write_node((const node_control_point_field_t&)node, state);
		case NODE_TYPE::CONTROL_POINT_INDEX_BUFFER_ARG:
			return write_node((const node_control_point_index_buffer_arg_t&)node, state);
		case NODE_TYPE::CONTROL_POINT_INPUT_ARG:
			return write_node((const node_control_point_input_arg_t&)node, state);
		case NODE_TYPE::CURVE_PARAMETER_ARG:
			return write_node((const node_curve_parameter_arg_t&)node, state);
		case NODE_TYPE::DEPTH2D_ARRAY_TYPE:
			return write_node((const node_depth2d_array_type_t&)node, state);
		case NODE_TYPE::DEPTH2D_MS_ARRAY_TYPE:
			return write_node((const node_depth2d_ms_array_type_t&)node, state);
		case NODE_TYPE::DEPTH2D_MS_TYPE:
			return write_node((const node_depth2d_ms_type_t&)node, state);
		case NODE_TYPE::DEPTH2D_TYPE:
			return write_node((const node_depth2d_type_t&)node, state);
		case NODE_TYPE::DEPTH_CUBE_ARRAY_TYPE:
			return write_node((const node_depth_cube_array_type_t&)node, state);
		case NODE_TYPE::DEPTH_CUBE_TYPE:
			return write_node((const node_depth_cube_type_t&)node, state);
		case NODE_TYPE::DEPTH_RET:
			return write_node((const node_depth_ret_t&)node, state);
		case NODE_TYPE::DEPTH_STENCIL_STATE_ARG:
			return write_node((const node_depth_stencil_state_arg_t&)node, state);
		case NODE_TYPE::DEPTH_STENCIL_STATE_TYPE:
			return write_node((const node_depth_stencil_state_type_t&)node, state);
		case NODE_TYPE::DIRECTION_ARG:
			return write_node((const node_direction_arg_t&)node, state);
		case NODE_TYPE::DISPATCH_QUADGROUPS_PER_THREADGROUP_ARG:
			return write_node((const node_dispatch_quadgroups_per_threadgroup_arg_t&)node, state);
		case NODE_TYPE::DISPATCH_SIMDGROUPS_PER_THREADGROUP_ARG:
			return write_node((const node_dispatch_simdgroups_per_threadgroup_arg_t&)node, state);
		case NODE_TYPE::DISPATCH_THREADS_PER_THREADGROUP_ARG:
			return write_node((const node_dispatch_threads_per_threadgroup_arg_t&)node, state);
		case NODE_TYPE::DISTANCE_ARG:
			return write_node((const node_distance_arg_t&)node, state);
		case NODE_TYPE::DISTANCE_RET:
			return write_node((const node_distance_ret_t&)node, state);
		case NODE_TYPE::DOUBLE_TYPE:
			return write_node((const node_double_type_t&)node, state);
		case NODE_TYPE::ENUM_TYPE:
			return write_node((const node_enum_type_t&)node, state);
		case NODE_TYPE::EXTENTS_TYPE:
			return write_node((const node_extents_type_t&)node, state);
		case NODE_TYPE::FLOAT_TYPE:
			return write_node((const node_float_type_t&)node, state);
		case NODE_TYPE::FRAGMENT_FUNCTION:
			return write_node((const node_fragment_function_t&)node, state);
		case NODE_TYPE::FRAGMENT_INPUT_ARG:
			return write_node((const node_fragment_input_arg_t&)node, state);
		case NODE_TYPE::FRONT_FACING_ARG:
			return write_node((const node_front_facing_arg_t&)node, state);
		case NODE_TYPE::FUNCTION_CONSTANT:
			return write_node((const node_function_constant_t&)node, state);
		case NODE_TYPE::FUNCTION_CONSTANT_PREDICATE_ATTR:
			return write_node((const node_function_constant_predicate_attr_t&)node, state);
		case NODE_TYPE::FUNCTION_HANDLE_ARG:
			return write_node((const node_function_handle_arg_t&)node, state);
		case NODE_TYPE::FUNCTION_HANDLE_TYPE:
			return write_node((const node_function_handle_type_t&)node, state);
		case NODE_TYPE::FUNCTION_ID_ARG:
			return write_node((const node_function_id_arg_t&)node, state);
		case NODE_TYPE::FUNCTION_TYPE:
			return write_node((const node_function_type_t&)node, state);
		case NODE_TYPE::GEOMETRY_ID_ARG:
			return write_node((const node_geometry_id_arg_t&)node, state);
		case NODE_TYPE::GEOMETRY_INTERSECTION_FUNCTION_TABLE_OFFSET_ARG:
			return write_node((const node_geometry_intersection_function_table_offset_arg_t&)node, state);
		case NODE_TYPE::GLOBAL_BINDING:
			return write_node((const node_global_binding_t&)node, state);
		case NODE_TYPE::HALF_TYPE:
			return write_node((const node_half_type_t&)node, state);
		case NODE_TYPE::IMAGEBLOCK_ARG:
			return write_node((const node_imageblock_arg_t&)node, state);
		case NODE_TYPE::IMAGEBLOCK_DATA_ARG:
			return write_node((const node_imageblock_data_arg_t&)node, state);
		case NODE_TYPE::IMAGEBLOCK_DATA_RET:
			return write_node((const node_imageblock_data_ret_t&)node, state);
		case NODE_TYPE::IMAGEBLOCK_TYPE:
			return write_node((const node_imageblock_type_t&)node, state);
		case NODE_TYPE::INDIRECT_BUFFER_ARG:
			return write_node((const node_indirect_buffer_arg_t&)node, state);
		case NODE_TYPE::INDIRECT_CONSTANT_ARG:
			return write_node((const node_indirect_constant_arg_t&)node, state);
		case NODE_TYPE::INLINE_TYPE_INFO:
			return write_node((const node_inline_type_info_t&)node, state);
		case NODE_TYPE::INSTANCE_ACCELERATION_STRUCTURE_ARG:
			return write_node((const node_instance_acceleration_structure_arg_t&)node, state);
		case NODE_TYPE::INSTANCE_ID_ARG:
			return write_node((const node_instance_id_arg_t&)node, state);
		case NODE_TYPE::INSTANCE_ID_COUNT_ARG:
			return write_node((const node_instance_id_count_arg_t&)node, state);
		case NODE_TYPE::INSTANCE_INTERSECTION_FUNCTION_TABLE_OFFSET_ARG:
			return write_node((const node_instance_intersection_function_table_offset_arg_t&)node, state);
		case NODE_TYPE::INT_TYPE:
			return write_node((const node_int_type_t&)node, state);
		case NODE_TYPE::INTERPOLANT_TYPE:
			return write_node((const node_interpolant_type_t&)node, state);
		case NODE_TYPE::INTERSECTION_FUNCTION:
			return write_node((const node_intersection_function_t&)node, state);
		case NODE_TYPE::INTERSECTION_FUNCTION_HANDLE_TYPE:
			return write_node((const node_intersection_function_handle_type_t&)node, state);
		case NODE_TYPE::INTERSECTION_FUNCTION_TABLE_ARG:
			return write_node((const node_intersection_function_table_arg_t&)node, state);
		case NODE_TYPE::INTERSECTION_FUNCTION_TABLE_TYPE:
			return write_node((const node_intersection_function_table_type_t&)node, state);
		case NODE_TYPE::INVARIANT_ATTR:
			return write_node((const node_invariant_attr_t&)node, state);
		case NODE_TYPE::KERNEL_FUNCTION:
			return write_node((const node_kernel_function_t&)node, state);
		case NODE_TYPE::KEY_FRAME_COUNT_ARG:
			return write_node((const node_key_frame_count_arg_t&)node, state);
		case NODE_TYPE::LLONG_TYPE:
			return write_node((const node_llong_type_t&)node, state);
		case NODE_TYPE::LVALUE_REFERENCE_TYPE:
			return write_node((const node_lvalue_reference_type_t&)node, state);
		case NODE_TYPE::LOCATION_INDEX_ATTR:
			return write_node((const node_location_index_attr_t&)node, state);
		case NODE_TYPE::LONG_TYPE:
			return write_node((const node_long_type_t&)node, state);
		case NODE_TYPE::MATRIX_TYPE:
			return write_node((const node_matrix_type_t&)node, state);
		case NODE_TYPE::MAX_DISTANCE_ARG:
			return write_node((const node_max_distance_arg_t&)node, state);
		case NODE_TYPE::MAX_MESH_WORKGROUPS_FN_ATTR:
			return write_node((const node_max_mesh_workgroups_fn_attr_t&)node, state);
		case NODE_TYPE::MESH_ARG:
			return write_node((const node_mesh_arg_t&)node, state);
		case NODE_TYPE::MESH_EMULATION_BLOCK:
			return write_node((const node_mesh_emulation_block_t&)node, state);
		case NODE_TYPE::MESH_EMULATION_FRAGMENT_ANALYSIS_RESULT:
			return write_node((const node_mesh_emulation_fragment_analysis_result_t&)node, state);
		case NODE_TYPE::MESH_EMULATION_MESH_KERNEL:
			return write_node((const node_mesh_emulation_mesh_kernel_t&)node, state);
		case NODE_TYPE::MESH_EMULATION_MESH_LAYOUT:
			return write_node((const node_mesh_emulation_mesh_layout_t&)node, state);
		case NODE_TYPE::MESH_EMULATION_MESH_VERTEX:
			return write_node((const node_mesh_emulation_mesh_vertex_t&)node, state);
		case NODE_TYPE::MESH_EMULATION_OBJECT_KERNEL:
			return write_node((const node_mesh_emulation_object_kernel_t&)node, state);
		case NODE_TYPE::MESH_EMULATION_VALUE_GROUP:
			return write_node((const node_mesh_emulation_value_group_t&)node, state);
		case NODE_TYPE::MESH_FUNCTION:
			return write_node((const node_mesh_function_t&)node, state);
		case NODE_TYPE::MESH_GRID_PROPERTIES_ARG:
			return write_node((const node_mesh_grid_properties_arg_t&)node, state);
		case NODE_TYPE::MESH_GRID_PROPERTIES_TYPE:
			return write_node((const node_mesh_grid_properties_type_t&)node, state);
		case NODE_TYPE::MESH_PRIMITIVE_DATA_RET:
			return write_node((const node_mesh_primitive_data_ret_t&)node, state);
		case NODE_TYPE::MESH_TYPE:
			return write_node((const node_mesh_type_t&)node, state);
		case NODE_TYPE::MESH_TYPE_INFO:
			return write_node((const node_mesh_type_info_t&)node, state);
		case NODE_TYPE::MESH_VERTEX_DATA_RET:
			return write_node((const node_mesh_vertex_data_ret_t&)node, state);
		case NODE_TYPE::MIN_DISTANCE_ARG:
			return write_node((const node_min_distance_arg_t&)node, state);
		case NODE_TYPE::MOTION_END_TIME_ARG:
			return write_node((const node_motion_end_time_arg_t&)node, state);
		case NODE_TYPE::MOTION_START_TIME_ARG:
			return write_node((const node_motion_start_time_arg_t&)node, state);
		case NODE_TYPE::OBJECT_FUNCTION:
			return write_node((const node_object_function_t&)node, state);
		case NODE_TYPE::OBJECT_TO_WORLD_TRANSFORM_ARG:
			return write_node((const node_object_to_world_transform_arg_t&)node, state);
		case NODE_TYPE::OPAQUE_PRIMITIVE_ARG:
			return write_node((const node_opaque_primitive_arg_t&)node, state);
		case NODE_TYPE::OPAQUE_TYPE:
			return write_node((const node_opaque_type_t&)node, state);
		case NODE_TYPE::ORIGIN_ARG:
			return write_node((const node_origin_arg_t&)node, state);
		case NODE_TYPE::PACKED_VECTOR_TYPE:
			return write_node((const node_packed_vector_type_t&)node, state);
		case NODE_TYPE::PATCH_CONTROL_POINT_TYPE:
			return write_node((const node_patch_control_point_type_t&)node, state);
		case NODE_TYPE::PATCH_FN_ATTR:
			return write_node((const node_patch_fn_attr_t&)node, state);
		case NODE_TYPE::PATCH_ID_ARG:
			return write_node((const node_patch_id_arg_t&)node, state);
		case NODE_TYPE::PATCH_INPUT_ARG:
			return write_node((const node_patch_input_arg_t&)node, state);
		case NODE_TYPE::PAYLOAD_ARG:
			return write_node((const node_payload_arg_t&)node, state);
		case NODE_TYPE::PIXEL_POSITION_IN_TILE_ARG:
			return write_node((const node_pixel_position_in_tile_arg_t&)node, state);
		case NODE_TYPE::PIXELS_PER_TILE_ARG:
			return write_node((const node_pixels_per_tile_arg_t&)node, state);
		case NODE_TYPE::POINT_COORD_ARG:
			return write_node((const node_point_coord_arg_t&)node, state);
		case NODE_TYPE::POINT_SIZE_ATTR:
			return write_node((const node_point_size_attr_t&)node, state);
		case NODE_TYPE::POINT_SIZE_RET:
			return write_node((const node_point_size_ret_t&)node, state);
		case NODE_TYPE::POINTER_TYPE:
			return write_node((const node_pointer_type_t&)node, state);
		case NODE_TYPE::POSITION_ARG:
			return write_node((const node_position_arg_t&)node, state);
		case NODE_TYPE::POSITION_ATTR:
			return write_node((const node_position_attr_t&)node, state);
		case NODE_TYPE::POSITION_IN_PATCH_ARG:
			return write_node((const node_position_in_patch_arg_t&)node, state);
		case NODE_TYPE::POSITION_RET:
			return write_node((const node_position_ret_t&)node, state);
		case NODE_TYPE::PRIMITIVE_ACCELERATION_STRUCTURE_ARG:
			return write_node((const node_primitive_acceleration_structure_arg_t&)node, state);
		case NODE_TYPE::PRIMITIVE_CULLED_ATTR:
			return write_node((const node_primitive_culled_attr_t&)node, state);
		case NODE_TYPE::PRIMITIVE_CULLED_RET:
			return write_node((const node_primitive_culled_ret_t&)node, state);
		case NODE_TYPE::PRIMITIVE_DATA_ARG:
			return write_node((const node_primitive_data_arg_t&)node, state);
		case NODE_TYPE::PRIMITIVE_ID_ARG:
			return write_node((const node_primitive_id_arg_t&)node, state);
		case NODE_TYPE::PRIMITIVE_ID_ATTR:
			return write_node((const node_primitive_id_attr_t&)node, state);
		case NODE_TYPE::PRIMITIVE_ID_RET:
			return write_node((const node_primitive_id_ret_t&)node, state);
		case NODE_TYPE::QUADGROUP_INDEX_IN_THREADGROUP_ARG:
			return write_node((const node_quadgroup_index_in_threadgroup_arg_t&)node, state);
		case NODE_TYPE::QUADGROUPS_PER_THREADGROUP_ARG:
			return write_node((const node_quadgroups_per_threadgroup_arg_t&)node, state);
		case NODE_TYPE::R16SNORM_TYPE:
			return write_node((const node_r16snorm_type_t&)node, state);
		case NODE_TYPE::R16UNORM_TYPE:
			return write_node((const node_r16unorm_type_t&)node, state);
		case NODE_TYPE::R8SNORM_TYPE:
			return write_node((const node_r8snorm_type_t&)node, state);
		case NODE_TYPE::R8UNORM_TYPE:
			return write_node((const node_r8unorm_type_t&)node, state);
		case NODE_TYPE::RG11B10F_TYPE:
			return write_node((const node_rg11b10f_type_t&)node, state);
		case NODE_TYPE::RG16SNORM_TYPE:
			return write_node((const node_rg16snorm_type_t&)node, state);
		case NODE_TYPE::RG16UNORM_TYPE:
			return write_node((const node_rg16unorm_type_t&)node, state);
		case NODE_TYPE::RG8SNORM_TYPE:
			return write_node((const node_rg8snorm_type_t&)node, state);
		case NODE_TYPE::RG8UNORM_TYPE:
			return write_node((const node_rg8unorm_type_t&)node, state);
		case NODE_TYPE::RGB10A2_TYPE:
			return write_node((const node_rgb10a2_type_t&)node, state);
		case NODE_TYPE::RGB9E5_TYPE:
			return write_node((const node_rgb9e5_type_t&)node, state);
		case NODE_TYPE::RGBA16SNORM_TYPE:
			return write_node((const node_rgba16snorm_type_t&)node, state);
		case NODE_TYPE::RGBA16UNORM_TYPE:
			return write_node((const node_rgba16unorm_type_t&)node, state);
		case NODE_TYPE::RGBA8SNORM_TYPE:
			return write_node((const node_rgba8snorm_type_t&)node, state);
		case NODE_TYPE::RGBA8UNORM_TYPE:
			return write_node((const node_rgba8unorm_type_t&)node, state);
		case NODE_TYPE::RVALUE_REFERENCE_TYPE:
			return write_node((const node_rvalue_reference_type_t&)node, state);
		case NODE_TYPE::RECORD_BASE:
			return write_node((const node_record_base_t&)node, state);
		case NODE_TYPE::RECORD_FIELD:
			return write_node((const node_record_field_t&)node, state);
		case NODE_TYPE::RENDER_PIPELINE_STATE_ARG:
			return write_node((const node_render_pipeline_state_arg_t&)node, state);
		case NODE_TYPE::RENDER_PIPELINE_STATE_TYPE:
			return write_node((const node_render_pipeline_state_type_t&)node, state);
		case NODE_TYPE::RENDER_TARGET_ARG:
			return write_node((const node_render_target_arg_t&)node, state);
		case NODE_TYPE::RENDER_TARGET_ARRAY_INDEX_ARG:
			return write_node((const node_render_target_array_index_arg_t&)node, state);
		case NODE_TYPE::RENDER_TARGET_ARRAY_INDEX_ATTR:
			return write_node((const node_render_target_array_index_attr_t&)node, state);
		case NODE_TYPE::RENDER_TARGET_ARRAY_INDEX_RET:
			return write_node((const node_render_target_array_index_ret_t&)node, state);
		case NODE_TYPE::RENDER_TARGET_ATTR:
			return write_node((const node_render_target_attr_t&)node, state);
		case NODE_TYPE::RENDER_TARGET_RET:
			return write_node((const node_render_target_ret_t&)node, state);
		case NODE_TYPE::SRGBA8UNORM_TYPE:
			return write_node((const node_srgba8unorm_type_t&)node, state);
		case NODE_TYPE::SAMPLE_ID_ARG:
			return write_node((const node_sample_id_arg_t&)node, state);
		case NODE_TYPE::SAMPLE_MASK_ARG:
			return write_node((const node_sample_mask_arg_t&)node, state);
		case NODE_TYPE::SAMPLE_MASK_RET:
			return write_node((const node_sample_mask_ret_t&)node, state);
		case NODE_TYPE::SAMPLER_ARG:
			return write_node((const node_sampler_arg_t&)node, state);
		case NODE_TYPE::SAMPLER_TYPE:
			return write_node((const node_sampler_type_t&)node, state);
		case NODE_TYPE::SHARED_ATTR:
			return write_node((const node_shared_attr_t&)node, state);
		case NODE_TYPE::SHORT_TYPE:
			return write_node((const node_short_type_t&)node, state);
		case NODE_TYPE::SIMDGROUP_INDEX_IN_THREADGROUP_ARG:
			return write_node((const node_simdgroup_index_in_threadgroup_arg_t&)node, state);
		case NODE_TYPE::SIMDGROUPS_PER_THREADGROUP_ARG:
			return write_node((const node_simdgroups_per_threadgroup_arg_t&)node, state);
		case NODE_TYPE::STAGE_IN_ARG:
			return write_node((const node_stage_in_arg_t&)node, state);
		case NODE_TYPE::STAGE_IN_GRID_ORIGIN_ARG:
			return write_node((const node_stage_in_grid_origin_arg_t&)node, state);
		case NODE_TYPE::STAGE_IN_GRID_SIZE_ARG:
			return write_node((const node_stage_in_grid_size_arg_t&)node, state);
		case NODE_TYPE::STENCIL_RET:
			return write_node((const node_stencil_ret_t&)node, state);
		case NODE_TYPE::STITCHING_ARGUMENT:
			return write_node((const node_stitching_argument_t&)node, state);
		case NODE_TYPE::STRUCT_TYPE:
			return write_node((const node_struct_type_t&)node, state);
		case NODE_TYPE::STRUCT_TYPE_INFO:
			return write_node((const node_struct_type_info_t&)node, state);
		case NODE_TYPE::TENSOR_ARG:
			return write_node((const node_tensor_arg_t&)node, state);
		case NODE_TYPE::TENSOR_TYPE:
			return write_node((const node_tensor_type_t&)node, state);
		case NODE_TYPE::TEXTURE1D_ARRAY_TYPE:
			return write_node((const node_texture1d_array_type_t&)node, state);
		case NODE_TYPE::TEXTURE1D_TYPE:
			return write_node((const node_texture1d_type_t&)node, state);
		case NODE_TYPE::TEXTURE2D_ARRAY_TYPE:
			return write_node((const node_texture2d_array_type_t&)node, state);
		case NODE_TYPE::TEXTURE2D_MS_ARRAY_TYPE:
			return write_node((const node_texture2d_ms_array_type_t&)node, state);
		case NODE_TYPE::TEXTURE2D_MS_TYPE:
			return write_node((const node_texture2d_ms_type_t&)node, state);
		case NODE_TYPE::TEXTURE2D_TYPE:
			return write_node((const node_texture2d_type_t&)node, state);
		case NODE_TYPE::TEXTURE3D_TYPE:
			return write_node((const node_texture3d_type_t&)node, state);
		case NODE_TYPE::TEXTURE_ARG:
			return write_node((const node_texture_arg_t&)node, state);
		case NODE_TYPE::TEXTURE_BUFFER1D_TYPE:
			return write_node((const node_texture_buffer1d_type_t&)node, state);
		case NODE_TYPE::TEXTURE_CUBE_ARRAY_TYPE:
			return write_node((const node_texture_cube_array_type_t&)node, state);
		case NODE_TYPE::TEXTURE_CUBE_TYPE:
			return write_node((const node_texture_cube_type_t&)node, state);
		case NODE_TYPE::THREAD_EXECUTION_WIDTH_ARG:
			return write_node((const node_thread_execution_width_arg_t&)node, state);
		case NODE_TYPE::THREAD_INDEX_IN_QUADGROUP_ARG:
			return write_node((const node_thread_index_in_quadgroup_arg_t&)node, state);
		case NODE_TYPE::THREAD_INDEX_IN_SIMDGROUP_ARG:
			return write_node((const node_thread_index_in_simdgroup_arg_t&)node, state);
		case NODE_TYPE::THREAD_INDEX_IN_THREADGROUP_ARG:
			return write_node((const node_thread_index_in_threadgroup_arg_t&)node, state);
		case NODE_TYPE::THREAD_POSITION_IN_GRID_ARG:
			return write_node((const node_thread_position_in_grid_arg_t&)node, state);
		case NODE_TYPE::THREAD_POSITION_IN_THREADGROUP_ARG:
			return write_node((const node_thread_position_in_threadgroup_arg_t&)node, state);
		case NODE_TYPE::THREADGROUP_POSITION_IN_GRID_ARG:
			return write_node((const node_threadgroup_position_in_grid_arg_t&)node, state);
		case NODE_TYPE::THREADGROUPS_PER_GRID_ARG:
			return write_node((const node_threadgroups_per_grid_arg_t&)node, state);
		case NODE_TYPE::THREADS_PER_GRID_ARG:
			return write_node((const node_threads_per_grid_arg_t&)node, state);
		case NODE_TYPE::THREADS_PER_SIMDGROUP_ARG:
			return write_node((const node_threads_per_simdgroup_arg_t&)node, state);
		case NODE_TYPE::THREADS_PER_THREADGROUP_ARG:
			return write_node((const node_threads_per_threadgroup_arg_t&)node, state);
		case NODE_TYPE::TILE_INDEX_ARG:
			return write_node((const node_tile_index_arg_t&)node, state);
		case NODE_TYPE::TIME_ARG:
			return write_node((const node_time_arg_t&)node, state);
		case NODE_TYPE::UCHAR_TYPE:
			return write_node((const node_uchar_type_t&)node, state);
		case NODE_TYPE::UINT_TYPE:
			return write_node((const node_uint_type_t&)node, state);
		case NODE_TYPE::ULLONG_TYPE:
			return write_node((const node_ullong_type_t&)node, state);
		case NODE_TYPE::ULONG_TYPE:
			return write_node((const node_ulong_type_t&)node, state);
		case NODE_TYPE::USHORT_TYPE:
			return write_node((const node_ushort_type_t&)node, state);
		case NODE_TYPE::UNION_TYPE:
			return write_node((const node_union_type_t&)node, state);
		case NODE_TYPE::USER_ANNOTATION_FN_ATTR:
			return write_node((const node_user_annotation_fn_attr_t&)node, state);
		case NODE_TYPE::USER_ATTR:
			return write_node((const node_user_attr_t&)node, state);
		case NODE_TYPE::USER_DATA_BUFFER_ARG:
			return write_node((const node_user_data_buffer_arg_t&)node, state);
		case NODE_TYPE::USER_INSTANCE_ID_ARG:
			return write_node((const node_user_instance_id_arg_t&)node, state);
		case NODE_TYPE::USER_INSTANCE_ID_COUNT_ARG:
			return write_node((const node_user_instance_id_count_arg_t&)node, state);
		case NODE_TYPE::VEC_TYPE_HINT_FN_ATTR:
			return write_node((const node_vec_type_hint_fn_attr_t&)node, state);
		case NODE_TYPE::VECTOR_TYPE:
			return write_node((const node_vector_type_t&)node, state);
		case NODE_TYPE::VERTEX_FUNCTION:
			return write_node((const node_vertex_function_t&)node, state);
		case NODE_TYPE::VERTEX_ID_ARG:
			return write_node((const node_vertex_id_arg_t&)node, state);
		case NODE_TYPE::VERTEX_INPUT_ARG:
			return write_node((const node_vertex_input_arg_t&)node, state);
		case NODE_TYPE::VERTEX_OUTPUT_RET:
			return write_node((const node_vertex_output_ret_t&)node, state);
		case NODE_TYPE::VERTEX_VALUE_TYPE:
			return write_node((const node_vertex_value_type_t&)node, state);
		case NODE_TYPE::VIEWPORT_ARRAY_INDEX_ARG:
			return write_node((const node_viewport_array_index_arg_t&)node, state);
		case NODE_TYPE::VIEWPORT_ARRAY_INDEX_ATTR:
			return write_node((const node_viewport_array_index_attr_t&)node, state);
		case NODE_TYPE::VIEWPORT_ARRAY_INDEX_RET:
			return write_node((const node_viewport_array_index_ret_t&)node, state);
		case NODE_TYPE::VISIBLE_FUNCTION:
			return write_node((const node_visible_function_t&)node, state);
		case NODE_TYPE::VISIBLE_FUNCTION_REFERENCE:
			return write_node((const node_visible_function_reference_t&)node, state);
		case NODE_TYPE::VISIBLE_FUNCTION_TABLE_ARG:
			return write_node((const node_visible_function_table_arg_t&)node, state);
		case NODE_TYPE::VISIBLE_FUNCTION_TABLE_TYPE:
			return write_node((const node_visible_function_table_type_t&)node, state);
		case NODE_TYPE::VOID_TYPE:
			return write_node((const node_void_type_t&)node, state);
		case NODE_TYPE::WORKGROUP_MAX_SIZE_FN_ATTR:
			return write_node((const node_workgroup_max_size_fn_attr_t&)node, state);
		case NODE_TYPE::WORKGROUP_SIZE_FN_ATTR:
			return write_node((const node_workgroup_size_fn_attr_t&)node, state);
		case NODE_TYPE::WORKGROUP_SIZE_HINT_FN_ATTR:
			return write_node((const node_workgroup_size_hint_fn_attr_t&)node, state);
		case NODE_TYPE::WORLD_SPACE_DIRECTION_ARG:
			return write_node((const node_world_space_direction_arg_t&)node, state);
		case NODE_TYPE::WORLD_SPACE_ORIGIN_ARG:
			return write_node((const node_world_space_origin_arg_t&)node, state);
		case NODE_TYPE::WORLD_TO_OBJECT_TRANSFORM_ARG:
			return write_node((const node_world_to_object_transform_arg_t&)node, state);
	}
}

static inline std::vector<uint8_t> write(const reflection_t& refl) {
	writer_state_t state {};
	state.data.reserve(256u + 64u * (refl.nodes ? refl.nodes->size() : 1u));

	state.write(0u); // offset to root, patched later
	state.write(0x52524941u); // AIRR magic
	state.patch(0u, write(refl, state)); // write reflection + patch initial offset

	return (!state.is_error ? state.data : std::vector<uint8_t> {});
}

} // namespace metal::reflection
