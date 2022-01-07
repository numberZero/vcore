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
#include <glm/gtc/matrix_transform.hpp>
#include "shader.hxx"
#include "time.hxx"
#include "mesh.hxx"
#include "map/map.hxx"
#include "util/io.hxx"
#include "cache.hxx"

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/euler_angles.hpp"

using namespace gl;
namespace fs = std::filesystem;
fs::path app_root = "/";

static GLFWwindow *window = nullptr;
extern std::array<glm::vec3, 18> const content_colors = {{
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

timespec mapgen_time = {0, 0};
timespec meshgen_time = {0, 0};
long mesh_size = 0;

int level = 0;
static float yaw = 0.0f;
static float pitch = 0.0f;

static Map map;
static std::atomic<bool> do_run = {true};

void mapgenth() {
	unsigned sum = 0;
	while (sum <= 100) {
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
		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
			break;
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
		map.tryGetMeshes(meshes, eye_pos, 200.f);
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
	fs::path self{fs::absolute(argv[0])};
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
