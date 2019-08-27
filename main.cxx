#include <cstdio>
#include <cstdlib>
#include <typeinfo>
#include <vector>
#include <unistd.h>
#include <fmt/printf.h>
#include <gl++/c.hxx>
#include <GLFW/glfw3.h>
#include "mapgen/minetest/v6/mapgen_v6.hxx"

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/hash.hpp"

using namespace gl;

class gl_exception: public std::runtime_error { using runtime_error::runtime_error; };

static GLFWwindow *window = nullptr;

struct Vertex {
	glm::vec3 position;
	glm::vec3 color;
};

using Index = std::uint16_t;

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
	std::vector<Index> indices;
};

Mesh make_mesh(MMVManip &mapfrag, glm::ivec3 blockpos) {
	Mesh result;
	glm::ivec3 base = 16 * blockpos;

	std::unordered_map<glm::ivec3, Index> vertex_ids;
	auto add_vertex = [&] (glm::ivec3 pos, glm::vec3 color) -> Index {
// 		int u = pos.x - base.x;
// 		int v = pos.y - base.y;
// 		return u * 17 + v;
		result.vertices.push_back({pos, color});
		return result.vertices.size() - 1;
// 		pos.z = 0;
// 		auto [iter, added] = vertex_ids.emplace(pos, result.vertices.size());
// 		if (added)
// 			result.vertices.push_back({pos});
// 		return iter->second;
	};
/*	for (int u = 0; u <= 16; ++u)
		for (int v = 0; v <= 16; ++v) {
			glm::ivec3 pos = {base.x + u, base.y + v, base.z};
			result.vertices.push_back({pos});
			vertex_ids[pos] = u * 17 + v;
		}*/
	auto add_index = [&] (Index i) -> void {
		result.indices.push_back(i);
	};
	result.vertices.reserve(17 * 17 * 17);
	result.indices.reserve(3 * 6 * 16 * 16 * 17);

	for (glm::ivec3 pos: space_range{base, base + glm::ivec3{16, 16, 16}}) {
		glm::ivec3 mpos = {pos.x, pos.z, pos.y};
		content_t self = mapfrag.get(mpos).content;
		assert(self != CONTENT_IGNORE);
		if (self == CONTENT_AIR)
			continue;
		content_t up = mapfrag.get(mpos + glm::ivec3{0, 1, 0}).content;
		if (up == CONTENT_AIR) {
			Index a = add_vertex(pos + glm::ivec3{0, 0, 1}, {0.2f, 0.2f, 0.2f});
			Index b = add_vertex(pos + glm::ivec3{1, 0, 1}, {0.2f, 0.0f, 0.0f});
			Index c = add_vertex(pos + glm::ivec3{0, 1, 1}, {0.0f, 0.2f, 0.0f});
			Index d = add_vertex(pos + glm::ivec3{1, 1, 1}, {0.0f, 0.0f, 0.0f});
			add_index(a);
			add_index(b);
			add_index(c);
			add_index(d);
			add_index(b);
			add_index(c);
		}
		content_t front = mapfrag.get(mpos + glm::ivec3{-1, 0, 0}).content;
		if (front == CONTENT_AIR) {
			Index a = add_vertex(pos + glm::ivec3{0, 0, 0}, {0.0f, 0.0f, 0.0f});
			Index b = add_vertex(pos + glm::ivec3{0, 1, 0}, {0.0f, 0.0f, 0.0f});
			Index c = add_vertex(pos + glm::ivec3{0, 0, 1}, {0.0f, 0.0f, 1.0f});
			Index d = add_vertex(pos + glm::ivec3{0, 1, 1}, {0.0f, 0.0f, 1.0f});
			add_index(a);
			add_index(b);
			add_index(c);
			add_index(d);
			add_index(b);
			add_index(c);
		}
		content_t left = mapfrag.get(mpos + glm::ivec3{0, 0, -1}).content;
		if (left == CONTENT_AIR) {
			Index a = add_vertex(pos + glm::ivec3{0, 0, 0}, {0.0f, 0.0f, 0.0f});
			Index b = add_vertex(pos + glm::ivec3{1, 0, 0}, {0.0f, 0.0f, 0.0f});
			Index c = add_vertex(pos + glm::ivec3{0, 0, 1}, {0.0f, 0.0f, 1.0f});
			Index d = add_vertex(pos + glm::ivec3{1, 0, 1}, {0.0f, 0.0f, 1.0f});
			add_index(a);
			add_index(b);
			add_index(c);
			add_index(d);
			add_index(b);
			add_index(c);
		}
		content_t back = mapfrag.get(mpos + glm::ivec3{1, 0, 0}).content;
		if (back == CONTENT_AIR) {
			Index a = add_vertex(pos + glm::ivec3{1, 0, 0}, {0.0f, 0.0f, 0.0f});
			Index b = add_vertex(pos + glm::ivec3{1, 1, 0}, {0.0f, 0.0f, 0.0f});
			Index c = add_vertex(pos + glm::ivec3{1, 0, 1}, {0.0f, 0.0f, 1.0f});
			Index d = add_vertex(pos + glm::ivec3{1, 1, 1}, {0.0f, 0.0f, 1.0f});
			add_index(a);
			add_index(b);
			add_index(c);
			add_index(d);
			add_index(b);
			add_index(c);
		}
		content_t right = mapfrag.get(mpos + glm::ivec3{0, 0, 1}).content;
		if (right == CONTENT_AIR) {
			Index a = add_vertex(pos + glm::ivec3{0, 1, 0}, {0.0f, 0.0f, 0.0f});
			Index b = add_vertex(pos + glm::ivec3{1, 1, 0}, {0.0f, 0.0f, 0.0f});
			Index c = add_vertex(pos + glm::ivec3{0, 1, 1}, {0.0f, 0.0f, 1.0f});
			Index d = add_vertex(pos + glm::ivec3{1, 1, 1}, {0.0f, 0.0f, 1.0f});
			add_index(a);
			add_index(b);
			add_index(c);
			add_index(d);
			add_index(b);
			add_index(c);
		}
	}
	result.vertices.shrink_to_fit();
	result.indices.shrink_to_fit();
	return result;
}

void run() {
	MapgenV6Params params;
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
	MapV6Params map_params;
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
	MapgenV6 mapgen(&params, map_params);

	fmt::print("Ground level: {}\n", mapgen.getGroundLevelAtPoint({0, 0}));
	fmt::print("Spawn level: {}\n", mapgen.getSpawnLevelAtPoint({0, 0}));

	MMVManip mapfrag({-3, -3, -3}, {3, 3, 3});
	BlockMakeData bmd;
	bmd.seed = 666;
	bmd.vmanip = &mapfrag;
	bmd.blockpos_min = {-2, -2, -2};
	bmd.blockpos_max = {2, 2, 2};
	mapgen.makeChunk(&bmd);
/*
	for (int y = -31; y < 48; y++) {
		for (int x = -31; x < 48; x++) {
			fmt::print("{:2}", mapfrag.get({x, y, 0}).content);
		}
		fmt::print("\n");
	}

	fmt::print("\n");
	for (auto pos: space_range{mapfrag.bstart, mapfrag.bstart + mapfrag.bsize})
		fmt::print("{:6}", mapfrag.getBlock(pos).rcounter);
	fmt::print("\n");
	fmt::print("\n");
	for (auto pos: space_range{mapfrag.bstart, mapfrag.bstart + mapfrag.bsize})
		fmt::print("{:6}", mapfrag.getBlock(pos).wcounter);
	fmt::print("\n");
*/

	std::list<Mesh> meshes;
	for (auto bp: space_range{{-2, -2, -2}, {3, 3, 3}})
		meshes.push_back(make_mesh(mapfrag, bp));
// 	meshes.push_back(make_mesh(mapfrag, {0, 0, 0}));

	glm::mat4 const m_iso{
		std::sqrt(3.f), 1.f, std::sqrt(2.f), 0.f,
		-std::sqrt(3.f), 1.f, std::sqrt(2.f), 0.f,
		0.f, 2.f, -std::sqrt(2.f), 0.f,
		0.f, 0.f, 0.f, std::sqrt(6.f),
	};

	auto vert_shader = R"(#version 150
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
	auto frag_shader = R"(#version 150
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
// 	fn.BindAttribLocation(prog, 0, "position");
// 	fn.BindAttribLocation(prog, 1, "color");
	int m_location = fn.GetUniformLocation(prog, "m");

	fn.ClearColor(0.2, 0.1, 0.3, 1.0);
	int max_v, max_i;
	fn.GetIntegerv(GL_MAX_ELEMENTS_VERTICES, &max_v);
	fn.GetIntegerv(GL_MAX_ELEMENTS_INDICES, &max_i);
	fmt::printf("Vertex limit: %d\nIndex limit: %d\n", max_v, max_i);
// 	unsigned buf;
// 	fn.GenBuffers(1, &buf);

	while (!glfwWindowShouldClose(window)) {
		int w, h;
		glfwGetWindowSize(window, &w, &h);
		fn.Viewport(0, 0, w, h);
		float aspect = 1.f * w / h;
		float v = .3f;
		float c = std::cos(v * glfwGetTime());
		float s = std::sin(v * glfwGetTime());
		glm::mat4 m_pos{
			1.f, 0.f, 0.f, 0.f,
			0.f, 1.f, 0.f, 0.f,
			0.f, 0.f, 1.f, 0.f,
			0.f, 0.f, -15.f, 1.f,
		};
		glm::mat4 m_rot{
			c, s, 0.f, 0.f,
			-s, c, 0.f, 0.f,
			0.f, 0.f, 1.f, 0.f,
			0.f, 0.f, 0.f, 1.f,
		};
		glm::mat4 m_trans{
			1.f, 0.f, 0.f, 0.f,
			0.f, 1.f, 0.f, 0.f,
			0.f, 0.f, 1.f, 0.f,
			0.f, 0.f, -3.f, 1.f,
		};
		glm::mat4 m_proj = glm::perspective(glm::radians(60.f), aspect, .1f, 100.f);
		m_proj[2] = -m_proj[2];
/*
		for (int k = -10; k <= 10; k++) {
			glm::vec4 v{1.0f, 1.0f, 1.0f * k, 1.0f};
			v = m_proj * v;
			fmt::printf("%.3f, %.3f, %.3f, %.3f; %.3f\n", v.x, v.y, v.z, v.w, v.z / v.w);
		}
		exit(0);
*/
		glm::mat4 m = m_proj * m_trans * m_iso * m_rot * m_pos;
		fn.Enable(GL_DEPTH);
		fn.Enable(GL_DEPTH_TEST);
		fn.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		fn.UniformMatrix4fv(m_location, 1, GL_FALSE, &m[0][0]);
		fn.UseProgram(prog);
// 		fn.BindBuffer(GL_ARRAY_BUFFER, buf);
		fn.EnableVertexAttribArray(0);
		fn.EnableVertexAttribArray(1);
		for (Mesh &mesh: meshes) {
// 			fn.BufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * mesh.vertices.size(), mesh.vertices.data(), GL_STREAM_DRAW);
			fn.VertexAttribPointer(p_location, 3, GL_FLOAT, false, sizeof(Vertex), &mesh.vertices[0].position);
			fn.VertexAttribPointer(c_location, 3, GL_FLOAT, false, sizeof(Vertex), &mesh.vertices[0].color);
// 			fn.VertexAttribPointer(p_location, 3, GL_FLOAT, false, sizeof(Vertex), (void*)offsetof(Vertex, position));
// 			fn.VertexAttribPointer(c_location, 3, GL_FLOAT, false, sizeof(Vertex), (void*)offsetof(Vertex, color));
			fn.DrawElements(GL_TRIANGLES, mesh.indices.size(), GL_UNSIGNED_SHORT, mesh.indices.data());
// 			fn.DrawRangeElements(GL_TRIANGLES, 0, mesh.vertices.size() - 1, mesh.indices.size(), GL_UNSIGNED_SHORT, mesh.indices.data());
		}
		glfwSwapBuffers(window);
		glfwPollEvents();
	}
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
