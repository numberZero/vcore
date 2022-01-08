/*
Minetest
Copyright (C) 2010-2018 celeron55, Perttu Ahola <celeron55@gmail.com>
Copyright (C) 2013-2018 kwolekr, Ryan Kwolek <kwolekr@minetest.net>
Copyright (C) 2014-2018 paramat
Copyright (C) 2019 numzero, Lobachevskiy Vitaliy <numzer0@yandex.ru>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "mapgen_v6.hxx"

#include <cmath>
#include "mapgen.hxx"
// #include "voxel.h"
#include "noise.hxx"
// #include "mapblock.h"
// #include "mapnode.h"
// #include "map.h"
//#include "serverobject.h"
// #include "content_sao.h"
// #include "nodedef.h"
// #include "voxelalgorithms.h"
//#include "profiler.h" // For TimeTaker
// #include "settings.h" // For g_settings
// #include "emerge.h"
// #include "dungeongen.h"
// #include "cavegen.h"
// #include "treegen.h"
// #include "mg_ore.h"
// #include "mg_decoration.h"

/*
FlagDesc flagdesc_mapgen_v6[] = {
	{"jungles",    MGV6_JUNGLES},
	{"biomeblend", MGV6_BIOMEBLEND},
	{"mudflow",    MGV6_MUDFLOW},
	{"snowbiomes", MGV6_SNOWBIOMES},
	{"flat",       MGV6_FLAT},
	{"trees",      MGV6_TREES},
	{NULL,         0}
};
*/

/////////////////////////////////////////////////////////////////////////////


MapgenV6::MapgenV6(MapgenV6Params *params, MapV6Params map_params)
	: Mapgen(MAPGEN_V6, params, nullptr)
{
	water_level = 1;

	mapgen_limit = params->mapgen_limit;
	flags = params->flags;
	csize = glm::ivec3(1, 1, 1) * (params->chunksize * MAP_BLOCKSIZE);

	seed = (s32)params->seed;

	ystride = csize.x;

	heightmap = new s16[csize.x * csize.z];

	spflags      = params->spflags;
	freq_desert  = params->freq_desert;
	freq_beach   = params->freq_beach;
	dungeon_ymin = params->dungeon_ymin;
	dungeon_ymax = params->dungeon_ymax;

	np_cave        = &params->np_cave;
	np_humidity    = &params->np_humidity;
	np_trees       = &params->np_trees;
	np_apple_trees = &params->np_apple_trees;

	np_dungeons = NoiseParams(0.9, 0.5, v3f(500.0, 500.0, 500.0), 0, 2, 0.8, 2.0);

	//// Create noise objects
	noise_terrain_base   = new Noise(&params->np_terrain_base,   seed, csize.x, csize.y);
	noise_terrain_higher = new Noise(&params->np_terrain_higher, seed, csize.x, csize.y);
	noise_steepness      = new Noise(&params->np_steepness,      seed, csize.x, csize.y);
	noise_height_select  = new Noise(&params->np_height_select,  seed, csize.x, csize.y);
	noise_mud            = new Noise(&params->np_mud,            seed, csize.x, csize.y);
	noise_beach          = new Noise(&params->np_beach,          seed, csize.x, csize.y);
	noise_biome          = new Noise(&params->np_biome,          seed,
			csize.x + 2 * MAP_BLOCKSIZE, csize.y + 2 * MAP_BLOCKSIZE);
	noise_humidity       = new Noise(&params->np_humidity,       seed,
			csize.x + 2 * MAP_BLOCKSIZE, csize.y + 2 * MAP_BLOCKSIZE);

	c = map_params;

	if (c.gravel == CONTENT_IGNORE)
		c.gravel = c.stone;
	if (c.desert_stone == CONTENT_IGNORE)
		c.desert_stone = c.stone;
	if (c.desert_sand == CONTENT_IGNORE)
		c.desert_sand = c.sand;
	if (c.dirt_with_snow == CONTENT_IGNORE)
		c.dirt_with_snow = c.dirt_with_grass;
	if (c.snow == CONTENT_IGNORE)
		c.snow = CONTENT_AIR;
	if (c.snowblock == CONTENT_IGNORE)
		c.snowblock = c.dirt_with_grass;
	if (c.ice == CONTENT_IGNORE)
		c.ice = c.water_source;
	if (c.mossycobble == CONTENT_IGNORE)
		c.mossycobble = c.cobble;
	if (c.stair_cobble == CONTENT_IGNORE)
		c.stair_cobble = c.cobble;
	if (c.stair_desert_stone == CONTENT_IGNORE)
		c.stair_desert_stone = c.desert_stone;
}


MapgenV6::~MapgenV6()
{
	delete noise_terrain_base;
	delete noise_terrain_higher;
	delete noise_steepness;
	delete noise_height_select;
	delete noise_mud;
	delete noise_beach;
	delete noise_biome;
	delete noise_humidity;

	delete[] heightmap;
}


MapgenV6Params::MapgenV6Params():
	np_terrain_base   (-4,   20.0, v3f(250.0, 250.0, 250.0), 82341,  5, 0.6,  2.0),
	np_terrain_higher (20,   16.0, v3f(500.0, 500.0, 500.0), 85039,  5, 0.6,  2.0),
	np_steepness      (0.85, 0.5,  v3f(125.0, 125.0, 125.0), -932,   5, 0.7,  2.0),
	np_height_select  (0,    1.0,  v3f(250.0, 250.0, 250.0), 4213,   5, 0.69, 2.0),
	np_mud            (4,    2.0,  v3f(200.0, 200.0, 200.0), 91013,  3, 0.55, 2.0),
	np_beach          (0,    1.0,  v3f(250.0, 250.0, 250.0), 59420,  3, 0.50, 2.0),
	np_biome          (0,    1.0,  v3f(500.0, 500.0, 500.0), 9130,   3, 0.50, 2.0),
	np_cave           (6,    6.0,  v3f(250.0, 250.0, 250.0), 34329,  3, 0.50, 2.0),
	np_humidity       (0.5,  0.5,  v3f(500.0, 500.0, 500.0), 72384,  3, 0.50, 2.0),
	np_trees          (0,    1.0,  v3f(125.0, 125.0, 125.0), 2,      4, 0.66, 2.0),
	np_apple_trees    (0,    1.0,  v3f(100.0, 100.0, 100.0), 342902, 3, 0.45, 2.0)
{
}

//////////////////////// Some helper functions for the map generator

// Returns y one under area minimum if not found
s16 MapgenV6::find_stone_level(v2s16 p2d)
{
	s16 y_nodes_max = vm->MaxEdge.y;
	s16 y_nodes_min = vm->MinEdge.y;
	s16 level = baseTerrainLevelFromMap(p2d);
	return glm::clamp<s16>(level, y_nodes_min - 1, y_nodes_max);
}


// Required by mapgen.h
bool MapgenV6::block_is_underground(u64 seed, v3s16 blockpos)
{
	/*s16 minimum_groundlevel = (s16)get_sector_minimum_ground_level(
			seed, v2s16(blockpos.x, blockpos.z));*/
	// Nah, this is just a heuristic, just return something
	s16 minimum_groundlevel = water_level;

	if(blockpos.y * MAP_BLOCKSIZE + MAP_BLOCKSIZE <= minimum_groundlevel)
		return true;

	return false;
}


//////////////////////// Base terrain height functions

float MapgenV6::baseTerrainLevel(float terrain_base, float terrain_higher,
	float steepness, float height_select)
{
	float base   = 1 + terrain_base;
	float higher = 1 + terrain_higher;

	// Limit higher ground level to at least base
	if(higher < base)
		higher = base;

	// Steepness factor of cliffs
	float b = steepness;
	b = glm::clamp(b, 0.0f, 1000.0f);
	b = 5 * b * b * b * b * b * b * b;
	b = glm::clamp(b, 0.5f, 1000.0f);

	// Values 1.5...100 give quite horrible looking slopes
	if (b > 1.5 && b < 100.0)
		b = (b < 10.0) ? 1.5 : 100.0;

	float a_off = -0.20; // Offset to more low
	float a = 0.5 + b * (a_off + height_select);
	a = glm::clamp(a, 0.0f, 1.0f); // Limit

	return base * (1.0 - a) + higher * a;
}


float MapgenV6::baseTerrainLevelFromNoise(v2s16 p)
{
	if (spflags & MGV6_FLAT)
		return water_level;

	float terrain_base   = NoisePerlin2D_PO(&noise_terrain_base->np,
							p.x, 0.5, p.y, 0.5, seed);
	float terrain_higher = NoisePerlin2D_PO(&noise_terrain_higher->np,
							p.x, 0.5, p.y, 0.5, seed);
	float steepness      = NoisePerlin2D_PO(&noise_steepness->np,
							p.x, 0.5, p.y, 0.5, seed);
	float height_select  = NoisePerlin2D_PO(&noise_height_select->np,
							p.x, 0.5, p.y, 0.5, seed);

	return baseTerrainLevel(terrain_base, terrain_higher,
							steepness, height_select);
}


float MapgenV6::baseTerrainLevelFromMap(v2s16 p)
{
	int index = (p.y - node_min.z) * ystride + (p.x - node_min.x);
	return baseTerrainLevelFromMap(index);
}


float MapgenV6::baseTerrainLevelFromMap(int index)
{
	if (spflags & MGV6_FLAT)
		return water_level;

	float terrain_base   = noise_terrain_base->result[index];
	float terrain_higher = noise_terrain_higher->result[index];
	float steepness      = noise_steepness->result[index];
	float height_select  = noise_height_select->result[index];

	return baseTerrainLevel(terrain_base, terrain_higher,
							steepness, height_select);
}


s16 MapgenV6::find_ground_level_from_noise(u64 seed, v2s16 p2d, s16 precision)
{
	return baseTerrainLevelFromNoise(p2d) + MGV6_AVERAGE_MUD_AMOUNT;
}


int MapgenV6::getGroundLevelAtPoint(v2s16 p)
{
	return baseTerrainLevelFromNoise(p) + MGV6_AVERAGE_MUD_AMOUNT;
}


int MapgenV6::getSpawnLevelAtPoint(v2s16 p)
{
	s16 level_at_point = baseTerrainLevelFromNoise(p) + MGV6_AVERAGE_MUD_AMOUNT;
	if (level_at_point <= water_level ||
			level_at_point > water_level + 16)
		return MAX_MAP_GENERATION_LIMIT;  // Unsuitable spawn point

	return level_at_point;
}


//////////////////////// Noise functions

float MapgenV6::getMudAmount(v2s16 p)
{
	int index = (p.y - node_min.z) * ystride + (p.x - node_min.x);
	return getMudAmount(index);
}


bool MapgenV6::getHaveBeach(v2s16 p)
{
	int index = (p.y - node_min.z) * ystride + (p.x - node_min.x);
	return getHaveBeach(index);
}


BiomeV6Type MapgenV6::getBiome(v2s16 p)
{
	int index = (p.y - full_node_min.z) * (ystride + 2 * MAP_BLOCKSIZE)
			+ (p.x - full_node_min.x);
	return getBiome(index, p);
}


float MapgenV6::getHumidity(v2s16 p)
{
	/*double noise = noise2d_perlin(
		0.5+(float)p.x/500, 0.5+(float)p.y/500,
		seed+72384, 4, 0.66);
	noise = (noise + 1.0)/2.0;*/

	int index = (p.y - full_node_min.z) * (ystride + 2 * MAP_BLOCKSIZE)
			+ (p.x - full_node_min.x);
	float noise = noise_humidity->result[index];

	if (noise < 0.0)
		noise = 0.0;
	if (noise > 1.0)
		noise = 1.0;
	return noise;
}


float MapgenV6::getTreeAmount(v2s16 p)
{
	/*double noise = noise2d_perlin(
			0.5+(float)p.x/125, 0.5+(float)p.y/125,
			seed+2, 4, 0.66);*/

	float noise = NoisePerlin2D(np_trees, p.x, p.y, seed);
	float zeroval = -0.39;
	if (noise < zeroval)
		return 0;

	return 0.04 * (noise - zeroval) / (1.0 - zeroval);
}


bool MapgenV6::getHaveAppleTree(v2s16 p)
{
	/*is_apple_tree = noise2d_perlin(
		0.5+(float)p.x/100, 0.5+(float)p.z/100,
		data->seed+342902, 3, 0.45) > 0.2;*/

	float noise = NoisePerlin2D(np_apple_trees, p.x, p.y, seed);

	return noise > 0.2;
}


float MapgenV6::getMudAmount(int index)
{
	if (spflags & MGV6_FLAT)
		return MGV6_AVERAGE_MUD_AMOUNT;

	/*return ((float)AVERAGE_MUD_AMOUNT + 2.0 * noise2d_perlin(
			0.5+(float)p.x/200, 0.5+(float)p.y/200,
			seed+91013, 3, 0.55));*/

	return noise_mud->result[index];
}


bool MapgenV6::getHaveBeach(int index)
{
	// Determine whether to have sand here
	/*double sandnoise = noise2d_perlin(
			0.2+(float)p2d.x/250, 0.7+(float)p2d.y/250,
			seed+59420, 3, 0.50);*/

	float sandnoise = noise_beach->result[index];
	return (sandnoise > freq_beach);
}


BiomeV6Type MapgenV6::getBiome(int index, v2s16 p)
{
	// Just do something very simple as for now
	/*double d = noise2d_perlin(
			0.6+(float)p2d.x/250, 0.2+(float)p2d.y/250,
			seed+9130, 3, 0.50);*/

	float d = noise_biome->result[index];
	float h = noise_humidity->result[index];

	if (spflags & MGV6_SNOWBIOMES) {
		float blend = (spflags & MGV6_BIOMEBLEND) ? noise2d(p.x, p.y, seed) / 40 : 0;

		if (d > MGV6_FREQ_HOT + blend) {
			if (h > MGV6_FREQ_JUNGLE + blend)
				return BT_JUNGLE;

			return BT_DESERT;
		}

		if (d < MGV6_FREQ_SNOW + blend) {
			if (h > MGV6_FREQ_TAIGA + blend)
				return BT_TAIGA;

			return BT_TUNDRA;
		}

		return BT_NORMAL;
	}

	if (d > freq_desert)
		return BT_DESERT;

	if ((spflags & MGV6_BIOMEBLEND) && (d > freq_desert - 0.10) &&
			((noise2d(p.x, p.y, seed) + 1.0) > (freq_desert - d) * 20.0))
		return BT_DESERT;

	if ((spflags & MGV6_JUNGLES) && h > 0.75)
		return BT_JUNGLE;

	return BT_NORMAL;

}


u32 MapgenV6::get_blockseed(u64 seed, v3s16 p)
{
	s32 x = p.x, y = p.y, z = p.z;
	return (u32)(seed % 0x100000000ULL) + z * 38134234 + y * 42123 + x * 23;
}


//////////////////////// Map generator

void MapgenV6::makeChunk(BlockMakeData *data)
{
	// Pre-conditions
	assert(data->vmanip);
// 	assert(data->nodedef);
// 	assert(data->blockpos_requested.x >= data->blockpos_min.x &&
// 		data->blockpos_requested.y >= data->blockpos_min.y &&
// 		data->blockpos_requested.z >= data->blockpos_min.z);
// 	assert(data->blockpos_requested.x <= data->blockpos_max.x &&
// 		data->blockpos_requested.y <= data->blockpos_max.y &&
// 		data->blockpos_requested.z <= data->blockpos_max.z);

	this->generating = true;
	this->vm   = data->vmanip;
// 	this->ndef = data->nodedef;

	// Hack: use minimum block coords for old code that assumes a single block
	v3s16 blockpos_min = data->blockpos_min;
	v3s16 blockpos_max = data->blockpos_max;

	// Area of central chunk
	node_min = blockpos_min * MAP_BLOCKSIZE;
	node_max = (blockpos_max + v3s16(1, 1, 1)) * MAP_BLOCKSIZE - v3s16(1, 1, 1);

	// Full allocated area
	full_node_min = node_min;
	full_node_max = node_max;
// 	full_node_min = (blockpos_min - 1) * MAP_BLOCKSIZE;
// 	full_node_max = (blockpos_max + 2) * MAP_BLOCKSIZE - v3s16(1, 1, 1);

	central_area_size = node_max - node_min + v3s16(1, 1, 1);
	assert(central_area_size.x == central_area_size.z);

	// Create a block-specific seed
	blockseed = get_blockseed(data->seed, full_node_min);

	// Make some noise
	calculateNoise();

	// Maximum height of the stone surface and obstacles.
	// This is used to guide the cave generation
	s16 stone_surface_max_y;

	// Generate general ground level to full area
	stone_surface_max_y = generateGround();

	// Create initial heightmap to limit caves
	updateHeightmap(node_min, node_max);

	const s16 max_spread_amount = MAP_BLOCKSIZE;
	// Limit dirt flow area by 1 because mud is flown into neighbors.
	s16 mudflow_minpos = -max_spread_amount + 1;
	s16 mudflow_maxpos = central_area_size.x + max_spread_amount - 2;

	// Loop this part, it will make stuff look older and newer nicely
	const u32 age_loops = 2;
	for (u32 i_age = 0; i_age < age_loops; i_age++) { // Aging loop
		// Make caves (this code is relatively horrible)
// 		if (flags & MG_CAVES)
// 			generateCaves(stone_surface_max_y);

		// Add mud to the central chunk
		addMud();

		// Flow mud away from steep edges
// 		if (spflags & MGV6_MUDFLOW)
// 			flowMud(mudflow_minpos, mudflow_maxpos);

	}

	// Update heightmap after mudflow
	updateHeightmap(node_min, node_max);

	// Add dungeons
	if ((flags & MG_DUNGEONS) && stone_surface_max_y >= node_min.y &&
			full_node_min.y >= dungeon_ymin && full_node_max.y <= dungeon_ymax) {
		u16 num_dungeons = std::fmax(std::floor(
			NoisePerlin3D(&np_dungeons, node_min.x, node_min.y, node_min.z, seed)), 0.0f);
/*
		if (num_dungeons >= 1) {
			DungeonParams dp;

			dp.seed             = seed;
			dp.only_in_ground   = true;
			dp.corridor_len_min = 1;
			dp.corridor_len_max = 13;
			dp.rooms_min        = 2;
			dp.rooms_max        = 16;

			dp.np_alt_wall
				= NoiseParams(-0.4, 1.0, v3f(40.0, 40.0, 40.0), 32474, 6, 1.1, 2.0);

			if (getBiome(0, v2s16(node_min.x, node_min.z)) == BT_DESERT) {
				dp.c.wall              = c.desert_stone;
				dp.c.alt_wall          = CONTENT_IGNORE;
				dp.c.stair             = c.stair_desert_stone;

				dp.diagonal_dirs       = true;
				dp.holesize            = v3s16(2, 3, 2);
				dp.room_size_min       = v3s16(6, 9, 6);
				dp.room_size_max       = v3s16(10, 11, 10);
				dp.room_size_large_min = v3s16(10, 13, 10);
				dp.room_size_large_max = v3s16(18, 21, 18);
				dp.notifytype          = GENNOTIFY_TEMPLE;
			} else {
				dp.c.wall              = c.cobble;
				dp.c.alt_wall          = c.mossycobble;
				dp.c.stair             = c.stair_cobble;

				dp.diagonal_dirs       = false;
				dp.holesize            = v3s16(1, 2, 1);
				dp.room_size_min       = v3s16(4, 4, 4);
				dp.room_size_max       = v3s16(8, 6, 8);
				dp.room_size_large_min = v3s16(8, 8, 8);
				dp.room_size_large_max = v3s16(16, 16, 16);
				dp.notifytype          = GENNOTIFY_DUNGEON;
			}

			DungeonGen dgen(ndef, &gennotify, &dp);
			dgen.generate(vm, blockseed, full_node_min, full_node_max, num_dungeons);
		}
*/
	}

	// Add top and bottom side of water to transforming_liquid queue
// 	updateLiquid(&data->transforming_liquid, full_node_min, full_node_max);

	// Add surface nodes
	growGrass();

	// Generate some trees, and add grass, if a jungle
// 	if (spflags & MGV6_TREES)
// 		placeTreesAndJungleGrass();

	// Generate the registered decorations
// 	if (flags & MG_DECORATIONS)
// 		m_emerge->decomgr->placeAllDecos(this, blockseed, node_min, node_max);

	// Generate the registered ores
// 	m_emerge->oremgr->placeAllOres(this, blockseed, node_min, node_max);

	// Calculate lighting
// 	if (flags & MG_LIGHT)
// 		calcLighting(node_min - v3s16(1, 1, 1) * MAP_BLOCKSIZE,
// 			node_max + v3s16(1, 0, 1) * MAP_BLOCKSIZE,
// 			full_node_min, full_node_max);

	this->generating = false;
}


void MapgenV6::calculateNoise()
{
	int x = node_min.x;
	int z = node_min.z;
	int fx = full_node_min.x;
	int fz = full_node_min.z;

	if (!(spflags & MGV6_FLAT)) {
		noise_terrain_base->perlinMap2D_PO(x, 0.5, z, 0.5);
		noise_terrain_higher->perlinMap2D_PO(x, 0.5, z, 0.5);
		noise_steepness->perlinMap2D_PO(x, 0.5, z, 0.5);
		noise_height_select->perlinMap2D_PO(x, 0.5, z, 0.5);
		noise_mud->perlinMap2D_PO(x, 0.5, z, 0.5);
	}

	noise_beach->perlinMap2D_PO(x, 0.2, z, 0.7);

	noise_biome->perlinMap2D_PO(fx, 0.6, fz, 0.2);
	noise_humidity->perlinMap2D_PO(fx, 0.0, fz, 0.0);
	// Humidity map does not need range limiting 0 to 1,
	// only humidity at point does
}


int MapgenV6::generateGround()
{
	//TimeTaker timer1("Generating ground level");
	MapNode n_air{CONTENT_AIR}, n_water_source{c.water_source};
	MapNode n_stone{c.stone}, n_desert_stone{c.desert_stone};
	MapNode n_ice{c.ice};
	int stone_surface_max_y = -MAX_MAP_GENERATION_LIMIT;

	u32 index = 0;
	for (s16 z = node_min.z; z <= node_max.z; z++)
	for (s16 x = node_min.x; x <= node_max.x; x++, index++) {
		// Surface height
		s16 surface_y = (s16)baseTerrainLevelFromMap(index);

		// Log it
		if (surface_y > stone_surface_max_y)
			stone_surface_max_y = surface_y;

		BiomeV6Type bt = getBiome(v2s16(x, z));

		// Fill ground with stone
		for (s16 y = node_min.y; y <= node_max.y; y++) {
			if (vm->get_r({x, y, z}).content == CONTENT_IGNORE) {
				Qube &q = vm->get_rw({x, y, z});
				if (y <= surface_y)
					q = (y >= MGV6_DESERT_STONE_BASE && bt == BT_DESERT) ? n_desert_stone : n_stone;
				else if (y <= water_level)
					q = (y >= MGV6_ICE_BASE && bt == BT_TUNDRA) ? n_ice : n_water_source;
				else
					q = n_air;
			}
		}
	}

	return stone_surface_max_y;
}


void MapgenV6::addMud()
{
	// 15ms @cs=8
	//TimeTaker timer1("add mud");
	MapNode n_dirt{c.dirt}, n_gravel{c.gravel};
	MapNode n_sand{c.sand}, n_desert_sand{c.desert_sand};
	MapNode addnode;

	u32 index = 0;
	for (s16 z = node_min.z; z <= node_max.z; z++)
	for (s16 x = node_min.x; x <= node_max.x; x++, index++) {
		// Randomize mud amount
		s16 mud_add_amount = getMudAmount(index) / 2.0 + 0.5;

		// Find ground level
		s16 surface_y = find_stone_level(v2s16(x, z));

		// Handle area not found
		if (surface_y == vm->MinEdge.y - 1)
			continue;

		BiomeV6Type bt = getBiome(v2s16(x, z));
		addnode = (bt == BT_DESERT) ? n_desert_sand : n_dirt;

		if (bt == BT_DESERT && surface_y + mud_add_amount <= water_level + 1) {
			addnode = n_sand;
		} else if (mud_add_amount <= 0) {
			mud_add_amount = 1 - mud_add_amount;
			addnode = n_gravel;
		} else if (bt != BT_DESERT && getHaveBeach(index) &&
				surface_y + mud_add_amount <= water_level + 2) {
			addnode = n_sand;
		}

		if ((bt == BT_DESERT || bt == BT_TUNDRA) && surface_y > 20)
			mud_add_amount = std::max(0, mud_add_amount - (surface_y - 20) / 5);

		/* If topmost node is grass, change it to mud.  It might be if it was
		// flown to there from a neighboring chunk and then converted.
		u32 i = vm->m_area.index(x, surface_y, z);
		if (vm->m_data[i].getContent() == c.dirt_with_grass)
			vm->m_data[i] = n_dirt;*/

		// Add mud on ground
		s16 mudcount = 0;
		s16 y_start = surface_y + 1;
		for (s16 y = y_start; y <= node_max.y; y++) {
			if (mudcount >= mud_add_amount)
				break;
			vm->get_rw({x, y, z}) = addnode;
			mudcount++;
		}
	}
}


void MapgenV6::flowMud(s16 &mudflow_minpos, s16 &mudflow_maxpos)
{
	// 340ms @cs=8
	//TimeTaker timer1("flow mud");

	// Iterate a few times
	for (s16 k = 0; k < 3; k++) {
		for (s16 z = mudflow_minpos; z <= mudflow_maxpos; z++)
		for (s16 x = mudflow_minpos; x <= mudflow_maxpos; x++) {
			v2s16 p2d = v2s16(node_min.x, node_min.z);
			// Invert coordinates every 2nd iteration
			if (k % 2 == 0) {
				p2d += mudflow_maxpos + mudflow_minpos;
				p2d -= v2s16(x, z);
			} else {
				p2d += v2s16(x, z);
			}

			for (s16 y = node_max.y; y >= node_min.y; y--) {
				content_t q = CONTENT_IGNORE;
				// Find mud
				for (; y >= node_min.y; y--) {
					q = vm->get_r({p2d.x, y, p2d.y}).content;
					if (q == c.dirt || q == c.dirt_with_grass || q == c.gravel)
						break;
				}

				// Stop if out of area
				if (y < node_min.y)
					break;

				if (q == c.dirt || q == c.dirt_with_grass) {
					// Make it exactly mud
					if (q != c.dirt)
						vm->get_rw({p2d.x, y, p2d.y}).content = c.dirt;

					// Don't flow it if the stuff under it is not mud
					glm::ivec3 p2{p2d.x, y - 1, p2d.y};
					// Cancel if out of area
					if (p2.y < node_min.y)
						continue;
					MapNode const &n2 = vm->get_r(p2);
					if (n2.content != c.dirt && n2.content != c.dirt_with_grass)
						continue;
				}

				static const v3s16 dirs4[4] = {
					v3s16(0, 0, 1), // back
					v3s16(1, 0, 0), // right
					v3s16(0, 0, -1), // front
					v3s16(-1, 0, 0), // left
				};

				// Check that upper is walkable. Cancel
				// dropping if upper keeps it in place.
				glm::vec3 p3{p2d.x, y + 1, p2d.y};
				if (is_walkable(vm->get_ign(p3).content))
					continue;

				// Drop mud on side
				for (const v3s16 &dirp : dirs4) { // NOTE: order is fixed, is that intended?
					glm::ivec3 p2 = {p2d.x, y, p2d.y};
					// Move to side
					p2 += dirp;
					// Fail if out of area
					if (!vm->in_area(p2))
						continue;
					// Check that side is air
					MapNode n2 = vm->get_r(p2);
					if (is_walkable(n2.content))
						continue;
					// Check that under side is air
					p2.y--;
					if (!vm->in_area(p2))
						continue;
					n2 = vm->get_r(p2);
					if (is_walkable(n2.content))
						continue;
					// Loop further down until not air
					bool dropped_to_unknown = false;
					do {
						p2.y--;
						n2 = vm->get_ign(p2);
						if (n2.content == CONTENT_IGNORE) {
							dropped_to_unknown = true;
							break;
						}
					} while (!is_walkable(n2.content));
					// Loop one up so that we're in air
					p2.y++;

					// Move mud to new place. Outside mapchunk remove
					// any decorations above removed or placed mud.
					if (!dropped_to_unknown)
						moveMud({p2d.x, y, p2d.y}, p2, p3, p2d);

					// Done
					break;
				}
			}
		}
	}
}


void MapgenV6::moveMud(v3s16 remove_index, v3s16 place_index,
	v3s16 above_remove_index, v2s16 pos)
{
	MapNode n_air{CONTENT_AIR};
	// Copy mud from old place to new place
	vm->get_rw(place_index) = vm->get_r(remove_index);
	// Set old place to be air
	vm->get_rw(remove_index) = n_air;
	// Outside the mapchunk decorations may need to be removed if above removed
	// mud or if half-buried in placed mud. Placed mud is to the side of pos so
	// use 'pos.x >= node_max.x' etc.
	if (pos.x >= node_max.x || pos.x <= node_min.x ||
			pos.y >= node_max.z || pos.y <= node_min.z) {
		// 'above remove' node is above removed mud. If it is not air, water or
		// 'ignore' it is a decoration that needs removing. Also search upwards
		// to remove a possible stacked decoration.
		// Check for 'ignore' because stacked decorations can penetrate into
		// 'ignore' nodes above the mapchunk.
		for (;;) {
			content_t above_remove = vm->get_ign(above_remove_index).content;
			if (above_remove == CONTENT_IGNORE || above_remove == CONTENT_AIR || above_remove == c.water_source)
				break;
			vm->get_rw(above_remove_index) = n_air;
			above_remove_index.y++;
		}
		// Mud placed may have partially-buried a stacked decoration, search
		// above and remove.
		for (;;) {
			place_index.y++;
			content_t place = vm->get_ign(place_index).content;
			if (place == CONTENT_IGNORE || place == CONTENT_AIR || place == c.water_source)
				break;
			vm->get_rw(place_index) = n_air;
		}
	}
}

/*
void MapgenV6::placeTreesAndJungleGrass()
{
	//TimeTaker t("placeTrees");
	if (node_max.y < water_level)
		return;

	PseudoRandom grassrandom(blockseed + 53);
	content_t c.junglegrass = ndef->getId("mapgen_junglegrass");
	// if we don't have junglegrass, don't place cignore... that's bad
	if (c.junglegrass == CONTENT_IGNORE)
		c.junglegrass = CONTENT_AIR;
	MapNode n_junglegrass{c.junglegrass};
	const v3s16 &em = vm->m_area.getExtent();

	// Divide area into parts
	s16 div = 8;
	s16 sidelen = central_area_size.x / div;
	double area = sidelen * sidelen;

	// N.B.  We must add jungle grass first, since tree leaves will
	// obstruct the ground, giving us a false ground level
	for (s16 z0 = 0; z0 < div; z0++)
	for (s16 x0 = 0; x0 < div; x0++) {
		// Center position of part of division
		v2s16 p2d_center(
			node_min.x + sidelen / 2 + sidelen * x0,
			node_min.z + sidelen / 2 + sidelen * z0
		);
		// Minimum edge of part of division
		v2s16 p2d_min(
			node_min.x + sidelen * x0,
			node_min.z + sidelen * z0
		);
		// Maximum edge of part of division
		v2s16 p2d_max(
			node_min.x + sidelen + sidelen * x0 - 1,
			node_min.z + sidelen + sidelen * z0 - 1
		);

		// Get biome at center position of part of division
		BiomeV6Type bt = getBiome(p2d_center);

		// Amount of trees
		u32 tree_count;
		if (bt == BT_JUNGLE || bt == BT_TAIGA || bt == BT_NORMAL) {
			tree_count = area * getTreeAmount(p2d_center);
			if (bt == BT_JUNGLE)
				tree_count *= 4;
		} else {
			tree_count = 0;
		}

		// Add jungle grass
		if (bt == BT_JUNGLE) {
			float humidity = getHumidity(p2d_center);
			u32 grass_count = 5 * humidity * tree_count;
			for (u32 i = 0; i < grass_count; i++) {
				s16 x = grassrandom.range(p2d_min.x, p2d_max.x);
				s16 z = grassrandom.range(p2d_min.y, p2d_max.y);
				int mapindex = central_area_size.x * (z - node_min.z)
								+ (x - node_min.x);
				s16 y = heightmap[mapindex];
				if (y < water_level)
					continue;

				u32 vi = vm->m_area.index(x, y, z);
				// place on dirt_with_grass, since we know it is exposed to sunlight
				if (vm->m_data[vi].getContent() == c.dirt_with_grass) {
					VoxelArea::add_y(em, vi, 1);
					vm->m_data[vi] = n_junglegrass;
				}
			}
		}

		// Put trees in random places on part of division
		for (u32 i = 0; i < tree_count; i++) {
			s16 x = myrand_range(p2d_min.x, p2d_max.x);
			s16 z = myrand_range(p2d_min.y, p2d_max.y);
			int mapindex = central_area_size.x * (z - node_min.z)
							+ (x - node_min.x);
			s16 y = heightmap[mapindex];
			// Don't make a tree under water level
			// Don't make a tree so high that it doesn't fit
			if (y < water_level || y > node_max.y - 6)
				continue;

			v3s16 p(x, y, z);
			// Trees grow only on mud and grass
			{
				u32 i = vm->m_area.index(p);
				content_t c = vm->m_data[i].getContent();
				if (c != c.dirt &&
						c != c.dirt_with_grass &&
						c != c.dirt_with_snow)
					continue;
			}
			p.y++;

			// Make a tree
			if (bt == BT_JUNGLE) {
				treegen::make_jungletree(*vm, p, ndef, myrand());
			} else if (bt == BT_TAIGA) {
				treegen::make_pine_tree(*vm, p - v3s16(0, 1, 0), ndef, myrand());
			} else if (bt == BT_NORMAL) {
				bool is_apple_tree = (myrand_range(0, 3) == 0) &&
							getHaveAppleTree(v2s16(x, z));
				treegen::make_tree(*vm, p, is_apple_tree, ndef, myrand());
			}
		}
	}
	//printf("placeTreesAndJungleGrass: %dms\n", t.stop());
}
*/
void MapgenV6::growGrass() // Add surface nodes
{
	MapNode n_dirt_with_grass{c.dirt_with_grass};
	MapNode n_dirt_with_snow{c.dirt_with_snow};
	MapNode n_snowblock{c.snowblock};
	MapNode n_snow{c.snow};

	u32 index = 0;
	for (s16 z = full_node_min.z; z <= full_node_max.z; z++)
	for (s16 x = full_node_min.x; x <= full_node_max.x; x++, index++) {
		// Find the lowest surface to which enough light ends up to make
		// grass grow.  Basically just wait until not air and not leaves.
		s16 surface_y = 0;
		{
			s16 y;
			// Go to ground level
			for (y = node_max.y; y >= full_node_min.y; y--) {
				MapNode n = vm->get_r({x, y, z});
				if (n.content != CONTENT_AIR) // FIXME: non-full-node decorations?
					break;
// 				if (ndef->get(n).param_type != CPT_LIGHT ||
// 						ndef->get(n).liquid_type != LIQUID_NONE ||
// 						n.getContent() == c.ice)
// 					break;
			}
			surface_y = (y >= full_node_min.y) ? y : full_node_min.y;
		}

		BiomeV6Type bt = getBiome(index, v2s16(x, z));
		glm::ivec3 pos = {x, surface_y, z};
		content_t n = vm->get_r(pos).content;
		if (surface_y >= water_level - 20) {
			if (bt == BT_TAIGA && n == c.dirt) {
				vm->get_rw(pos) = n_dirt_with_snow;
			} else if (bt == BT_TUNDRA) {
				if (n == c.dirt) {
					vm->get_rw(pos) = n_snowblock;
					pos.y--;
					vm->get_rw(pos) = n_dirt_with_snow;
				} else if (n == c.stone && surface_y < node_max.y) {
					pos.y++;
					vm->get_rw(pos) = n_snowblock;
				}
			} else if (n == c.dirt) {
				vm->get_rw(pos) = n_dirt_with_grass;
			}
		}
	}
}
/*
void MapgenV6::generateCaves(int max_stone_y)
{
	float cave_amount = NoisePerlin2D(np_cave, node_min.x, node_min.y, seed);
	int volume_nodes = (node_max.x - node_min.x + 1) *
					   (node_max.y - node_min.y + 1) * MAP_BLOCKSIZE;
	cave_amount = MYMAX(0.0, cave_amount);
	u32 caves_count = cave_amount * volume_nodes / 50000;
	u32 bruises_count = 1;
	PseudoRandom ps(blockseed + 21343);
	PseudoRandom ps2(blockseed + 1032);

	if (ps.range(1, 6) == 1)
		bruises_count = ps.range(0, ps.range(0, 2));

	if (getBiome(v2s16(node_min.x, node_min.z)) == BT_DESERT) {
		caves_count   /= 3;
		bruises_count /= 3;
	}

	for (u32 i = 0; i < caves_count + bruises_count; i++) {
		CavesV6 cave(ndef, &gennotify, water_level, c.water_source, c.lava_source);

		bool large_cave = (i >= caves_count);
		cave.makeCave(vm, node_min, node_max, &ps, &ps2,
			large_cave, max_stone_y, heightmap);
	}
}
*/
