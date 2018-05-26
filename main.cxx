#include <cstdio>
#include <cstdlib>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "map/manager.hxx"
#include "mapgen/heightmap.hxx"

int main(int argc, char **argv) {
	int result = EXIT_FAILURE;
	GLFWwindow *window;
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
	if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
		fprintf(stderr, "Can't load OpenGL extensions");
		goto err_after_window;
	}

	glClearColor(0.2, 0.1, 0.3, 1.0);

	while (!glfwWindowShouldClose(window)) {
		glClear(GL_COLOR_BUFFER_BIT);
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	result = EXIT_SUCCESS;
err_after_window:
	glfwDestroyWindow(window);
err_after_glfw:
	glfwTerminate();
err_early:
	return result;
}
