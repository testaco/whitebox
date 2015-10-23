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
set(PROJECT_VERSION_CODENAME "Charlie")
set(PROJECT_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")
# Patch level version bumps should not change API/ABI.
set(SOVERSION ${PROJECT_VERSION})
if(PROJECT_VERSION_PATCH)
	set(PROJECT_VERSION "${PROJECT_VERSION}.${PROJECT_VERSION_PATCH}")
endif()

# The build type is either HOST_BUILD or TARGET_BUILD, depending on
# if there is a CMAKE_TOOLCHAIN_FILE for cross compiling on an embedded
# system.
if (CMAKE_TOOLCHAIN_FILE)
    set(HOST_BUILD false)
    set(TARGET_BUILD true)
    set(BUILD_TYPE "TARGET")
else()
    set(HOST_BUILD true)
    set(TARGET_BUILD false)
    set(BUILD_TYPE "HOST")
endif()

# The IMAGE_TYPE is a space separated list of features to enable for
# development.  Options include:
#  - devel - turn on debugging, NFS mount, etc for development
#  - radioserver - build the main radioserver application
#  - wifiap - enable a wifi access point on the target
if (NOT DEFINED IMAGE_TYPE)
    set(IMAGE_TYPE "devel radioserver")
endif()
string(REPLACE " " ";" IMAGE_TYPE_LIST ${IMAGE_TYPE})
list(REMOVE_DUPLICATES IMAGE_TYPE_LIST)

list(FIND IMAGE_TYPE_LIST "radioserver" RADIOSERVER_POS)
if (NOT RADIOSERVER_POS EQUAL -1)
    set(RADIOSERVER_ENABLED TRUE)
else()
    set(RADIOSERVER_ENABLED FALSE)
endif()

message(STATUS "whitebox version: V${PROJECT_VERSION} ${PROJECT_VERSION_CODENAME} BUILD_TYPE=${BUILD_TYPE} IMAGE_TYPE=\"${IMAGE_TYPE}\"")

if (TARGET_BUILD)
    if(IS_DIRECTORY ${TFTP_DIR})
        message(INFO "TFTP support enabled")
    endif()

    if(EXISTS ${TTY})
        set(REMOTE_DEBUG TRUE)
        if(NOT DEFINED BAUD)
            set(BAUD "115200")
        endif()
        message(INFO "Remote debugging enabled (${BAUD} baud)")
    else()
        set(REMOTE_DEBUG FALSE)
    endif()
endif()

add_subdirectory(linux-cortexm)

add_subdirectory(hdl)

add_subdirectory(driver)

add_subdirectory(lib)

if (RADIOSERVER_ENABLED)
    add_subdirectory(radioserver)
endif()

add_subdirectory(gnuradio)

add_subdirectory(docs)

add_dependencies(docs linux)

add_custom_target(target
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/util/serial_command.py --tty=${TTY} --baud=${BAUD} --u-boot --reset "run update"
)

#
# Cpack configuration to go here...
#

