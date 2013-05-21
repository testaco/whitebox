# First, generate the doxygen files
find_package(Doxygen REQUIRED)

set(DOXYGEN_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/_doxygen")

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in"
    "${DOXYGEN_BUILD_DIR}/Doxyfile"
    @ONLY
)

add_custom_target(whitebox_doxygen_docs
    ${DOXYGEN_EXECUTABLE}
    WORKING_DIRECTORY ${DOXYGEN_BUILD_DIR}
    COMMENT "Building Doxygen XML"
)

# Now, compile everything together with Sphinx
find_package(Sphinx REQUIRED)

if(NOT DEFINED SPHINX_THEME)
    set(SPHINX_THEME default)
endif()

if(NOT DEFINED SPHINX_THEME_DIR)
    set(SPHINX_THEME_DIR)
endif()

# configured documentation tools and intermediate build results
set(BINARY_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/_build")

# Sphinx cache with pickled ReST documents
set(SPHINX_CACHE_DIR "${CMAKE_CURRENT_BINARY_DIR}/_doctrees")

# HTML output directory
set(SPHINX_HTML_DIR "${CMAKE_CURRENT_BINARY_DIR}/html")

# HTML output directory
set(SPHINX_PDF_DIR "${CMAKE_CURRENT_BINARY_DIR}/pdf")

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/conf.py.in"
    "${BINARY_BUILD_DIR}/conf.py"
    @ONLY
)

add_custom_target(whitebox_html_docs
    ${SPHINX_EXECUTABLE}
    -q -b html
    -c "${BINARY_BUILD_DIR}"
    -d "${SPHINX_CACHE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${SPHINX_HTML_DIR}"
    COMMENT "Building HTML documentation with Sphinx"
)

add_dependencies(whitebox_html_docs DEPENDS whitebox_doxygen_docs)

add_custom_target(whitebox_pdf_docs
    ${SPHINX_EXECUTABLE}
    -q -b latex
    -c "${BINARY_BUILD_DIR}"
    -d "${SPHINX_CACHE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${SPHINX_PDF_DIR}"
    COMMENT "Building PDF documentation with Sphinx"
)

add_dependencies(whitebox_pdf_docs DEPENDS whitebox_doxygen_docs)

add_custom_target(docs)
add_dependencies(docs DEPENDS whitebox_pdf_docs whitebox_html_docs)
