#pragma once
#include <random>
#include "slices.hxx"
#include "swizzle.hxx"

SliceSet<> make_slices(VManip const &mapfrag, glm::ivec3 blockpos) {
	SliceSet<> result;
	std::memset(&result, -1, sizeof(result));
	glm::ivec3 base = 16 * blockpos;
	for (glm::ivec3 rel: space_range{glm::ivec3{0, 0, 0}, glm::ivec3{16, 16, 16}}) {
		glm::ivec3 pos = base + rel;
		content_t self = mapfrag.get(pos).content;
		assert(self != CONTENT_IGNORE);
		if (self == CONTENT_AIR)
			continue;
		if (mapfrag.get(pos + glm::ivec3{-1, 0, 0}).content == CONTENT_AIR) result.xn.at(15 - rel.x).get_rw(pack_xn(rel)) = self;
		if (mapfrag.get(pos + glm::ivec3{ 1, 0, 0}).content == CONTENT_AIR) result.xp.at(rel.x).get_rw(pack_xp(rel)) = self;
		if (mapfrag.get(pos + glm::ivec3{0, -1, 0}).content == CONTENT_AIR) result.yn.at(15 - rel.y).get_rw(pack_yn(rel)) = self;
		if (mapfrag.get(pos + glm::ivec3{0,  1, 0}).content == CONTENT_AIR) result.yp.at(rel.y).get_rw(pack_yp(rel)) = self;
		if (mapfrag.get(pos + glm::ivec3{0, 0, -1}).content == CONTENT_AIR) result.zn.at(15 - rel.z).get_rw(pack_zn(rel)) = self;
		if (mapfrag.get(pos + glm::ivec3{0, 0,  1}).content == CONTENT_AIR) result.zp.at(rel.z).get_rw(pack_zp(rel)) = self;
	}
	return result;
}

template <int h_level>
Slice<h_level> merge_slices(Slice<h_level> const &bottom, Slice<h_level> const &top) {
	Slice<h_level> result;
	for (int k = 0; k < result.data_size; k++) {
		content_t t = top.face[k];
		content_t b = bottom.face[k];
		result.face[k] = t == CONTENT_IGNORE ? b : t;
	}
	return result;
}

template <int h_level, int v_level>
auto flatten_slices(SlicePack<h_level, v_level> const &slices) {
	SlicePack<h_level, v_level + 1> result;
	for (int k = 0; k < result.size(); k++)
		result[k] = merge_slices(slices[2 * k], slices[2 * k + 1]);
	return result;
}

template <int h_level, int v_level>
auto flatten_slices(SliceSet<h_level, v_level> const &slices) {
	SliceSet<h_level, v_level + 1> result;
	result.xn = flatten_slices<h_level, v_level>(slices.xn);
	result.xp = flatten_slices<h_level, v_level>(slices.xp);
	result.yn = flatten_slices<h_level, v_level>(slices.yn);
	result.yp = flatten_slices<h_level, v_level>(slices.yp);
	result.zn = flatten_slices<h_level, v_level>(slices.zn);
	result.zp = flatten_slices<h_level, v_level>(slices.zp);
	return result;
}

template <int h_level>
Slice<h_level + 1> hmerge_slice(Slice<h_level> const &slice) {
	static std::mt19937 rnd(std::time(nullptr));
	Slice<h_level + 1> result;
	std::vector<content_t> v;
	v.reserve(4);
	for (int j = 0; j < result.size; j++)
	for (int i = 0; i < result.size; i++) {
		v.clear();
		content_t a = slice.get_r({2 * i    , 2 * j    });
		content_t b = slice.get_r({2 * i    , 2 * j + 1});
		content_t c = slice.get_r({2 * i + 1, 2 * j    });
		content_t d = slice.get_r({2 * i + 1, 2 * j + 1});
		if (a != CONTENT_IGNORE) v.push_back(a);
		if (b != CONTENT_IGNORE) v.push_back(b);
		if (c != CONTENT_IGNORE) v.push_back(c);
		if (d != CONTENT_IGNORE) v.push_back(d);
		content_t r = CONTENT_IGNORE;
		if (!v.empty()) {
			std::uniform_int_distribution<std::size_t> dist{0, v.size() - 1};
			r = v.at(dist(rnd));
		}
		result.get_rw({i, j}) = r;
	}
	return result;
}

template <int h_level, int v_level>
auto hmerge_slices(SlicePack<h_level, v_level> const &slices) {
	SlicePack<h_level + 1, v_level> result;
	for (int k = 0; k < result.size(); k++)
		result[k] = hmerge_slice(slices[k]);
	return result;
}

template <int h_level, int v_level>
auto hmerge_slices(SliceSet<h_level, v_level> const &slices) {
	SliceSet<h_level + 1, v_level> result;
	result.xn = hmerge_slices<h_level, v_level>(slices.xn);
	result.xp = hmerge_slices<h_level, v_level>(slices.xp);
	result.yn = hmerge_slices<h_level, v_level>(slices.yn);
	result.yp = hmerge_slices<h_level, v_level>(slices.yp);
	result.zn = hmerge_slices<h_level, v_level>(slices.zn);
	result.zp = hmerge_slices<h_level, v_level>(slices.zp);
	return result;
}
