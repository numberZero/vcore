#pragma once
#include <cstdint>
#include <glm/vec3.hpp>

using qube_type = std::uint16_t;
using qube_pos_t = glm::ivec3;
using qube_rel_pos_t = glm::ivec3;

static constexpr int block_size = 16;
static constexpr int block_data_size = block_size * block_size * block_size;

struct Qube {
	static constexpr qube_type q_invalid = 0;
	qube_type type = q_invalid;
};

struct BlockData {
	Qube qube[block_data_size];

	static int index_unsafe(qube_rel_pos_t relative) noexcept {
		return relative.z + block_size * (relative.y + block_size * relative.x);
	}

	static int index(qube_rel_pos_t relative) {
		if (relative.x < 0 || relative.x >= block_size || relative.y < 0 || relative.y >= block_size || relative.z < 0 || relative.z >= block_size)
			throw std::out_of_range("Qube coordinates are out of block");
		return index_unsafe(relative);
	}
};
