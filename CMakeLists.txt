cmake_minimum_required (VERSION 2.8)

project(whitebox)

# Prevent in-source builds
set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

# Build shared libraries by default
set(BUILD_SHARED_LIBS FALSE)

# Enable testing
enable_testing()

# Set the toolchain
set(CMAKE_TOOLCHAIN_FILE cmake/Toolchains/arm_cortex_m3_native.cmake)

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

if(NOT IS_DIRECTORY ${TFTP_DIR})
    message(WARNING "no tftp destination")
endif()

if(NOT EXISTS ${TTY})
    message(WARNING "no tty for the target")
endif()

if(NOT DEFINED BAUD)
    set(BAUD "115200")
endif()


add_subdirectory(linux-cortexm)

add_subdirectory(hdl)

add_subdirectory(driver)

add_subdirectory(lib)

add_subdirectory(gnuradio)

add_subdirectory(docs)

add_dependencies(docs linux)

add_custom_target(target
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/util/serial_command.py --tty=${TTY} --baud=${BAUD} --u-boot --reset "run update"
)

#
# Cpack configuration to go here...
#

