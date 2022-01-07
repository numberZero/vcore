#pragma once

template <int level>
struct Slice {
	static constexpr int size = MAP_BLOCKSIZE >> level;
	static constexpr int data_size = size * size;

	content_t face[data_size];

	static int index_unsafe(glm::ivec2 pos) noexcept {
		return pos.y + size * pos.x;
	}

	static int index(glm::ivec2 pos) {
		if (pos.x < 0 || pos.x >= size || pos.y < 0 || pos.y >= size)
			throw std::out_of_range("Face coordinates are out of slice");
		return index_unsafe(pos);
	}

	content_t get_r(glm::ivec2 pos) const {
		return face[index(pos)];
	}

	content_t &get_rw(glm::ivec2 pos) {
		return face[index(pos)];
	}
};

template <int h_level = 0, int v_level = h_level>
using SlicePack = std::array<Slice<h_level>, (MAP_BLOCKSIZE >> v_level)>;

template <int h_level = 0, int v_level = h_level>
struct SliceSet {
	using Pack = SlicePack<h_level, v_level>;
	Pack xn, xp, yn, yp, zn, zp;
};
