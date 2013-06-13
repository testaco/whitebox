cmake_minimum_required (VERSION 2.8)

# Prevent in-source builds
set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

# Set the toolchain
set(CMAKE_TOOLCHAIN_FILE cmake/Toolchains/arm_cortex_m3_native.cmake)

project(whitebox)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/Modules")

#
# Set project version information. This should probably be done via external
# file at some point.
#
set(PROJECT_VERSION_MAJOR 0)
set(PROJECT_VERSION_MINOR 1)
# Set to patch level is needed, otherwise leave FALSE.
set(PROJECT_VERSION_PATCH FALSE)
set(PROJECT_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")
# Patch level version bumps should not change API/ABI.
set(SOVERSION ${PROJECT_VERSION})
if(PROJECT_VERSION_PATCH)
	set(PROJECT_VERSION "${PROJECT_VERSION}.${PROJECT_VERSION_PATCH}")
endif()

message(STATUS "whitebox version: ${PROJECT_VERSION}")

# Build shared libraries by default
set(BUILD_SHARED_LIBS FALSE)

add_subdirectory(linux-cortexm)

add_subdirectory(hdl)

add_subdirectory(driver)

add_subdirectory(lib)

add_subdirectory(gnuradio)

# MUST be after everything else has been generated
add_subdirectory(docs)

#
# Cpack configuration to go here...
#
