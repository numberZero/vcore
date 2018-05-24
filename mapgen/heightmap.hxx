#pragma once
#include <functional>
#include "interface.hxx"

/// Function returning terrain height at given coordinates. All values are in qubes here.
using HeightmapFn = std::function<long(long x, long y)>;

/// Basic map generator. Solidifies a height map (in form of @c HeightmapFn).
/// @note Stateless (and thus thread-safe).
class HeightmapMapgen: public IMapgen {
public:
	QubeType const air = 0;
	QubeType const solid = 1;
	HeightmapFn const h;

	/// Creates a map generator.
	/// @param _h Should return the height map, point by point.
	/// @param _solid Qube to use to fill the terrain body. Is placed everywhere
	/// below the level @p _h returns.
	HeightmapMapgen(HeightmapFn _h, QubeType _solid = 1);
	Block generate(BlockPosition block_pos) override;
private:
};
