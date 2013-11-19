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

#add_custom_command(
#    OUTPUT signalgenerator.v
#    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/signalgenerator.py
#    DEPENDS signalgenerator.py
#) 

add_custom_command(
    OUTPUT exciter.v
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/exciter.py
    DEPENDS exciter.py apb3_utils.py fifo.py duc.py dds.py rfe.py
) 

#add_custom_command(
#    OUTPUT receiver.v
#    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/receiver.py
#    DEPENDS receiver.py apb3_utils.py fifo.py dds.py
#) 

add_custom_target(hdl
    DEPENDS exciter.v #receiver.v signalgenerator.v
)

add_test(NAME test_apb3_simple_slave
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/apb3_utils.py
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

add_test(NAME test_exciter_apb3_transaction
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_exciter.py TestApb3Transaction
)
    

add_test(NAME test_exciter_dds
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_exciter.py TestDDS
)

add_test(NAME test_exciter_overrun_underrun
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_exciter.py TestOverrunUnderrun
)

add_test(NAME test_exciter_halt
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_exciter.py TestHalt
)

add_test(NAME test_exciter_pipe_samples
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_exciter.py TestPipeSamples
)

add_subdirectory(constraint)

add_dependencies(hdl constraints)
