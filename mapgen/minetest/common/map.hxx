#pragma once
#include <cstdint>
#include <cstring>
#include <map>
#include <stdexcept>
#include <glm/vec3.hpp>

static constexpr int MAP_BLOCKSIZE = 16;
static constexpr int block_data_size = MAP_BLOCKSIZE * MAP_BLOCKSIZE * MAP_BLOCKSIZE;

using content_t = std::uint16_t;
using param_t = std::uint8_t;

static constexpr content_t CONTENT_IGNORE = (content_t)-1;
static constexpr content_t CONTENT_AIR = 0;

inline static std::pair<long, unsigned> divrem(long num, unsigned den) {
	if (num >= 0)
		return {num / den, num % den};
	else
		return {num / den - 1, num % den + den};
}

struct Qube {
	content_t content = CONTENT_IGNORE;
	param_t light = 0x00;
	param_t param = 0x00;
};

struct Block {
	Qube qube[block_data_size];

	static int index_unsafe(glm::ivec3 relative) noexcept {
		return relative.x + MAP_BLOCKSIZE * (relative.y + MAP_BLOCKSIZE * relative.z);
	}

	static int index(glm::ivec3 relative) {
		if (relative.x < 0 || relative.x >= MAP_BLOCKSIZE || relative.y < 0 || relative.y >= MAP_BLOCKSIZE || relative.z < 0 || relative.z >= MAP_BLOCKSIZE)
			throw std::out_of_range("Qube coordinates are out of block");
		return index_unsafe(relative);
	}
};
/*
class VManip {
private:
	std::map<glm::ivec3, Block> blocks;

	Block &getBlock(glm::ivec3 vblock) {
		auto iblock = blocks.find(vblock);
		if (iblock != blocks.end())
			return iblock->second;
		auto &pblock = blocks[vblock];
		std::memset(pblock.qube, 0xFF, sizeof(pblock.qube));
		return pblock;
	}

public:
	Qube &get(glm::ivec3 vqube) {
		glm::ivec3 vblock, rqube;
		std::tie(vblock.x, rqube.x) = divrem(vqube.x, MAP_BLOCKSIZE);
		std::tie(vblock.y, rqube.y) = divrem(vqube.y, MAP_BLOCKSIZE);
		std::tie(vblock.z, rqube.z) = divrem(vqube.z, MAP_BLOCKSIZE);
		return getBlock(vblock).qube[Block::index_unsafe(rqube)];
	}
};
*/
class MMVManip {
private:
	std::vector<Block> blocks;
	glm::ivec3 bstart;
	glm::ivec3 bsize;

	int index_unsafe(glm::ivec3 vblock) noexcept {
		return vblock.x - bstart.x + bsize.x * (vblock.y - bstart.y + bsize.y * (vblock.z - bstart.z));
	}

	int index(glm::ivec3 vblock) {
// 		if (vblock.x < MinEdge.x || vblock.x > MaxEdge.x ||
// 				vblock.y < MinEdge.y || vblock.y > MaxEdge.y ||
// 				vblock.z < MinEdge.z || vblock.z > MaxEdge.z)
// 			throw std::out_of_range("Block coordinates are out of MMVManip");
		return index_unsafe(vblock);
	}

	Block &getBlock(glm::ivec3 vblock) {
		return blocks[index(vblock)];
	}

public:
	glm::ivec3 const MinEdge;
	glm::ivec3 const MaxEdge;

	MMVManip(glm::ivec3 a, glm::ivec3 b)
		: bstart(a)
		, bsize(b - a + 1)
		, MinEdge(MAP_BLOCKSIZE * a)
		, MaxEdge(MAP_BLOCKSIZE * b + MAP_BLOCKSIZE - 1)
	{
		blocks.resize(bsize.x * bsize.y * bsize.z);
	}

	Qube &get(glm::ivec3 vqube) {
		glm::ivec3 vblock, rqube;
		std::tie(vblock.x, rqube.x) = divrem(vqube.x, MAP_BLOCKSIZE);
		std::tie(vblock.y, rqube.y) = divrem(vqube.y, MAP_BLOCKSIZE);
		std::tie(vblock.z, rqube.z) = divrem(vqube.z, MAP_BLOCKSIZE);
		return getBlock(vblock).qube[Block::index_unsafe(rqube)];
	}
};

using MapNode = Qube;
using MapBlock = Block;

inline static bool is_walkable(content_t content) { // FIXME: stub
	return content != CONTENT_AIR && content != CONTENT_IGNORE;
}
