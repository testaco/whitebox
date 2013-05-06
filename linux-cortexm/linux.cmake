set(TOOLCHAIN arm-2010q1)

set(ENV{INSTALL_ROOT} ${INSTALL_ROOT})

set(TOOLS_PATH ${INSTALL_ROOT}/tools)
set(CROSS_PATH ${TOOLS_PATH}/${TOOLCHAIN}/bin)
set(ENV{PATH} "${TOOLS_PATH}/bin:${CROSS_PATH}:$ENV{PATH}")

set(ENV{CROSS_COMPILE} arm-uclinuxeabi-)
set(ENV{CORSS_COMPILE_APPS} arm-uclinuxeabi-)

set(ENV{MCU} A2F)

set(ENV{SAMPLE} ${PROJECT_NAME})

execute_process(
    COMMAND make -f Emcraft.make ${COMMAND}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
