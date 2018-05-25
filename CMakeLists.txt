cmake_minimum_required(VERSION 3.0)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

project(vcore)

find_package(GLFW REQUIRED)
find_package(GLM REQUIRED)

add_executable(vcore
	main.cxx
	mapgen/heightmap.cxx
)

target_include_directories(vcore PUBLIC ${CMAKE_SOURCE_DIR})

target_link_libraries(vcore PUBLIC
	GLFW
	GLM
	GL
)
