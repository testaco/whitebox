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

find_python_module(myhdl REQUIRED)
find_python_module(matplotlib REQUIRED)
find_python_module(numpy REQUIRED)

add_custom_command(
    OUTPUT whitebox.v
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/whitebox.py
    DEPENDS whitebox.py apb3_utils.py fifo.py duc.py ddc.py dds.py rfe.py dsp.py ram.py fir.py
) 

add_custom_target(hdl
    DEPENDS whitebox.v
)

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

add_test(NAME test_whitebox_upsampler_3x_impulse_response
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_whitebox.py TestUpsampler3xImpulseResponse
)

add_test(NAME test_whitebox_fir_impulse_response
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_whitebox.py TestFirImpulseResponse
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

add_subdirectory(constraint)

add_dependencies(hdl constraints)
