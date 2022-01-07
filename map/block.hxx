#pragma once
#include <cstdint>
#include <glm/vec3.hpp>

using QubeType = std::uint16_t;
using QubePos = glm::ivec3;
using QubeRelPos = glm::ivec3;
using BlockPos = glm::ivec3;

using content_t = std::uint16_t;
using param_t = std::uint8_t;

static constexpr int block_size = 16;
static constexpr int block_data_size = block_size * block_size * block_size;

static constexpr content_t CONTENT_IGNORE = (content_t)-1;
static constexpr content_t CONTENT_AIR = 0;

struct Qube {
	content_t content = CONTENT_IGNORE;
	param_t light = 0x00;
	param_t param = 0x00;
};

struct Block {
	BlockPos pos;
	Qube qube[block_data_size];
// 	mutable int rcounter = 0;
// 	mutable int wcounter = 0;

	static int index_unsafe(QubeRelPos pos) noexcept {
		return pos.z + block_size * (pos.y + block_size * pos.x);
	}

	static int index(QubeRelPos pos) {
		if (pos.x < 0 || pos.x >= block_size || pos.y < 0 || pos.y >= block_size || pos.z < 0 || pos.z >= block_size)
			throw std::out_of_range("Qube coordinates are out of block");
		return index_unsafe(pos);
	}

	Qube get_r(QubeRelPos pos) const {
// 		++rcounter;
		return qube[index(pos)];
	}

	Qube &get_rw(QubeRelPos pos) {
// 		++wcounter;
		return qube[index(pos)];
	}
};

class VManip {
private:
	std::vector<Block *> blocks;

public:
	BlockPos const bstart;
	BlockPos const bsize;

	bool in_manip(BlockPos pos) const noexcept {
		pos -= bstart;
		return
				pos.x >= 0 && pos.x < bsize.x &&
				pos.y >= 0 && pos.y < bsize.y &&
				pos.z >= 0 && pos.z < bsize.z;
	}

	int index_unsafe(BlockPos pos) const noexcept {
		return pos.x - bstart.x + bsize.x * (pos.y - bstart.y + bsize.y * (pos.z - bstart.z));
	}

	int index(BlockPos pos) const {
		if (!in_manip(pos))
			throw std::out_of_range("Block coordinates are out of VManip");
		return index_unsafe(pos);
	}

	Block &getBlock(BlockPos pos) {
		return *blocks[index(pos)];
	}

	Block const &getBlock(BlockPos pos) const {
		return *blocks[index(pos)];
	}

	static std::pair<BlockPos, QubeRelPos> split(QubePos pos) {
		BlockPos block;
		QubeRelPos qube;
		std::tie(block.x, qube.x) = divrem(pos.x, block_size);
		std::tie(block.y, qube.y) = divrem(pos.y, block_size);
		std::tie(block.z, qube.z) = divrem(pos.z, block_size);
		return {block, qube};
	}

	VManip(BlockPos a, BlockPos b, std::function<Block *(BlockPos)> getter)
		: bstart(a)
		, bsize(b - a + 1)
	{
		blocks.resize(bsize.x * bsize.y * bsize.z);
		for (BlockPos pos: space_range{a, b + 1})
			blocks[index(pos)] = getter(pos);// ?: throw std::logic_error("Can't fill VManip");
	}

	Qube const &get(QubePos pos) const {
		auto [block, qube] = split(pos);
		return getBlock(block).qube[Block::index_unsafe(qube)];
	}
};
