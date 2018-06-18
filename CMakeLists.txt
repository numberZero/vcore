cmake_minimum_required(VERSION 3.0)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

project(vcore)

find_package(GLFW REQUIRED)
find_package(GLM REQUIRED)

add_executable(vcore
	main.cxx
	mapgen/heightmap.cxx
	util/perlin.cxx
	glad/glad.c
)

target_include_directories(vcore PUBLIC ${CMAKE_SOURCE_DIR})

target_compile_definitions(vcore PUBLIC
	GLFW_INCLUDE_NONE
)

target_link_libraries(vcore PUBLIC
	GLFW
	GLM
	${CMAKE_DL_LIBS}
)
