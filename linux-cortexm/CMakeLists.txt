if(NOT DEFINED INSTALL_ROOT)
    set(INSTALL_ROOT ${CMAKE_BINARY_DIR}/linux-cortexm-1.9.0)
endif()

if(NOT DEFINED KERNELDIR)
    set(KERNELDIR ${INSTALL_ROOT}/linux)
endif()

if(NOT DEFINED TOOLS_PATH)
    set(TOOLS_PATH ${INSTALL_ROOT}/tools)
endif()

configure_file(Emcraft.make Emcraft.make
    COPYONLY
)

configure_file(whitebox.kernel.A2F whitebox.kernel.A2F
    COPYONLY
)

configure_file(whitebox.initramfs.in whitebox.initramfs
    @ONLY
)

configure_file(whitebox.busybox whitebox.busybox
    COPYONLY
)

add_custom_target(linux_patch
    COMMAND patch -Np1 < ${CMAKE_CURRENT_SOURCE_DIR}/a2fgpio.patch
    DEPENDS a2fgpio.patch
    WORKING_DIRECTORY ${KERNELDIR}
    VERBATIM
)

macro(emcraft_build CMD)
    add_custom_target(${CMD}
        COMMAND ${CMAKE_COMMAND} -DPROJECT_NAME=${CMAKE_PROJECT_NAME} -DINSTALL_ROOT=${INSTALL_ROOT} -DCOMMAND=${CMD} -P ${CMAKE_CURRENT_SOURCE_DIR}/linux.cmake
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        VERBATIM
    )
endmacro(emcraft_build)

emcraft_build(linux)
emcraft_build(kmenuconfig)
emcraft_build(bmenuconfig)
emcraft_build(busybox)
emcraft_build(kclean)
emcraft_build(bclean)
