# Minetest mapgens

add_library(minetest_mapgen_core SHARED
	common/mapgen.cxx
	common/noise.cxx
)

target_include_directories(minetest_mapgen_core PUBLIC
	common/
)

link_libraries(minetest_mapgen_core)

add_library(mapgen_minetest_v6 SHARED
	v6/mapgen_v6.cxx
)
