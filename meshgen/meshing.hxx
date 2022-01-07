#pragma once
#include <mesh.hxx>
#include "slices.hxx"
#include "swizzle.hxx"

template <int level, glm::ivec3 transform(glm::ivec2)>
void slice_to_mesh(std::vector<Vertex> &dest, Slice<level> const &slice, glm::ivec3 base, float brightness = 1.0f) {
	extern std::array<glm::vec3, 18> const content_colors;
	int scale = 1 << level;
	float inv_scale = 1.0f / slice.size * block_size / 16.0f;
	for (int j = 0; j < slice.size; j++)
	for (int i = 0; i < slice.size; i++) {
		content_t self = slice.get_r({i, j});
		if (self == CONTENT_IGNORE)
			continue;
		auto color = brightness * content_colors.at(self);
		auto make_vertex = [&] (int u, int v) {
			auto ipos = glm::ivec2{i + u, j + v};
			auto uv = inv_scale * glm::vec2(ipos);
			dest.push_back({base + scale * transform(ipos), self, color, brightness, uv});
		};
		make_vertex(0, 0);
		make_vertex(1, 0);
		make_vertex(1, 1);
		make_vertex(0, 1);
	}
}

template <int h_level, int v_level>
auto make_mesh(SliceSet<h_level, v_level> const &slices, glm::ivec3 offset) {
	auto result = std::make_unique<Mesh>();
	result->vertices.reserve(4 * 3 * block_size * block_size * (block_size + 1));
	for (int index = 0; index < MAP_BLOCKSIZE >> v_level; index++) {
		int op = (index + 1) << v_level;
		int on = MAP_BLOCKSIZE - op;
		slice_to_mesh<h_level, unpack_xn>(result->vertices, slices.xn.at(index), offset + glm::ivec3{on, 0, 0}, 0.8f);
		slice_to_mesh<h_level, unpack_xp>(result->vertices, slices.xp.at(index), offset + glm::ivec3{op, 0, 0}, 0.8f);
		slice_to_mesh<h_level, unpack_yn>(result->vertices, slices.yn.at(index), offset + glm::ivec3{0, on, 0}, 0.9f);
		slice_to_mesh<h_level, unpack_yp>(result->vertices, slices.yp.at(index), offset + glm::ivec3{0, op, 0}, 0.7f);
		slice_to_mesh<h_level, unpack_zn>(result->vertices, slices.zn.at(index), offset + glm::ivec3{0, 0, on}, 0.5f);
		slice_to_mesh<h_level, unpack_zp>(result->vertices, slices.zp.at(index), offset + glm::ivec3{0, 0, op}, 1.0f);
	}
	result->vertices.shrink_to_fit();
	return std::move(result);
}
