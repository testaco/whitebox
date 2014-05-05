find_package(PythonInterp REQUIRED)

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
    http.c
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
    http.h
)

#install(FILES ${WHITEBOX_PUBLIC_HEADERS}
#	DESTINATION include/whitebox
#)

include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_SOURCE_DIR}/driver)

add_executable(test_adf4351 test_adf4351.c)
target_link_libraries(test_adf4351 ${CMAKE_REQUIRED_LIBRARIES} whitebox)

add_executable(test_cmx991 test_cmx991.c)
target_link_libraries(test_cmx991 ${CMAKE_REQUIRED_LIBRARIES} whitebox)

add_executable(test_driver test_driver.c)
target_link_libraries(test_driver ${CMAKE_REQUIRED_LIBRARIES} whitebox)

add_executable(test_device test_device.c)
target_link_libraries(test_device ${CMAKE_REQUIRED_LIBRARIES} whitebox)

#add_executable(cw cw.c)
#target_link_libraries(cw ${CMAKE_REQUIRED_LIBRARIES} whitebox m)

add_executable(rfcat rfcat.c)
target_link_libraries(rfcat ${CMAKE_REQUIRED_LIBRARIES} whitebox)

add_executable(gnuradio_peripheral gnuradio_peripheral.c)
target_link_libraries(gnuradio_peripheral ${CMAKE_REQUIRED_LIBRARIES} whitebox)

add_executable(qa_tx qa_tx.c)
target_link_libraries(qa_tx ${CMAKE_REQUIRED_LIBRARIES} whitebox)

add_executable(qa_rx qa_rx.c)
target_link_libraries(qa_rx ${CMAKE_REQUIRED_LIBRARIES} whitebox)

add_executable(qa_lo qa_lo.c)
target_link_libraries(qa_lo ${CMAKE_REQUIRED_LIBRARIES} whitebox)

add_executable(whiteboxd whiteboxd.c)
target_link_libraries(whiteboxd ${CMAKE_REQUIRED_LIBRARIES} whitebox)

add_custom_target(apps
    DEPENDS test_adf4351 test_cmx991 test_driver test_device gnuradio_peripheral qa_tx qa_rx qa_lo whiteboxd)

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
