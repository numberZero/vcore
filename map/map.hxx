#pragma once
#include <cstdint>
#include <cstring>
#include <bitset>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <glm/vec3.hpp>
#include "helpers.hxx"
#include "mesh.hxx"
#include "mapgen/minetest/common/map.hxx"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

/*
using block_pos_t = glm::ivec3;
using block_pos_rel_t = glm::ivec3;

enum class BlockPartStatus: char {
	None,
	InProgress,
	Partial,
	Complete,
};

struct BlockInfo {
	std::shared_ptr<BlockData> data;
	std::bitset<27> neighbors_generated;
	BlockPartStatus generation_status;
	BlockPartStatus mesh_status;
};

class MapArea {
private:
	std::vector<std::shared_ptr<BlockData>> blocks;

	int index_unsafe(block_pos_rel_t pos) const noexcept {
		return pos.z + size.z * (pos.y + size.y * pos.x);
	}

	bool in_area_rel(block_pos_rel_t pos) const noexcept {
		return
			pos.x >= 0 && pos.x < size.x &&
			pos.y >= 0 && pos.y < size.y &&
			pos.z >= 0 && pos.z < size.z;
	}

	int index(block_pos_t pos) const {
		block_pos_rel_t relative = pos - origin;
		if (!in_area_rel(relative))
			throw std::out_of_range("Block coordinates are out of area");
		return index_unsafe(relative);
	}

public:
	block_pos_t const origin;
	block_pos_t const size;

	bool in_area(block_pos_t pos) const noexcept {
		return in_area_rel(pos - origin);
	}

	Block &getBlock(block_pos_t vblock) {
		return *blocks[index(vblock)];
	}

	Block const &getBlock(block_pos_t vblock) const {
		return *blocks[index(vblock)];
	}

	static std::pair<block_pos_t, qube_rel_pos_t> split(qube_pos_t vqube) const noexcept {
		block_pos_t vblock;
		qube_rel_pos_t rqube;
		std::tie(vblock.x, rqube.x) = divrem(vqube.x, MAP_BLOCKSIZE);
		std::tie(vblock.y, rqube.y) = divrem(vqube.y, MAP_BLOCKSIZE);
		std::tie(vblock.z, rqube.z) = divrem(vqube.z, MAP_BLOCKSIZE);
		return {vblock, rqube};
	}
};

class MapCache {
public:
	void requestData(block_pos_t pos, std::function<void(std::shared_ptr<BlockData const>)> cb);
};
*/

struct ClientMapBlock {
	std::unique_ptr<Block> content;
	std::unique_ptr<Mesh> mesh;
	int neighbours = 0;
};

class Map {
private:
	std::unordered_map<glm::ivec3, ClientMapBlock> data;
	mutable std::mutex mtx;

	void generateMesh(glm::ivec3 blockpos);
	void pushBlock(std::unique_ptr<Block> block);

	void getMeshesUnlocked(std::vector<Mesh const *> &to, glm::vec3 pos, float mip_range) const;

public:
	void requestBlock(glm::ivec3 blockpos);

	std::vector<Mesh const *> getMeshes(glm::vec3 pos, float mip_range) const;
	bool tryGetMeshes(std::vector<Mesh const *> &to, glm::vec3 pos, float mip_range) const;

	std::size_t size() const { return data.size(); }
};
