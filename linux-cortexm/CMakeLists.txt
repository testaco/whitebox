include(ExternalProject)

set(INSTALL_ROOT ${CMAKE_BINARY_DIR}/linux-cortexm-1.12.0)
include_directories(${INSTALL_ROOT}/A2F/root/usr/include)

if(NOT DEFINED KERNELDIR)
    set(KERNELDIR ${INSTALL_ROOT}/linux)
endif()

if(NOT DEFINED TOOLS_PATH)
    set(TOOLS_PATH ${INSTALL_ROOT}/tools)
endif()

if(NOT DEFINED TARGET_ROOT_PATH)
    set(TOOLS_PATH ${INSTALL_ROOT}/A2F/root)
endif()

if(NOT DEFINED IMAGE_TYPE)
    set(IMAGE_TYPE devel)
endif()
message(INFO " Building an image of type ${IMAGE_TYPE}")

if (IMAGE_TYPE STREQUAL "demo1")
    set(INCLUDE_DEMO1_SAMPLES "")
else()
    set(INCLUDE_DEMO1_SAMPLES "#")
endif()

configure_file(Emcraft.make Emcraft.make
    COPYONLY
)

configure_file(whitebox.kernel.M2S whitebox.kernel.M2S
    COPYONLY
)

configure_file(whitebox.initramfs.in whitebox.initramfs
    @ONLY
)

configure_file(local/etc/rc.in local/etc/rc
    @ONLY
)

configure_file(whitebox.busybox whitebox.busybox
    COPYONLY
)

macro(emcraft_build CMD)
    add_custom_target(${CMD}
        COMMAND ${CMAKE_COMMAND} -DPROJECT_NAME=${CMAKE_PROJECT_NAME} -DINSTALL_ROOT=${INSTALL_ROOT} -DCOMMAND=${CMD} -P ${CMAKE_CURRENT_SOURCE_DIR}/linux.cmake
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        VERBATIM
    )
endmacro(emcraft_build)

emcraft_build(kmenuconfig)
emcraft_build(bmenuconfig)
emcraft_build(busybox)
emcraft_build(kclean)
emcraft_build(bclean)

add_custom_command(OUTPUT whitebox.uImage
    COMMAND ${CMAKE_COMMAND} -DPROJECT_NAME=${CMAKE_PROJECT_NAME} -DINSTALL_ROOT=${INSTALL_ROOT} -DCOMMAND=linux -P ${CMAKE_CURRENT_SOURCE_DIR}/linux.cmake
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS whitebox.initramfs.in drivers apps
    VERBATIM
)

add_custom_target(linux
    DEPENDS whitebox.uImage)

if (IS_DIRECTORY ${TFTP_DIR})
message(INFO " Installing into TFTP directory ${TFTP_DIR}")
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/whitebox.uImage
        DESTINATION ${TFTP_DIR})
endif()

# Wifi drivers
ExternalProject_add(
    rtl8188eu
    URL https://github.com/lwfinger/rtl8188eu/archive/3091828c8f4b4a01cbec6025128bf77e6e7b9f97.zip
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND echo
    BUILD_COMMAND ARCH=arm CROSS_COMPILE=arm-uclinuxeabi- KSRC=${KERNELDIR} USER_EXTRA_CFLAGS="-DCONFIG_WEXT_PRIV" make V=1
    INSTALL_COMMAND echo
    LOG_INSTALL 1
)

ExternalProject_add_step(
    rtl8188eu hostapd-config
    COMMAND cd <SOURCE_DIR>/hostapd-0.8/hostapd && cp defconfig .config
    DEPENDEES build
)

#ExternalProject_add_step(
#    rtl8188eu hostapd
#    COMMAND CC=${CMAKE_C_COMPILER} CFLAGS=${CMAKE_C_FLAGS} LDFLAGS=${CMAKE_C_FLAGS}; cd hostapd-0.8/hostapd && make
#    DEPENDEES hostapd-config
#    DEPENDERS install
#)

#ExternalProject_add_step(
#    rtl8188eu hostapd-flthdr
#    COMMAND cd hostapd-0.8/hostapd && arm-uclinuxeabi-flthdr -s 6000000 hostapd
#    COMMAND cd hostapd-0.8/hostapd && arm-uclinuxeabi-flthdr -s 6000000 hostapd_cli
#    DEPENDEES hostapd-config
#    DEPENDERS install
#)

