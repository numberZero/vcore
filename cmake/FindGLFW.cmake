# Copyright (c) 2018 Lobachevskiy Vitaliy

find_package(PkgConfig)
pkg_check_modules(PC_GLFW glfw3)

find_path(GLFW_INCLUDE_DIR
	NAMES GLFW/glfw3.h
	PATHS ${PC_GLFW_INCLUDE_DIR}
)

find_library(GLFW_LIBRARY
	NAMES glfw glfw3
	PATHS ${PC_GLFW_LIBRARY_DIRS}
)

set(GLFW_VERSION ${PC_GLFW_VERSION})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GLFW
	FOUND_VAR GLFW_FOUND
	REQUIRED_VARS
		GLFW_LIBRARY
		GLFW_INCLUDE_DIR
	VERSION_VAR GLFW_VERSION
)

if(GLFW_FOUND)
	set(GLFW_LIBRARIES ${GLFW_LIBRARY})
	set(GLFW_INCLUDE_DIRS ${GLFW_INCLUDE_DIR})
	set(GLFW_DEFINITIONS ${PC_GLFW_CFLAGS_OTHER})
endif()

if(GLFW_FOUND AND NOT TARGET GLFW)
	add_library(GLFW UNKNOWN IMPORTED)
	set_target_properties(GLFW PROPERTIES
		IMPORTED_LOCATION "${GLFW_LIBRARY}"
		INTERFACE_COMPILE_OPTIONS "${PC_GLFW_CFLAGS_OTHER}"
		INTERFACE_INCLUDE_DIRECTORIES "${GLFW_INCLUDE_DIR}"
	)
endif()

mark_as_advanced(GLFW_INCLUDE_DIR GLFW_LIBRARY)
