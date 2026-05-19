# sshutil module CMake configuration
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

set(SSHUTIL_QPP_SRC
    src/ql_sshutil.qpp
    src/QC_AbstractSshClientIdentityProvider.qpp
    src/QC_AbstractSshServerHostKeyProvider.qpp
    src/QC_AbstractSshHostKeyStore.qpp
    src/QC_FilesystemSshClientIdentityProvider.qpp
    src/QC_FilesystemSshServerHostKeyProvider.qpp
    src/QC_SshUtilRegistry.qpp
)

set(SSHUTIL_CPP_SRC
    src/sshutil-module.cpp
    src/sshutil.cpp
)

set(QORE_DOX_SSHUTIL_TMPL_SRC
    docs/mainpage.dox.tmpl
)

qore_wrap_qpp_value(SSHUTIL_QPP_SOURCES DOXLIST _dox_src ${SSHUTIL_QPP_SRC})
add_custom_target(SSHUTIL_QPP_GENERATED_FILES DEPENDS qpp ${SSHUTIL_QPP_SOURCES})

set(sshutil_module_name "sshutil")

add_library(${sshutil_module_name} MODULE ${SSHUTIL_CPP_SRC} ${SSHUTIL_QPP_SOURCES})
add_dependencies(${sshutil_module_name} libqore SSHUTIL_QPP_GENERATED_FILES)

set(MODULE_DOX_INPUT ${CMAKE_BINARY_DIR}/modules/sshutil/mainpage.dox ${_dox_src})
qore_binary_module_qore(${sshutil_module_name} "${VERSION}" "/${VERSION}")

if (DOXYGEN_FOUND)
    qore_wrap_dox(QORE_DOX_SSHUTIL_SRC ${QORE_DOX_SSHUTIL_TMPL_SRC})
    add_custom_target(QORE_SSHUTIL_MOD_DOX_FILES DEPENDS ${QORE_DOX_SSHUTIL_SRC})
    add_dependencies(docs-sshutil QORE_SSHUTIL_MOD_DOX_FILES)
endif()
