if (CMAKE_C_COMPILER_ID STREQUAL GNU)
	set(CMAKE_C_FLAGS_DEBUG_INIT "-g -Og")
endif()
if (CMAKE_CXX_COMPILER_ID STREQUAL GNU)
	set(CMAKE_CXX_FLAGS_DEBUG_INIT "-g -Og")
endif()

string(REPLACE "-O2" "-O3" CMAKE_C_FLAGS_RELEASE_INIT "${CMAKE_C_FLAGS_RELEASE_INIT}")
string(REPLACE "-O2" "-O3" CMAKE_CXX_FLAGS_RELEASE_INIT "${CMAKE_CXX_FLAGS_RELEASE_INIT}")

# Building with LTO causes an internal compiler error in GCC 15.1.0
if (CMAKE_CXX_COMPILER_ID STREQUAL GNU AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 15.0 AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 15.1.1)
	set(ENABLE_LTO_RELEASE OFF CACHE BOOL "Enable LTO for release builds" FORCE)
	set(ENABLE_LTO OFF CACHE BOOL "Enable LTO" FORCE)
endif()
