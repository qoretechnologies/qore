# json module CMake configuration
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

# jsoncons is vendored in-tree (header-only, Boost Software License 1.0).  It is NOT
# discovered from the system: jsoncons is packaged only on Debian/Ubuntu among the
# platforms Qore targets, the json module is now non-optional, and picking up a
# system copy of a different major version silently breaks the JsonSchema and CBOR
# builds.  A pinned copy is the only configuration that is the same everywhere.
# See design/json-module-migration.md.
set(JSONCONS_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third-party/jsoncons/include)
include_directories(${JSONCONS_INCLUDE_DIR})

include_directories(${OPENSSL_INCLUDE_DIR})

# QPP source files
set(JSON_QPP_SRC
    src/QC_JsonRpcClient.qpp
    src/QC_JsonSaxParser.qpp
    src/QC_JsonSchema.qpp
    src/QC_JsonStreamWriter.qpp
    src/ql_json.qpp
    src/ql_cbor.qpp
    src/ql_toon.qpp
)

# C++ source files
set(JSON_CPP_SRC
    src/json-module.cpp
    src/JsonSchemaImpl.cpp
    src/JsonStreamReadHandler.cpp
)

set(QORE_DOX_JSON_TMPL_SRC
    docs/mainpage.dox.tmpl
)

# Process QPP files
qore_wrap_qpp_value(JSON_QPP_SOURCES DOXLIST _dox_src ${JSON_QPP_SRC})
add_custom_target(JSON_QPP_GENERATED_FILES DEPENDS qpp ${JSON_QPP_SOURCES})

# Create the module library
set(json_module_name "json")

add_library(${json_module_name} MODULE ${JSON_CPP_SRC} ${JSON_QPP_SOURCES})
target_link_libraries(${json_module_name} ${OPENSSL_LIBRARIES})
add_dependencies(${json_module_name} libqore JSON_QPP_GENERATED_FILES)

set(MODULE_DOX_INPUT ${CMAKE_BINARY_DIR}/modules/json/mainpage.dox ${_dox_src})
qore_binary_module_qore(${json_module_name} "${VERSION}" "/${VERSION}")

if (DOXYGEN_FOUND)
    qore_wrap_dox(QORE_DOX_JSON_SRC ${QORE_DOX_JSON_TMPL_SRC})
    add_custom_target(QORE_JSON_MOD_DOX_FILES DEPENDS ${QORE_DOX_JSON_SRC})
    add_dependencies(docs-json QORE_JSON_MOD_DOX_FILES)
endif()
