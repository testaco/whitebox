########################################################################
# Toolchain file for building native on a ARM Cortex M3
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=<this file> <source directory>
########################################################################
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_CXX_COMPILER arm-uclinuxeabi-g++)
set(CMAKE_C_COMPILER  arm-uclinuxeabi-gcc)
set(CMAKE_CXX_FLAGS "-march=armv7-m -mtune=cortex-m3 -mthumb" CACHE STRING "" FORCE)
set(CMAKE_C_FLAGS ${CMAKE_CXX_FLAGS} CACHE STRING "" FORCE)
set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")
#set(CMAKE_FIND_ROOT_PATH ${CMAKE_CXXlinux-cortexm-1.9.0/projects/radio/local)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
