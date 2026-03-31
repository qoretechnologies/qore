# Tokenizer module CMake configuration
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

# utf8proc is required for Unicode normalization
find_package(PkgConfig QUIET)
set(UTF8PROC_FOUND FALSE)
if (PkgConfig_FOUND)
    pkg_check_modules(UTF8PROC QUIET libutf8proc)
endif()
if (NOT UTF8PROC_FOUND)
    find_library(UTF8PROC_LIBRARY NAMES utf8proc)
    find_path(UTF8PROC_INCLUDE_DIR utf8proc.h)
    if (UTF8PROC_LIBRARY AND UTF8PROC_INCLUDE_DIR)
        set(UTF8PROC_FOUND TRUE)
        set(UTF8PROC_LIBRARIES ${UTF8PROC_LIBRARY})
        set(UTF8PROC_INCLUDE_DIRS ${UTF8PROC_INCLUDE_DIR})
    endif()
endif()

if (NOT UTF8PROC_FOUND)
    message(STATUS "utf8proc not found - tokenizer module will NOT be built")
    return()
endif()

message(STATUS "utf8proc found - building tokenizer module")

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
set(TOK_QPP_SRC
    src/QC_HFTokenizer.qpp
    src/ql_tokenizer.qpp
)

# C++ source files
set(TOK_CPP_SRC
    src/tokenizer-module.cpp
    src/tokenizer-c-api.cpp
    src/HFTokenizer.cpp
    src/normalizers/AbstractNormalizer.cpp
    src/normalizers/BertNormalizer.cpp
    src/normalizers/SequenceNormalizer.cpp
    src/normalizers/UnicodeNormalizer.cpp
    src/normalizers/ReplaceNormalizer.cpp
    src/normalizers/PrependNormalizer.cpp
    src/normalizers/PrecompiledNormalizer.cpp
    src/pretokenizers/AbstractPreTokenizer.cpp
    src/pretokenizers/BertPreTokenizer.cpp
    src/pretokenizers/ByteLevelPreTokenizer.cpp
    src/pretokenizers/MetaspacePreTokenizer.cpp
    src/pretokenizers/WhitespacePreTokenizer.cpp
    src/pretokenizers/SequencePreTokenizer.cpp
    src/models/AbstractTokenizerModel.cpp
    src/models/BPEModel.cpp
    src/models/WordPieceModel.cpp
    src/models/UnigramModel.cpp
    src/postprocessors/AbstractPostProcessor.cpp
    src/postprocessors/TemplateProcessor.cpp
    src/decoders/AbstractDecoder.cpp
    src/decoders/WordPieceDecoder.cpp
    src/decoders/ByteLevelDecoder.cpp
    src/decoders/MetaspaceDecoder.cpp
    src/decoders/ByteFallbackDecoder.cpp
    src/decoders/SequenceDecoder.cpp
    src/utils/byte_level_mapping.cpp
    src/utils/darts_trie.cpp
)

set(QORE_DOX_TOK_TMPL_SRC
    docs/mainpage.dox.tmpl
)

# Process QPP files
qore_wrap_qpp_value(TOK_QPP_SOURCES DOXLIST _dox_src ${TOK_QPP_SRC})
add_custom_target(TOK_QPP_GENERATED_FILES DEPENDS qpp ${TOK_QPP_SOURCES})

# Create the module library
set(tok_module_name "tokenizer")

add_library(${tok_module_name} MODULE ${TOK_CPP_SRC} ${TOK_QPP_SOURCES})
target_include_directories(${tok_module_name} PRIVATE ${UTF8PROC_INCLUDE_DIRS})
if (UTF8PROC_LIBRARY_DIRS)
    target_link_directories(${tok_module_name} PRIVATE ${UTF8PROC_LIBRARY_DIRS})
endif()
target_link_libraries(${tok_module_name} ${UTF8PROC_LIBRARIES})
add_dependencies(${tok_module_name} libqore TOK_QPP_GENERATED_FILES)

set(MODULE_DOX_INPUT ${CMAKE_BINARY_DIR}/modules/tokenizer/mainpage.dox ${_dox_src})
qore_binary_module_qore(${tok_module_name} "${VERSION}" "/${VERSION}")

if (DOXYGEN_FOUND)
    qore_wrap_dox(QORE_DOX_TOK_SRC ${QORE_DOX_TOK_TMPL_SRC})
    add_custom_target(QORE_TOK_MOD_DOX_FILES DEPENDS ${QORE_DOX_TOK_SRC})
    add_dependencies(docs-tokenizer QORE_TOK_MOD_DOX_FILES)
endif()
