#pragma once
#include "tty.hxx"
#include <glm/vec4.hpp>

class GLTTY: public TTY {
public:
	void init();
	void render();

	glm::vec4 color = {0.0, 1.0, 0.0, 1.0};

private:
	unsigned prog;
	unsigned font;
	unsigned data_texture;
};
