#pragma once
#include <cstdint>
#include <vector>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>

struct Vertex {
	glm::vec3 position;
	float brightness;
	glm::vec3 color;
	std::uint32_t type;
	glm::vec2 uv;
};

using Index = std::uint16_t;

struct Mesh {
	std::vector<Vertex> vertices;
};
