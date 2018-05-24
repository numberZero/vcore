#include "heightmap.hxx"
#include <cstring>

HeightmapMapgen::HeightmapMapgen(HeightmapFn _f, QubeType _solid) :
	f(_f), solid(_solid)
{
}

Block HeightmapMapgen::generate(BlockPosition block_pos)
{
	Block block;
	std::memset(&block, 0, sizeof(block));
	for (int i = 0; i < block_size; i++)
		for (int j = 0; j < block_size; j++) {
			int x = block_size * block_pos.x + i;
			int y = block_size * block_pos.y + j;
			int z_bottom = block_size * block_pos.z;
			int z_top = f(x, y);
			int h = z_top - z_bottom;
			if (h <= 0)
				continue;
			if (h > block_size)
				h = block_size;
			for (int k = 0; k < h; k++)
				block.qube[Block::index(i, j, k)] = solid;
		}
	return block;
}
