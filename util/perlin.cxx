#include "perlin.hxx"
#include <cmath>
#include <array>
#include <random>

constexpr float smoothstep(float x)
{
	return (3.0f - 2.0f * x) * x * x;
}

glm::vec3 &PerlinNoise3d::point(int x, int y, int z)
{
	return base_points[x + nodes[0] * (y + nodes[1] * z)];
}

glm::vec3 const &PerlinNoise3d::point(int x, int y, int z) const
{
	return base_points[x + nodes[0] * (y + nodes[1] * z)];
}

float PerlinNoise3d::partial(std::array<int, 3> cell, std::array<float, 3> sub, std::array<bool, 3> mirror) const
{
	for (int k = 0; k < 3; ++k)
		if (mirror[k]) {
			cell[k] += 1;
			sub[k] -= 1.0f;
		}
	float coef = 1.0f;
	for (int k = 0; k < 3; ++k)
		coef *= smoothstep(1.0f - std::fabs(sub[k]));
	glm::vec3 base = point(cell[0], cell[1], cell[2]);
	glm::vec3 shift {sub[0], sub[1], sub[2]};
	return coef * glm::dot(shift, base);
}

PerlinNoise3d::PerlinNoise3d(int size_x, int size_y, int size_z) :
		cells{size_x, size_y, size_z},
		nodes{size_x + 1, size_y + 1, size_z + 1}
{
	base_points.resize(nodes[0] * nodes[1] * nodes[2]);
}

void PerlinNoise3d::generate(unsigned seed)
{
	std::mt19937 prng(seed);
	std::uniform_real_distribution<float> gen(-1.0f, 1.0f);
	unsigned seed2 = prng();
	for (int x = 0; x < nodes[0]; ++x)
		for (int y = 0; y < nodes[1]; ++y) {
			prng.seed(seed2 + x << 16 + y);
			for (int z = 0; z < nodes[2]; ++z)
				for(;;) {
					glm::vec3 vec {gen(prng), gen(prng), gen(prng)};
					float len2 = glm::dot(vec, vec);
					if ((len2 > 1.0f) || (len2 < 0.01f))
						continue;
					float c = 1.0f / std::sqrt(len2);
					point(x, y, z) = c * vec;
					break;
				}
		}
}

float PerlinNoise3d::get(float x, float y, float z) const
{
	std::array<float, 3> pos {x, y, z};
	std::array<int, 3> cell;
	std::array<float, 3> sub;
	for (int k = 0; k < 3; ++k) {
		cell[k] = int(pos[k]);
		if((cell[k] < 0) || (cell[k] >= cells[k]))
			throw std::range_error("Noise coordinates out of range");
		sub[k] = pos[k] - cell[k];
	}
	float value = 0;
	for (int k = 0; k < 8; ++k)
		value += partial(cell, sub, {k & 1, k & 2, k & 4});
	return value;
}

float PerlinNoise3d::get(glm::vec3 v) const
{
	return get(v.x, v.y, v.z);
}

FractalNoise2d::FractalNoise2d(float size_x, float size_y, float large_cell_size, int octaves) :
		octaves(octaves), persistence(0.5), scale(1.0 / large_cell_size)
{
	int int_size_x = int(std::ceil(size_x / large_cell_size));
	int int_size_y = int(std::ceil(size_y / large_cell_size));
	noise.reserve(octaves);
	for (int k = 0; k < octaves; ++k)
		noise.emplace_back(int_size_x << k, int_size_y << k, 2);
}

void FractalNoise2d::generate(int seed)
{
	for (int k = 0; k < octaves; ++k)
		noise[k].generate(seed + k);
}

float FractalNoise2d::get(float x, float y) const
{
	float value = 0.0f;
	float coef = 1.0f;
	for (int k = 0; k < octaves; ++k) {
		int scale2 = 1 << k;
		value += coef * noise[k].get(scale2 * scale * x, scale2 * scale * y, 0.3f);
		coef *= persistence;
	}
	return value;
}

float FractalNoise2d::get(glm::vec2 v) const
{
	return get(v.x, v.y);
}
