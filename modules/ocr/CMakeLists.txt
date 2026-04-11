# OCR module CMake configuration
#
# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.

# tesseract is required for OCR
find_package(PkgConfig QUIET)
set(TESSERACT_FOUND FALSE)
if (PkgConfig_FOUND)
    pkg_check_modules(TESSERACT QUIET tesseract)
endif()
if (NOT TESSERACT_FOUND)
    find_library(TESSERACT_LIBRARY NAMES tesseract)
    find_path(TESSERACT_INCLUDE_DIR tesseract/baseapi.h)
    if (TESSERACT_LIBRARY AND TESSERACT_INCLUDE_DIR)
        set(TESSERACT_FOUND TRUE)
        set(TESSERACT_LIBRARIES ${TESSERACT_LIBRARY})
        set(TESSERACT_INCLUDE_DIRS ${TESSERACT_INCLUDE_DIR})
    endif()
endif()

if (NOT TESSERACT_FOUND)
    message(STATUS "tesseract not found - ocr module will NOT be built")
    return()
endif()

# leptonica is required for image handling
set(LEPTONICA_FOUND FALSE)
if (PkgConfig_FOUND)
    pkg_check_modules(LEPTONICA QUIET lept)
endif()
if (NOT LEPTONICA_FOUND)
    find_library(LEPTONICA_LIBRARY NAMES leptonica lept)
    find_path(LEPTONICA_INCLUDE_DIR leptonica/allheaders.h)
    if (LEPTONICA_LIBRARY AND LEPTONICA_INCLUDE_DIR)
        set(LEPTONICA_FOUND TRUE)
        set(LEPTONICA_LIBRARIES ${LEPTONICA_LIBRARY})
        set(LEPTONICA_INCLUDE_DIRS ${LEPTONICA_INCLUDE_DIR})
    endif()
endif()

if (NOT LEPTONICA_FOUND)
    message(STATUS "leptonica not found - ocr module will NOT be built")
    return()
endif()

message(STATUS "tesseract + leptonica found - building ocr module")

# Start of hacks to simulate installed QoreConfig.cmake
if (NOT QORE_MODULES_DIR)
    set(QORE_MODULES_DIR ${MODULE_DIR})
endif()

if (NOT QORE_LIBRARY)
    set(QORE_LIBRARY libqore)
endif()

set(QORE_API_VERSION ${MODULE_API_MAJOR}.${MODULE_API_MINOR})

include(${CMAKE_SOURCE_DIR}/cmake/QoreMacros.cmake)
### end of hacks

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_BINARY_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/include)

# QPP source files
set(OCR_QPP_SRC
    src/QC_TesseractEngine.qpp
    src/ql_ocr.qpp
)

# C++ source files
set(OCR_CPP_SRC
    src/ocr-module.cpp
    src/TesseractEngine.cpp
)

set(QORE_DOX_OCR_TMPL_SRC
    docs/mainpage.dox.tmpl
)

# Process QPP files
qore_wrap_qpp_value(OCR_QPP_SOURCES DOXLIST _dox_src ${OCR_QPP_SRC})
add_custom_target(OCR_QPP_GENERATED_FILES DEPENDS qpp ${OCR_QPP_SOURCES})

# Create the module library
set(ocr_module_name "ocr")

add_library(${ocr_module_name} MODULE ${OCR_CPP_SRC} ${OCR_QPP_SOURCES})
target_include_directories(${ocr_module_name} PRIVATE
    ${TESSERACT_INCLUDE_DIRS}
    ${LEPTONICA_INCLUDE_DIRS}
)
if (TESSERACT_LIBRARY_DIRS)
    target_link_directories(${ocr_module_name} PRIVATE ${TESSERACT_LIBRARY_DIRS})
endif()
if (LEPTONICA_LIBRARY_DIRS)
    target_link_directories(${ocr_module_name} PRIVATE ${LEPTONICA_LIBRARY_DIRS})
endif()
target_link_libraries(${ocr_module_name} ${TESSERACT_LIBRARIES} ${LEPTONICA_LIBRARIES})
add_dependencies(${ocr_module_name} libqore OCR_QPP_GENERATED_FILES)

set(MODULE_DOX_INPUT ${CMAKE_BINARY_DIR}/modules/ocr/mainpage.dox ${_dox_src})
qore_binary_module_qore(${ocr_module_name} "${VERSION}" "/${VERSION}")

if (DOXYGEN_FOUND)
    qore_wrap_dox(QORE_DOX_OCR_SRC ${QORE_DOX_OCR_TMPL_SRC})
    add_custom_target(QORE_OCR_MOD_DOX_FILES DEPENDS ${QORE_DOX_OCR_SRC})
    add_dependencies(docs-ocr QORE_OCR_MOD_DOX_FILES)
endif()
