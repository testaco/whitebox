set(INSTALL_ROOT ${CMAKE_BINARY_DIR}/linux-cortexm-1.12.0)
include_directories(${INSTALL_ROOT}/A2F/root/usr/include)

if(NOT DEFINED KERNELDIR)
    set(KERNELDIR ${INSTALL_ROOT}/linux)
endif()

if(NOT DEFINED TOOLS_PATH)
    set(TOOLS_PATH ${INSTALL_ROOT}/tools)
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

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/whitebox.uImage
    DESTINATION ${TFTP_DIR})
