#pragma once
#include <utility>
#include <glm/vec3.hpp>

inline static std::pair<long, unsigned> divrem(long num, unsigned den) {
	if (num >= 0)
		return {num / den, num % den};
	else
		return {(num + 1) / den - 1, (num + 1) % den + den - 1};
}

struct space_range {
	glm::ivec3 const a;
	glm::ivec3 const b;
};

struct space_iterator {
	space_range const &range;
	glm::ivec3 p;

	space_iterator &operator++ () {
		if (++p.x != range.b.x)
			return *this;
		p.x = range.a.x;
		if (++p.y != range.b.y)
			return *this;
		p.y = range.a.y;
		if (++p.z != range.b.z)
			return *this;
		p = range.b;
		return *this;
	}

	glm::ivec3 operator* () const {
		return p;
	}

	bool operator== (space_iterator const &b) {
		assert(&range == &b.range);
		return p == b.p;
	}

	bool operator!= (space_iterator const &b) {
		assert(&range == &b.range);
		return p != b.p;
	}
};

inline static space_iterator begin(space_range const &range) {
	return {range, range.a};
}

inline static space_iterator end(space_range const &range) {
	return {range, range.b};
}
