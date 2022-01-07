cmake_minimum_required(VERSION 3.0)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
set(CMAKE_CXX_STANDARD 17)

project(vcore)

find_package(Threads REQUIRED)
find_package(GLFW REQUIRED)
find_package(GLM REQUIRED)

add_library(GLXX IMPORTED STATIC)
set_target_properties(GLXX PROPERTIES
	IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/../gl++/build/libgl++.a"
	INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/../"
)

add_executable(vcore
	main.cxx
	map/map.cxx
# 	mapgen/heightmap.cxx
	shader.cxx
	util/io.cxx
# 	util/perlin.cxx
)

include_directories(${CMAKE_SOURCE_DIR})
target_include_directories(vcore PUBLIC ${CMAKE_SOURCE_DIR})

target_compile_definitions(vcore PUBLIC
	GLFW_INCLUDE_NONE
)

target_link_libraries(vcore PUBLIC
	fmt
	GLFW
	GLM
	GLXX
	SDL2
	SDL2_image
	mapgen_minetest_v6
	stdc++fs
	Threads::Threads
	${CMAKE_DL_LIBS}
)

add_subdirectory(mapgen/minetest/)
add_subdirectory(test/)
