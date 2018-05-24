cmake_minimum_required(VERSION 3.0)
project(vcore)

add_executable(vcore
	main.cxx
)

target_include_directories(vcore PUBLIC ${CMAKE_SOURCE_DIR})
