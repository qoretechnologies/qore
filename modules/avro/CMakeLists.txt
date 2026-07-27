# avro module CMake configuration
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

# The Avro schema parser consumes the json module's C++ API (<qore/QoreJsonApi.h>), resolved at
# run time rather than linked, so there is no build-time dependency on the json module here.  zlib
# provides the container-file "deflate" codec and OpenSSL provides random sync markers; both are
# already hard libqore dependencies, so no new dependency is introduced.  The Avro binary codec
# itself is hand-written -- see design/avro-module.md for why Apache avro-cpp is not vendored.
include_directories(${OPENSSL_INCLUDE_DIR})
include_directories(${ZLIB_INCLUDE_DIRS})

# QPP source files
set(AVRO_QPP_SRC
    src/QC_AvroSchema.qpp
    src/QC_AvroFileReader.qpp
    src/QC_AvroFileWriter.qpp
    src/ql_avro.qpp
)

# C++ source files
set(AVRO_CPP_SRC
    src/avro-module.cpp
    src/AvroCppApi.cpp
    src/AvroSchema.cpp
    src/AvroDecimal.cpp
    src/AvroDecoder.cpp
    src/AvroResolve.cpp
    src/AvroEncoder.cpp
    src/AvroContainer.cpp
)

set(QORE_DOX_AVRO_TMPL_SRC
    docs/mainpage.dox.tmpl
    docs/getting-started.dox.tmpl
    docs/cookbook.dox.tmpl
    docs/schema-guide.dox.tmpl
    docs/release-notes.dox.tmpl
)

# Process QPP files
qore_wrap_qpp_value(AVRO_QPP_SOURCES DOXLIST _dox_src ${AVRO_QPP_SRC})
add_custom_target(AVRO_QPP_GENERATED_FILES DEPENDS qpp ${AVRO_QPP_SOURCES})

# Create the module library
set(avro_module_name "avro")

add_library(${avro_module_name} MODULE ${AVRO_CPP_SRC} ${AVRO_QPP_SOURCES})
target_link_libraries(${avro_module_name} ${OPENSSL_LIBRARIES} ${ZLIB_LIBRARIES})
add_dependencies(${avro_module_name} libqore AVRO_QPP_GENERATED_FILES)

set(MODULE_DOX_INPUT ${CMAKE_BINARY_DIR}/modules/avro/mainpage.dox
    ${CMAKE_BINARY_DIR}/modules/avro/getting-started.dox
    ${CMAKE_BINARY_DIR}/modules/avro/cookbook.dox
    ${CMAKE_BINARY_DIR}/modules/avro/schema-guide.dox
    ${CMAKE_BINARY_DIR}/modules/avro/release-notes.dox
    ${_dox_src})
qore_binary_module_qore(${avro_module_name} "${VERSION}" "/${VERSION}")

if (DOXYGEN_FOUND)
    qore_wrap_dox(QORE_DOX_AVRO_SRC ${QORE_DOX_AVRO_TMPL_SRC})
    add_custom_target(QORE_AVRO_MOD_DOX_FILES DEPENDS ${QORE_DOX_AVRO_SRC})
    add_dependencies(docs-avro QORE_AVRO_MOD_DOX_FILES)
endif()
