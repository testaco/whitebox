if(NOT DEFINED INSTALL_ROOT)
    set(INSTALL_ROOT ${CMAKE_BINARY_DIR}/linux-cortexm-1.12.0)
endif()

if(NOT DEFINED KERNELDIR)
    set(KERNELDIR ${INSTALL_ROOT}/linux)
endif()

configure_file(Kbuild Kbuild
    COPYONLY
)

set(DRIVER_SOURCES
        whitebox_driver.c
        whitebox.h
        whitebox_ioctl.h
        pdma.c pdma.h
        whitebox_block.h
        whitebox_rf_sink.c whitebox_rf_source.c
        whitebox_user_source.c whitebox_user_sink.c
        whitebox_exciter.h whitebox_exciter.c whitebox_tx.c
        whitebox_receiver.h whitebox_receiver.c whitebox_rx.c
        whitebox_gpio.h whitebox_gpio.c)

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
