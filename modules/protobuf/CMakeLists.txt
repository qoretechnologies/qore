# Protobuf module CMake configuration
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
set(PROTOBUF_QPP_SRC
    src/QC_ProtobufSchema.qpp
)

# C++ source files
set(PROTOBUF_CPP_SRC
    src/protobuf-module.cpp
    src/QoreProtobufSchema.cpp
    src/ProtobufHelper.cpp
)

set(QORE_DOX_PROTOBUF_TMPL_SRC
    docs/mainpage.dox.tmpl
)

# Process QPP files
qore_wrap_qpp_value(PROTOBUF_QPP_SOURCES DOXLIST _dox_src ${PROTOBUF_QPP_SRC})
add_custom_target(PROTOBUF_QPP_GENERATED_FILES DEPENDS qpp ${PROTOBUF_QPP_SOURCES})

# Create the module library
set(protobuf_module_name "protobuf")

add_library(${protobuf_module_name} MODULE ${PROTOBUF_CPP_SRC} ${PROTOBUF_QPP_SOURCES})
target_link_libraries(${protobuf_module_name} ${Protobuf_LIBRARIES})
target_include_directories(${protobuf_module_name} PRIVATE ${Protobuf_INCLUDE_DIRS})
add_dependencies(${protobuf_module_name} libqore PROTOBUF_QPP_GENERATED_FILES)

set(MODULE_DOX_INPUT ${CMAKE_BINARY_DIR}/modules/protobuf/mainpage.dox ${_dox_src})
qore_binary_module_qore(${protobuf_module_name} "${VERSION}" "/${VERSION}")

if (DOXYGEN_FOUND)
    qore_wrap_dox(QORE_DOX_PROTOBUF_SRC ${QORE_DOX_PROTOBUF_TMPL_SRC})
    add_custom_target(QORE_PROTOBUF_MOD_DOX_FILES DEPENDS ${QORE_DOX_PROTOBUF_SRC})
    add_dependencies(docs-protobuf QORE_PROTOBUF_MOD_DOX_FILES)
endif()
