
#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>

namespace metal::reflection {

enum class NODE_TYPE : uint32_t;

struct node_base_t {
	virtual ~node_base_t() = default;
};

} // namespace metal::reflection

// include generated types/nodes
#include "metal_reflection_types.hpp"
