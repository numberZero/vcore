#include <cstdio>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "map/manager.hxx"
#include "mapgen/heightmap.hxx"

int main(int argc, char **argv) {
	GLFWwindow* window;
	if (!glfwInit()) {
		fprintf(stderr, "Can't initialize GLFW");
		return -1;
	}

	window = glfwCreateWindow(800, 600, "V_CORE", NULL, NULL);
	if (!window) {
		fprintf(stderr, "Can't create window");
		glfwTerminate();
		return -1;
	}

	glfwMakeContextCurrent(window);
	gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);

	glClearColor(0.2, 0.1, 0.3, 1.0);

	while (!glfwWindowShouldClose(window)) {
		glClear(GL_COLOR_BUFFER_BIT);
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}
