# Copyright (c) 2018 Lobachevskiy Vitaliy

find_package(PkgConfig)
pkg_check_modules(PC_GLM glm)

find_path(GLM_INCLUDE_DIR
	NAMES glm/glm.hpp
	PATHS ${PC_GLM_INCLUDE_DIR}
)

set(GLM_VERSION ${PC_GLM_VERSION})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GLM
	FOUND_VAR GLM_FOUND
	REQUIRED_VARS
		GLM_INCLUDE_DIR
	VERSION_VAR GLM_VERSION
)

if(GLM_FOUND)
	set(GLM_INCLUDE_DIRS ${GLM_INCLUDE_DIR})
	set(GLM_DEFINITIONS ${PC_GLM_CFLAGS_OTHER})
endif()

if(GLM_FOUND AND NOT TARGET GLM)
	add_library(GLM INTERFACE)
	set_target_properties(GLM PROPERTIES
		INTERFACE_COMPILE_OPTIONS "${PC_GLM_CFLAGS_OTHER}"
		INTERFACE_INCLUDE_DIRECTORIES "${GLM_INCLUDE_DIR}"
	)
endif()

mark_as_advanced(GLM_INCLUDE_DIR GLM_LIBRARY)
