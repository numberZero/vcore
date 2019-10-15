#include <cstdio>
#include <cstdlib>
#include <atomic>
#include <memory>
#include <mutex>
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

using namespace gl;

class gl_exception: public std::runtime_error { using runtime_error::runtime_error; };

static GLFWwindow *window = nullptr;

struct Vertex {
	glm::vec3 position;
	glm::vec3 color;
};

using Index = std::uint16_t;

static glm::vec3 const content_colors[] = {
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
	[10] = {0.0f, 0.0f, 0.0f}, // dirt_with_snow
	[11] = {0.0f, 0.0f, 0.0f}, // snow
	[12] = {0.0f, 0.0f, 0.0f}, // snowblock
	[13] = {0.0f, 0.0f, 0.0f}, // ice
	[14] = {0.0f, 0.0f, 0.0f}, // cobble
	[15] = {0.0f, 0.0f, 0.0f}, // mossycobble
	[16] = {0.0f, 0.0f, 0.0f}, // stair_cobble
	[17] = {0.0f, 0.0f, 0.0f}, // stair_desert_stone
};

int plain_shader = 0;
int map_shader = 0;

unsigned compile_shader(GLenum type, std::string const &source) {
	unsigned shader = fn.CreateShader(type);
	if (!shader)
		throw gl_exception(fmt::sprintf("Can't create shader (of type %#x)", type));
	char const *source_data = source.data();
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

struct Mesh {
	std::vector<Vertex> vertices;
};

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

public:
	void requestBlock(glm::ivec3 blockpos);

	std::vector<Mesh const *> getMeshes() const;
	bool tryGetMeshes(std::vector<Mesh const *> &to) const;
};

static timespec mapgen_time = {0, 0};
static timespec meshgen_time = {0, 0};

Mesh make_mesh(VManip &mapfrag, glm::ivec3 blockpos) {
	Mesh result;
	glm::ivec3 base = 16 * blockpos;
	auto add_vertex = [&] (glm::ivec3 pos, glm::vec3 color) -> void {
		result.vertices.push_back({pos, color});
	};
	result.vertices.reserve(4 * 3 * 16 * 16 * 17);

	for (glm::ivec3 pos: space_range{base, base + glm::ivec3{16, 16, 16}}) {
		content_t self = mapfrag.get(pos).content;
		assert(self != CONTENT_IGNORE);
		if (self == CONTENT_AIR)
			continue;
		auto color = content_colors[self];
		content_t up = mapfrag.get(pos + glm::ivec3{0, 0, 1}).content;
		if (up == CONTENT_AIR) {
			add_vertex(pos + glm::ivec3{0, 0, 1}, 1.0f * color);
			add_vertex(pos + glm::ivec3{1, 0, 1}, 1.0f * color);
			add_vertex(pos + glm::ivec3{1, 1, 1}, 1.0f * color);
			add_vertex(pos + glm::ivec3{0, 1, 1}, 1.0f * color);
		}
		content_t down = mapfrag.get(pos + glm::ivec3{0, 0, -1}).content;
		if (down == CONTENT_AIR) {
			add_vertex(pos + glm::ivec3{0, 0, 0}, 0.5f * color);
			add_vertex(pos + glm::ivec3{0, 1, 0}, 0.5f * color);
			add_vertex(pos + glm::ivec3{1, 1, 0}, 0.5f * color);
			add_vertex(pos + glm::ivec3{1, 0, 0}, 0.5f * color);
		}
		content_t left = mapfrag.get(pos + glm::ivec3{-1, 0, 0}).content;
		if (left == CONTENT_AIR) {
			add_vertex(pos + glm::ivec3{0, 0, 0}, 0.8f * color);
			add_vertex(pos + glm::ivec3{0, 0, 1}, 0.8f * color);
			add_vertex(pos + glm::ivec3{0, 1, 1}, 0.8f * color);
			add_vertex(pos + glm::ivec3{0, 1, 0}, 0.8f * color);
		}
		content_t front = mapfrag.get(pos + glm::ivec3{0, -1, 0}).content;
		if (front == CONTENT_AIR) {
			add_vertex(pos + glm::ivec3{0, 0, 0}, 0.9f * color);
			add_vertex(pos + glm::ivec3{1, 0, 0}, 0.9f * color);
			add_vertex(pos + glm::ivec3{1, 0, 1}, 0.9f * color);
			add_vertex(pos + glm::ivec3{0, 0, 1}, 0.9f * color);
		}
		content_t right = mapfrag.get(pos + glm::ivec3{1, 0, 0}).content;
		if (right == CONTENT_AIR) {
			add_vertex(pos + glm::ivec3{1, 0, 0}, 0.8f * color);
			add_vertex(pos + glm::ivec3{1, 1, 0}, 0.8f * color);
			add_vertex(pos + glm::ivec3{1, 1, 1}, 0.8f * color);
			add_vertex(pos + glm::ivec3{1, 0, 1}, 0.8f * color);
		}
		content_t back = mapfrag.get(pos + glm::ivec3{0, 1, 0}).content;
		if (back == CONTENT_AIR) {
			add_vertex(pos + glm::ivec3{0, 1, 0}, 0.7f * color);
			add_vertex(pos + glm::ivec3{0, 1, 1}, 0.7f * color);
			add_vertex(pos + glm::ivec3{1, 1, 1}, 0.7f * color);
			add_vertex(pos + glm::ivec3{1, 1, 0}, 0.7f * color);
		}
	}
	result.vertices.shrink_to_fit();
	return result;
}

static int level = 0;
static float yaw = 0.0f;
static float pitch = 0.0f;

void Map::generateMesh(glm::ivec3 blockpos) {
	// thread-local - no locking
	static const glm::ivec3 dirs[6] = {
		{-1, 0, 0},
		{1, 0, 0},
		{0, -1, 0},
		{0, 1, 0},
		{0, 0, -1},
		{0, 0, 1},
	};
	for (auto dir: dirs) {
		ClientMapBlock &mblock2 = data[blockpos + dir];
		if (!mblock2.content)
			return;
	}
	VManip vm{blockpos - 1, blockpos + 1, [this] (glm::ivec3 pos) {
		return data[pos].content.get();
	}};
	timespec t0 = thread_cpu_clock();
	Mesh mesh = make_mesh(vm, blockpos);
	timespec t1 = thread_cpu_clock();
	meshgen_time = meshgen_time + (t1 - t0);
	if (mesh.vertices.empty())
		return; // don’t need to store it
	data[blockpos].mesh = std::make_unique<Mesh>(std::move(mesh));
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

	// synchronizing with the main thread
	std::lock_guard<std::mutex> guard(mtx);
	for (auto pos: space_range{base, base + 5})
		pushBlock(mapfrag.takeBlock(pos));
	for (auto pos: space_range{base - 1, base + 7})
		generateMesh(pos);
}

std::vector<Mesh const *> Map::getMeshes() const {
	std::vector<Mesh const *> result;
	std::lock_guard<std::mutex> guard(mtx);
	for (auto &&[pos, block]: data) {
		if (block.mesh)
			result.push_back(block.mesh.get());
	}
	return result;
}

bool Map::tryGetMeshes(std::vector<Mesh const *> &to) const {
	std::unique_lock<std::mutex> guard(mtx, std::try_to_lock);
	if (!guard.owns_lock())
		return false;
	to.clear();
	for (auto &&[pos, block]: data) {
		if (block.mesh)
			to.push_back(block.mesh.get());
	}
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
		fmt::printf("Layer %d generated. Time: mapgen: %.3f s, meshgen: %.3f s\n", sum, to_double(mapgen_time), to_double(meshgen_time));
		sum++;
		mapgen_time = {0, 0};
		meshgen_time = {0, 0};
	}
}

void run() {
	map.requestBlock({0, 0, 0});
	std::thread th(mapgenth);

	auto vert_shader = R"(#version 330
uniform mat4 m;

in vec3 position;
in vec3 color;
out vec3 pos;
out vec3 v_color;

void main() {
	pos = position;
	v_color = color;
	gl_Position = m * vec4(position, 1.0);
}
)";
	auto frag_shader = R"(#version 330
in vec3 pos;
in vec3 v_color;
void main() {
// 	gl_FragColor = vec4(pos.z / 16.0, 1.0, 0.0, 1.0);
	gl_FragColor = vec4(v_color, 1.0);
}
)";
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

	auto meshes = map.getMeshes();
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
		glm::mat4 m_view = glm::translate(glm::eulerAngleXZ(pitch, yaw), -(pos + glm::vec3{0.0f, 0.0f, eye_level}));
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
		map.tryGetMeshes(meshes);
		for (Mesh const *mesh: meshes) {
			fn.VertexAttribPointer(p_location, 3, GL_FLOAT, false, sizeof(Vertex), &mesh->vertices[0].position);
			fn.VertexAttribPointer(c_location, 3, GL_FLOAT, false, sizeof(Vertex), &mesh->vertices[0].color);
			fn.DrawArrays(GL_QUADS, 0, mesh->vertices.size());
		}
		glfwSwapBuffers(window);
		glfwPollEvents();
	}
	do_run = false;
	th.join();
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

	try {
		run();
		result = EXIT_SUCCESS;
	} catch(std::exception const &e) {
		fprintf(stderr, "Exception caught of class %s with message:\n%s\n", typeid(e).name(), e.what());
	} catch(...) {
		fprintf(stderr, "Invalid exception caught\n");
	}

err_after_window:
	glfwDestroyWindow(window);
err_after_glfw:
	glfwTerminate();
err_early:
	return result;
}
