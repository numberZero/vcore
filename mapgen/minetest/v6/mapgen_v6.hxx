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

#pragma once

#include "mapgen.hxx"
#include "noise.hxx"

#define MGV6_AVERAGE_MUD_AMOUNT 4
#define MGV6_DESERT_STONE_BASE -32
#define MGV6_ICE_BASE 0
#define MGV6_FREQ_HOT 0.4
#define MGV6_FREQ_SNOW -0.4
#define MGV6_FREQ_TAIGA 0.5
#define MGV6_FREQ_JUNGLE 0.5

//////////// Mapgen V6 flags
#define MGV6_JUNGLES    0x01
#define MGV6_BIOMEBLEND 0x02
#define MGV6_MUDFLOW    0x04
#define MGV6_SNOWBIOMES 0x08
#define MGV6_FLAT       0x10
#define MGV6_TREES      0x20

/*
extern FlagDesc flagdesc_mapgen_v6[];
*/

enum BiomeV6Type
{
	BT_NORMAL,
	BT_DESERT,
	BT_JUNGLE,
	BT_TUNDRA,
	BT_TAIGA,
};


struct MapgenV6Params : public MapgenParams {
	u32 spflags = MGV6_JUNGLES | MGV6_SNOWBIOMES | MGV6_TREES |
		MGV6_BIOMEBLEND | MGV6_MUDFLOW;
	float freq_desert = 0.45f;
	float freq_beach = 0.15f;
	s16 dungeon_ymin = -31000;
	s16 dungeon_ymax = 31000;

	NoiseParams np_terrain_base;
	NoiseParams np_terrain_higher;
	NoiseParams np_steepness;
	NoiseParams np_height_select;
	NoiseParams np_mud;
	NoiseParams np_beach;
	NoiseParams np_biome;
	NoiseParams np_cave;
	NoiseParams np_humidity;
	NoiseParams np_trees;
	NoiseParams np_apple_trees;

	MapgenV6Params();
	~MapgenV6Params() = default;
};

struct MapV6Params {
	content_t stone           = CONTENT_IGNORE;
	content_t dirt            = CONTENT_IGNORE;
	content_t dirt_with_grass = CONTENT_IGNORE;
	content_t sand            = CONTENT_IGNORE;
	content_t water_source    = CONTENT_IGNORE;
	content_t lava_source     = CONTENT_IGNORE;
	content_t gravel          = CONTENT_IGNORE;
	content_t desert_stone    = CONTENT_IGNORE;
	content_t desert_sand     = CONTENT_IGNORE;
	content_t dirt_with_snow  = CONTENT_IGNORE;
	content_t snow            = CONTENT_IGNORE;
	content_t snowblock       = CONTENT_IGNORE;
	content_t ice             = CONTENT_IGNORE;
	content_t cobble             = CONTENT_IGNORE;
	content_t mossycobble        = CONTENT_IGNORE;
	content_t stair_cobble       = CONTENT_IGNORE;
	content_t stair_desert_stone = CONTENT_IGNORE;
};

class MapgenV6 : public Mapgen {
public:
	int ystride;
	u32 spflags;

	v3s16 node_min;
	v3s16 node_max;
	v3s16 full_node_min;
	v3s16 full_node_max;
	v3s16 central_area_size;

	Noise *noise_terrain_base;
	Noise *noise_terrain_higher;
	Noise *noise_steepness;
	Noise *noise_height_select;
	Noise *noise_mud;
	Noise *noise_beach;
	Noise *noise_biome;
	Noise *noise_humidity;
	NoiseParams *np_cave;
	NoiseParams *np_humidity;
	NoiseParams *np_trees;
	NoiseParams *np_apple_trees;

	NoiseParams np_dungeons;

	float freq_desert;
	float freq_beach;
	s16 dungeon_ymin;
	s16 dungeon_ymax;

	MapV6Params c;

	MapgenV6(MapgenV6Params *params, MapV6Params map_params);
	~MapgenV6();

	void makeChunk(BlockMakeData *data) override;
	int getGroundLevelAtPoint(v2s16 p) override;
	int getSpawnLevelAtPoint(v2s16 p) override;

	float baseTerrainLevel(float terrain_base, float terrain_higher,
		float steepness, float height_select);
	virtual float baseTerrainLevelFromNoise(v2s16 p);
	virtual float baseTerrainLevelFromMap(v2s16 p);
	virtual float baseTerrainLevelFromMap(int index);

	s16 find_stone_level(v2s16 p2d);
	bool block_is_underground(u64 seed, v3s16 blockpos);
	s16 find_ground_level_from_noise(u64 seed, v2s16 p2d, s16 precision);

	float getHumidity(v2s16 p);
	float getTreeAmount(v2s16 p);
	bool getHaveAppleTree(v2s16 p);
	float getMudAmount(v2s16 p);
	virtual float getMudAmount(int index);
	bool getHaveBeach(v2s16 p);
	bool getHaveBeach(int index);
	BiomeV6Type getBiome(v2s16 p);
	BiomeV6Type getBiome(int index, v2s16 p);

	u32 get_blockseed(u64 seed, v3s16 p);

	virtual void calculateNoise();
	int generateGround();
	void addMud();
	void flowMud(s16 &mudflow_minpos, s16 &mudflow_maxpos);
	void moveMud(v3s16 remove_index, v3s16 place_index,
		v3s16 above_remove_index, v2s16 pos);
	void growGrass();
	void placeTreesAndJungleGrass();
// 	virtual void generateCaves(int max_stone_y);
};

