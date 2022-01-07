#include "map.hxx"
#include <fmt/printf.h>
#include "time.hxx"
#include <meshgen/slicing.hxx>
#include <meshgen/meshing.hxx>
#include <mapgen/minetest/v6/mapgen_v6.hxx>

extern timespec mapgen_time;
extern timespec meshgen_time;
extern long mesh_size;
extern int level;

static std::vector<Mesh const *> queue;

void Map::generateMesh(glm::ivec3 blockpos) {
	static const glm::ivec3 dirs[6] = {
		{-1, 0, 0},
		{1, 0, 0},
		{0, -1, 0},
		{0, 1, 0},
		{0, 0, -1},
		{0, 0, 1},
	};
	std::unique_lock<std::mutex> guard(mtx);
	for (auto dir: dirs) {
		ClientMapBlock &mblock2 = data[blockpos + dir];
		if (!mblock2.content)
			return;
	}
	VManip vm{blockpos - 1, blockpos + 1, [this] (glm::ivec3 pos) {
		return data[pos].content.get();
	}};
	guard.unlock();

	timespec t0 = thread_cpu_clock();
	auto pos = MAP_BLOCKSIZE * blockpos;
	auto slices0 = make_slices(vm, blockpos);
	auto mesh0 = make_mesh(slices0, pos);
	if (mesh0->vertices.empty())
		return; // don’t need to store it
	auto slices1 = hmerge_slices(flatten_slices(slices0));
	auto mesh1 = make_mesh(slices1, pos);
	auto slices2 = hmerge_slices(flatten_slices(slices1));
	auto mesh2 = make_mesh(slices2, pos);
	auto slices3 = hmerge_slices(flatten_slices(slices2));
	auto mesh3 = make_mesh(slices3, pos);
	auto slices4 = hmerge_slices(flatten_slices(slices3));
	auto mesh4 = make_mesh(slices4, pos);
	timespec t1 = thread_cpu_clock();
	meshgen_time = meshgen_time + (t1 - t0);

	guard.lock();
	if (data[blockpos].mesh[0]) {
		guard.unlock();
		fmt::printf("Warning: not replacing already generated mesh for %d, %d, %d\n", blockpos.x, blockpos.y, blockpos.z);
// 		fmt::printf("Warning: not replacing already generated mesh for %d\n", blockpos);
		return;
	}
	data[blockpos].mesh[0] = std::move(mesh0);
	data[blockpos].mesh[1] = std::move(mesh1);
	data[blockpos].mesh[2] = std::move(mesh2);
	data[blockpos].mesh[3] = std::move(mesh3);
	data[blockpos].mesh[4] = std::move(mesh4);
	queue.push_back(data[blockpos].mesh[0].get());
	guard.unlock();
}

void Map::pushBlock(std::unique_ptr<Block> block) {
	static const glm::ivec3 dirs[6] = {
		{-1, 0, 0},
		{1, 0, 0},
		{0, -1, 0},
		{0, 1, 0},
		{0, 0, -1},
		{0, 0, 1},
	};
	auto pos = block->pos;
	ClientMapBlock &mblock = data[pos];
	if (mblock.content)
		throw std::logic_error("Block already exists");
	mblock.content = std::move(block);
}

inline static long round_to(long value, unsigned step, unsigned bias) {
	if (bias > step)
		throw std::invalid_argument("Rounding bias is insanely large");
	return step * divrem(value + bias, step).first - bias;
}

inline static long round_to(long value, unsigned step) {
	return round_to(value, step, 0);
}

void Map::requestBlock(glm::ivec3 blockpos) {
	// thread-local - no locking
	ClientMapBlock &block = data[blockpos];
	if (block.content)
		return; // generated already

	static MapgenV6Params params;
	params.np_terrain_base.seed = 33;
	params.np_terrain_higher.seed = 2;
	params.np_steepness.seed = 3;
	params.np_height_select.seed = 4;
	params.np_mud.seed = 5;
	params.np_beach.seed = 6;
	params.np_biome.seed = 7;
	params.np_cave.seed = 8;
	params.np_humidity.seed = 9;
	params.np_trees.seed = 10;
	params.np_apple_trees.seed = 11;
	static MapV6Params map_params;
	map_params.stone = 1;
	map_params.dirt = 2;
	map_params.dirt_with_grass = 3;
	map_params.sand = 4;
	map_params.water_source = 5;
	map_params.lava_source = 6;
	map_params.gravel = 7;
	map_params.desert_stone = 8;
	map_params.desert_sand = 9;
	map_params.dirt_with_snow = 10;
	map_params.snow = 11;
	map_params.snowblock = 12;
	map_params.ice = 13;
	map_params.cobble = 14;
	map_params.mossycobble = 15;
	map_params.stair_cobble = 16;
	map_params.stair_desert_stone = 17;
	static MapgenV6 mapgen(&params, map_params);

// 	fmt::print("Ground level: {}\n", mapgen.getGroundLevelAtPoint({0, 0}));
// 	fmt::print("Spawn level: {}\n", mapgen.getSpawnLevelAtPoint({0, 0}));

	glm::ivec3 base{round_to(blockpos.x, 5, 2), round_to(blockpos.y, 5, 2), round_to(blockpos.z, 5, 2)};

	if (data[base].content) {
		fmt::printf("Warning: %d,%d,%d is not generated but %d,%d,%d is\n",
			blockpos.x, blockpos.y, blockpos.z,
			base.x, base.y, base.z
		);
		return; // generated already
	}

	MMVManip mapfrag{base, base + (5 - 1)};
	BlockMakeData bmd;
	bmd.seed = 666;
	bmd.vmanip = &mapfrag;
	bmd.blockpos_min = vcore_to_mt(base);
	bmd.blockpos_max = vcore_to_mt(base + (5 - 1));
	timespec t0 = thread_cpu_clock();
	mapgen.makeChunk(&bmd);
	timespec t1 = thread_cpu_clock();
	mapgen_time = mapgen_time + (t1 - t0);
	if (!level)
		level = mapgen.getSpawnLevelAtPoint({0, 0});

	{ // synchronizing with the main thread
		std::lock_guard<std::mutex> guard(mtx);
		for (auto pos: space_range{base, base + 5})
			pushBlock(mapfrag.takeBlock(pos));
	}
	for (auto pos: space_range{base - 1, base + 6})
		generateMesh(pos);
}

void Map::getMeshesUnlocked(std::vector<Mesh const *> &result, glm::vec3 eye_pos, float mip_range) const {
	result.insert(result.end(), queue.begin(), queue.end());
	queue.clear();
}

std::vector<Mesh const *> Map::getMeshes(glm::vec3 eye_pos, float mip_range) const {
	std::vector<Mesh const *> result;
	std::lock_guard<std::mutex> guard(mtx);
	getMeshesUnlocked(result, eye_pos, mip_range);
	return result;
}

bool Map::tryGetMeshes(std::vector<Mesh const *> &to, glm::vec3 eye_pos, float mip_range) const {
	std::unique_lock<std::mutex> guard(mtx, std::try_to_lock);
	if (!guard.owns_lock())
		return false;
	getMeshesUnlocked(to, eye_pos, mip_range);
	return true;
}
