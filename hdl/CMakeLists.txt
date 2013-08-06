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

add_custom_command(
    OUTPUT signalgenerator.v
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/signalgenerator.py
    DEPENDS signalgenerator.py
) 

add_custom_command(
    OUTPUT exciter.v
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/exciter.py
    DEPENDS exciter.py apb3_utils.py fifo.py
) 

add_custom_command(
    OUTPUT receiver.v
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/receiver.py
    DEPENDS receiver.py apb3_utils.py fifo.py dds.py
) 

add_custom_target(hdl
    DEPENDS signalgenerator.v exciter.v receiver.v
)

add_subdirectory(constraint)

add_dependencies(hdl constraint)
