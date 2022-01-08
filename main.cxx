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
#include <unordered_map>
#include <vector>
#include <unistd.h>
#include <fmt/printf.h>
#include <gl++/c.hxx>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <SDL2/SDL_surface.h>
#include <SDL2/SDL_image.h>
#include "shader.hxx"
#include "time.hxx"
#include "mesh.hxx"
#include "map/map.hxx"
#include "util/io.hxx"
#include "cache.hxx"
#include "terminal/gltty.hxx"
#include "timer.hxx"

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
static std::atomic<int> s;

static Map map;
static std::atomic<bool> do_run = {true};

static GLTTY tty;
static FrameTimer timer;

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
		s = sum;
		sum++;
		mapgen_time = {0, 0};
		meshgen_time = {0, 0};
	}
}

unsigned nodeTexture = 0;

void loadTextures() {
	static constexpr auto extensions = {"png", "jpg"};
	static constexpr auto mip_levels = 10;
	static constexpr auto texture_size = 1 << (mip_levels - 1);
	static constexpr auto type_count = 16;
	fn.CreateTextures(GL_TEXTURE_2D_ARRAY, 1, &nodeTexture);
	fn.TextureParameteri(nodeTexture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	fn.TextureParameteri(nodeTexture, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
	fn.TextureStorage3D(nodeTexture, mip_levels, GL_RGBA8, texture_size, texture_size, type_count);
	for (int k = 1; k < type_count; k++) {
		SDL_Surface *image = nullptr;
		for (auto &&ext: extensions) {
			auto filename = app_root / fmt::sprintf("textures/%d.%s", k, ext);
			image = IMG_Load(filename.c_str());
			if (image)
				break;
		}
		if (!image) {
			fmt::printf("Can't load Image #%d: %s\n", k, SDL_GetError());
			continue;
		}
		if (int err = SDL_LockSurface(image); err) {
			fmt::printf("Can't lock surface of image #%d: %s\n", k, SDL_GetError());
			continue;
		}
		if (image->w != texture_size || image->h != texture_size) {
			fmt::printf("Wrong image #%d size: %dx%d instead of %d^2\n", k, image->w, image->h, texture_size);
			goto unlock;
		}
		switch (image->format->BitsPerPixel) {
			case 24:
				fn.TextureSubImage3D(nodeTexture, 0, 0, 0, k, texture_size, texture_size, 1, GL_RGB, GL_UNSIGNED_BYTE, image->pixels);
				break;
			case 32:
				fn.TextureSubImage3D(nodeTexture, 0, 0, 0, k, texture_size, texture_size, 1, GL_RGBA, GL_UNSIGNED_BYTE, image->pixels);
				break;
			default:
				fmt::printf("Unsupported image #%d BPP: %d\n", k, image->format->BitsPerPixel);
				goto unlock;
		}
	unlock:
		SDL_UnlockSurface(image);
		SDL_FreeSurface(image);
	}
	fn.GenerateTextureMipmap(nodeTexture);
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
	int u_location =  fn.GetAttribLocation(prog, "uv");
	int k_location =  fn.GetAttribLocation(prog, "type");
	int m_location = fn.GetUniformLocation(prog, "m");
	int t_location = fn.GetUniformLocation(prog, "tex");

	loadTextures();

	fn.ClearColor(0.2, 0.1, 0.3, 1.0);
	int max_v, max_i;
	fn.GetIntegerv(GL_MAX_ELEMENTS_VERTICES, &max_v);
	fn.GetIntegerv(GL_MAX_ELEMENTS_INDICES, &max_i);
	fmt::printf("Vertex limit: %d\nIndex limit: %d\n", max_v, max_i);

	tty.init();

	glm::vec3 pos{0.0f, 0.0f, level};
	std::vector<Mesh const *> meshes;
	std::unordered_map<Mesh const *, unsigned> buffers;
	timer.start();
	while (!glfwWindowShouldClose(window)) {
		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
			break;
		timer.begin_frame();

		tty.clear();
		tty.println("FPS: {:.0f}", timer.fps());

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
		pos += timer.dt() * 10.f * glm::orientate3(-yaw) * v;

		float aspect = 1.f * w / h;
		float eye_level = 1.75f;
		glm::vec3 eye_pos = pos + glm::vec3{0.0f, 0.0f, eye_level};
		glm::mat4 m_view = glm::translate(glm::eulerAngleXZ(pitch, yaw), -eye_pos);
		glm::mat4 m_proj = glm::infinitePerspective(glm::radians(60.f), aspect, .1f);
		m_proj[2] = -m_proj[2];
		std::swap(m_proj[1], m_proj[2]);
		glm::mat4 m_render = m_proj * m_view;

		fn.Disable(GL_BLEND);
		fn.Enable(GL_DEPTH_TEST);
		fn.Enable(GL_CULL_FACE);
		fn.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		fn.UseProgram(prog);
		fn.UniformMatrix4fv(m_location, 1, GL_FALSE, &m_render[0][0]);
		fn.Uniform1i(t_location, 0);
		fn.BindTextureUnit(0, nodeTexture);
		fn.EnableVertexAttribArray(p_location);
		fn.EnableVertexAttribArray(c_location);
		fn.EnableVertexAttribArray(k_location);
		fn.EnableVertexAttribArray(u_location);
		map.tryGetMeshes(meshes, eye_pos, 200.f);
		for (Mesh const *mesh: meshes) {
			auto &buf = buffers[mesh];
			if (!buf) {
				fn.CreateBuffers(1, &buf);
				fn.NamedBufferStorage(buf, sizeof(Vertex) * mesh->vertices.size(), mesh->vertices.data(), 0);
			}
			fn.BindBuffer(GL_ARRAY_BUFFER, buf);
			fn.VertexAttribPointer(p_location, 3, GL_FLOAT, false, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, position)));
			fn.VertexAttribPointer(c_location, 4, GL_FLOAT, false, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, color)));
			fn.VertexAttribIPointer(k_location, 1, GL_UNSIGNED_INT, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, type)));
			fn.VertexAttribPointer(u_location, 2, GL_FLOAT, false, sizeof(Vertex), reinterpret_cast<void *>(offsetof(Vertex, uv)));
			fn.DrawArrays(GL_QUADS, 0, mesh->vertices.size());
		}

		fn.Disable(GL_DEPTH_TEST);
		fn.Enable(GL_BLEND);
		fn.BlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		tty.render();

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

	IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG);

// 	glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
// 	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
// 	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
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
