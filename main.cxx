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
	auto add_vertex = [&] (glm::ivec3 pos, glm::vec3 color) -> Index {
		result.vertices.push_back({pos, color});
		return result.vertices.size() - 1;
	};
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
			Index a = add_vertex(pos + glm::ivec3{0, 0, 1}, {0.1f, 0.3f, 0.0f});
			Index b = add_vertex(pos + glm::ivec3{1, 0, 1}, {0.0f, 0.4f, 0.0f});
			Index d = add_vertex(pos + glm::ivec3{1, 1, 1}, {0.1f, 0.3f, 0.0f});
			Index c = add_vertex(pos + glm::ivec3{0, 1, 1}, {0.0f, 0.4f, 0.0f});
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
			Index d = add_vertex(pos + glm::ivec3{0, 1, 1}, {0.0f, 0.0f, 1.0f});
			Index c = add_vertex(pos + glm::ivec3{0, 0, 1}, {0.0f, 0.0f, 1.0f});
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
			Index d = add_vertex(pos + glm::ivec3{1, 0, 1}, {0.0f, 0.0f, 1.0f});
			Index c = add_vertex(pos + glm::ivec3{0, 0, 1}, {0.0f, 0.0f, 1.0f});
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
			Index d = add_vertex(pos + glm::ivec3{1, 1, 1}, {0.0f, 0.0f, 1.0f});
			Index c = add_vertex(pos + glm::ivec3{1, 0, 1}, {0.0f, 0.0f, 1.0f});
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
			Index d = add_vertex(pos + glm::ivec3{1, 1, 1}, {0.0f, 0.0f, 1.0f});
			Index c = add_vertex(pos + glm::ivec3{0, 1, 1}, {0.0f, 0.0f, 1.0f});
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

static float yaw = 0.0f;
static float pitch = 0.0f;

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
	int level = mapgen.getSpawnLevelAtPoint({0, 0});

	std::list<Mesh> meshes;
	for (auto bp: space_range{{-2, -2, -2}, {3, 3, 3}})
		meshes.push_back(make_mesh(mapfrag, bp));

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
		glm::mat4 m_proj = glm::perspective(glm::radians(60.f), aspect, .1f, 100.f);
		m_proj[2] = -m_proj[2];
		std::swap(m_proj[1], m_proj[2]);
		glm::mat4 m_render = m_proj * m_view;
		fn.Enable(GL_DEPTH);
		fn.Enable(GL_DEPTH_TEST);
		fn.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		fn.UniformMatrix4fv(m_location, 1, GL_FALSE, &m_render[0][0]);
		fn.UseProgram(prog);
		fn.EnableVertexAttribArray(0);
		fn.EnableVertexAttribArray(1);
		for (Mesh &mesh: meshes) {
			fn.VertexAttribPointer(p_location, 3, GL_FLOAT, false, sizeof(Vertex), &mesh.vertices[0].position);
			fn.VertexAttribPointer(c_location, 3, GL_FLOAT, false, sizeof(Vertex), &mesh.vertices[0].color);
			fn.DrawArrays(GL_QUADS, 0, mesh.vertices.size());
// 			fn.DrawElements(GL_TRIANGLES, mesh.indices.size(), GL_UNSIGNED_SHORT, mesh.indices.data());
// 			fn.DrawRangeElements(GL_TRIANGLES, 0, mesh.vertices.size() - 1, mesh.indices.size(), GL_UNSIGNED_SHORT, mesh.indices.data());
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
	glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
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
