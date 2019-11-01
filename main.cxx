#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <atomic>
#include <filesystem>
#include <memory>
#include <mutex>
#include <random>
#include <thread>
#include <typeinfo>
#include <vector>
#include <unistd.h>
#include <fmt/printf.h>
#include <gl++/c.hxx>
#include <GLFW/glfw3.h>
#include "mapgen/minetest/v6/mapgen_v6.hxx"
#include "time.hxx"

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/euler_angles.hpp"
#include "glm/gtx/hash.hpp"
#include "util/io.hxx"

using namespace gl;
namespace fs = std::filesystem;
fs::path app_root = "/";

class gl_exception: public std::runtime_error { using runtime_error::runtime_error; };

static GLFWwindow *window = nullptr;

struct Vertex {
	glm::vec3 position;
	glm::vec3 color;
};

using Index = std::uint16_t;

static std::array<glm::vec3, 18> const content_colors = {{
	[0] = {1.0f, 1.0f, 1.0f}, // air
	[1] = {0.5f, 0.5f, 0.5f}, // stone
	[2] = {0.5f, 0.2f, 0.1f}, // dirt
	[3] = {0.2f, 0.6f, 0.0f}, // dirt_with_grass
	[4] = {0.9f, 0.8f, 0.6f}, // sand
	[5] = {0.3f, 0.4f, 0.9f}, // water_source
	[6] = {0.9f, 0.6f, 0.0f}, // lava_source
	[7] = {0.3f, 0.3f, 0.3f}, // gravel
	[8] = {0.0f, 0.0f, 0.0f}, // desert_stone
	[9] = {0.0f, 0.0f, 0.0f}, // desert_sand
	[10] = {0.7f, 0.8f, 0.9f}, // dirt_with_snow
	[11] = {0.0f, 0.0f, 0.0f}, // snow
	[12] = {0.8f, 0.9f, 1.0f}, // snowblock
	[13] = {0.6f, 0.7f, 1.0f}, // ice
	[14] = {0.0f, 0.0f, 0.0f}, // cobble
	[15] = {0.0f, 0.0f, 0.0f}, // mossycobble
	[16] = {0.0f, 0.0f, 0.0f}, // stair_cobble
	[17] = {0.0f, 0.0f, 0.0f}, // stair_desert_stone
}};

int plain_shader = 0;
int map_shader = 0;

unsigned compile_shader(GLenum type, bytearray const &source) {
	unsigned shader = fn.CreateShader(type);
	if (!shader)
		throw gl_exception(fmt::sprintf("Can't create shader (of type %#x)", type));
	char const *source_data = reinterpret_cast<char const *>(source.data());
	int source_length = source.size();
	fn.ShaderSource(shader, 1, &source_data, &source_length);
	fn.CompileShader(shader);
	int compiled = 0;
	fn.GetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if (compiled)
		return shader;
	int log_length = 0;
	fn.GetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
	if (!log_length)
		throw gl_exception(fmt::sprintf("Can't compile shader (of type %#x); no further information available.", type));
	std::vector<char> log(log_length);
	fn.GetShaderInfoLog(shader, log.size(), &log_length, log.data());
	assert(log.size() == log_length + 1);
	throw gl_exception(fmt::sprintf("Can't compile shader (of type %#x): %s", type, log.data()));
}

unsigned link_program(std::vector<unsigned> shaders, bool delete_shaders = true) {
	unsigned program = fn.CreateProgram();
	if (!program)
		throw gl_exception("Can't create program");
	for (GLenum shader: shaders)
		fn.AttachShader(program, shader);
	fn.LinkProgram(program);
	for (GLenum shader: shaders)
		fn.DetachShader(program, shader);
	int linked = 0;
	fn.GetProgramiv(program, GL_LINK_STATUS, &linked );
	if (linked) {
		if (delete_shaders)
			for (GLenum shader: shaders)
				fn.DeleteShader(shader);
		return program;
	}
	int log_length = 0;
	fn.GetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
	if (!log_length)
		throw gl_exception("Can't link program; no further information available.");
	std::vector<char> log(log_length);
	fn.GetProgramInfoLog(program, log.size(), &log_length, log.data());
	assert(log.size() == log_length + 1);
	throw gl_exception(fmt::sprintf("Can't link program: %s", log.data()));
}

template <int level>
struct Slice {
	static constexpr int size = MAP_BLOCKSIZE >> level;
	static constexpr int data_size = size * size;

	content_t face[data_size];

	static int index_unsafe(glm::ivec2 pos) noexcept {
		return pos.y + size * pos.x;
	}

	static int index(glm::ivec2 pos) {
		if (pos.x < 0 || pos.x >= size || pos.y < 0 || pos.y >= size)
			throw std::out_of_range("Face coordinates are out of slice");
		return index_unsafe(pos);
	}

	content_t get_r(glm::ivec2 pos) const {
		return face[index(pos)];
	}

	content_t &get_rw(glm::ivec2 pos) {
		return face[index(pos)];
	}
};

template <int h_level = 0, int v_level = h_level>
using SlicePack = std::array<Slice<h_level>, (MAP_BLOCKSIZE >> v_level)>;

template <int h_level = 0, int v_level = h_level>
struct SliceSet {
	using Pack = SlicePack<h_level, v_level>;
	Pack xn, xp, yn, yp, zn, zp;
};

struct Mesh {
	std::vector<Vertex> vertices;
};

struct ClientMapBlock {
	std::unique_ptr<Block> content;
	std::array<std::unique_ptr<Mesh>, 5> mesh;
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
};

static timespec mapgen_time = {0, 0};
static timespec meshgen_time = {0, 0};
static long mesh_size = 0;

inline static glm::ivec2 pack_xn(glm::ivec3 rel) { return {rel.z, rel.y}; }
inline static glm::ivec2 pack_xp(glm::ivec3 rel) { return {rel.y, rel.z}; }
inline static glm::ivec2 pack_yn(glm::ivec3 rel) { return {rel.x, rel.z}; }
inline static glm::ivec2 pack_yp(glm::ivec3 rel) { return {rel.z, rel.x}; }
inline static glm::ivec2 pack_zn(glm::ivec3 rel) { return {rel.y, rel.x}; }
inline static glm::ivec2 pack_zp(glm::ivec3 rel) { return {rel.x, rel.y}; }

inline static glm::ivec3 unpack_xn(glm::ivec2 uv) { return {0, uv.y, uv.x}; }
inline static glm::ivec3 unpack_xp(glm::ivec2 uv) { return {0, uv.x, uv.y}; }
inline static glm::ivec3 unpack_yn(glm::ivec2 uv) { return {uv.x, 0, uv.y}; }
inline static glm::ivec3 unpack_yp(glm::ivec2 uv) { return {uv.y, 0, uv.x}; }
inline static glm::ivec3 unpack_zn(glm::ivec2 uv) { return {uv.y, uv.x, 0}; }
inline static glm::ivec3 unpack_zp(glm::ivec2 uv) { return {uv.x, uv.y, 0}; }

SliceSet<> make_slices(VManip const &mapfrag, glm::ivec3 blockpos) {
	SliceSet<> result;
	std::memset(&result, -1, sizeof(result));
	glm::ivec3 base = 16 * blockpos;
	for (glm::ivec3 rel: space_range{glm::ivec3{0, 0, 0}, glm::ivec3{16, 16, 16}}) {
		glm::ivec3 pos = base + rel;
		content_t self = mapfrag.get(pos).content;
		assert(self != CONTENT_IGNORE);
		if (self == CONTENT_AIR)
			continue;
		if (mapfrag.get(pos + glm::ivec3{-1, 0, 0}).content == CONTENT_AIR) result.xn.at(15 - rel.x).get_rw(pack_xn(rel)) = self;
		if (mapfrag.get(pos + glm::ivec3{ 1, 0, 0}).content == CONTENT_AIR) result.xp.at(rel.x).get_rw(pack_xp(rel)) = self;
		if (mapfrag.get(pos + glm::ivec3{0, -1, 0}).content == CONTENT_AIR) result.yn.at(15 - rel.y).get_rw(pack_yn(rel)) = self;
		if (mapfrag.get(pos + glm::ivec3{0,  1, 0}).content == CONTENT_AIR) result.yp.at(rel.y).get_rw(pack_yp(rel)) = self;
		if (mapfrag.get(pos + glm::ivec3{0, 0, -1}).content == CONTENT_AIR) result.zn.at(15 - rel.z).get_rw(pack_zn(rel)) = self;
		if (mapfrag.get(pos + glm::ivec3{0, 0,  1}).content == CONTENT_AIR) result.zp.at(rel.z).get_rw(pack_zp(rel)) = self;
	}
	return result;
}

template <int h_level>
Slice<h_level> merge_slices(Slice<h_level> const &bottom, Slice<h_level> const &top) {
	Slice<h_level> result;
	for (int k = 0; k < result.data_size; k++) {
		content_t t = top.face[k];
		content_t b = bottom.face[k];
		result.face[k] = t == CONTENT_IGNORE ? b : t;
	}
	return result;
}

template <int h_level, int v_level>
auto flatten_slices(SlicePack<h_level, v_level> const &slices) {
	SlicePack<h_level, v_level + 1> result;
	for (int k = 0; k < result.size(); k++)
		result[k] = merge_slices(slices[2 * k], slices[2 * k + 1]);
	return result;
}

template <int h_level, int v_level>
auto flatten_slices(SliceSet<h_level, v_level> const &slices) {
	SliceSet<h_level, v_level + 1> result;
	result.xn = flatten_slices<h_level, v_level>(slices.xn);
	result.xp = flatten_slices<h_level, v_level>(slices.xp);
	result.yn = flatten_slices<h_level, v_level>(slices.yn);
	result.yp = flatten_slices<h_level, v_level>(slices.yp);
	result.zn = flatten_slices<h_level, v_level>(slices.zn);
	result.zp = flatten_slices<h_level, v_level>(slices.zp);
	return result;
}

template <int h_level>
Slice<h_level + 1> hmerge_slice(Slice<h_level> const &slice) {
	static std::mt19937 rnd(std::time(nullptr));
	Slice<h_level + 1> result;
	std::vector<content_t> v;
	v.reserve(4);
	for (int j = 0; j < result.size; j++)
	for (int i = 0; i < result.size; i++) {
		v.clear();
		content_t a = slice.get_r({2 * i    , 2 * j    });
		content_t b = slice.get_r({2 * i    , 2 * j + 1});
		content_t c = slice.get_r({2 * i + 1, 2 * j    });
		content_t d = slice.get_r({2 * i + 1, 2 * j + 1});
		if (a != CONTENT_IGNORE) v.push_back(a);
		if (b != CONTENT_IGNORE) v.push_back(b);
		if (c != CONTENT_IGNORE) v.push_back(c);
		if (d != CONTENT_IGNORE) v.push_back(d);
		content_t r = CONTENT_IGNORE;
		if (!v.empty()) {
			std::uniform_int_distribution<std::size_t> dist{0, v.size() - 1};
			r = v.at(dist(rnd));
		}
		result.get_rw({i, j}) = r;
	}
	return result;
}

template <int h_level, int v_level>
auto hmerge_slices(SlicePack<h_level, v_level> const &slices) {
	SlicePack<h_level + 1, v_level> result;
	for (int k = 0; k < result.size(); k++)
		result[k] = hmerge_slice(slices[k]);
	return result;
}

template <int h_level, int v_level>
auto hmerge_slices(SliceSet<h_level, v_level> const &slices) {
	SliceSet<h_level + 1, v_level> result;
	result.xn = hmerge_slices<h_level, v_level>(slices.xn);
	result.xp = hmerge_slices<h_level, v_level>(slices.xp);
	result.yn = hmerge_slices<h_level, v_level>(slices.yn);
	result.yp = hmerge_slices<h_level, v_level>(slices.yp);
	result.zn = hmerge_slices<h_level, v_level>(slices.zn);
	result.zp = hmerge_slices<h_level, v_level>(slices.zp);
	return result;
}

template <int level, glm::ivec3 transform(glm::ivec2)>
void slice_to_mesh(std::vector<Vertex> &dest, Slice<level> slice, glm::ivec3 base, float brightness = 1.0f) {
	int scale = 1 << level;
	for (int j = 0; j < slice.size; j++)
	for (int i = 0; i < slice.size; i++) {
		content_t self = slice.get_r({i, j});
		if (self == CONTENT_IGNORE)
			continue;
		auto color = brightness * content_colors.at(self);
		dest.push_back({base + scale * transform({i    , j    }), color});
		dest.push_back({base + scale * transform({i + 1, j    }), color});
		dest.push_back({base + scale * transform({i + 1, j + 1}), color});
		dest.push_back({base + scale * transform({i    , j + 1}), color});
	}
}

template <int h_level, int v_level>
auto make_mesh(SliceSet<h_level, v_level> slices, glm::ivec3 offset) {
	auto result = std::make_unique<Mesh>();
	result->vertices.reserve(4 * 3 * 16 * 16 * 17);
	for (int index = 0; index < MAP_BLOCKSIZE >> v_level; index++) {
		int op = (index + 1) << v_level;
		int on = MAP_BLOCKSIZE - op;
		slice_to_mesh<h_level, unpack_xn>(result->vertices, slices.xn.at(index), offset + glm::ivec3{on, 0, 0}, 0.8f);
		slice_to_mesh<h_level, unpack_xp>(result->vertices, slices.xp.at(index), offset + glm::ivec3{op, 0, 0}, 0.8f);
		slice_to_mesh<h_level, unpack_yn>(result->vertices, slices.yn.at(index), offset + glm::ivec3{0, on, 0}, 0.9f);
		slice_to_mesh<h_level, unpack_yp>(result->vertices, slices.yp.at(index), offset + glm::ivec3{0, op, 0}, 0.7f);
		slice_to_mesh<h_level, unpack_zn>(result->vertices, slices.zn.at(index), offset + glm::ivec3{0, 0, on}, 0.5f);
		slice_to_mesh<h_level, unpack_zp>(result->vertices, slices.zp.at(index), offset + glm::ivec3{0, 0, op}, 1.0f);
	}
	result->vertices.shrink_to_fit();
	return std::move(result);
}


static int level = 0;
static float yaw = 0.0f;
static float pitch = 0.0f;

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
		return;
	}
	data[blockpos].mesh[0] = std::move(mesh0);
	data[blockpos].mesh[1] = std::move(mesh1);
	data[blockpos].mesh[2] = std::move(mesh2);
	data[blockpos].mesh[3] = std::move(mesh3);
	data[blockpos].mesh[4] = std::move(mesh4);
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
	result.clear();
	for (auto &&[pos, block]: data) {
		if (!block.mesh[0])
			continue;
		glm::vec3 center = glm::vec3(MAP_BLOCKSIZE * pos + MAP_BLOCKSIZE / 2);
		float distance = glm::length(center - eye_pos);
		int mip_level = 0;
		std::frexp(distance / mip_range, &mip_level);
		if (mip_level < 0)
			mip_level = 0;
		else if (mip_level >= block.mesh.size())
			mip_level = block.mesh.size() - 1;
		Mesh const *mesh = block.mesh.at(mip_level).get();
		if (!mesh) {
			fmt::printf("Warning: mip level %d doesn't exist for %d,%d,%d\n", mip_level, pos.x, pos.y, pos.z);
			mesh = block.mesh[0].get();
		}
		result.push_back(mesh);
	}
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
	to.clear();
	getMeshesUnlocked(to, eye_pos, mip_range);
	return true;
}

static Map map;
static std::atomic<bool> do_run = {true};

void mapgenth() {
	unsigned sum = 0;
	while (sum <= 40) {
		for (int i = 0; i <= sum; i++)
			for (int j = 0; j <= sum - i; j++) {
				int k = sum - i - j;
				if (!do_run)
					return;
				map.requestBlock({i, j, k});
			}
		fmt::printf("Layer %d generated. Time: mapgen: %.3f s, meshgen: %.3f s; mesh size: %d quads\n", sum, to_double(mapgen_time), to_double(meshgen_time), mesh_size);
		sum++;
		mapgen_time = {0, 0};
		meshgen_time = {0, 0};
	}
}

void run() {
	auto vert_shader = read_file(app_root / "shaders/land.vert");
	auto frag_shader = read_file(app_root / "shaders/land.frag");
	auto prog = link_program({
		compile_shader(GL_VERTEX_SHADER, vert_shader),
		compile_shader(GL_FRAGMENT_SHADER, frag_shader),
	});
	int p_location =  fn.GetAttribLocation(prog, "position");
	int c_location =  fn.GetAttribLocation(prog, "color");
	int m_location = fn.GetUniformLocation(prog, "m");

	fn.ClearColor(0.2, 0.1, 0.3, 1.0);
	int max_v, max_i;
	fn.GetIntegerv(GL_MAX_ELEMENTS_VERTICES, &max_v);
	fn.GetIntegerv(GL_MAX_ELEMENTS_INDICES, &max_i);
	fmt::printf("Vertex limit: %d\nIndex limit: %d\n", max_v, max_i);

	glm::vec3 pos{0.0f, 0.0f, level};
	float t = glfwGetTime();

	std::vector<Mesh const *> meshes;
	while (!glfwWindowShouldClose(window)) {
		float t2 = glfwGetTime();
		float dt = t2 - t;
		t = t2;
		int w, h;
		glfwGetWindowSize(window, &w, &h);
		fn.Viewport(0, 0, w, h);
		glm::vec3 v{0.0f, 0.0f, 0.0f};
		if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
			v.y += 1.0;
		if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
			v.x -= 1.0;
		if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
			v.y -= 1.0;
		if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
			v.x += 1.0;
		if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
			v.z += 1.0;
		if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
			v.z -= 1.0;
		pos += dt * 10.f * glm::orientate3(-yaw) * v;
		float aspect = 1.f * w / h;
		float eye_level = 1.75f;
		glm::vec3 eye_pos = pos + glm::vec3{0.0f, 0.0f, eye_level};
		glm::mat4 m_view = glm::translate(glm::eulerAngleXZ(pitch, yaw), -eye_pos);
		glm::mat4 m_proj = glm::infinitePerspective(glm::radians(60.f), aspect, .1f);
		m_proj[2] = -m_proj[2];
		std::swap(m_proj[1], m_proj[2]);
		glm::mat4 m_render = m_proj * m_view;
		fn.Enable(GL_DEPTH);
		fn.Enable(GL_DEPTH_TEST);
		fn.Enable(GL_CULL_FACE);
		fn.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		fn.UniformMatrix4fv(m_location, 1, GL_FALSE, &m_render[0][0]);
		fn.UseProgram(prog);
		fn.EnableVertexAttribArray(0);
		fn.EnableVertexAttribArray(1);
		map.tryGetMeshes(meshes, eye_pos, 64.f);
		for (Mesh const *mesh: meshes) {
			fn.VertexAttribPointer(p_location, 3, GL_FLOAT, false, sizeof(Vertex), &mesh->vertices[0].position);
			fn.VertexAttribPointer(c_location, 3, GL_FLOAT, false, sizeof(Vertex), &mesh->vertices[0].color);
			fn.DrawArrays(GL_QUADS, 0, mesh->vertices.size());
		}
		glfwSwapBuffers(window);
		glfwPollEvents();
	}
}

static void on_mouse_move(GLFWwindow *window, double xpos, double ypos) {
	static double base_xpos = xpos;
	static double base_ypos = ypos;
	xpos -= base_xpos;
	ypos -= base_ypos;
	yaw = xpos * 1e-2;
	pitch = ypos * 1e-2;
}

int main(int argc, char **argv) {
	fs::path self{argv[0]};
	if (self.has_parent_path())
		app_root = self.parent_path().parent_path();
	int result = EXIT_FAILURE;
	if (!glfwInit()) {
		fprintf(stderr, "Can't initialize GLFW");
		goto err_early;
	}

// 	glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
// 	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
// 	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
	glfwWindowHint(GLFW_DEPTH_BITS, 16);
	glfwWindowHint(GLFW_SAMPLES, 4);
	window = glfwCreateWindow(800, 600, "V_CORE", NULL, NULL);
	if (!window) {
		fprintf(stderr, "Can't create window");
		goto err_after_glfw;
	}
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
#ifdef GLFW_RAW_MOUSE_MOTION
	glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
#endif
	glfwSetCursorPosCallback(window, on_mouse_move);

	glfwMakeContextCurrent(window);
	loadAll(glfwGetProcAddress);

	map.requestBlock({0, 0, 0});
	{
	std::thread th(mapgenth);
	try {
		run();
		result = EXIT_SUCCESS;
	} catch(std::exception const &e) {
		fprintf(stderr, "Exception caught of class %s with message:\n%s\n", typeid(e).name(), e.what());
	} catch(...) {
		fprintf(stderr, "Invalid exception caught\n");
	}
	do_run = false;
	th.join();
	}

err_after_window:
	glfwDestroyWindow(window);
err_after_glfw:
	glfwTerminate();
err_early:
	return result;
}
