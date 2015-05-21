find_package(PythonInterp REQUIRED)
include(ExternalProject)

link_directories(${CMAKE_BINARY_DIR}/linux-cortexm-1.12.0/A2F/root/usr/lib)
include_directories(${CMAKE_BINARY_DIR}/linux-cortexm-1.12.0/A2F/root/usr/include)

ExternalProject_add(
    alsa-lib-build
    URL ftp://ftp.alsa-project.org/pub/lib/alsa-lib-1.0.21.tar.bz2
    PATCH_COMMAND patch -p1 -t -N < ${CMAKE_CURRENT_SOURCE_DIR}/alsa_lib_cortex_m3.patch
    CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${CMAKE_BINARY_DIR}/linux-cortexm-1.12.0/A2F/root/usr --host=arm-uclinuxeabi --with-softfloat --with-libdl=no --with-pthread=yes --enable-shared=no --enable-static=yes --with-debug=yes --with-versioned=no CC=${CMAKE_C_COMPILER} CFLAGS=${CMAKE_C_FLAGS}
    LOG_INSTALL 1
)

#ExternalProject_Add_Step(alsa-lib-build forcebuild
#    COMMAND ${CMAKE_COMMAND} -E echo "Force build of alsa-lib"
#    DEPENDEES configure
#    DEPENDERS build
#    ALWAYS 1)
#find_library(libasound NAMES libasound.a PATHS ${CMAKE_BINARY_DIR}/linux-cortexm-1.12.0/A2F/root/usr/lib NO_DEFAULT_PATH)

ExternalProject_add(
    alsa-util-build
    URL ftp://ftp.alsa-project.org/pub/utils/alsa-utils-1.0.21.tar.bz2
    PATCH_COMMAND patch -p1 -t -N < ${CMAKE_CURRENT_SOURCE_DIR}/alsa_util_cortex_m3.patch
    CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${CMAKE_BINARY_DIR}/linux-cortexm-1.12.0/A2F/root/usr --host=arm-uclinuxeabi --with-udev-rules-dir=${CMAKE_BINARY_DIR}/linux-cortexm-1.12.0/A2F/root --with-alsa-prefix=${CMAKE_BINARY_DIR}/linux-cortexm-1.12.0/A2F/root/usr/lib --with-alsa-inc-prefix=${CMAKE_BINARY_DIR}/linux-cortexm-1.12.0/A2F/root/usr/include --disable-alsamixer --disable-xmlto CC=${CMAKE_C_COMPILER} CFLAGS=${CMAKE_C_FLAGS}
    LOG_INSTALL 1
)
#ExternalProject_Add_Step(alsa-util-build forcebuild
#    COMMAND ${CMAKE_COMMAND} -E echo "Force build of alsa-util"
#    DEPENDEES configure
#    DEPENDERS build
#    ALWAYS 1)

#ExternalProject_add(
#    zlib-build
#    URL http://zlib.net/zlib-1.2.8.tar.gz
#    CONFIGURE_COMMAND CC=${CMAKE_C_COMPILER}
#<SOURCE_DIR>/configure --prefix=${CMAKE_BINARY_DIR}/linux-cortexm-1.12.0/A2F/root/usr --static
#)

ExternalProject_add(
    libwebsockets-build
    GIT_REPOSITORY git://git.libwebsockets.org/libwebsockets
    CMAKE_ARGS -DCMAKE_TOOLCHAIN_FILE=${CMAKE_SOURCE_DIR}/cmake/Toolchains/arm_cortex_m3_native.cmake -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_BINARY_DIR}/linux-cortexm-1.12.0/A2F/root/usr -DZLIB_INCLUDE_DIRS=${CMAKE_BINARY_DIR}/linux-cortexm-1.12.0/A2F/root/usr/include -DLWS_WITH_SSL=0  -DLWS_WITHOUT_EXTENSIONS=1 -DLWS_WITH_STATIC=1 -DLWS_WITH_SHARED=0
    LOG_INSTALL 1
#    DEPENDS zlib-build
)

add_custom_command(
    OUTPUT adf4351.c adf4351.h
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/util/genregs.py --name=adf4351 --src-dir=${CMAKE_CURRENT_SOURCE_DIR} --build-dir=${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/adf4351.regs ${CMAKE_CURRENT_SOURCE_DIR}/adf4351_impl.c.tmpl ${CMAKE_CURRENT_SOURCE_DIR}/adf4351_impl.h.tmpl
)

add_custom_command(
    OUTPUT cmx991.c cmx991.h
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/util/genregs.py --name=cmx991 --src-dir=${CMAKE_CURRENT_SOURCE_DIR} --build-dir=${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/cmx991.regs ${CMAKE_CURRENT_SOURCE_DIR}/cmx991_impl.c.tmpl ${CMAKE_CURRENT_SOURCE_DIR}/cmx991_impl.h.tmpl
)

set(WHITEBOX_SRCS
    whitebox.c
    adf4351.c
    cmx991.c
    dsp.c
)

add_library(whitebox ${WHITEBOX_SRCS})
target_link_libraries(whitebox m ${CMAKE_REQUIRED_LIBRARIES})
set_target_properties(whitebox PROPERTIES SOVERSION ${SOVERSION})

set(WHITEBOX_PUBLIC_HEADERS
    whitebox_test.h
    whitebox.h
    adf4351.h
    cmx991.h
    dsp.h
)

include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_SOURCE_DIR}/driver)

add_executable(test_adf4351 test_adf4351.c)
target_link_libraries(test_adf4351 ${CMAKE_REQUIRED_LIBRARIES} whitebox)

add_executable(test_cmx991 test_cmx991.c)
target_link_libraries(test_cmx991 ${CMAKE_REQUIRED_LIBRARIES} whitebox)

add_executable(test_rf test_rf.c)
target_link_libraries(test_rf ${CMAKE_REQUIRED_LIBRARIES} whitebox)

add_executable(test_driver test_driver.c)
target_link_libraries(test_driver ${CMAKE_REQUIRED_LIBRARIES} whitebox)

add_executable(test_device test_device.c)
target_link_libraries(test_device ${CMAKE_REQUIRED_LIBRARIES} whitebox)

add_executable(test_dsp test_dsp.c)
target_link_libraries(test_dsp ${CMAKE_REQUIRED_LIBRARIES} whitebox)

find_library(WebSockets.lib NAMES libwebsockets.a)

set(Main.sources
    main.cpp
    radio.cpp
    server.cpp
    resources.cpp
    soundcard.cpp
    modem.cpp
    cJSON.c
)
set_source_files_properties(${Main.sources} PROPERTIES COMPILE_FLAGS "-DMAJOR_VERSION=${PROJECT_VERSION_MAJOR} -DMINOR_VERSION=${PROJECT_VERSION_MINOR} -DPATCH_VERSION=${PROJECT_VERSION_PATCH} -DCODENAME=\"${PROJECT_VERSION_CODENAME}\" -DSYSTEM=\"${CMAKE_SYSTEM}\" -g")
add_executable(main ${Main.sources})
target_link_libraries(main ${CMAKE_REQUIRED_LIBRARIES} whitebox
    ${WebSockets.lib} pthread libasound.a)

add_custom_command(TARGET main POST_BUILD
    COMMAND arm-uclinuxeabi-flthdr -s 6000000 main
)

add_executable(qa_tx qa_tx.c)
target_link_libraries(qa_tx ${CMAKE_REQUIRED_LIBRARIES} whitebox)

add_executable(qa_rx qa_rx.c)
target_link_libraries(qa_rx ${CMAKE_REQUIRED_LIBRARIES} whitebox)

add_executable(qa_lo qa_lo.c)
target_link_libraries(qa_lo ${CMAKE_REQUIRED_LIBRARIES} whitebox)

add_custom_target(apps
    DEPENDS test_adf4351 test_cmx991 test_driver test_device test_rf test_dsp main qa_tx qa_rx qa_lo)

if (${REMOTE_DEBUG})
    add_test(NAME target_netboot
        COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/util/serial_command.py --tty=${TTY} --baud=${BAUD} --u-boot run netboot
    )

    add_test(NAME target_test_cmx991
        COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/util/serial_command.py --tty=${TTY} --baud=${BAUD} test_cmx991
    )

    add_test(NAME target_test_adf4351
        COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/util/serial_command.py --tty=${TTY} --baud=${BAUD} test_adf4351
    )

    add_test(NAME target_test_driver
        COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/util/serial_command.py --tty=${TTY} --baud=${BAUD} test_driver
    )

    add_test(NAME target_test_device
        COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/util/serial_command.py --tty=${TTY} --baud=${BAUD} test_device
    )
endif()
