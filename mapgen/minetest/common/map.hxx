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
		return {(num + 1) / den - 1, (num + 1) % den + den - 1};
}

struct space_range {
	glm::ivec3 const a;
	glm::ivec3 const b;
};

struct space_iterator {
	space_range const &range;
	glm::ivec3 p;

	space_iterator &operator++ () {
		if (++p.x != range.b.x)
			return *this;
		p.x = range.a.x;
		if (++p.y != range.b.y)
			return *this;
		p.y = range.a.y;
		if (++p.z != range.b.z)
			return *this;
		p = range.b;
		return *this;
	}

	glm::ivec3 operator* () const {
		return p;
	}

	bool operator== (space_iterator const &b) {
		assert(&range == &b.range);
		return p == b.p;
	}

	bool operator!= (space_iterator const &b) {
		assert(&range == &b.range);
		return p != b.p;
	}
};

space_iterator begin(space_range const &range) {
	return {range, range.a};
}

space_iterator end(space_range const &range) {
	return {range, range.b};
}

struct Qube {
	content_t content = CONTENT_IGNORE;
	param_t light = 0x00;
	param_t param = 0x00;
};

struct Block {
	glm::ivec3 pos;
	Qube qube[block_data_size];
	mutable int rcounter = 0;
	mutable int wcounter = 0;

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

public:
	glm::ivec3 const bstart;
	glm::ivec3 const bsize;

	bool in_manip(glm::ivec3 vblock) const noexcept {
		vblock -= bstart;
		return
			vblock.x >= 0 && vblock.x < bsize.x &&
			vblock.y >= 0 && vblock.y < bsize.y &&
			vblock.z >= 0 && vblock.z < bsize.z;
	}

	int index_unsafe(glm::ivec3 vblock) const noexcept {
		return vblock.x - bstart.x + bsize.x * (vblock.y - bstart.y + bsize.y * (vblock.z - bstart.z));
	}

	int index(glm::ivec3 vblock) const {
		if (!in_manip(vblock))
			throw std::out_of_range("Block coordinates are out of MMVManip");
		return index_unsafe(vblock);
	}

	Block &getBlock(glm::ivec3 vblock) {
		return blocks[index(vblock)];
	}

	Block const &getBlock(glm::ivec3 vblock) const {
		return blocks[index(vblock)];
	}

	std::pair<glm::ivec3, glm::ivec3> split(glm::ivec3 vqube) const noexcept {
		glm::ivec3 vblock, rqube;
		std::tie(vblock.x, rqube.x) = divrem(vqube.x, MAP_BLOCKSIZE);
		std::tie(vblock.y, rqube.y) = divrem(vqube.y, MAP_BLOCKSIZE);
		std::tie(vblock.z, rqube.z) = divrem(vqube.z, MAP_BLOCKSIZE);
		return {vblock, rqube};
	}

	glm::ivec3 const MinEdge;
	glm::ivec3 const MaxEdge;

	MMVManip(glm::ivec3 a, glm::ivec3 b)
		: bstart(a)
		, bsize(b - a + 1)
		, MinEdge(MAP_BLOCKSIZE * a)
		, MaxEdge(MAP_BLOCKSIZE * b + MAP_BLOCKSIZE - 1)
	{
		blocks.resize(bsize.x * bsize.y * bsize.z);
		for (auto pos: space_range{a, b})
			getBlock(pos).pos = pos;
	}

	bool in_area(glm::ivec3 vqube) {
		auto [vblock, rqube] = split(vqube);
		return in_manip(vblock);
	}

	Qube const &get(glm::ivec3 vqube) {
		auto [vblock, rqube] = split(vqube);
		return getBlock(vblock).qube[Block::index_unsafe(rqube)];
	}

	Qube get_r(glm::ivec3 vqube) const {
		auto [vblock, rqube] = split(vqube);
		auto &&block = getBlock(vblock);
		++block.rcounter;
		return block.qube[Block::index_unsafe(rqube)];
	}

	Qube &get_rw(glm::ivec3 vqube) {
		auto [vblock, rqube] = split(vqube);
		auto &&block = getBlock(vblock);
		++block.wcounter;
		return block.qube[Block::index_unsafe(rqube)];
	}

	Qube get_ign(glm::ivec3 vqube) {
		auto [vblock, rqube] = split(vqube);
		if (!in_manip(vblock))
			return {CONTENT_IGNORE};
		auto &&block = getBlock(vblock);
		++block.rcounter;
		return block.qube[Block::index_unsafe(rqube)];
	}
};

using MapNode = Qube;
using MapBlock = Block;

inline static bool is_walkable(content_t content) { // FIXME: stub
	return content != CONTENT_AIR && content != CONTENT_IGNORE;
}
