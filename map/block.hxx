#pragma once
#include <cstdint>
#include <glm/integer.hpp>

static constexpr int block_size = 30;
static constexpr int block_border = 1;
static constexpr int block_size_pow2 = 32;

static_assert ((block_size_pow2 & (block_size_pow2 - 1)) == 0,
	  "block_size_pow2 must be a power of 2");
static_assert (block_size + 2 * block_border == block_size_pow2,
	  "Block size, together with the border, must be equal to its pow-2 size (block_size_pow2)");

typedef std::uint16_t QubeType;
typedef glm::ivec3 BlockPosition;
typedef float LightLevel;

struct Block {
	BlockPosition position;
	QubeType qube[block_size_pow2][block_size_pow2][block_size_pow2];
	LightLevel light[block_size_pow2][block_size_pow2][block_size_pow2];
	// Visual data
	//  - mesh
	//  - volume textures
	// Low-res visual
	//  - mesh
	//  - textures
};
