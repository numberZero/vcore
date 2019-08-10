cmake_minimum_required(VERSION 3.0)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

project(vcore)

find_package(GLFW REQUIRED)
find_package(GLM REQUIRED)

add_library(GLXX IMPORTED STATIC)
set_target_properties(GLXX PROPERTIES
	IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/../gl++/build/libgl++.a"
	INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/../"
)

add_executable(vcore
	main.cxx
	mapgen/heightmap.cxx
	util/perlin.cxx
)

target_include_directories(vcore PUBLIC ${CMAKE_SOURCE_DIR})

target_compile_definitions(vcore PUBLIC
	GLFW_INCLUDE_NONE
)

target_link_libraries(vcore PUBLIC
	fmt
	GLFW
	GLM
	GLXX
	mapgen_minetest_v6
	${CMAKE_DL_LIBS}
)

add_subdirectory(mapgen/minetest/)
