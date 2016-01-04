find_package(PythonInterp REQUIRED)

# Find if a Python module is installed
# Found at http://www.cmake.org/pipermail/cmake/2011-January/041666.html
# To use do: find_python_module(PyQt4 REQUIRED)
function(find_python_module module)
    string(TOUPPER ${module} module_upper)
    if(NOT PY_${module_upper})
        if(ARGC GREATER 1 AND ARGV1 STREQUAL "REQUIRED")
            set(${module}_FIND_REQUIRED TRUE)
        endif()
        # A module's location is usually a directory, but for binary modules
        # it's a .so file.
        execute_process(COMMAND "${PYTHON_EXECUTABLE}" "-c" 
            "import re, ${module}; print(re.compile('/__init__.py.*').sub('',${module}.__file__))"
            RESULT_VARIABLE _${module}_status 
            OUTPUT_VARIABLE _${module}_location
            ERROR_QUIET 
            OUTPUT_STRIP_TRAILING_WHITESPACE)
        if(NOT _${module}_status)
            set(PY_${module_upper} ${_${module}_location} CACHE STRING 
                "Location of Python module ${module}")
        endif(NOT _${module}_status)
    endif(NOT PY_${module_upper})
    find_package_handle_standard_args(PY_${module} DEFAULT_MSG PY_${module_upper})
endfunction(find_python_module)

find_python_module(Cheetah REQUIRED)
find_python_module(myhdl REQUIRED)
find_python_module(matplotlib REQUIRED)
find_python_module(numpy REQUIRED)
find_python_module(scipy REQUIRED)

# WHITEBOX

add_custom_command(
    OUTPUT whitebox.v
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/whitebox.py
    DEPENDS whitebox.py apb3_utils.py fifo.py duc.py ddc.py dds.py rfe.py dsp.py ram.py fir.py streamer.py tuner.py modem.py converter.py
) 

set(whitebox_verilated_sources
    obj_dir/Vwhitebox.cpp
    obj_dir/Vwhitebox__Syms.cpp
    obj_dir/Vwhitebox__Trace.cpp
    obj_dir/Vwhitebox__Trace__Slow.cpp
)

add_custom_command(
    OUTPUT ${whitebox_verilated_sources}
    COMMAND verilator -Wall -Wno-fatal --trace -I${CMAKE_CURRENT_SOURCE_DIR} -O3 --cc ${CMAKE_CURRENT_BINARY_DIR}/whitebox.v
    DEPENDS whitebox.v
)

set(test_whitebox_sources
    ${whitebox_verilated_sources}
    ${CMAKE_CURRENT_SOURCE_DIR}/test_whitebox.cpp
    /usr/share/verilator/include/verilated.cpp
    /usr/share/verilator/include/verilated_vcd_c.cpp
)

add_custom_target(test_whitebox
    DEPENDS ${test_whitebox_sources}
    #COMMAND g++ -O2 -o test_whitebox -I${CMAKE_CURRENT_BINARY_DIR} -I/usr/share/verilator/include -I/usr/share/verilator/include/vltstd ${test_whitebox_sources}
)

# MODEM

add_custom_command(
    OUTPUT modem.v
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/modem.py
    DEPENDS modem.py
)

set(modem_verilated_sources
    obj_dir/Vmodem.cpp
    obj_dir/Vmodem__Syms.cpp
    obj_dir/Vmodem__Trace.cpp
    obj_dir/Vmodem__Trace__Slow.cpp
)

add_custom_command(
    OUTPUT ${modem_verilated_sources}
    COMMAND verilator -Wall -Wno-fatal --trace -I${CMAKE_CURRENT_SOURCE_DIR} -O3 --cc ${CMAKE_CURRENT_BINARY_DIR}/modem.v
    DEPENDS modem.v # modem_controller.v
)

add_custom_target(test_modem
    DEPENDS ${modem_verilated_sources}
)

# TUNER

add_custom_command(
    OUTPUT tuner.v
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tuner.py
    DEPENDS tuner.py apb3_utils.py dsp.py dds.py cic.py
)

set(tuner_verilated_sources
    obj_dir/Vtuner.cpp
    obj_dir/Vtuner__Syms.cpp
    obj_dir/Vtuner__Trace.cpp
    obj_dir/Vtuner__Trace__Slow.cpp
)

add_custom_command(
    OUTPUT ${tuner_verilated_sources}
    COMMAND verilator -Wall -Wno-fatal --trace -I${CMAKE_CURRENT_SOURCE_DIR} -O3 --cc ${CMAKE_CURRENT_BINARY_DIR}/tuner.v
    DEPENDS tuner.v # tuner_controller.v
)

set(test_tuner_sources
    ${tuner_verilated_sources}
    ${CMAKE_CURRENT_SOURCE_DIR}/test_tuner.cpp
    /usr/share/verilator/include/verilated.cpp
    /usr/share/verilator/include/verilated_vcd_c.cpp
)

add_custom_target(test_tuner
    DEPENDS ${test_tuner_sources}
    COMMAND g++ -O2 -o test_tuner -I${CMAKE_CURRENT_BINARY_DIR} -I/usr/share/verilator/include -I/usr/share/verilator/include/vltstd -LWebSockets ${test_tuner_sources}
)

# STREAMER
add_custom_command(
    OUTPUT streamer.v
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/streamer.py
    DEPENDS streamer.py apb3_utils.py dsp.py ram.py
)

add_custom_target(hdl
    DEPENDS whitebox.v modem.v tuner.v streamer.v test_modem test_tuner test_whitebox
)

configure_file(cosim_whitebox.v cosim_whitebox.v COPYONLY)

add_test(NAME test_apb3_simple_slave
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/apb3_utils.py
)

add_test(NAME test_fifo
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/fifo.py
)

add_test(NAME test_ram
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/ram.py
)
    
add_test(NAME test_dsp_offset_corrector
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_dsp.py TestOffsetCorrector
)

add_test(NAME test_dsp_gain_corrector
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_dsp.py TestGainCorrector
)

add_test(NAME test_dsp_binary_offseter
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_dsp.py TestBinaryOffseter
)

add_test(NAME test_duc_cic_impulse
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_duc.py TestCicImpulse
)

add_test(NAME test_duc_cic_sin
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_duc.py TestCicSin
)

add_test(NAME test_duc_cic_interpolate_20
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_duc.py TestCicInterpolate20
)

add_test(NAME test_cic_decim_math
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_duc.py TestCicDecimMath
)

add_test(NAME test_cic_decim_impulse
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_duc.py TestCicDecimImpulse
)

add_test(NAME test_fir_impulse
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_fir.py TestFirImpulse
)

add_test(NAME test_fir_bypass
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_fir.py TestFirBypass
)

add_test(NAME test_fir_design
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_fir.py TestFirDesign
)

add_test(NAME test_whitebox_apb3_transaction
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_whitebox.py TestApb3Transaction
)

add_test(NAME test_whitebox_overrun_underrun
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_whitebox.py TestOverrunUnderrun
)

add_test(NAME test_whitebox_loop
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_whitebox.py TestLoop
)

add_test(NAME test_whitebox_upsampler_impulse_response
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_whitebox.py TestUpsamplerImpulseResponse
)

add_test(NAME test_duc_upsampler_impulse_response
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_duc.py TestUpsamplerImpulseResponse
)

add_test(NAME test_whitebox_upsampler_3x_impulse_response
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_whitebox.py TestUpsampler3xImpulseResponse
)

add_test(NAME test_whitebox_fir_impulse_response
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_whitebox.py TestFirImpulseResponse
)

add_test(NAME test_whitebox_dds_spectrum_mask
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_whitebox.py TestDdsSpectrumMask
)

add_test(NAME test_whitebox_cic_impulse_response
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_whitebox.py TestCicImpulseResponse
)

add_test(NAME test_whitebox_cic_3x_impulse_response
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_whitebox.py TestCic3xImpulseResponse
)

add_test(NAME test_whitebox_rx
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_whitebox.py TestRx
)

add_test(NAME test_whitebox_rx_overrun
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_whitebox.py TestRxOverrun
)

add_test(NAME test_whitebox_rx_sequence
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_whitebox.py TestRxSequence
)

add_test(NAME test_dsp_tx_chain
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/duc_controller --script test_tx_chain
)

add_test(NAME test_dsp_rx_chain
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/duc_controller --script test_rx_chain
)

add_subdirectory(constraint)

add_dependencies(hdl constraints)
