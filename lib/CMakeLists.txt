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
    adf4351.c
    cmx991.c
    radio.c
    gpio.c
)

add_library(whitebox ${WHITEBOX_SRCS})
target_link_libraries(whitebox ${CMAKE_REQUIRED_LIBRARIES})
set_target_properties(whitebox PROPERTIES SOVERSION ${SOVERSION})

set(WHITEBOX_PUBLIC_HEADERS
    adf4351.h
    cmx991.h
    radio.h
    gpio.h
)

install(FILES ${WHITEBOX_PUBLIC_HEADERS}
	DESTINATION include/whitebox
)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_executable(test_adf4351 test_adf4351.c)
target_link_libraries(test_adf4351 ${CMAKE_REQUIRED_LIBRARIES} whitebox)

add_executable(test_cmx991 test_cmx991.c)
target_link_libraries(test_cmx991 ${CMAKE_REQUIRED_LIBRARIES} whitebox)

add_executable(test_radio test_radio.c)
target_link_libraries(test_radio ${CMAKE_REQUIRED_LIBRARIES} whitebox)

add_executable(app app.c)
target_link_libraries(app ${CMAKE_REQUIRED_LIBRARIES} whitebox)

add_executable(cw cw.c)
target_link_libraries(cw ${CMAKE_REQUIRED_LIBRARIES} whitebox m)

add_executable(samples samples.c)
target_link_libraries(samples ${CMAKE_REQUIRED_LIBRARIES} whitebox)

install(TARGETS
	whitebox
    app
    cw
    samples
    test_adf4351
    test_cmx991
    test_radio
	LIBRARY DESTINATION lib${LIB_SUFFIX}
	ARCHIVE DESTINATION lib${LIB_SUFFIX}
	RUNTIME DESTINATION bin
)
