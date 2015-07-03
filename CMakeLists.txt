cmake_minimum_required (VERSION 2.8)

project(whitebox)

# Prevent in-source builds
set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

# Build shared libraries by default
set(BUILD_SHARED_LIBS FALSE)

# Enable testing
enable_testing()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules")

set(PROJECT_VERSION_MAJOR 0)
set(PROJECT_VERSION_MINOR 2)
set(PROJECT_VERSION_PATCH 0)
set(PROJECT_VERSION_CODENAME "Bravo")
set(PROJECT_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")
# Patch level version bumps should not change API/ABI.
set(SOVERSION ${PROJECT_VERSION})
if(PROJECT_VERSION_PATCH)
	set(PROJECT_VERSION "${PROJECT_VERSION}.${PROJECT_VERSION_PATCH}")
endif()

if (CMAKE_TOOLCHAIN_FILE)
    set(HOST_BUILD false)
    set(TARGET_BUILD true)
    set(BUILD_TYPE "TARGET")
else()
    set(HOST_BUILD true)
    set(TARGET_BUILD false)
    set(BUILD_TYPE "HOST")
endif()


message(STATUS "whitebox version: ${PROJECT_VERSION} ${PROJECT_VERSION_CODENAME} ${BUILD_TYPE}")

if (TARGET_BUILD)
    if(NOT IS_DIRECTORY ${TFTP_DIR})
        message(WARNING "no tftp destination")
    endif()

    if(NOT EXISTS ${TTY})
        set(REMOTE_DEBUG FALSE)
        message(WARNING "no tty for the target, remote debugging disabled")
    else()
        set(REMOTE_DEBUG TRUE)
    endif()

    if(NOT DEFINED BAUD)
        set(BAUD "115200")
    endif()
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

