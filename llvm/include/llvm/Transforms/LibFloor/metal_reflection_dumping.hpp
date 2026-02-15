
#pragma once

#include <string>
#include <vector>
#include <optional>
#include <span>
#include <string_view>
#include <cstdint>

namespace metal::reflection {

static void dump_node(const node_base_t& node, llvm::raw_ostream& out, const uint32_t depth);

// forward declare this, because instantiation order doesn't match
static inline void dump(const uint_value_t& obj, llvm::raw_ostream& out, const uint32_t depth);

static inline void dump(const version_t& obj, llvm::raw_ostream& out, const uint32_t depth) {
	const std::string depth_prefix(depth, '\t');
	out << depth_prefix << "major: " << obj.major << '\n';
	out << depth_prefix << "minor: " << obj.minor << '\n';
	out << depth_prefix << "sub_minor: " << obj.sub_minor << '\n';
}

static inline void dump(const bitfield_info_t& obj, llvm::raw_ostream& out, const uint32_t depth) {
	const std::string depth_prefix(depth, '\t');
	out << depth_prefix << "bit_offset: " << obj.bit_offset << '\n';
	out << depth_prefix << "bit_size: " << obj.bit_size << '\n';
	out << depth_prefix << "storage_size: " << obj.storage_size << '\n';
}

static inline void dump(const bool_value_t& obj, llvm::raw_ostream& out, const uint32_t depth) {
	const std::string depth_prefix(depth, '\t');
	out << depth_prefix << "value: " << obj.value << '\n';
}

static inline void dump(const local_allocation_t& obj, llvm::raw_ostream& out, const uint32_t depth) {
	const std::string depth_prefix(depth, '\t');
	if (obj.size) {
		out << depth_prefix << "size: " << *obj.size << '\n';
	}
	if (obj.alignment) {
		out << depth_prefix << "alignment: " << *obj.alignment << '\n';
	}
}

static inline void dump(const node_id_t& obj, llvm::raw_ostream& out, const uint32_t depth) {
	const std::string depth_prefix(depth, '\t');
	out << depth_prefix << "id: " << obj.id << '\n';
}

static inline void dump(const reflection_t& obj, llvm::raw_ostream& out, const uint32_t depth) {
	const std::string depth_prefix(depth, '\t');
	if (obj.version) {
		out << depth_prefix << "version:" << '\n';
		dump(*obj.version, out, depth + 1u);
	}
	if (obj.nodes) {
		out << depth_prefix << "nodes:\n";
		for (const auto& elem : *obj.nodes) {
			if (elem) { dump_node(*elem, out, depth + 1u); }
		}
	}
	if (obj.fragment_functions) {
		out << depth_prefix << "fragment_functions:\n";
		for (const auto& elem : *obj.fragment_functions) {
			dump(elem, out, depth + 1u);
		}
	}
	if (obj.intersection_functions) {
		out << depth_prefix << "intersection_functions:\n";
		for (const auto& elem : *obj.intersection_functions) {
			dump(elem, out, depth + 1u);
		}
	}
	if (obj.kernel_functions) {
		out << depth_prefix << "kernel_functions:\n";
		for (const auto& elem : *obj.kernel_functions) {
			dump(elem, out, depth + 1u);
		}
	}
	if (obj.vertex_functions) {
		out << depth_prefix << "vertex_functions:\n";
		for (const auto& elem : *obj.vertex_functions) {
			dump(elem, out, depth + 1u);
		}
	}
	if (obj.visible_functions) {
		out << depth_prefix << "visible_functions:\n";
		for (const auto& elem : *obj.visible_functions) {
			dump(elem, out, depth + 1u);
		}
	}
	if (obj.mesh_functions) {
		out << depth_prefix << "mesh_functions:\n";
		for (const auto& elem : *obj.mesh_functions) {
			dump(elem, out, depth + 1u);
		}
	}
	if (obj.object_functions) {
		out << depth_prefix << "object_functions:\n";
		for (const auto& elem : *obj.object_functions) {
			dump(elem, out, depth + 1u);
		}
	}
	if (obj.function_constants) {
		out << depth_prefix << "function_constants:\n";
		for (const auto& elem : *obj.function_constants) {
			dump(elem, out, depth + 1u);
		}
	}
	if (obj.static_local_allocations) {
		out << depth_prefix << "static_local_allocations:\n";
		for (const auto& elem : *obj.static_local_allocations) {
			dump(elem, out, depth + 1u);
			out << depth_prefix << "\t---\n";
		}
	}
	if (obj.emulations) {
		out << depth_prefix << "emulations:\n";
		for (const auto& elem : *obj.emulations) {
			dump(elem, out, depth + 1u);
		}
	}
	if (obj.global_bindings) {
		out << depth_prefix << "global_bindings:\n";
		for (const auto& elem : *obj.global_bindings) {
			dump(elem, out, depth + 1u);
		}
	}
	if (obj.visible_function_references) {
		out << depth_prefix << "visible_function_references:\n";
		for (const auto& elem : *obj.visible_function_references) {
			dump(elem, out, depth + 1u);
		}
	}
	if (obj.ci_functions) {
		out << depth_prefix << "ci_functions:\n";
		for (const auto& elem : *obj.ci_functions) {
			dump(elem, out, depth + 1u);
		}
	}
}

static inline void dump(const stitching_info_t& obj, llvm::raw_ostream& out, const uint32_t depth) {
	const std::string depth_prefix(depth, '\t');
	if (obj.return_type) {
		out << depth_prefix << "return_type:" << '\n';
		dump(*obj.return_type, out, depth + 1u);
	}
	if (obj.arguments) {
		out << depth_prefix << "arguments:\n";
		for (const auto& elem : *obj.arguments) {
			dump(elem, out, depth + 1u);
		}
	}
}

static inline void dump(const struct_type_info_field_t& obj, llvm::raw_ostream& out, const uint32_t depth) {
	const std::string depth_prefix(depth, '\t');
	if (obj.struct_type_info) {
		out << depth_prefix << "struct_type_info:" << '\n';
		dump(*obj.struct_type_info, out, depth + 1u);
	}
	if (obj.offset) {
		out << depth_prefix << "offset: " << *obj.offset << '\n';
	}
	if (obj.size) {
		out << depth_prefix << "size: " << *obj.size << '\n';
	}
	if (obj.array_entries) {
		out << depth_prefix << "array_entries: " << *obj.array_entries << '\n';
	}
	if (obj.type_name) {
		out << depth_prefix << "type_name: " << *obj.type_name << '\n';
	}
	if (obj.field_name) {
		out << depth_prefix << "field_name: " << *obj.field_name << '\n';
	}
	if (obj.attribute_name) {
		out << depth_prefix << "attribute_name: " << *obj.attribute_name << '\n';
	}
	if (obj.indirect_argument) {
		out << depth_prefix << "indirect_argument:" << '\n';
		dump(*obj.indirect_argument, out, depth + 1u);
	}
	if (obj.indirect_location) {
		out << depth_prefix << "indirect_location:" << '\n';
		dump(*obj.indirect_location, out, depth + 1u);
	}
	if (obj.raster_order_group) {
		out << depth_prefix << "raster_order_group:" << '\n';
		dump(*obj.raster_order_group, out, depth + 1u);
	}
	if (obj.render_target_index) {
		out << depth_prefix << "render_target_index:" << '\n';
		dump(*obj.render_target_index, out, depth + 1u);
	}
	if (obj.inline_type_info) {
		out << depth_prefix << "inline_type_info:" << '\n';
		dump(*obj.inline_type_info, out, depth + 1u);
	}
}

static inline void dump(const uint_value_t& obj, llvm::raw_ostream& out, const uint32_t depth) {
	const std::string depth_prefix(depth, '\t');
	out << depth_prefix << "value: " << obj.value << '\n';
}

static inline void dump_node(const node_acceleration_structure_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "acceleration-structure-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	if (node.instancing) {
		out << depth_prefix << "instancing: " << *node.instancing << '\n';
	}
	if (node.primitive_motion) {
		out << depth_prefix << "primitive_motion: " << *node.primitive_motion << '\n';
	}
	if (node.instance_motion) {
		out << depth_prefix << "instance_motion: " << *node.instance_motion << '\n';
	}
}

static inline void dump_node(const node_accept_intersection_ret_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "accept-intersection-ret\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
}

static inline void dump_node(const node_address_space_type_qual_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "address-space-type-qual\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.address_space) {
		out << depth_prefix << "address_space: " << address_space_to_string(*node.address_space) << '\n';
	}
}

static inline void dump_node(const node_amplification_count_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "amplification-count-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_amplification_id_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "amplification-id-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_array_of_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "array-of-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	out << depth_prefix << "element_type:" << '\n';
	dump(node.element_type, out, depth + 2u);
	if (node.num_elements) {
		out << depth_prefix << "num_elements: " << *node.num_elements << '\n';
	}
}

static inline void dump_node(const node_array_ref_of_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "array-ref-of-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	out << depth_prefix << "element_type:" << '\n';
	dump(node.element_type, out, depth + 2u);
}

static inline void dump_node(const node_array_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "array-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	out << depth_prefix << "element_type:" << '\n';
	dump(node.element_type, out, depth + 2u);
	if (node.num_elements) {
		out << depth_prefix << "num_elements: " << *node.num_elements << '\n';
	}
}

static inline void dump_node(const node_bfloat_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "bfloat-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	out << depth_prefix << "size: " << node.size << '\n';
	out << depth_prefix << "alignment: " << node.alignment << '\n';
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
}

static inline void dump_node(const node_barycentric_coord_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "barycentric-coord-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	if (node.sampling_qualifier) {
		out << depth_prefix << "sampling_qualifier: " << sampling_qualifier_to_string(*node.sampling_qualifier) << '\n';
	}
	if (node.interpolation_qualifier) {
		out << depth_prefix << "interpolation_qualifier: " << interpolation_qualifier_to_string(*node.interpolation_qualifier) << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_base_instance_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "base-instance-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_base_vertex_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "base-vertex-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_bool_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "bool-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	out << depth_prefix << "size: " << node.size << '\n';
	out << depth_prefix << "alignment: " << node.alignment << '\n';
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
}

static inline void dump_node(const node_buffer_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "buffer-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	if (node.buffer_size) {
		out << depth_prefix << "buffer_size:" << '\n';
		dump(*node.buffer_size, out, depth + 2u);
	}
	if (node.location_index) {
		out << depth_prefix << "location_index:" << '\n';
		dump(*node.location_index, out, depth + 2u);
	}
	if (node.location_count) {
		out << depth_prefix << "location_count:" << '\n';
		dump(*node.location_count, out, depth + 2u);
	}
	if (node.access_qualifier) {
		out << depth_prefix << "access_qualifier: " << access_qualifier_to_string(*node.access_qualifier) << '\n';
	}
	out << depth_prefix << "address_space: " << address_space_to_string(node.address_space) << '\n';
	if (node.struct_type_info) {
		out << depth_prefix << "struct_type_info:" << '\n';
		dump(*node.struct_type_info, out, depth + 2u);
	}
	if (node.raster_order_group) {
		out << depth_prefix << "raster_order_group:" << '\n';
		dump(*node.raster_order_group, out, depth + 2u);
	}
	if (node.type_size) {
		out << depth_prefix << "type_size:" << '\n';
		dump(*node.type_size, out, depth + 2u);
	}
	if (node.type_align) {
		out << depth_prefix << "type_align:" << '\n';
		dump(*node.type_align, out, depth + 2u);
	}
	if (node.type_name) {
		out << depth_prefix << "type_name: " << *node.type_name << '\n';
	}
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
	if (node.inline_type_info) {
		out << depth_prefix << "inline_type_info:" << '\n';
		dump(*node.inline_type_info, out, depth + 2u);
	}
}

static inline void dump_node(const node_buffer_stride_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "buffer-stride-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	if (node.location_index) {
		out << depth_prefix << "location_index:" << '\n';
		dump(*node.location_index, out, depth + 2u);
	}
	if (node.location_count) {
		out << depth_prefix << "location_count:" << '\n';
		dump(*node.location_count, out, depth + 2u);
	}
	if (node.type_name) {
		out << depth_prefix << "type_name: " << *node.type_name << '\n';
	}
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_ciarray_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "ciarray-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.inline_type_info) {
		out << depth_prefix << "inline_type_info:" << '\n';
		dump(*node.inline_type_info, out, depth + 2u);
	}
	if (node.struct_type_info) {
		out << depth_prefix << "struct_type_info:" << '\n';
		dump(*node.struct_type_info, out, depth + 2u);
	}
	out << depth_prefix << "type_size:" << '\n';
	dump(node.type_size, out, depth + 2u);
	out << depth_prefix << "type_align:" << '\n';
	dump(node.type_align, out, depth + 2u);
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
}

static inline void dump_node(const node_cibuiltin_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "cibuiltin-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
}

static inline void dump_node(const node_cibuiltin_ret_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "cibuiltin-ret\n";
	const std::string depth_prefix(depth + 1u, '\t');
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
}

static inline void dump_node(const node_ci_function_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "ci-function\n";
	const std::string depth_prefix(depth + 1u, '\t');
	out << depth_prefix << "name: " << node.name << '\n';
	if (node.return_types) {
		out << depth_prefix << "return_types:\n";
		for (const auto& elem : *node.return_types) {
			dump(elem, out, depth + 2u);
		}
	}
	if (node.arguments) {
		out << depth_prefix << "arguments:\n";
		for (const auto& elem : *node.arguments) {
			dump(elem, out, depth + 2u);
		}
	}
	if (node.user_annotation) {
		out << depth_prefix << "user_annotation:" << '\n';
		dump(*node.user_annotation, out, depth + 2u);
	}
}

static inline void dump_node(const node_ciimageblock_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "ciimageblock-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
}

static inline void dump_node(const node_ciimageblock_ret_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "ciimageblock-ret\n";
	const std::string depth_prefix(depth + 1u, '\t');
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
}

static inline void dump_node(const node_cimatrix_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "cimatrix-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
}

static inline void dump_node(const node_cimatrix_ret_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "cimatrix-ret\n";
	const std::string depth_prefix(depth + 1u, '\t');
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
}

static inline void dump_node(const node_cipadding_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "cipadding-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
}

static inline void dump_node(const node_cipointer_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "cipointer-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.access_qualifier) {
		out << depth_prefix << "access_qualifier: " << access_qualifier_to_string(*node.access_qualifier) << '\n';
	}
	out << depth_prefix << "address_space: " << address_space_to_string(node.address_space) << '\n';
	if (node.inline_type_info) {
		out << depth_prefix << "inline_type_info:" << '\n';
		dump(*node.inline_type_info, out, depth + 2u);
	}
	if (node.struct_type_info) {
		out << depth_prefix << "struct_type_info:" << '\n';
		dump(*node.struct_type_info, out, depth + 2u);
	}
	out << depth_prefix << "type_size:" << '\n';
	dump(node.type_size, out, depth + 2u);
	out << depth_prefix << "type_align:" << '\n';
	dump(node.type_align, out, depth + 2u);
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
}

static inline void dump_node(const node_cipointer_ret_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "cipointer-ret\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.access_qualifier) {
		out << depth_prefix << "access_qualifier: " << access_qualifier_to_string(*node.access_qualifier) << '\n';
	}
	out << depth_prefix << "address_space: " << address_space_to_string(node.address_space) << '\n';
	if (node.inline_type_info) {
		out << depth_prefix << "inline_type_info:" << '\n';
		dump(*node.inline_type_info, out, depth + 2u);
	}
	if (node.struct_type_info) {
		out << depth_prefix << "struct_type_info:" << '\n';
		dump(*node.struct_type_info, out, depth + 2u);
	}
	out << depth_prefix << "type_size:" << '\n';
	dump(node.type_size, out, depth + 2u);
	out << depth_prefix << "type_align:" << '\n';
	dump(node.type_align, out, depth + 2u);
	out << depth_prefix << "type_name: " << node.type_name << '\n';
}

static inline void dump_node(const node_cisampler_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "cisampler-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
}

static inline void dump_node(const node_cisampler_ret_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "cisampler-ret\n";
	const std::string depth_prefix(depth + 1u, '\t');
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
}

static inline void dump_node(const node_cistruct_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "cistruct-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.struct_type_info) {
		out << depth_prefix << "struct_type_info:" << '\n';
		dump(*node.struct_type_info, out, depth + 2u);
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
}

static inline void dump_node(const node_cistruct_ret_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "cistruct-ret\n";
	const std::string depth_prefix(depth + 1u, '\t');
	out << depth_prefix << "struct_type_info:" << '\n';
	dump(node.struct_type_info, out, depth + 2u);
	out << depth_prefix << "type_name: " << node.type_name << '\n';
}

static inline void dump_node(const node_citexture_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "citexture-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	out << depth_prefix << "location_count:" << '\n';
	dump(node.location_count, out, depth + 2u);
	if (node.access_qualifier) {
		out << depth_prefix << "access_qualifier: " << access_qualifier_to_string(*node.access_qualifier) << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
}

static inline void dump_node(const node_citexture_ret_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "citexture-ret\n";
	const std::string depth_prefix(depth + 1u, '\t');
	out << depth_prefix << "location_count:" << '\n';
	dump(node.location_count, out, depth + 2u);
	if (node.access_qualifier) {
		out << depth_prefix << "access_qualifier: " << access_qualifier_to_string(*node.access_qualifier) << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
}

static inline void dump_node(const node_char_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "char-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	out << depth_prefix << "size: " << node.size << '\n';
	out << depth_prefix << "alignment: " << node.alignment << '\n';
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
}

static inline void dump_node(const node_clip_distance_attr_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "clip-distance-attr\n";
	const std::string depth_prefix(depth + 1u, '\t');
}

static inline void dump_node(const node_clip_distance_ret_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "clip-distance-ret\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	if (node.array_size) {
		out << depth_prefix << "array_size:" << '\n';
		dump(*node.array_size, out, depth + 2u);
	}
	if (node.shared) {
		out << depth_prefix << "shared: " << *node.shared << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
}

static inline void dump_node(const node_command_buffer_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "command-buffer-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	if (node.location_index) {
		out << depth_prefix << "location_index:" << '\n';
		dump(*node.location_index, out, depth + 2u);
	}
	if (node.location_count) {
		out << depth_prefix << "location_count:" << '\n';
		dump(*node.location_count, out, depth + 2u);
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_command_buffer_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "command-buffer-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
}

static inline void dump_node(const node_compute_pipeline_state_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "compute-pipeline-state-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	if (node.location_index) {
		out << depth_prefix << "location_index:" << '\n';
		dump(*node.location_index, out, depth + 2u);
	}
	if (node.location_count) {
		out << depth_prefix << "location_count:" << '\n';
		dump(*node.location_count, out, depth + 2u);
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_compute_pipeline_state_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "compute-pipeline-state-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
}

static inline void dump_node(const node_constant_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "constant-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	if (node.location_index) {
		out << depth_prefix << "location_index:" << '\n';
		dump(*node.location_index, out, depth + 2u);
	}
	if (node.location_count) {
		out << depth_prefix << "location_count:" << '\n';
		dump(*node.location_count, out, depth + 2u);
	}
	if (node.struct_type_info) {
		out << depth_prefix << "struct_type_info:" << '\n';
		dump(*node.struct_type_info, out, depth + 2u);
	}
	out << depth_prefix << "type_size:" << '\n';
	dump(node.type_size, out, depth + 2u);
	out << depth_prefix << "type_align:" << '\n';
	dump(node.type_align, out, depth + 2u);
	if (node.type_name) {
		out << depth_prefix << "type_name: " << *node.type_name << '\n';
	}
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_continue_search_ret_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "continue-search-ret\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
}

static inline void dump_node(const node_control_point_field_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "control-point-field\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	if (node.location_index) {
		out << depth_prefix << "location_index:" << '\n';
		dump(*node.location_index, out, depth + 2u);
	}
	if (node.location_count) {
		out << depth_prefix << "location_count:" << '\n';
		dump(*node.location_count, out, depth + 2u);
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_control_point_index_buffer_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "control-point-index-buffer-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_control_point_input_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "control-point-input-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	if (node.fields) {
		out << depth_prefix << "fields:\n";
		for (const auto& elem : *node.fields) {
			dump(elem, out, depth + 2u);
		}
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_curve_parameter_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "curve-parameter-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_depth2d_array_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "depth2d-array-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	out << depth_prefix << "channel_type:" << '\n';
	dump(node.channel_type, out, depth + 2u);
	if (node.access_qualifier) {
		out << depth_prefix << "access_qualifier: " << access_qualifier_to_string(*node.access_qualifier) << '\n';
	}
}

static inline void dump_node(const node_depth2d_ms_array_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "depth2d-ms-array-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	out << depth_prefix << "channel_type:" << '\n';
	dump(node.channel_type, out, depth + 2u);
	if (node.access_qualifier) {
		out << depth_prefix << "access_qualifier: " << access_qualifier_to_string(*node.access_qualifier) << '\n';
	}
}

static inline void dump_node(const node_depth2d_ms_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "depth2d-ms-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	out << depth_prefix << "channel_type:" << '\n';
	dump(node.channel_type, out, depth + 2u);
	if (node.access_qualifier) {
		out << depth_prefix << "access_qualifier: " << access_qualifier_to_string(*node.access_qualifier) << '\n';
	}
}

static inline void dump_node(const node_depth2d_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "depth2d-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	out << depth_prefix << "channel_type:" << '\n';
	dump(node.channel_type, out, depth + 2u);
	if (node.access_qualifier) {
		out << depth_prefix << "access_qualifier: " << access_qualifier_to_string(*node.access_qualifier) << '\n';
	}
}

static inline void dump_node(const node_depth_cube_array_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "depth-cube-array-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	out << depth_prefix << "channel_type:" << '\n';
	dump(node.channel_type, out, depth + 2u);
	if (node.access_qualifier) {
		out << depth_prefix << "access_qualifier: " << access_qualifier_to_string(*node.access_qualifier) << '\n';
	}
}

static inline void dump_node(const node_depth_cube_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "depth-cube-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	out << depth_prefix << "channel_type:" << '\n';
	dump(node.channel_type, out, depth + 2u);
	if (node.access_qualifier) {
		out << depth_prefix << "access_qualifier: " << access_qualifier_to_string(*node.access_qualifier) << '\n';
	}
}

static inline void dump_node(const node_depth_ret_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "depth-ret\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	if (node.depth_qualifier) {
		out << depth_prefix << "depth_qualifier: " << depth_qualifier_to_string(*node.depth_qualifier) << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
}

static inline void dump_node(const node_depth_stencil_state_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "depth-stencil-state-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	if (node.location_index) {
		out << depth_prefix << "location_index:" << '\n';
		dump(*node.location_index, out, depth + 2u);
	}
	if (node.location_count) {
		out << depth_prefix << "location_count:" << '\n';
		dump(*node.location_count, out, depth + 2u);
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_depth_stencil_state_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "depth-stencil-state-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
}

static inline void dump_node(const node_direction_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "direction-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_dispatch_quadgroups_per_threadgroup_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "dispatch-quadgroups-per-threadgroup-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_dispatch_simdgroups_per_threadgroup_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "dispatch-simdgroups-per-threadgroup-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_dispatch_threads_per_threadgroup_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "dispatch-threads-per-threadgroup-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_distance_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "distance-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_distance_ret_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "distance-ret\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
}

static inline void dump_node(const node_double_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "double-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	out << depth_prefix << "size: " << node.size << '\n';
	out << depth_prefix << "alignment: " << node.alignment << '\n';
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
}

static inline void dump_node(const node_enum_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "enum-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.underlying_type) {
		out << depth_prefix << "underlying_type:" << '\n';
		dump(*node.underlying_type, out, depth + 2u);
	}
}

static inline void dump_node(const node_extents_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "extents-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	out << depth_prefix << "index_type:" << '\n';
	dump(node.index_type, out, depth + 2u);
	out << depth_prefix << "extents:\n";
	for (const auto& elem : node.extents) {
		out << depth_prefix << "\t" << elem << '\n';
	}
}

static inline void dump_node(const node_float_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "float-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	out << depth_prefix << "size: " << node.size << '\n';
	out << depth_prefix << "alignment: " << node.alignment << '\n';
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
}

static inline void dump_node(const node_fragment_function_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "fragment-function\n";
	const std::string depth_prefix(depth + 1u, '\t');
	out << depth_prefix << "name: " << node.name << '\n';
	if (node.return_type) {
		out << depth_prefix << "return_type:\n";
		for (const auto& elem : *node.return_type) {
			dump(elem, out, depth + 2u);
		}
	}
	if (node.arguments) {
		out << depth_prefix << "arguments:\n";
		for (const auto& elem : *node.arguments) {
			dump(elem, out, depth + 2u);
		}
	}
	if (node.early_fragment_tests) {
		out << depth_prefix << "early_fragment_tests: " << *node.early_fragment_tests << '\n';
	}
	if (node.user_annotation) {
		out << depth_prefix << "user_annotation:" << '\n';
		dump(*node.user_annotation, out, depth + 2u);
	}
}

static inline void dump_node(const node_fragment_input_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "fragment-input-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "attribute_name: " << node.attribute_name << '\n';
	if (node.location) {
		out << depth_prefix << "location:" << '\n';
		dump(*node.location, out, depth + 2u);
	}
	if (node.sampling_qualifier) {
		out << depth_prefix << "sampling_qualifier: " << sampling_qualifier_to_string(*node.sampling_qualifier) << '\n';
	}
	if (node.interpolation_qualifier) {
		out << depth_prefix << "interpolation_qualifier: " << interpolation_qualifier_to_string(*node.interpolation_qualifier) << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_front_facing_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "front-facing-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_function_constant_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "function-constant\n";
	const std::string depth_prefix(depth + 1u, '\t');
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	out << depth_prefix << "name: " << node.name << '\n';
	if (node.index) {
		out << depth_prefix << "index: " << *node.index << '\n';
	}
	if (node.required) {
		out << depth_prefix << "required: " << *node.required << '\n';
	}
}

static inline void dump_node(const node_function_constant_predicate_attr_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "function-constant-predicate-attr\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.predicate) {
		out << depth_prefix << "predicate:" << '\n';
		dump(*node.predicate, out, depth + 2u);
	}
}

static inline void dump_node(const node_function_handle_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "function-handle-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	if (node.location_index) {
		out << depth_prefix << "location_index:" << '\n';
		dump(*node.location_index, out, depth + 2u);
	}
	if (node.location_count) {
		out << depth_prefix << "location_count:" << '\n';
		dump(*node.location_count, out, depth + 2u);
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_function_handle_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "function-handle-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
}

static inline void dump_node(const node_function_id_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "function-id-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_function_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "function-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	out << depth_prefix << "return_type:" << '\n';
	dump(node.return_type, out, depth + 2u);
	if (node.param_types) {
		out << depth_prefix << "param_types:\n";
		for (const auto& elem : *node.param_types) {
			dump(elem, out, depth + 2u);
		}
	}
}

static inline void dump_node(const node_geometry_id_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "geometry-id-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_geometry_intersection_function_table_offset_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "geometry-intersection-function-table-offset-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_global_binding_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "global-binding\n";
	const std::string depth_prefix(depth + 1u, '\t');
	out << depth_prefix << "name: " << node.name << '\n';
	out << depth_prefix << "argument:" << '\n';
	dump(node.argument, out, depth + 2u);
}

static inline void dump_node(const node_half_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "half-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	out << depth_prefix << "size: " << node.size << '\n';
	out << depth_prefix << "alignment: " << node.alignment << '\n';
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
}

static inline void dump_node(const node_imageblock_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "imageblock-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	if (node.data_size) {
		out << depth_prefix << "data_size:" << '\n';
		dump(*node.data_size, out, depth + 2u);
	}
	if (node.struct_type_info) {
		out << depth_prefix << "struct_type_info:" << '\n';
		dump(*node.struct_type_info, out, depth + 2u);
	}
	if (node.alias_all_render_targets) {
		out << depth_prefix << "alias_all_render_targets: " << *node.alias_all_render_targets << '\n';
	}
	if (node.alias_render_target_index) {
		out << depth_prefix << "alias_render_target_index:" << '\n';
		dump(*node.alias_render_target_index, out, depth + 2u);
	}
	out << depth_prefix << "type_align:" << '\n';
	dump(node.type_align, out, depth + 2u);
	if (node.type_name) {
		out << depth_prefix << "type_name: " << *node.type_name << '\n';
	}
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_imageblock_data_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "imageblock-data-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "data_size:" << '\n';
	dump(node.data_size, out, depth + 2u);
	if (node.struct_type_info) {
		out << depth_prefix << "struct_type_info:" << '\n';
		dump(*node.struct_type_info, out, depth + 2u);
	}
	if (node.master) {
		out << depth_prefix << "master:" << '\n';
		dump(*node.master, out, depth + 2u);
	}
	if (node.alias_all_render_targets) {
		out << depth_prefix << "alias_all_render_targets: " << *node.alias_all_render_targets << '\n';
	}
	if (node.alias_render_target_index) {
		out << depth_prefix << "alias_render_target_index:" << '\n';
		dump(*node.alias_render_target_index, out, depth + 2u);
	}
	out << depth_prefix << "type_align:" << '\n';
	dump(node.type_align, out, depth + 2u);
	if (node.type_name) {
		out << depth_prefix << "type_name: " << *node.type_name << '\n';
	}
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_imageblock_data_ret_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "imageblock-data-ret\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	if (node.data_size) {
		out << depth_prefix << "data_size:" << '\n';
		dump(*node.data_size, out, depth + 2u);
	}
	if (node.struct_type_info) {
		out << depth_prefix << "struct_type_info:" << '\n';
		dump(*node.struct_type_info, out, depth + 2u);
	}
	if (node.master) {
		out << depth_prefix << "master:" << '\n';
		dump(*node.master, out, depth + 2u);
	}
	if (node.alias_all_render_targets) {
		out << depth_prefix << "alias_all_render_targets: " << *node.alias_all_render_targets << '\n';
	}
	if (node.alias_render_target_index) {
		out << depth_prefix << "alias_render_target_index:" << '\n';
		dump(*node.alias_render_target_index, out, depth + 2u);
	}
	out << depth_prefix << "type_align:" << '\n';
	dump(node.type_align, out, depth + 2u);
	if (node.type_name) {
		out << depth_prefix << "type_name: " << *node.type_name << '\n';
	}
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
}

static inline void dump_node(const node_imageblock_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "imageblock-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	if (node.layout) {
		out << depth_prefix << "layout: " << imageblock_layout_to_string(*node.layout) << '\n';
	}
	out << depth_prefix << "data_type:" << '\n';
	dump(node.data_type, out, depth + 2u);
}

static inline void dump_node(const node_indirect_buffer_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "indirect-buffer-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	if (node.buffer_size) {
		out << depth_prefix << "buffer_size:" << '\n';
		dump(*node.buffer_size, out, depth + 2u);
	}
	if (node.location_index) {
		out << depth_prefix << "location_index:" << '\n';
		dump(*node.location_index, out, depth + 2u);
	}
	if (node.location_count) {
		out << depth_prefix << "location_count:" << '\n';
		dump(*node.location_count, out, depth + 2u);
	}
	if (node.access_qualifier) {
		out << depth_prefix << "access_qualifier: " << access_qualifier_to_string(*node.access_qualifier) << '\n';
	}
	out << depth_prefix << "address_space: " << address_space_to_string(node.address_space) << '\n';
	if (node.struct_type_info) {
		out << depth_prefix << "struct_type_info:" << '\n';
		dump(*node.struct_type_info, out, depth + 2u);
	}
	if (node.raster_order_group) {
		out << depth_prefix << "raster_order_group:" << '\n';
		dump(*node.raster_order_group, out, depth + 2u);
	}
	if (node.type_size) {
		out << depth_prefix << "type_size:" << '\n';
		dump(*node.type_size, out, depth + 2u);
	}
	if (node.type_align) {
		out << depth_prefix << "type_align:" << '\n';
		dump(*node.type_align, out, depth + 2u);
	}
	if (node.type_name) {
		out << depth_prefix << "type_name: " << *node.type_name << '\n';
	}
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
	if (node.inline_type_info) {
		out << depth_prefix << "inline_type_info:" << '\n';
		dump(*node.inline_type_info, out, depth + 2u);
	}
}

static inline void dump_node(const node_indirect_constant_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "indirect-constant-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	if (node.location_index) {
		out << depth_prefix << "location_index:" << '\n';
		dump(*node.location_index, out, depth + 2u);
	}
	if (node.location_count) {
		out << depth_prefix << "location_count:" << '\n';
		dump(*node.location_count, out, depth + 2u);
	}
	if (node.type_name) {
		out << depth_prefix << "type_name: " << *node.type_name << '\n';
	}
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_inline_type_info_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "inline-type-info\n";
	const std::string depth_prefix(depth + 1u, '\t');
	out << depth_prefix << "address_space: " << address_space_to_string(node.address_space) << '\n';
	if (node.inline_type_info) {
		out << depth_prefix << "inline_type_info:" << '\n';
		dump(*node.inline_type_info, out, depth + 2u);
	}
	if (node.struct_type_info) {
		out << depth_prefix << "struct_type_info:" << '\n';
		dump(*node.struct_type_info, out, depth + 2u);
	}
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.array_entries) {
		out << depth_prefix << "array_entries: " << *node.array_entries << '\n';
	}
	if (node.type_name) {
		out << depth_prefix << "type_name: " << *node.type_name << '\n';
	}
	if (node.indirect_argument) {
		out << depth_prefix << "indirect_argument:" << '\n';
		dump(*node.indirect_argument, out, depth + 2u);
	}
	if (node.indirect_location) {
		out << depth_prefix << "indirect_location:" << '\n';
		dump(*node.indirect_location, out, depth + 2u);
	}
}

static inline void dump_node(const node_instance_acceleration_structure_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "instance-acceleration-structure-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	if (node.location_index) {
		out << depth_prefix << "location_index:" << '\n';
		dump(*node.location_index, out, depth + 2u);
	}
	if (node.location_count) {
		out << depth_prefix << "location_count:" << '\n';
		dump(*node.location_count, out, depth + 2u);
	}
	if (node.access_qualifier) {
		out << depth_prefix << "access_qualifier: " << access_qualifier_to_string(*node.access_qualifier) << '\n';
	}
	if (node.raster_order_group) {
		out << depth_prefix << "raster_order_group:" << '\n';
		dump(*node.raster_order_group, out, depth + 2u);
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_instance_id_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "instance-id-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_instance_id_count_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "instance-id-count-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_instance_intersection_function_table_offset_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "instance-intersection-function-table-offset-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_int_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "int-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	out << depth_prefix << "size: " << node.size << '\n';
	out << depth_prefix << "alignment: " << node.alignment << '\n';
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
}

static inline void dump_node(const node_interpolant_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "interpolant-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	if (node.perspective) {
		out << depth_prefix << "perspective: " << *node.perspective << '\n';
	}
	out << depth_prefix << "value_type:" << '\n';
	dump(node.value_type, out, depth + 2u);
}

static inline void dump_node(const node_intersection_function_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "intersection-function\n";
	const std::string depth_prefix(depth + 1u, '\t');
	out << depth_prefix << "name: " << node.name << '\n';
	if (node.return_types) {
		out << depth_prefix << "return_types:\n";
		for (const auto& elem : *node.return_types) {
			dump(elem, out, depth + 2u);
		}
	}
	if (node.arguments) {
		out << depth_prefix << "arguments:\n";
		for (const auto& elem : *node.arguments) {
			dump(elem, out, depth + 2u);
		}
	}
	if (node.primitive_kind) {
		out << depth_prefix << "primitive_kind: " << primitive_kind_to_string(*node.primitive_kind) << '\n';
	}
	if (node.instancing) {
		out << depth_prefix << "instancing: " << *node.instancing << '\n';
	}
	if (node.triangle_data) {
		out << depth_prefix << "triangle_data: " << *node.triangle_data << '\n';
	}
	if (node.world_space_data) {
		out << depth_prefix << "world_space_data: " << *node.world_space_data << '\n';
	}
	if (node.primitive_motion) {
		out << depth_prefix << "primitive_motion: " << *node.primitive_motion << '\n';
	}
	if (node.instance_motion) {
		out << depth_prefix << "instance_motion: " << *node.instance_motion << '\n';
	}
	if (node.extended_limits) {
		out << depth_prefix << "extended_limits: " << *node.extended_limits << '\n';
	}
	if (node.curve_data) {
		out << depth_prefix << "curve_data: " << *node.curve_data << '\n';
	}
	if (node.multi_level_instancing) {
		out << depth_prefix << "multi_level_instancing: " << *node.multi_level_instancing << '\n';
	}
	if (node.intersection_function_buffer) {
		out << depth_prefix << "intersection_function_buffer: " << *node.intersection_function_buffer << '\n';
	}
	if (node.user_data) {
		out << depth_prefix << "user_data: " << *node.user_data << '\n';
	}
	if (node.user_annotation) {
		out << depth_prefix << "user_annotation:" << '\n';
		dump(*node.user_annotation, out, depth + 2u);
	}
}

static inline void dump_node(const node_intersection_function_handle_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "intersection-function-handle-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	if (node.intersection_function_buffer) {
		out << depth_prefix << "intersection_function_buffer: " << *node.intersection_function_buffer << '\n';
	}
	if (node.instancing) {
		out << depth_prefix << "instancing: " << *node.instancing << '\n';
	}
	if (node.multi_level_instancing) {
		out << depth_prefix << "multi_level_instancing: " << *node.multi_level_instancing << '\n';
	}
	if (node.triangle_data) {
		out << depth_prefix << "triangle_data: " << *node.triangle_data << '\n';
	}
	if (node.curve_data) {
		out << depth_prefix << "curve_data: " << *node.curve_data << '\n';
	}
	if (node.world_space_data) {
		out << depth_prefix << "world_space_data: " << *node.world_space_data << '\n';
	}
	if (node.user_data) {
		out << depth_prefix << "user_data: " << *node.user_data << '\n';
	}
	if (node.primitive_motion) {
		out << depth_prefix << "primitive_motion: " << *node.primitive_motion << '\n';
	}
	if (node.instance_motion) {
		out << depth_prefix << "instance_motion: " << *node.instance_motion << '\n';
	}
	if (node.extended_limits) {
		out << depth_prefix << "extended_limits: " << *node.extended_limits << '\n';
	}
}

static inline void dump_node(const node_intersection_function_table_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "intersection-function-table-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	if (node.location_index) {
		out << depth_prefix << "location_index:" << '\n';
		dump(*node.location_index, out, depth + 2u);
	}
	if (node.location_count) {
		out << depth_prefix << "location_count:" << '\n';
		dump(*node.location_count, out, depth + 2u);
	}
	if (node.access_qualifier) {
		out << depth_prefix << "access_qualifier: " << access_qualifier_to_string(*node.access_qualifier) << '\n';
	}
	if (node.raster_order_group) {
		out << depth_prefix << "raster_order_group:" << '\n';
		dump(*node.raster_order_group, out, depth + 2u);
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_intersection_function_table_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "intersection-function-table-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	if (node.instancing) {
		out << depth_prefix << "instancing: " << *node.instancing << '\n';
	}
	if (node.triangle_data) {
		out << depth_prefix << "triangle_data: " << *node.triangle_data << '\n';
	}
	if (node.world_space_data) {
		out << depth_prefix << "world_space_data: " << *node.world_space_data << '\n';
	}
	if (node.primitive_motion) {
		out << depth_prefix << "primitive_motion: " << *node.primitive_motion << '\n';
	}
	if (node.instance_motion) {
		out << depth_prefix << "instance_motion: " << *node.instance_motion << '\n';
	}
	if (node.extended_limits) {
		out << depth_prefix << "extended_limits: " << *node.extended_limits << '\n';
	}
	if (node.curve_data) {
		out << depth_prefix << "curve_data: " << *node.curve_data << '\n';
	}
	if (node.multi_level_instancing) {
		out << depth_prefix << "multi_level_instancing: " << *node.multi_level_instancing << '\n';
	}
}

static inline void dump_node(const node_invariant_attr_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "invariant-attr\n";
	const std::string depth_prefix(depth + 1u, '\t');
}

static inline void dump_node(const node_kernel_function_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "kernel-function\n";
	const std::string depth_prefix(depth + 1u, '\t');
	out << depth_prefix << "name: " << node.name << '\n';
	if (node.return_types) {
		out << depth_prefix << "return_types:\n";
		for (const auto& elem : *node.return_types) {
			dump(elem, out, depth + 2u);
		}
	}
	if (node.arguments) {
		out << depth_prefix << "arguments:\n";
		for (const auto& elem : *node.arguments) {
			dump(elem, out, depth + 2u);
		}
	}
	if (node.vec_type_hint) {
		out << depth_prefix << "vec_type_hint:" << '\n';
		dump(*node.vec_type_hint, out, depth + 2u);
	}
	if (node.workgroup_size) {
		out << depth_prefix << "workgroup_size:" << '\n';
		dump(*node.workgroup_size, out, depth + 2u);
	}
	if (node.workgroup_size_hint) {
		out << depth_prefix << "workgroup_size_hint:" << '\n';
		dump(*node.workgroup_size_hint, out, depth + 2u);
	}
	if (node.workgroup_max_size) {
		out << depth_prefix << "workgroup_max_size:" << '\n';
		dump(*node.workgroup_max_size, out, depth + 2u);
	}
	if (node.user_annotation) {
		out << depth_prefix << "user_annotation:" << '\n';
		dump(*node.user_annotation, out, depth + 2u);
	}
}

static inline void dump_node(const node_key_frame_count_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "key-frame-count-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_llong_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "llong-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	out << depth_prefix << "size: " << node.size << '\n';
	out << depth_prefix << "alignment: " << node.alignment << '\n';
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
}

static inline void dump_node(const node_lvalue_reference_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "lvalue-reference-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	out << depth_prefix << "pointee_type:" << '\n';
	dump(node.pointee_type, out, depth + 2u);
}

static inline void dump_node(const node_location_index_attr_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "location-index-attr\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.index) {
		out << depth_prefix << "index:" << '\n';
		dump(*node.index, out, depth + 2u);
	}
	if (node.count) {
		out << depth_prefix << "count:" << '\n';
		dump(*node.count, out, depth + 2u);
	}
}

static inline void dump_node(const node_long_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "long-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	out << depth_prefix << "size: " << node.size << '\n';
	out << depth_prefix << "alignment: " << node.alignment << '\n';
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
}

static inline void dump_node(const node_matrix_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "matrix-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	out << depth_prefix << "element_type:" << '\n';
	dump(node.element_type, out, depth + 2u);
	if (node.num_columns) {
		out << depth_prefix << "num_columns: " << *node.num_columns << '\n';
	}
	if (node.num_rows) {
		out << depth_prefix << "num_rows: " << *node.num_rows << '\n';
	}
}

static inline void dump_node(const node_max_distance_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "max-distance-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_max_mesh_workgroups_fn_attr_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "max-mesh-workgroups-fn-attr\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.workgroups) {
		out << depth_prefix << "workgroups:" << '\n';
		dump(*node.workgroups, out, depth + 2u);
	}
}

static inline void dump_node(const node_mesh_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "mesh-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	if (node.mesh_type_info) {
		out << depth_prefix << "mesh_type_info:" << '\n';
		dump(*node.mesh_type_info, out, depth + 2u);
	}
	if (node.type_name) {
		out << depth_prefix << "type_name: " << *node.type_name << '\n';
	}
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_mesh_emulation_block_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "mesh-emulation-block\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.value_groups) {
		out << depth_prefix << "value_groups:\n";
		for (const auto& elem : *node.value_groups) {
			dump(elem, out, depth + 2u);
		}
	}
}

static inline void dump_node(const node_mesh_emulation_fragment_analysis_result_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "mesh-emulation-fragment-analysis-result\n";
	const std::string depth_prefix(depth + 1u, '\t');
	out << depth_prefix << "function: " << node.function << '\n';
	if (node.used_inputs) {
		out << depth_prefix << "used_inputs:\n";
		for (const auto& elem : *node.used_inputs) {
			out << depth_prefix << "\t" << elem << '\n';
		}
	}
}

static inline void dump_node(const node_mesh_emulation_mesh_kernel_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "mesh-emulation-mesh-kernel\n";
	const std::string depth_prefix(depth + 1u, '\t');
	out << depth_prefix << "function: " << node.function << '\n';
	if (node.emulation_buffer_index) {
		out << depth_prefix << "emulation_buffer_index: " << *node.emulation_buffer_index << '\n';
	}
	if (node.layout) {
		out << depth_prefix << "layout:" << '\n';
		dump(*node.layout, out, depth + 2u);
	}
}

static inline void dump_node(const node_mesh_emulation_mesh_layout_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "mesh-emulation-mesh-layout\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.max_vertices) {
		out << depth_prefix << "max_vertices: " << *node.max_vertices << '\n';
	}
	if (node.max_primitives) {
		out << depth_prefix << "max_primitives: " << *node.max_primitives << '\n';
	}
	if (node.max_indices) {
		out << depth_prefix << "max_indices: " << *node.max_indices << '\n';
	}
	if (node.max_indices_padding) {
		out << depth_prefix << "max_indices_padding: " << *node.max_indices_padding << '\n';
	}
	out << depth_prefix << "vertices_primitives_block:" << '\n';
	dump(node.vertices_primitives_block, out, depth + 2u);
	if (node.primitive_culled_block) {
		out << depth_prefix << "primitive_culled_block:" << '\n';
		dump(*node.primitive_culled_block, out, depth + 2u);
	}
}

static inline void dump_node(const node_mesh_emulation_mesh_vertex_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "mesh-emulation-mesh-vertex\n";
	const std::string depth_prefix(depth + 1u, '\t');
	out << depth_prefix << "function: " << node.function << '\n';
	if (node.emulation_buffer_index) {
		out << depth_prefix << "emulation_buffer_index: " << *node.emulation_buffer_index << '\n';
	}
	if (node.layout) {
		out << depth_prefix << "layout:" << '\n';
		dump(*node.layout, out, depth + 2u);
	}
}

static inline void dump_node(const node_mesh_emulation_object_kernel_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "mesh-emulation-object-kernel\n";
	const std::string depth_prefix(depth + 1u, '\t');
	out << depth_prefix << "function: " << node.function << '\n';
	if (node.emulation_buffer_index) {
		out << depth_prefix << "emulation_buffer_index: " << *node.emulation_buffer_index << '\n';
	}
	if (node.max_mesh_workgroups) {
		out << depth_prefix << "max_mesh_workgroups:" << '\n';
		dump(*node.max_mesh_workgroups, out, depth + 2u);
	}
}

static inline void dump_node(const node_mesh_emulation_value_group_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "mesh-emulation-value-group\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.value_alignment) {
		out << depth_prefix << "value_alignment: " << *node.value_alignment << '\n';
	}
	if (node.value_size) {
		out << depth_prefix << "value_size: " << *node.value_size << '\n';
	}
	if (node.max_value_count) {
		out << depth_prefix << "max_value_count: " << *node.max_value_count << '\n';
	}
	if (node.member_type) {
		out << depth_prefix << "member_type:" << '\n';
		dump(*node.member_type, out, depth + 2u);
	}
	if (node.member_index) {
		out << depth_prefix << "member_index: " << *node.member_index << '\n';
	}
}

static inline void dump_node(const node_mesh_function_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "mesh-function\n";
	const std::string depth_prefix(depth + 1u, '\t');
	out << depth_prefix << "name: " << node.name << '\n';
	if (node.return_types) {
		out << depth_prefix << "return_types:\n";
		for (const auto& elem : *node.return_types) {
			dump(elem, out, depth + 2u);
		}
	}
	if (node.arguments) {
		out << depth_prefix << "arguments:\n";
		for (const auto& elem : *node.arguments) {
			dump(elem, out, depth + 2u);
		}
	}
	if (node.workgroup_max_size) {
		out << depth_prefix << "workgroup_max_size:" << '\n';
		dump(*node.workgroup_max_size, out, depth + 2u);
	}
	if (node.user_annotation) {
		out << depth_prefix << "user_annotation:" << '\n';
		dump(*node.user_annotation, out, depth + 2u);
	}
	if (node.workgroup_size) {
		out << depth_prefix << "workgroup_size:" << '\n';
		dump(*node.workgroup_size, out, depth + 2u);
	}
}

static inline void dump_node(const node_mesh_grid_properties_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "mesh-grid-properties-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_mesh_grid_properties_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "mesh-grid-properties-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
}

static inline void dump_node(const node_mesh_primitive_data_ret_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "mesh-primitive-data-ret\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	if (node.id) {
		out << depth_prefix << "id: " << *node.id << '\n';
	}
	out << depth_prefix << "attribute_name: " << node.attribute_name << '\n';
	if (node.shared) {
		out << depth_prefix << "shared: " << *node.shared << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
}

static inline void dump_node(const node_mesh_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "mesh-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	out << depth_prefix << "vertex_type:" << '\n';
	dump(node.vertex_type, out, depth + 2u);
	out << depth_prefix << "primitive_type:" << '\n';
	dump(node.primitive_type, out, depth + 2u);
	if (node.max_vertices) {
		out << depth_prefix << "max_vertices: " << *node.max_vertices << '\n';
	}
	if (node.max_primitives) {
		out << depth_prefix << "max_primitives: " << *node.max_primitives << '\n';
	}
	if (node.topology) {
		out << depth_prefix << "topology: " << topology_to_string(*node.topology) << '\n';
	}
}

static inline void dump_node(const node_mesh_type_info_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "mesh-type-info\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.vertex_types) {
		out << depth_prefix << "vertex_types:\n";
		for (const auto& elem : *node.vertex_types) {
			dump(elem, out, depth + 2u);
		}
	}
	if (node.primitive_types) {
		out << depth_prefix << "primitive_types:\n";
		for (const auto& elem : *node.primitive_types) {
			dump(elem, out, depth + 2u);
		}
	}
	if (node.max_vertices) {
		out << depth_prefix << "max_vertices: " << *node.max_vertices << '\n';
	}
	if (node.max_primitives) {
		out << depth_prefix << "max_primitives: " << *node.max_primitives << '\n';
	}
	out << depth_prefix << "topology: " << topology_to_string(node.topology) << '\n';
}

static inline void dump_node(const node_mesh_vertex_data_ret_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "mesh-vertex-data-ret\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	if (node.id) {
		out << depth_prefix << "id: " << *node.id << '\n';
	}
	out << depth_prefix << "attribute_name: " << node.attribute_name << '\n';
	if (node.shared) {
		out << depth_prefix << "shared: " << *node.shared << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
}

static inline void dump_node(const node_min_distance_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "min-distance-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_motion_end_time_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "motion-end-time-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_motion_start_time_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "motion-start-time-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_object_function_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "object-function\n";
	const std::string depth_prefix(depth + 1u, '\t');
	out << depth_prefix << "name: " << node.name << '\n';
	if (node.return_types) {
		out << depth_prefix << "return_types:\n";
		for (const auto& elem : *node.return_types) {
			dump(elem, out, depth + 2u);
		}
	}
	if (node.arguments) {
		out << depth_prefix << "arguments:\n";
		for (const auto& elem : *node.arguments) {
			dump(elem, out, depth + 2u);
		}
	}
	if (node.workgroup_max_size) {
		out << depth_prefix << "workgroup_max_size:" << '\n';
		dump(*node.workgroup_max_size, out, depth + 2u);
	}
	if (node.max_mesh_workgroups) {
		out << depth_prefix << "max_mesh_workgroups:" << '\n';
		dump(*node.max_mesh_workgroups, out, depth + 2u);
	}
	if (node.user_annotation) {
		out << depth_prefix << "user_annotation:" << '\n';
		dump(*node.user_annotation, out, depth + 2u);
	}
	if (node.workgroup_size) {
		out << depth_prefix << "workgroup_size:" << '\n';
		dump(*node.workgroup_size, out, depth + 2u);
	}
}

static inline void dump_node(const node_object_to_world_transform_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "object-to-world-transform-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_opaque_primitive_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "opaque-primitive-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_opaque_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "opaque-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	out << depth_prefix << "name: " << node.name << '\n';
}

static inline void dump_node(const node_origin_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "origin-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_packed_vector_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "packed-vector-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	out << depth_prefix << "element_type:" << '\n';
	dump(node.element_type, out, depth + 2u);
	if (node.num_elements) {
		out << depth_prefix << "num_elements: " << *node.num_elements << '\n';
	}
}

static inline void dump_node(const node_patch_control_point_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "patch-control-point-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	out << depth_prefix << "control_point_type:" << '\n';
	dump(node.control_point_type, out, depth + 2u);
}

static inline void dump_node(const node_patch_fn_attr_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "patch-fn-attr\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.kind) {
		out << depth_prefix << "kind: " << patch_kind_to_string(*node.kind) << '\n';
	}
	if (node.control_points) {
		out << depth_prefix << "control_points:" << '\n';
		dump(*node.control_points, out, depth + 2u);
	}
}

static inline void dump_node(const node_patch_id_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "patch-id-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_patch_input_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "patch-input-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	if (node.location_index) {
		out << depth_prefix << "location_index:" << '\n';
		dump(*node.location_index, out, depth + 2u);
	}
	if (node.location_count) {
		out << depth_prefix << "location_count:" << '\n';
		dump(*node.location_count, out, depth + 2u);
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_payload_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "payload-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	if (node.struct_type_info) {
		out << depth_prefix << "struct_type_info:" << '\n';
		dump(*node.struct_type_info, out, depth + 2u);
	}
	out << depth_prefix << "type_size:" << '\n';
	dump(node.type_size, out, depth + 2u);
	out << depth_prefix << "type_align:" << '\n';
	dump(node.type_align, out, depth + 2u);
	if (node.type_name) {
		out << depth_prefix << "type_name: " << *node.type_name << '\n';
	}
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
	if (node.inline_type_info) {
		out << depth_prefix << "inline_type_info:" << '\n';
		dump(*node.inline_type_info, out, depth + 2u);
	}
}

static inline void dump_node(const node_pixel_position_in_tile_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "pixel-position-in-tile-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_pixels_per_tile_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "pixels-per-tile-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_point_coord_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "point-coord-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_point_size_attr_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "point-size-attr\n";
	const std::string depth_prefix(depth + 1u, '\t');
}

static inline void dump_node(const node_point_size_ret_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "point-size-ret\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	if (node.shared) {
		out << depth_prefix << "shared: " << *node.shared << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
}

static inline void dump_node(const node_pointer_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "pointer-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	out << depth_prefix << "pointee_type:" << '\n';
	dump(node.pointee_type, out, depth + 2u);
}

static inline void dump_node(const node_position_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "position-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "sampling_qualifier: " << sampling_qualifier_to_string(node.sampling_qualifier) << '\n';
	out << depth_prefix << "interpolation_qualifier: " << interpolation_qualifier_to_string(node.interpolation_qualifier) << '\n';
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_position_attr_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "position-attr\n";
	const std::string depth_prefix(depth + 1u, '\t');
}

static inline void dump_node(const node_position_in_patch_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "position-in-patch-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_position_ret_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "position-ret\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	if (node.invariant) {
		out << depth_prefix << "invariant: " << *node.invariant << '\n';
	}
	if (node.shared) {
		out << depth_prefix << "shared: " << *node.shared << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
}

static inline void dump_node(const node_primitive_acceleration_structure_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "primitive-acceleration-structure-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	if (node.location_index) {
		out << depth_prefix << "location_index:" << '\n';
		dump(*node.location_index, out, depth + 2u);
	}
	if (node.location_count) {
		out << depth_prefix << "location_count:" << '\n';
		dump(*node.location_count, out, depth + 2u);
	}
	if (node.access_qualifier) {
		out << depth_prefix << "access_qualifier: " << access_qualifier_to_string(*node.access_qualifier) << '\n';
	}
	if (node.raster_order_group) {
		out << depth_prefix << "raster_order_group:" << '\n';
		dump(*node.raster_order_group, out, depth + 2u);
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_primitive_culled_attr_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "primitive-culled-attr\n";
	const std::string depth_prefix(depth + 1u, '\t');
}

static inline void dump_node(const node_primitive_culled_ret_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "primitive-culled-ret\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	if (node.shared) {
		out << depth_prefix << "shared: " << *node.shared << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
}

static inline void dump_node(const node_primitive_data_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "primitive-data-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_primitive_id_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "primitive-id-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_primitive_id_attr_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "primitive-id-attr\n";
	const std::string depth_prefix(depth + 1u, '\t');
}

static inline void dump_node(const node_primitive_id_ret_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "primitive-id-ret\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	if (node.shared) {
		out << depth_prefix << "shared: " << *node.shared << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
}

static inline void dump_node(const node_quadgroup_index_in_threadgroup_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "quadgroup-index-in-threadgroup-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_quadgroups_per_threadgroup_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "quadgroups-per-threadgroup-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_r16snorm_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "r16snorm-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	out << depth_prefix << "alu_type:" << '\n';
	dump(node.alu_type, out, depth + 2u);
}

static inline void dump_node(const node_r16unorm_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "r16unorm-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	out << depth_prefix << "alu_type:" << '\n';
	dump(node.alu_type, out, depth + 2u);
}

static inline void dump_node(const node_r8snorm_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "r8snorm-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	out << depth_prefix << "alu_type:" << '\n';
	dump(node.alu_type, out, depth + 2u);
}

static inline void dump_node(const node_r8unorm_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "r8unorm-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	out << depth_prefix << "alu_type:" << '\n';
	dump(node.alu_type, out, depth + 2u);
}

static inline void dump_node(const node_rg11b10f_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "rg11b10f-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	out << depth_prefix << "alu_type:" << '\n';
	dump(node.alu_type, out, depth + 2u);
}

static inline void dump_node(const node_rg16snorm_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "rg16snorm-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	out << depth_prefix << "alu_type:" << '\n';
	dump(node.alu_type, out, depth + 2u);
}

static inline void dump_node(const node_rg16unorm_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "rg16unorm-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	out << depth_prefix << "alu_type:" << '\n';
	dump(node.alu_type, out, depth + 2u);
}

static inline void dump_node(const node_rg8snorm_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "rg8snorm-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	out << depth_prefix << "alu_type:" << '\n';
	dump(node.alu_type, out, depth + 2u);
}

static inline void dump_node(const node_rg8unorm_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "rg8unorm-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	out << depth_prefix << "alu_type:" << '\n';
	dump(node.alu_type, out, depth + 2u);
}

static inline void dump_node(const node_rgb10a2_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "rgb10a2-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	out << depth_prefix << "alu_type:" << '\n';
	dump(node.alu_type, out, depth + 2u);
}

static inline void dump_node(const node_rgb9e5_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "rgb9e5-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	out << depth_prefix << "alu_type:" << '\n';
	dump(node.alu_type, out, depth + 2u);
}

static inline void dump_node(const node_rgba16snorm_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "rgba16snorm-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	out << depth_prefix << "alu_type:" << '\n';
	dump(node.alu_type, out, depth + 2u);
}

static inline void dump_node(const node_rgba16unorm_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "rgba16unorm-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	out << depth_prefix << "alu_type:" << '\n';
	dump(node.alu_type, out, depth + 2u);
}

static inline void dump_node(const node_rgba8snorm_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "rgba8snorm-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	out << depth_prefix << "alu_type:" << '\n';
	dump(node.alu_type, out, depth + 2u);
}

static inline void dump_node(const node_rgba8unorm_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "rgba8unorm-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	out << depth_prefix << "alu_type:" << '\n';
	dump(node.alu_type, out, depth + 2u);
}

static inline void dump_node(const node_rvalue_reference_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "rvalue-reference-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	out << depth_prefix << "pointee_type:" << '\n';
	dump(node.pointee_type, out, depth + 2u);
}

static inline void dump_node(const node_record_base_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "record-base\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.offset) {
		out << depth_prefix << "offset: " << *node.offset << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	out << depth_prefix << "type:" << '\n';
	dump(node.type, out, depth + 2u);
}

static inline void dump_node(const node_record_field_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "record-field\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.offset) {
		out << depth_prefix << "offset: " << *node.offset << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	out << depth_prefix << "type:" << '\n';
	dump(node.type, out, depth + 2u);
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.attributes) {
		out << depth_prefix << "attributes:\n";
		for (const auto& elem : *node.attributes) {
			dump(elem, out, depth + 2u);
		}
	}
	if (node.bitfield) {
		out << depth_prefix << "bitfield:" << '\n';
		dump(*node.bitfield, out, depth + 2u);
	}
}

static inline void dump_node(const node_render_pipeline_state_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "render-pipeline-state-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	if (node.location_index) {
		out << depth_prefix << "location_index:" << '\n';
		dump(*node.location_index, out, depth + 2u);
	}
	if (node.location_count) {
		out << depth_prefix << "location_count:" << '\n';
		dump(*node.location_count, out, depth + 2u);
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_render_pipeline_state_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "render-pipeline-state-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
}

static inline void dump_node(const node_render_target_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "render-target-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	if (node.render_target_index) {
		out << depth_prefix << "render_target_index:" << '\n';
		dump(*node.render_target_index, out, depth + 2u);
	}
	if (node.raster_order_group) {
		out << depth_prefix << "raster_order_group:" << '\n';
		dump(*node.raster_order_group, out, depth + 2u);
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_render_target_array_index_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "render-target-array-index-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_render_target_array_index_attr_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "render-target-array-index-attr\n";
	const std::string depth_prefix(depth + 1u, '\t');
}

static inline void dump_node(const node_render_target_array_index_ret_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "render-target-array-index-ret\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	if (node.shared) {
		out << depth_prefix << "shared: " << *node.shared << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
}

static inline void dump_node(const node_render_target_attr_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "render-target-attr\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.index) {
		out << depth_prefix << "index:" << '\n';
		dump(*node.index, out, depth + 2u);
	}
}

static inline void dump_node(const node_render_target_ret_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "render-target-ret\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	if (node.render_target_index) {
		out << depth_prefix << "render_target_index:" << '\n';
		dump(*node.render_target_index, out, depth + 2u);
	}
	if (node.blend_source_index) {
		out << depth_prefix << "blend_source_index:" << '\n';
		dump(*node.blend_source_index, out, depth + 2u);
	}
	if (node.raster_order_group) {
		out << depth_prefix << "raster_order_group:" << '\n';
		dump(*node.raster_order_group, out, depth + 2u);
	}
	if (node.rounding_mode) {
		out << depth_prefix << "rounding_mode: " << rounding_mode_to_string(*node.rounding_mode) << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
}

static inline void dump_node(const node_srgba8unorm_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "srgba8unorm-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	out << depth_prefix << "alu_type:" << '\n';
	dump(node.alu_type, out, depth + 2u);
}

static inline void dump_node(const node_sample_id_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "sample-id-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_sample_mask_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "sample-mask-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	if (node.post_depth_coverage) {
		out << depth_prefix << "post_depth_coverage: " << *node.post_depth_coverage << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_sample_mask_ret_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "sample-mask-ret\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
}

static inline void dump_node(const node_sampler_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "sampler-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	if (node.location_index) {
		out << depth_prefix << "location_index:" << '\n';
		dump(*node.location_index, out, depth + 2u);
	}
	if (node.location_count) {
		out << depth_prefix << "location_count:" << '\n';
		dump(*node.location_count, out, depth + 2u);
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_sampler_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "sampler-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
}

static inline void dump_node(const node_shared_attr_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "shared-attr\n";
	const std::string depth_prefix(depth + 1u, '\t');
}

static inline void dump_node(const node_short_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "short-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	out << depth_prefix << "size: " << node.size << '\n';
	out << depth_prefix << "alignment: " << node.alignment << '\n';
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
}

static inline void dump_node(const node_simdgroup_index_in_threadgroup_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "simdgroup-index-in-threadgroup-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_simdgroups_per_threadgroup_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "simdgroups-per-threadgroup-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_stage_in_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "stage-in-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	if (node.location_index) {
		out << depth_prefix << "location_index:" << '\n';
		dump(*node.location_index, out, depth + 2u);
	}
	if (node.location_count) {
		out << depth_prefix << "location_count:" << '\n';
		dump(*node.location_count, out, depth + 2u);
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_stage_in_grid_origin_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "stage-in-grid-origin-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_stage_in_grid_size_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "stage-in-grid-size-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_stencil_ret_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "stencil-ret\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
}

static inline void dump_node(const node_stitching_argument_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "stitching-argument\n";
	const std::string depth_prefix(depth + 1u, '\t');
	out << depth_prefix << "type:" << '\n';
	dump(node.type, out, depth + 2u);
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
}

static inline void dump_node(const node_struct_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "struct-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.members) {
		out << depth_prefix << "members:\n";
		for (const auto& elem : *node.members) {
			dump(elem, out, depth + 2u);
		}
	}
}

static inline void dump_node(const node_struct_type_info_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "struct-type-info\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.fields) {
		out << depth_prefix << "fields:\n";
		for (const auto& elem : *node.fields) {
			dump(elem, out, depth + 2u);
			out << depth_prefix << "\t---\n";
		}
	}
}

static inline void dump_node(const node_tensor_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "tensor-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	if (node.location_index) {
		out << depth_prefix << "location_index:" << '\n';
		dump(*node.location_index, out, depth + 2u);
	}
	if (node.location_count) {
		out << depth_prefix << "location_count:" << '\n';
		dump(*node.location_count, out, depth + 2u);
	}
	if (node.access_qualifier) {
		out << depth_prefix << "access_qualifier: " << access_qualifier_to_string(*node.access_qualifier) << '\n';
	}
	if (node.raster_order_group) {
		out << depth_prefix << "raster_order_group:" << '\n';
		dump(*node.raster_order_group, out, depth + 2u);
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_tensor_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "tensor-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	out << depth_prefix << "element_type:" << '\n';
	dump(node.element_type, out, depth + 2u);
	out << depth_prefix << "extents_type:" << '\n';
	dump(node.extents_type, out, depth + 2u);
	if (node.kind) {
		out << depth_prefix << "kind: " << tensor_kind_to_string(*node.kind) << '\n';
	}
}

static inline void dump_node(const node_texture1d_array_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "texture1d-array-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	out << depth_prefix << "channel_type:" << '\n';
	dump(node.channel_type, out, depth + 2u);
	if (node.access_qualifier) {
		out << depth_prefix << "access_qualifier: " << access_qualifier_to_string(*node.access_qualifier) << '\n';
	}
}

static inline void dump_node(const node_texture1d_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "texture1d-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	out << depth_prefix << "channel_type:" << '\n';
	dump(node.channel_type, out, depth + 2u);
	if (node.access_qualifier) {
		out << depth_prefix << "access_qualifier: " << access_qualifier_to_string(*node.access_qualifier) << '\n';
	}
}

static inline void dump_node(const node_texture2d_array_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "texture2d-array-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	out << depth_prefix << "channel_type:" << '\n';
	dump(node.channel_type, out, depth + 2u);
	if (node.access_qualifier) {
		out << depth_prefix << "access_qualifier: " << access_qualifier_to_string(*node.access_qualifier) << '\n';
	}
}

static inline void dump_node(const node_texture2d_ms_array_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "texture2d-ms-array-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	out << depth_prefix << "channel_type:" << '\n';
	dump(node.channel_type, out, depth + 2u);
	if (node.access_qualifier) {
		out << depth_prefix << "access_qualifier: " << access_qualifier_to_string(*node.access_qualifier) << '\n';
	}
}

static inline void dump_node(const node_texture2d_ms_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "texture2d-ms-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	out << depth_prefix << "channel_type:" << '\n';
	dump(node.channel_type, out, depth + 2u);
	if (node.access_qualifier) {
		out << depth_prefix << "access_qualifier: " << access_qualifier_to_string(*node.access_qualifier) << '\n';
	}
}

static inline void dump_node(const node_texture2d_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "texture2d-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	out << depth_prefix << "channel_type:" << '\n';
	dump(node.channel_type, out, depth + 2u);
	if (node.access_qualifier) {
		out << depth_prefix << "access_qualifier: " << access_qualifier_to_string(*node.access_qualifier) << '\n';
	}
}

static inline void dump_node(const node_texture3d_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "texture3d-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	out << depth_prefix << "channel_type:" << '\n';
	dump(node.channel_type, out, depth + 2u);
	if (node.access_qualifier) {
		out << depth_prefix << "access_qualifier: " << access_qualifier_to_string(*node.access_qualifier) << '\n';
	}
}

static inline void dump_node(const node_texture_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "texture-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	if (node.location_index) {
		out << depth_prefix << "location_index:" << '\n';
		dump(*node.location_index, out, depth + 2u);
	}
	if (node.location_count) {
		out << depth_prefix << "location_count:" << '\n';
		dump(*node.location_count, out, depth + 2u);
	}
	if (node.access_qualifier) {
		out << depth_prefix << "access_qualifier: " << access_qualifier_to_string(*node.access_qualifier) << '\n';
	}
	if (node.raster_order_group) {
		out << depth_prefix << "raster_order_group:" << '\n';
		dump(*node.raster_order_group, out, depth + 2u);
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_texture_buffer1d_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "texture-buffer1d-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	out << depth_prefix << "channel_type:" << '\n';
	dump(node.channel_type, out, depth + 2u);
	if (node.access_qualifier) {
		out << depth_prefix << "access_qualifier: " << access_qualifier_to_string(*node.access_qualifier) << '\n';
	}
}

static inline void dump_node(const node_texture_cube_array_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "texture-cube-array-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	out << depth_prefix << "channel_type:" << '\n';
	dump(node.channel_type, out, depth + 2u);
	if (node.access_qualifier) {
		out << depth_prefix << "access_qualifier: " << access_qualifier_to_string(*node.access_qualifier) << '\n';
	}
}

static inline void dump_node(const node_texture_cube_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "texture-cube-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	out << depth_prefix << "channel_type:" << '\n';
	dump(node.channel_type, out, depth + 2u);
	if (node.access_qualifier) {
		out << depth_prefix << "access_qualifier: " << access_qualifier_to_string(*node.access_qualifier) << '\n';
	}
}

static inline void dump_node(const node_thread_execution_width_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "thread-execution-width-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_thread_index_in_quadgroup_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "thread-index-in-quadgroup-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_thread_index_in_simdgroup_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "thread-index-in-simdgroup-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_thread_index_in_threadgroup_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "thread-index-in-threadgroup-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_thread_position_in_grid_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "thread-position-in-grid-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_thread_position_in_threadgroup_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "thread-position-in-threadgroup-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_threadgroup_position_in_grid_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "threadgroup-position-in-grid-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_threadgroups_per_grid_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "threadgroups-per-grid-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_threads_per_grid_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "threads-per-grid-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_threads_per_simdgroup_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "threads-per-simdgroup-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_threads_per_threadgroup_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "threads-per-threadgroup-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_tile_index_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "tile-index-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_time_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "time-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_uchar_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "uchar-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	out << depth_prefix << "size: " << node.size << '\n';
	out << depth_prefix << "alignment: " << node.alignment << '\n';
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
}

static inline void dump_node(const node_uint_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "uint-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	out << depth_prefix << "size: " << node.size << '\n';
	out << depth_prefix << "alignment: " << node.alignment << '\n';
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
}

static inline void dump_node(const node_ullong_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "ullong-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	out << depth_prefix << "size: " << node.size << '\n';
	out << depth_prefix << "alignment: " << node.alignment << '\n';
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
}

static inline void dump_node(const node_ulong_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "ulong-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	out << depth_prefix << "size: " << node.size << '\n';
	out << depth_prefix << "alignment: " << node.alignment << '\n';
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
}

static inline void dump_node(const node_ushort_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "ushort-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	out << depth_prefix << "size: " << node.size << '\n';
	out << depth_prefix << "alignment: " << node.alignment << '\n';
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
}

static inline void dump_node(const node_union_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "union-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.members) {
		out << depth_prefix << "members:\n";
		for (const auto& elem : *node.members) {
			dump(elem, out, depth + 2u);
		}
	}
}

static inline void dump_node(const node_user_annotation_fn_attr_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "user-annotation-fn-attr\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.annotation) {
		out << depth_prefix << "annotation: " << *node.annotation << '\n';
	}
}

static inline void dump_node(const node_user_attr_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "user-attr\n";
	const std::string depth_prefix(depth + 1u, '\t');
	out << depth_prefix << "name: " << node.name << '\n';
}

static inline void dump_node(const node_user_data_buffer_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "user-data-buffer-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	if (node.access_qualifier) {
		out << depth_prefix << "access_qualifier: " << access_qualifier_to_string(*node.access_qualifier) << '\n';
	}
	if (node.inline_type_info) {
		out << depth_prefix << "inline_type_info:" << '\n';
		dump(*node.inline_type_info, out, depth + 2u);
	}
	if (node.struct_type_info) {
		out << depth_prefix << "struct_type_info:" << '\n';
		dump(*node.struct_type_info, out, depth + 2u);
	}
	if (node.type_size) {
		out << depth_prefix << "type_size:" << '\n';
		dump(*node.type_size, out, depth + 2u);
	}
	if (node.type_align) {
		out << depth_prefix << "type_align:" << '\n';
		dump(*node.type_align, out, depth + 2u);
	}
	if (node.type_name) {
		out << depth_prefix << "type_name: " << *node.type_name << '\n';
	}
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_user_instance_id_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "user-instance-id-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_user_instance_id_count_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "user-instance-id-count-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_vec_type_hint_fn_attr_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "vec-type-hint-fn-attr\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.type_name) {
		out << depth_prefix << "type_name: " << *node.type_name << '\n';
	}
}

static inline void dump_node(const node_vector_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "vector-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	out << depth_prefix << "element_type:" << '\n';
	dump(node.element_type, out, depth + 2u);
	if (node.num_elements) {
		out << depth_prefix << "num_elements: " << *node.num_elements << '\n';
	}
}

static inline void dump_node(const node_vertex_function_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "vertex-function\n";
	const std::string depth_prefix(depth + 1u, '\t');
	out << depth_prefix << "name: " << node.name << '\n';
	if (node.return_types) {
		out << depth_prefix << "return_types:\n";
		for (const auto& elem : *node.return_types) {
			dump(elem, out, depth + 2u);
		}
	}
	if (node.arguments) {
		out << depth_prefix << "arguments:\n";
		for (const auto& elem : *node.arguments) {
			dump(elem, out, depth + 2u);
		}
	}
	if (node.patch) {
		out << depth_prefix << "patch:" << '\n';
		dump(*node.patch, out, depth + 2u);
	}
	if (node.user_annotation) {
		out << depth_prefix << "user_annotation:" << '\n';
		dump(*node.user_annotation, out, depth + 2u);
	}
}

static inline void dump_node(const node_vertex_id_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "vertex-id-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_vertex_input_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "vertex-input-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	if (node.location_index) {
		out << depth_prefix << "location_index:" << '\n';
		dump(*node.location_index, out, depth + 2u);
	}
	if (node.location_count) {
		out << depth_prefix << "location_count:" << '\n';
		dump(*node.location_count, out, depth + 2u);
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_vertex_output_ret_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "vertex-output-ret\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "attribute_name: " << node.attribute_name << '\n';
	if (node.location) {
		out << depth_prefix << "location:" << '\n';
		dump(*node.location, out, depth + 2u);
	}
	if (node.shared) {
		out << depth_prefix << "shared: " << *node.shared << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
}

static inline void dump_node(const node_vertex_value_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "vertex-value-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	out << depth_prefix << "underlying_type:" << '\n';
	dump(node.underlying_type, out, depth + 2u);
}

static inline void dump_node(const node_viewport_array_index_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "viewport-array-index-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_viewport_array_index_attr_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "viewport-array-index-attr\n";
	const std::string depth_prefix(depth + 1u, '\t');
}

static inline void dump_node(const node_viewport_array_index_ret_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "viewport-array-index-ret\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	if (node.shared) {
		out << depth_prefix << "shared: " << *node.shared << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
}

static inline void dump_node(const node_visible_function_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "visible-function\n";
	const std::string depth_prefix(depth + 1u, '\t');
	out << depth_prefix << "name: " << node.name << '\n';
	if (node.stitching_info) {
		out << depth_prefix << "stitching_info:" << '\n';
		dump(*node.stitching_info, out, depth + 2u);
	}
	if (node.user_annotation) {
		out << depth_prefix << "user_annotation:" << '\n';
		dump(*node.user_annotation, out, depth + 2u);
	}
}

static inline void dump_node(const node_visible_function_reference_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "visible-function-reference\n";
	const std::string depth_prefix(depth + 1u, '\t');
	out << depth_prefix << "function_name: " << node.function_name << '\n';
}

static inline void dump_node(const node_visible_function_table_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "visible-function-table-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	if (node.location_index) {
		out << depth_prefix << "location_index:" << '\n';
		dump(*node.location_index, out, depth + 2u);
	}
	if (node.location_count) {
		out << depth_prefix << "location_count:" << '\n';
		dump(*node.location_count, out, depth + 2u);
	}
	if (node.access_qualifier) {
		out << depth_prefix << "access_qualifier: " << access_qualifier_to_string(*node.access_qualifier) << '\n';
	}
	if (node.raster_order_group) {
		out << depth_prefix << "raster_order_group:" << '\n';
		dump(*node.raster_order_group, out, depth + 2u);
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_visible_function_table_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "visible-function-table-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
	out << depth_prefix << "function_type:" << '\n';
	dump(node.function_type, out, depth + 2u);
}

static inline void dump_node(const node_void_type_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "void-type\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size: " << *node.size << '\n';
	}
	if (node.alignment) {
		out << depth_prefix << "alignment: " << *node.alignment << '\n';
	}
	if (node.qualifiers) {
		out << depth_prefix << "qualifiers:\n";
		for (const auto& elem : *node.qualifiers) {
			dump(elem, out, depth + 2u);
		}
	}
}

static inline void dump_node(const node_workgroup_max_size_fn_attr_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "workgroup-max-size-fn-attr\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.size) {
		out << depth_prefix << "size:" << '\n';
		dump(*node.size, out, depth + 2u);
	}
}

static inline void dump_node(const node_workgroup_size_fn_attr_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "workgroup-size-fn-attr\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.width) {
		out << depth_prefix << "width:" << '\n';
		dump(*node.width, out, depth + 2u);
	}
	if (node.height) {
		out << depth_prefix << "height:" << '\n';
		dump(*node.height, out, depth + 2u);
	}
	if (node.depth) {
		out << depth_prefix << "depth:" << '\n';
		dump(*node.depth, out, depth + 2u);
	}
}

static inline void dump_node(const node_workgroup_size_hint_fn_attr_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "workgroup-size-hint-fn-attr\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.width) {
		out << depth_prefix << "width:" << '\n';
		dump(*node.width, out, depth + 2u);
	}
	if (node.height) {
		out << depth_prefix << "height:" << '\n';
		dump(*node.height, out, depth + 2u);
	}
	if (node.depth) {
		out << depth_prefix << "depth:" << '\n';
		dump(*node.depth, out, depth + 2u);
	}
}

static inline void dump_node(const node_world_space_direction_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "world-space-direction-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_world_space_origin_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "world-space-origin-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

static inline void dump_node(const node_world_to_object_transform_arg_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	out << std::string(depth, '\t') << "world-to-object-transform-arg\n";
	const std::string depth_prefix(depth + 1u, '\t');
	if (node.function_constant) {
		out << depth_prefix << "function_constant: " << *node.function_constant << '\n';
	}
	out << depth_prefix << "type_name: " << node.type_name << '\n';
	if (node.name) {
		out << depth_prefix << "name: " << *node.name << '\n';
	}
	if (node.unused) {
		out << depth_prefix << "unused: " << *node.unused << '\n';
	}
}

struct dummy_node_t : node_base_t { const NODE_TYPE node_type { NODE_TYPE::NONE }; };

static inline void dump_node(const node_base_t& node, llvm::raw_ostream& out, const uint32_t depth) {
	switch (((const dummy_node_t*)&node)->node_type) {
		default:
			break;
		case NODE_TYPE::ACCELERATION_STRUCTURE_TYPE:
			dump_node((const node_acceleration_structure_type_t&)node, out, depth);
			break;
		case NODE_TYPE::ACCEPT_INTERSECTION_RET:
			dump_node((const node_accept_intersection_ret_t&)node, out, depth);
			break;
		case NODE_TYPE::ADDRESS_SPACE_TYPE_QUAL:
			dump_node((const node_address_space_type_qual_t&)node, out, depth);
			break;
		case NODE_TYPE::AMPLIFICATION_COUNT_ARG:
			dump_node((const node_amplification_count_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::AMPLIFICATION_ID_ARG:
			dump_node((const node_amplification_id_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::ARRAY_OF_TYPE:
			dump_node((const node_array_of_type_t&)node, out, depth);
			break;
		case NODE_TYPE::ARRAY_REF_OF_TYPE:
			dump_node((const node_array_ref_of_type_t&)node, out, depth);
			break;
		case NODE_TYPE::ARRAY_TYPE:
			dump_node((const node_array_type_t&)node, out, depth);
			break;
		case NODE_TYPE::BFLOAT_TYPE:
			dump_node((const node_bfloat_type_t&)node, out, depth);
			break;
		case NODE_TYPE::BARYCENTRIC_COORD_ARG:
			dump_node((const node_barycentric_coord_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::BASE_INSTANCE_ARG:
			dump_node((const node_base_instance_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::BASE_VERTEX_ARG:
			dump_node((const node_base_vertex_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::BOOL_TYPE:
			dump_node((const node_bool_type_t&)node, out, depth);
			break;
		case NODE_TYPE::BUFFER_ARG:
			dump_node((const node_buffer_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::BUFFER_STRIDE_ARG:
			dump_node((const node_buffer_stride_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::CIARRAY_ARG:
			dump_node((const node_ciarray_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::CIBUILTIN_ARG:
			dump_node((const node_cibuiltin_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::CIBUILTIN_RET:
			dump_node((const node_cibuiltin_ret_t&)node, out, depth);
			break;
		case NODE_TYPE::CI_FUNCTION:
			dump_node((const node_ci_function_t&)node, out, depth);
			break;
		case NODE_TYPE::CIIMAGEBLOCK_ARG:
			dump_node((const node_ciimageblock_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::CIIMAGEBLOCK_RET:
			dump_node((const node_ciimageblock_ret_t&)node, out, depth);
			break;
		case NODE_TYPE::CIMATRIX_ARG:
			dump_node((const node_cimatrix_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::CIMATRIX_RET:
			dump_node((const node_cimatrix_ret_t&)node, out, depth);
			break;
		case NODE_TYPE::CIPADDING_ARG:
			dump_node((const node_cipadding_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::CIPOINTER_ARG:
			dump_node((const node_cipointer_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::CIPOINTER_RET:
			dump_node((const node_cipointer_ret_t&)node, out, depth);
			break;
		case NODE_TYPE::CISAMPLER_ARG:
			dump_node((const node_cisampler_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::CISAMPLER_RET:
			dump_node((const node_cisampler_ret_t&)node, out, depth);
			break;
		case NODE_TYPE::CISTRUCT_ARG:
			dump_node((const node_cistruct_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::CISTRUCT_RET:
			dump_node((const node_cistruct_ret_t&)node, out, depth);
			break;
		case NODE_TYPE::CITEXTURE_ARG:
			dump_node((const node_citexture_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::CITEXTURE_RET:
			dump_node((const node_citexture_ret_t&)node, out, depth);
			break;
		case NODE_TYPE::CHAR_TYPE:
			dump_node((const node_char_type_t&)node, out, depth);
			break;
		case NODE_TYPE::CLIP_DISTANCE_ATTR:
			dump_node((const node_clip_distance_attr_t&)node, out, depth);
			break;
		case NODE_TYPE::CLIP_DISTANCE_RET:
			dump_node((const node_clip_distance_ret_t&)node, out, depth);
			break;
		case NODE_TYPE::COMMAND_BUFFER_ARG:
			dump_node((const node_command_buffer_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::COMMAND_BUFFER_TYPE:
			dump_node((const node_command_buffer_type_t&)node, out, depth);
			break;
		case NODE_TYPE::COMPUTE_PIPELINE_STATE_ARG:
			dump_node((const node_compute_pipeline_state_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::COMPUTE_PIPELINE_STATE_TYPE:
			dump_node((const node_compute_pipeline_state_type_t&)node, out, depth);
			break;
		case NODE_TYPE::CONSTANT_ARG:
			dump_node((const node_constant_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::CONTINUE_SEARCH_RET:
			dump_node((const node_continue_search_ret_t&)node, out, depth);
			break;
		case NODE_TYPE::CONTROL_POINT_FIELD:
			dump_node((const node_control_point_field_t&)node, out, depth);
			break;
		case NODE_TYPE::CONTROL_POINT_INDEX_BUFFER_ARG:
			dump_node((const node_control_point_index_buffer_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::CONTROL_POINT_INPUT_ARG:
			dump_node((const node_control_point_input_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::CURVE_PARAMETER_ARG:
			dump_node((const node_curve_parameter_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::DEPTH2D_ARRAY_TYPE:
			dump_node((const node_depth2d_array_type_t&)node, out, depth);
			break;
		case NODE_TYPE::DEPTH2D_MS_ARRAY_TYPE:
			dump_node((const node_depth2d_ms_array_type_t&)node, out, depth);
			break;
		case NODE_TYPE::DEPTH2D_MS_TYPE:
			dump_node((const node_depth2d_ms_type_t&)node, out, depth);
			break;
		case NODE_TYPE::DEPTH2D_TYPE:
			dump_node((const node_depth2d_type_t&)node, out, depth);
			break;
		case NODE_TYPE::DEPTH_CUBE_ARRAY_TYPE:
			dump_node((const node_depth_cube_array_type_t&)node, out, depth);
			break;
		case NODE_TYPE::DEPTH_CUBE_TYPE:
			dump_node((const node_depth_cube_type_t&)node, out, depth);
			break;
		case NODE_TYPE::DEPTH_RET:
			dump_node((const node_depth_ret_t&)node, out, depth);
			break;
		case NODE_TYPE::DEPTH_STENCIL_STATE_ARG:
			dump_node((const node_depth_stencil_state_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::DEPTH_STENCIL_STATE_TYPE:
			dump_node((const node_depth_stencil_state_type_t&)node, out, depth);
			break;
		case NODE_TYPE::DIRECTION_ARG:
			dump_node((const node_direction_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::DISPATCH_QUADGROUPS_PER_THREADGROUP_ARG:
			dump_node((const node_dispatch_quadgroups_per_threadgroup_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::DISPATCH_SIMDGROUPS_PER_THREADGROUP_ARG:
			dump_node((const node_dispatch_simdgroups_per_threadgroup_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::DISPATCH_THREADS_PER_THREADGROUP_ARG:
			dump_node((const node_dispatch_threads_per_threadgroup_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::DISTANCE_ARG:
			dump_node((const node_distance_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::DISTANCE_RET:
			dump_node((const node_distance_ret_t&)node, out, depth);
			break;
		case NODE_TYPE::DOUBLE_TYPE:
			dump_node((const node_double_type_t&)node, out, depth);
			break;
		case NODE_TYPE::ENUM_TYPE:
			dump_node((const node_enum_type_t&)node, out, depth);
			break;
		case NODE_TYPE::EXTENTS_TYPE:
			dump_node((const node_extents_type_t&)node, out, depth);
			break;
		case NODE_TYPE::FLOAT_TYPE:
			dump_node((const node_float_type_t&)node, out, depth);
			break;
		case NODE_TYPE::FRAGMENT_FUNCTION:
			dump_node((const node_fragment_function_t&)node, out, depth);
			break;
		case NODE_TYPE::FRAGMENT_INPUT_ARG:
			dump_node((const node_fragment_input_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::FRONT_FACING_ARG:
			dump_node((const node_front_facing_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::FUNCTION_CONSTANT:
			dump_node((const node_function_constant_t&)node, out, depth);
			break;
		case NODE_TYPE::FUNCTION_CONSTANT_PREDICATE_ATTR:
			dump_node((const node_function_constant_predicate_attr_t&)node, out, depth);
			break;
		case NODE_TYPE::FUNCTION_HANDLE_ARG:
			dump_node((const node_function_handle_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::FUNCTION_HANDLE_TYPE:
			dump_node((const node_function_handle_type_t&)node, out, depth);
			break;
		case NODE_TYPE::FUNCTION_ID_ARG:
			dump_node((const node_function_id_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::FUNCTION_TYPE:
			dump_node((const node_function_type_t&)node, out, depth);
			break;
		case NODE_TYPE::GEOMETRY_ID_ARG:
			dump_node((const node_geometry_id_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::GEOMETRY_INTERSECTION_FUNCTION_TABLE_OFFSET_ARG:
			dump_node((const node_geometry_intersection_function_table_offset_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::GLOBAL_BINDING:
			dump_node((const node_global_binding_t&)node, out, depth);
			break;
		case NODE_TYPE::HALF_TYPE:
			dump_node((const node_half_type_t&)node, out, depth);
			break;
		case NODE_TYPE::IMAGEBLOCK_ARG:
			dump_node((const node_imageblock_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::IMAGEBLOCK_DATA_ARG:
			dump_node((const node_imageblock_data_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::IMAGEBLOCK_DATA_RET:
			dump_node((const node_imageblock_data_ret_t&)node, out, depth);
			break;
		case NODE_TYPE::IMAGEBLOCK_TYPE:
			dump_node((const node_imageblock_type_t&)node, out, depth);
			break;
		case NODE_TYPE::INDIRECT_BUFFER_ARG:
			dump_node((const node_indirect_buffer_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::INDIRECT_CONSTANT_ARG:
			dump_node((const node_indirect_constant_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::INLINE_TYPE_INFO:
			dump_node((const node_inline_type_info_t&)node, out, depth);
			break;
		case NODE_TYPE::INSTANCE_ACCELERATION_STRUCTURE_ARG:
			dump_node((const node_instance_acceleration_structure_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::INSTANCE_ID_ARG:
			dump_node((const node_instance_id_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::INSTANCE_ID_COUNT_ARG:
			dump_node((const node_instance_id_count_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::INSTANCE_INTERSECTION_FUNCTION_TABLE_OFFSET_ARG:
			dump_node((const node_instance_intersection_function_table_offset_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::INT_TYPE:
			dump_node((const node_int_type_t&)node, out, depth);
			break;
		case NODE_TYPE::INTERPOLANT_TYPE:
			dump_node((const node_interpolant_type_t&)node, out, depth);
			break;
		case NODE_TYPE::INTERSECTION_FUNCTION:
			dump_node((const node_intersection_function_t&)node, out, depth);
			break;
		case NODE_TYPE::INTERSECTION_FUNCTION_HANDLE_TYPE:
			dump_node((const node_intersection_function_handle_type_t&)node, out, depth);
			break;
		case NODE_TYPE::INTERSECTION_FUNCTION_TABLE_ARG:
			dump_node((const node_intersection_function_table_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::INTERSECTION_FUNCTION_TABLE_TYPE:
			dump_node((const node_intersection_function_table_type_t&)node, out, depth);
			break;
		case NODE_TYPE::INVARIANT_ATTR:
			dump_node((const node_invariant_attr_t&)node, out, depth);
			break;
		case NODE_TYPE::KERNEL_FUNCTION:
			dump_node((const node_kernel_function_t&)node, out, depth);
			break;
		case NODE_TYPE::KEY_FRAME_COUNT_ARG:
			dump_node((const node_key_frame_count_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::LLONG_TYPE:
			dump_node((const node_llong_type_t&)node, out, depth);
			break;
		case NODE_TYPE::LVALUE_REFERENCE_TYPE:
			dump_node((const node_lvalue_reference_type_t&)node, out, depth);
			break;
		case NODE_TYPE::LOCATION_INDEX_ATTR:
			dump_node((const node_location_index_attr_t&)node, out, depth);
			break;
		case NODE_TYPE::LONG_TYPE:
			dump_node((const node_long_type_t&)node, out, depth);
			break;
		case NODE_TYPE::MATRIX_TYPE:
			dump_node((const node_matrix_type_t&)node, out, depth);
			break;
		case NODE_TYPE::MAX_DISTANCE_ARG:
			dump_node((const node_max_distance_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::MAX_MESH_WORKGROUPS_FN_ATTR:
			dump_node((const node_max_mesh_workgroups_fn_attr_t&)node, out, depth);
			break;
		case NODE_TYPE::MESH_ARG:
			dump_node((const node_mesh_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::MESH_EMULATION_BLOCK:
			dump_node((const node_mesh_emulation_block_t&)node, out, depth);
			break;
		case NODE_TYPE::MESH_EMULATION_FRAGMENT_ANALYSIS_RESULT:
			dump_node((const node_mesh_emulation_fragment_analysis_result_t&)node, out, depth);
			break;
		case NODE_TYPE::MESH_EMULATION_MESH_KERNEL:
			dump_node((const node_mesh_emulation_mesh_kernel_t&)node, out, depth);
			break;
		case NODE_TYPE::MESH_EMULATION_MESH_LAYOUT:
			dump_node((const node_mesh_emulation_mesh_layout_t&)node, out, depth);
			break;
		case NODE_TYPE::MESH_EMULATION_MESH_VERTEX:
			dump_node((const node_mesh_emulation_mesh_vertex_t&)node, out, depth);
			break;
		case NODE_TYPE::MESH_EMULATION_OBJECT_KERNEL:
			dump_node((const node_mesh_emulation_object_kernel_t&)node, out, depth);
			break;
		case NODE_TYPE::MESH_EMULATION_VALUE_GROUP:
			dump_node((const node_mesh_emulation_value_group_t&)node, out, depth);
			break;
		case NODE_TYPE::MESH_FUNCTION:
			dump_node((const node_mesh_function_t&)node, out, depth);
			break;
		case NODE_TYPE::MESH_GRID_PROPERTIES_ARG:
			dump_node((const node_mesh_grid_properties_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::MESH_GRID_PROPERTIES_TYPE:
			dump_node((const node_mesh_grid_properties_type_t&)node, out, depth);
			break;
		case NODE_TYPE::MESH_PRIMITIVE_DATA_RET:
			dump_node((const node_mesh_primitive_data_ret_t&)node, out, depth);
			break;
		case NODE_TYPE::MESH_TYPE:
			dump_node((const node_mesh_type_t&)node, out, depth);
			break;
		case NODE_TYPE::MESH_TYPE_INFO:
			dump_node((const node_mesh_type_info_t&)node, out, depth);
			break;
		case NODE_TYPE::MESH_VERTEX_DATA_RET:
			dump_node((const node_mesh_vertex_data_ret_t&)node, out, depth);
			break;
		case NODE_TYPE::MIN_DISTANCE_ARG:
			dump_node((const node_min_distance_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::MOTION_END_TIME_ARG:
			dump_node((const node_motion_end_time_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::MOTION_START_TIME_ARG:
			dump_node((const node_motion_start_time_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::OBJECT_FUNCTION:
			dump_node((const node_object_function_t&)node, out, depth);
			break;
		case NODE_TYPE::OBJECT_TO_WORLD_TRANSFORM_ARG:
			dump_node((const node_object_to_world_transform_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::OPAQUE_PRIMITIVE_ARG:
			dump_node((const node_opaque_primitive_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::OPAQUE_TYPE:
			dump_node((const node_opaque_type_t&)node, out, depth);
			break;
		case NODE_TYPE::ORIGIN_ARG:
			dump_node((const node_origin_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::PACKED_VECTOR_TYPE:
			dump_node((const node_packed_vector_type_t&)node, out, depth);
			break;
		case NODE_TYPE::PATCH_CONTROL_POINT_TYPE:
			dump_node((const node_patch_control_point_type_t&)node, out, depth);
			break;
		case NODE_TYPE::PATCH_FN_ATTR:
			dump_node((const node_patch_fn_attr_t&)node, out, depth);
			break;
		case NODE_TYPE::PATCH_ID_ARG:
			dump_node((const node_patch_id_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::PATCH_INPUT_ARG:
			dump_node((const node_patch_input_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::PAYLOAD_ARG:
			dump_node((const node_payload_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::PIXEL_POSITION_IN_TILE_ARG:
			dump_node((const node_pixel_position_in_tile_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::PIXELS_PER_TILE_ARG:
			dump_node((const node_pixels_per_tile_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::POINT_COORD_ARG:
			dump_node((const node_point_coord_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::POINT_SIZE_ATTR:
			dump_node((const node_point_size_attr_t&)node, out, depth);
			break;
		case NODE_TYPE::POINT_SIZE_RET:
			dump_node((const node_point_size_ret_t&)node, out, depth);
			break;
		case NODE_TYPE::POINTER_TYPE:
			dump_node((const node_pointer_type_t&)node, out, depth);
			break;
		case NODE_TYPE::POSITION_ARG:
			dump_node((const node_position_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::POSITION_ATTR:
			dump_node((const node_position_attr_t&)node, out, depth);
			break;
		case NODE_TYPE::POSITION_IN_PATCH_ARG:
			dump_node((const node_position_in_patch_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::POSITION_RET:
			dump_node((const node_position_ret_t&)node, out, depth);
			break;
		case NODE_TYPE::PRIMITIVE_ACCELERATION_STRUCTURE_ARG:
			dump_node((const node_primitive_acceleration_structure_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::PRIMITIVE_CULLED_ATTR:
			dump_node((const node_primitive_culled_attr_t&)node, out, depth);
			break;
		case NODE_TYPE::PRIMITIVE_CULLED_RET:
			dump_node((const node_primitive_culled_ret_t&)node, out, depth);
			break;
		case NODE_TYPE::PRIMITIVE_DATA_ARG:
			dump_node((const node_primitive_data_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::PRIMITIVE_ID_ARG:
			dump_node((const node_primitive_id_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::PRIMITIVE_ID_ATTR:
			dump_node((const node_primitive_id_attr_t&)node, out, depth);
			break;
		case NODE_TYPE::PRIMITIVE_ID_RET:
			dump_node((const node_primitive_id_ret_t&)node, out, depth);
			break;
		case NODE_TYPE::QUADGROUP_INDEX_IN_THREADGROUP_ARG:
			dump_node((const node_quadgroup_index_in_threadgroup_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::QUADGROUPS_PER_THREADGROUP_ARG:
			dump_node((const node_quadgroups_per_threadgroup_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::R16SNORM_TYPE:
			dump_node((const node_r16snorm_type_t&)node, out, depth);
			break;
		case NODE_TYPE::R16UNORM_TYPE:
			dump_node((const node_r16unorm_type_t&)node, out, depth);
			break;
		case NODE_TYPE::R8SNORM_TYPE:
			dump_node((const node_r8snorm_type_t&)node, out, depth);
			break;
		case NODE_TYPE::R8UNORM_TYPE:
			dump_node((const node_r8unorm_type_t&)node, out, depth);
			break;
		case NODE_TYPE::RG11B10F_TYPE:
			dump_node((const node_rg11b10f_type_t&)node, out, depth);
			break;
		case NODE_TYPE::RG16SNORM_TYPE:
			dump_node((const node_rg16snorm_type_t&)node, out, depth);
			break;
		case NODE_TYPE::RG16UNORM_TYPE:
			dump_node((const node_rg16unorm_type_t&)node, out, depth);
			break;
		case NODE_TYPE::RG8SNORM_TYPE:
			dump_node((const node_rg8snorm_type_t&)node, out, depth);
			break;
		case NODE_TYPE::RG8UNORM_TYPE:
			dump_node((const node_rg8unorm_type_t&)node, out, depth);
			break;
		case NODE_TYPE::RGB10A2_TYPE:
			dump_node((const node_rgb10a2_type_t&)node, out, depth);
			break;
		case NODE_TYPE::RGB9E5_TYPE:
			dump_node((const node_rgb9e5_type_t&)node, out, depth);
			break;
		case NODE_TYPE::RGBA16SNORM_TYPE:
			dump_node((const node_rgba16snorm_type_t&)node, out, depth);
			break;
		case NODE_TYPE::RGBA16UNORM_TYPE:
			dump_node((const node_rgba16unorm_type_t&)node, out, depth);
			break;
		case NODE_TYPE::RGBA8SNORM_TYPE:
			dump_node((const node_rgba8snorm_type_t&)node, out, depth);
			break;
		case NODE_TYPE::RGBA8UNORM_TYPE:
			dump_node((const node_rgba8unorm_type_t&)node, out, depth);
			break;
		case NODE_TYPE::RVALUE_REFERENCE_TYPE:
			dump_node((const node_rvalue_reference_type_t&)node, out, depth);
			break;
		case NODE_TYPE::RECORD_BASE:
			dump_node((const node_record_base_t&)node, out, depth);
			break;
		case NODE_TYPE::RECORD_FIELD:
			dump_node((const node_record_field_t&)node, out, depth);
			break;
		case NODE_TYPE::RENDER_PIPELINE_STATE_ARG:
			dump_node((const node_render_pipeline_state_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::RENDER_PIPELINE_STATE_TYPE:
			dump_node((const node_render_pipeline_state_type_t&)node, out, depth);
			break;
		case NODE_TYPE::RENDER_TARGET_ARG:
			dump_node((const node_render_target_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::RENDER_TARGET_ARRAY_INDEX_ARG:
			dump_node((const node_render_target_array_index_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::RENDER_TARGET_ARRAY_INDEX_ATTR:
			dump_node((const node_render_target_array_index_attr_t&)node, out, depth);
			break;
		case NODE_TYPE::RENDER_TARGET_ARRAY_INDEX_RET:
			dump_node((const node_render_target_array_index_ret_t&)node, out, depth);
			break;
		case NODE_TYPE::RENDER_TARGET_ATTR:
			dump_node((const node_render_target_attr_t&)node, out, depth);
			break;
		case NODE_TYPE::RENDER_TARGET_RET:
			dump_node((const node_render_target_ret_t&)node, out, depth);
			break;
		case NODE_TYPE::SRGBA8UNORM_TYPE:
			dump_node((const node_srgba8unorm_type_t&)node, out, depth);
			break;
		case NODE_TYPE::SAMPLE_ID_ARG:
			dump_node((const node_sample_id_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::SAMPLE_MASK_ARG:
			dump_node((const node_sample_mask_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::SAMPLE_MASK_RET:
			dump_node((const node_sample_mask_ret_t&)node, out, depth);
			break;
		case NODE_TYPE::SAMPLER_ARG:
			dump_node((const node_sampler_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::SAMPLER_TYPE:
			dump_node((const node_sampler_type_t&)node, out, depth);
			break;
		case NODE_TYPE::SHARED_ATTR:
			dump_node((const node_shared_attr_t&)node, out, depth);
			break;
		case NODE_TYPE::SHORT_TYPE:
			dump_node((const node_short_type_t&)node, out, depth);
			break;
		case NODE_TYPE::SIMDGROUP_INDEX_IN_THREADGROUP_ARG:
			dump_node((const node_simdgroup_index_in_threadgroup_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::SIMDGROUPS_PER_THREADGROUP_ARG:
			dump_node((const node_simdgroups_per_threadgroup_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::STAGE_IN_ARG:
			dump_node((const node_stage_in_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::STAGE_IN_GRID_ORIGIN_ARG:
			dump_node((const node_stage_in_grid_origin_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::STAGE_IN_GRID_SIZE_ARG:
			dump_node((const node_stage_in_grid_size_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::STENCIL_RET:
			dump_node((const node_stencil_ret_t&)node, out, depth);
			break;
		case NODE_TYPE::STITCHING_ARGUMENT:
			dump_node((const node_stitching_argument_t&)node, out, depth);
			break;
		case NODE_TYPE::STRUCT_TYPE:
			dump_node((const node_struct_type_t&)node, out, depth);
			break;
		case NODE_TYPE::STRUCT_TYPE_INFO:
			dump_node((const node_struct_type_info_t&)node, out, depth);
			break;
		case NODE_TYPE::TENSOR_ARG:
			dump_node((const node_tensor_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::TENSOR_TYPE:
			dump_node((const node_tensor_type_t&)node, out, depth);
			break;
		case NODE_TYPE::TEXTURE1D_ARRAY_TYPE:
			dump_node((const node_texture1d_array_type_t&)node, out, depth);
			break;
		case NODE_TYPE::TEXTURE1D_TYPE:
			dump_node((const node_texture1d_type_t&)node, out, depth);
			break;
		case NODE_TYPE::TEXTURE2D_ARRAY_TYPE:
			dump_node((const node_texture2d_array_type_t&)node, out, depth);
			break;
		case NODE_TYPE::TEXTURE2D_MS_ARRAY_TYPE:
			dump_node((const node_texture2d_ms_array_type_t&)node, out, depth);
			break;
		case NODE_TYPE::TEXTURE2D_MS_TYPE:
			dump_node((const node_texture2d_ms_type_t&)node, out, depth);
			break;
		case NODE_TYPE::TEXTURE2D_TYPE:
			dump_node((const node_texture2d_type_t&)node, out, depth);
			break;
		case NODE_TYPE::TEXTURE3D_TYPE:
			dump_node((const node_texture3d_type_t&)node, out, depth);
			break;
		case NODE_TYPE::TEXTURE_ARG:
			dump_node((const node_texture_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::TEXTURE_BUFFER1D_TYPE:
			dump_node((const node_texture_buffer1d_type_t&)node, out, depth);
			break;
		case NODE_TYPE::TEXTURE_CUBE_ARRAY_TYPE:
			dump_node((const node_texture_cube_array_type_t&)node, out, depth);
			break;
		case NODE_TYPE::TEXTURE_CUBE_TYPE:
			dump_node((const node_texture_cube_type_t&)node, out, depth);
			break;
		case NODE_TYPE::THREAD_EXECUTION_WIDTH_ARG:
			dump_node((const node_thread_execution_width_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::THREAD_INDEX_IN_QUADGROUP_ARG:
			dump_node((const node_thread_index_in_quadgroup_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::THREAD_INDEX_IN_SIMDGROUP_ARG:
			dump_node((const node_thread_index_in_simdgroup_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::THREAD_INDEX_IN_THREADGROUP_ARG:
			dump_node((const node_thread_index_in_threadgroup_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::THREAD_POSITION_IN_GRID_ARG:
			dump_node((const node_thread_position_in_grid_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::THREAD_POSITION_IN_THREADGROUP_ARG:
			dump_node((const node_thread_position_in_threadgroup_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::THREADGROUP_POSITION_IN_GRID_ARG:
			dump_node((const node_threadgroup_position_in_grid_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::THREADGROUPS_PER_GRID_ARG:
			dump_node((const node_threadgroups_per_grid_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::THREADS_PER_GRID_ARG:
			dump_node((const node_threads_per_grid_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::THREADS_PER_SIMDGROUP_ARG:
			dump_node((const node_threads_per_simdgroup_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::THREADS_PER_THREADGROUP_ARG:
			dump_node((const node_threads_per_threadgroup_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::TILE_INDEX_ARG:
			dump_node((const node_tile_index_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::TIME_ARG:
			dump_node((const node_time_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::UCHAR_TYPE:
			dump_node((const node_uchar_type_t&)node, out, depth);
			break;
		case NODE_TYPE::UINT_TYPE:
			dump_node((const node_uint_type_t&)node, out, depth);
			break;
		case NODE_TYPE::ULLONG_TYPE:
			dump_node((const node_ullong_type_t&)node, out, depth);
			break;
		case NODE_TYPE::ULONG_TYPE:
			dump_node((const node_ulong_type_t&)node, out, depth);
			break;
		case NODE_TYPE::USHORT_TYPE:
			dump_node((const node_ushort_type_t&)node, out, depth);
			break;
		case NODE_TYPE::UNION_TYPE:
			dump_node((const node_union_type_t&)node, out, depth);
			break;
		case NODE_TYPE::USER_ANNOTATION_FN_ATTR:
			dump_node((const node_user_annotation_fn_attr_t&)node, out, depth);
			break;
		case NODE_TYPE::USER_ATTR:
			dump_node((const node_user_attr_t&)node, out, depth);
			break;
		case NODE_TYPE::USER_DATA_BUFFER_ARG:
			dump_node((const node_user_data_buffer_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::USER_INSTANCE_ID_ARG:
			dump_node((const node_user_instance_id_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::USER_INSTANCE_ID_COUNT_ARG:
			dump_node((const node_user_instance_id_count_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::VEC_TYPE_HINT_FN_ATTR:
			dump_node((const node_vec_type_hint_fn_attr_t&)node, out, depth);
			break;
		case NODE_TYPE::VECTOR_TYPE:
			dump_node((const node_vector_type_t&)node, out, depth);
			break;
		case NODE_TYPE::VERTEX_FUNCTION:
			dump_node((const node_vertex_function_t&)node, out, depth);
			break;
		case NODE_TYPE::VERTEX_ID_ARG:
			dump_node((const node_vertex_id_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::VERTEX_INPUT_ARG:
			dump_node((const node_vertex_input_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::VERTEX_OUTPUT_RET:
			dump_node((const node_vertex_output_ret_t&)node, out, depth);
			break;
		case NODE_TYPE::VERTEX_VALUE_TYPE:
			dump_node((const node_vertex_value_type_t&)node, out, depth);
			break;
		case NODE_TYPE::VIEWPORT_ARRAY_INDEX_ARG:
			dump_node((const node_viewport_array_index_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::VIEWPORT_ARRAY_INDEX_ATTR:
			dump_node((const node_viewport_array_index_attr_t&)node, out, depth);
			break;
		case NODE_TYPE::VIEWPORT_ARRAY_INDEX_RET:
			dump_node((const node_viewport_array_index_ret_t&)node, out, depth);
			break;
		case NODE_TYPE::VISIBLE_FUNCTION:
			dump_node((const node_visible_function_t&)node, out, depth);
			break;
		case NODE_TYPE::VISIBLE_FUNCTION_REFERENCE:
			dump_node((const node_visible_function_reference_t&)node, out, depth);
			break;
		case NODE_TYPE::VISIBLE_FUNCTION_TABLE_ARG:
			dump_node((const node_visible_function_table_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::VISIBLE_FUNCTION_TABLE_TYPE:
			dump_node((const node_visible_function_table_type_t&)node, out, depth);
			break;
		case NODE_TYPE::VOID_TYPE:
			dump_node((const node_void_type_t&)node, out, depth);
			break;
		case NODE_TYPE::WORKGROUP_MAX_SIZE_FN_ATTR:
			dump_node((const node_workgroup_max_size_fn_attr_t&)node, out, depth);
			break;
		case NODE_TYPE::WORKGROUP_SIZE_FN_ATTR:
			dump_node((const node_workgroup_size_fn_attr_t&)node, out, depth);
			break;
		case NODE_TYPE::WORKGROUP_SIZE_HINT_FN_ATTR:
			dump_node((const node_workgroup_size_hint_fn_attr_t&)node, out, depth);
			break;
		case NODE_TYPE::WORLD_SPACE_DIRECTION_ARG:
			dump_node((const node_world_space_direction_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::WORLD_SPACE_ORIGIN_ARG:
			dump_node((const node_world_space_origin_arg_t&)node, out, depth);
			break;
		case NODE_TYPE::WORLD_TO_OBJECT_TRANSFORM_ARG:
			dump_node((const node_world_to_object_transform_arg_t&)node, out, depth);
			break;
	}
}

} // namespace metal::reflection
