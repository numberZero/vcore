#pragma once
#include <cstdint>
#include <glm/integer.hpp>

static constexpr int block_size = 30;
static constexpr int block_border = 1;
static constexpr int block_size_pow2 = 32;

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
