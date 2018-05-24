#pragma once
#include <cstdint>
#include <stdexcept>
#include <glm/integer.hpp>

static constexpr int block_size = 30;
static constexpr int block_border = 1;
static constexpr int block_size_pow2 = 32;
static constexpr int block_data_size = block_size_pow2 * block_size_pow2 * block_size_pow2;

static_assert ((block_size_pow2 & (block_size_pow2 - 1)) == 0,
	  "block_size_pow2 must be a power of 2");
static_assert (block_size + 2 * block_border == block_size_pow2,
	  "Block size, together with the border, must be equal to its pow-2 size (block_size_pow2)");

typedef std::uint16_t QubeType;
typedef glm::ivec3 BlockPosition;
typedef float LightLevel;

struct Block {
	BlockPosition position;
	QubeType qube[block_data_size];
	LightLevel light[block_data_size];
	// Visual data
	//  - mesh
	//  - volume textures
	// Low-res visual
	//  - mesh
	//  - textures

	static int index_unsafe(int x, int y, int z) noexcept {
		x += block_border;
		y += block_border;
		z += block_border;
		return x + block_data_size * (y + block_data_size * z);
	}

	static int index(int x, int y, int z) {
		if (x < 0 || x >= block_size || y < 0 || y >= block_size || z < 0 || z >= block_size)
			throw std::out_of_range("Qube coordinates are out of block");
		return index_unsafe(x, y, z);
	}
};
