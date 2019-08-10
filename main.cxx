#include <cstdio>
#include <cstdlib>
#include <typeinfo>
#include <vector>
#include <fmt/printf.h>
#include <gl++/c.hxx>
#include <GLFW/glfw3.h>
#include "mapgen/minetest/v6/mapgen_v6.hxx"

using namespace gl;

class gl_exception: public std::runtime_error { using runtime_error::runtime_error; };

static GLFWwindow *window = nullptr;

struct Vertex {
	glm::vec3 position;
};

long heightmap(long x, long y) {
	return x * y / 32;
}

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

void run() {
	MapgenV6Params params;
	params.np_terrain_base.seed = 1;
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

	for (int y = -31; y < 48; y++) {
		for (int x = -31; x < 48; x++) {
			fmt::print("{:2}", mapfrag.get({x, y, 0}).content);
		}
		fmt::print("\n");
	}

	Vertex vertices[32][4];
	std::uint16_t indices[32][6];

	for (int k = 0; k < 32; k++) {
		float z = k / 32.0f;
		vertices[k][0].position = {0.0f, 0.0f, z};
		vertices[k][1].position = {1.0f, 0.0f, z};
		vertices[k][2].position = {0.0f, 1.0f, z};
		vertices[k][3].position = {1.0f, 1.0f, z};
		indices[k][0] = 4 * k + 0;
		indices[k][1] = 4 * k + 1;
		indices[k][2] = 4 * k + 2;
		indices[k][3] = 4 * k + 3;
		indices[k][4] = 4 * k + 1;
		indices[k][5] = 4 * k + 2;
	}

	auto vert_shader = "#version 130\nin vec3 position; void main( void ) { gl_Position = vec4(position.x, position.y, position.z, 1.0); }";
	auto frag_shader = "#version 130\nvoid main() { gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0); }";
	auto prog = link_program({
		compile_shader(GL_VERTEX_SHADER, vert_shader),
		compile_shader(GL_FRAGMENT_SHADER, frag_shader),
	});
	fn.BindAttribLocation(prog, 0, "position");

	fn.ClearColor(0.2, 0.1, 0.3, 1.0);

	while (!glfwWindowShouldClose(window)) {
		fn.Clear(GL_COLOR_BUFFER_BIT);
		fn.UseProgram(prog);
		fn.EnableVertexAttribArray(0);
		fn.VertexAttribPointer(0, 3, GL_FLOAT, false, sizeof(Vertex), &vertices[0][0].position);
		fn.DrawElements(GL_TRIANGLES, 32 * 2, GL_UNSIGNED_SHORT, indices);
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
