#pragma once
#include <array>
#include <vector>
#include <glm/glm.hpp>

class PerlinNoise3d
{
	int const cells[3];
	int const nodes[3];
	std::vector<glm::vec3> base_points;

	glm::vec3 &point(int x, int y, int z);
	glm::vec3 const &point(int x, int y, int z) const;
	float partial(std::array<int, 3> cell, std::array<float, 3> sub, std::array<bool, 3> mirror) const;

public:
	PerlinNoise3d(int size_x, int size_y, int size_z);
	void generate(unsigned seed);
	float get(float x, float y, float z) const;
	float get(glm::vec3 v) const;
};

class FractalNoise2d
{
	std::vector<PerlinNoise3d> noise;
public:
	int const octaves;
	float persistence;
	float const scale;

	FractalNoise2d(float size_x, float size_y, float large_cell_size, int octaves);
	void generate(int seed);
	float get(float x, float y) const;
	float get(glm::vec2 v) const;
};
