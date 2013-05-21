if(NOT DEFINED INSTALL_ROOT)
    set(INSTALL_ROOT ${CMAKE_BINARY_DIR}/linux-cortexm-1.9.0)
endif()

if(NOT DEFINED KERNELDIR)
    set(KERNELDIR ${INSTALL_ROOT}/linux)
endif()

configure_file(Kbuild Kbuild
    COPYONLY
)

set(DRIVER_SOURCES fluidsp.h fluidsp_impl.c fluidsp_info.c fluidsp_driver.c radioctl.c)

foreach (DRIVER_SOURCE ${DRIVER_SOURCES})
    configure_file(${DRIVER_SOURCE} ${DRIVER_SOURCE}
        COPYONLY
    )
endforeach(DRIVER_SOURCE)

add_custom_target(drivers
    COMMAND ${CMAKE_COMMAND} -DINSTALL_ROOT=${INSTALL_ROOT} -DKERNELDIR=${KERNELDIR} -DCOMMAND=modules -P ${CMAKE_CURRENT_SOURCE_DIR}/kbuild.cmake
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS ${DRIVER_SOURCES}
    VERBATIM
)

add_custom_target(clean_drivers
    COMMAND ${CMAKE_COMMAND} -DINSTALL_ROOT=${INSTALL_ROOT} -DKERNELDIR=${KERNELDIR} -DCOMMAND=clean -P ${CMAKE_CURRENT_SOURCE_DIR}/kbuild.cmake; rm -f modules.order
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    VERBATIM
)
