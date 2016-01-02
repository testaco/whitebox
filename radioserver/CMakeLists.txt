include(ExternalProject)

message(STATUS "Radioserver is enabled")

set(CMAKE_FIND_ROOT_PATH ${CMAKE_BINARY_DIR}/linux-cortexm-1.12.0/A2F/root/usr)
link_directories(${CMAKE_BINARY_DIR}/linux-cortexm-1.12.0/A2F/root/usr/lib)
include_directories(${CMAKE_BINARY_DIR}/linux-cortexm-1.12.0/A2F/root/usr/include)

ExternalProject_add(
    zlib-build
    URL http://zlib.net/zlib-1.2.8.tar.gz
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND CC=${CMAKE_C_COMPILER} <SOURCE_DIR>/configure --prefix=${CMAKE_BINARY_DIR}/linux-cortexm-1.12.0/A2F/root/usr --static
)

find_library(WEBSOCKETS_LIBRARIES NAMES libwebsockets.so)

if (WEBSOCKETS_LIBRARIES)
    message(STATUS "Websockets library found at ${WEBSOCKETS_LIBRARIES}")
else()
    message(STATUS "Building websockets")
    if (TARGET_BUILD)
        set (CMAKE_TOOLCHAIN_ARG -DCMAKE_TOOLCHAIN_FILE=${CMAKE_SOURCE_DIR}/cmake/Toolchains/arm_cortex_m3_native.cmake)
    endif()
    ExternalProject_add(
        libwebsockets-build
        #SOURCE_DIR libwebsockets-build-prefix/src/libwebsockets-build/libwebsockets
        #DOWNLOAD_COMMAND cp -r ${CMAKE_CURRENT_SOURCE_DIR}/libwebsockets/ ${CMAKE_CURRENT_BINARY_DIR}/libwebsockets-build-prefix/src/libwebsockets-build
        GIT_REPOSITORY https://github.com/warmcat/libwebsockets.git
        GIT_TAG v1.5-chrome47-firefox41
        CMAKE_ARGS ${CMAKE_TOOLCHAIN_ARG} -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_BINARY_DIR}/linux-cortexm-1.12.0/A2F/root/usr -DZLIB_INCLUDE_DIRS=${CMAKE_BINARY_DIR}/linux-cortexm-1.12.0/A2F/root/usr/include -DLWS_WITH_SSL=0  -DLWS_WITHOUT_EXTENSIONS=1 -DLWS_WITH_STATIC=1 -DLWS_WITH_SHARED=0 -DLWS_WITH_OLD_API_WRAPPERS=1
        LOG_INSTALL 1
        DEPENDS zlib-build
    )
    set(WEBSOCKETS_LIBRARIES ${CMAKE_BINARY_DIR}/linux-cortexm-1.12.0/A2F/root/usr/lib/libwebsockets.a)
    set(BUILD_WEBSOCKETS_LIBRARIES true)
endif()

if (HOST_BUILD)
    add_definitions(-DETC_DIR="${CMAKE_SOURCE_DIR}/linux-cortexm/local/etc/"
                    -DLIB_DIR="${CMAKE_SOURCE_DIR}/linux-cortexm/local/usr/lib/")
else()
    add_definitions(-DETC_DIR="/etc/"
                    -DLIB_DIR="/usr/lib/")
endif()

set(Main.sources
    main.cpp
    radio.cpp
    server.cpp
    repeater.cpp
    controller.cpp
    cJSON.c
)
set_source_files_properties(${Main.sources} PROPERTIES COMPILE_FLAGS "-DMAJOR_VERSION=${PROJECT_VERSION_MAJOR} -DMINOR_VERSION=${PROJECT_VERSION_MINOR} -DPATCH_VERSION=${PROJECT_VERSION_PATCH} -DCODENAME=\"${PROJECT_VERSION_CODENAME}\" -DSYSTEM=\"${CMAKE_SYSTEM}\" -g -DLWS_WITH_OLD_API_WRAPPERS=1")
add_executable(main ${Main.sources})

include(FindALSA)
include(FindOpenSSL)
target_link_libraries(main ${CMAKE_REQUIRED_LIBRARIES} whitebox
    ${WEBSOCKETS_LIBRARIES} ${ALSA_LIBRARIES} rt pthread)

if (TARGET_BUILD)
    add_custom_command(TARGET main POST_BUILD
        COMMAND arm-uclinuxeabi-flthdr -s 6000000 main
    )
endif()

include_directories(${CMAKE_SOURCE_DIR}/lib
    ${CMAKE_BINARY_DIR}/lib
    ${CMAKE_SOURCE_DIR}/driver)
add_custom_target(radioserver
    DEPENDS main
)
if(BUILD_WEBSOCKETS_LIBRARIES)
    add_dependencies(radioserver libwebsockets-build)
endif()

