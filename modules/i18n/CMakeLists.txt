# I18n module CMake configuration
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

if (NOT ICU_FOUND)
    find_package(PkgConfig QUIET)
    if (PkgConfig_FOUND)
        pkg_check_modules(ICU QUIET icu-uc icu-i18n)
    endif()

    if (NOT ICU_FOUND)
        find_package(ICU COMPONENTS uc i18n REQUIRED)
    endif()
endif()

message(STATUS "ICU found - building i18n module")

# Bootstrap values normally provided by installed QoreConfig.cmake.
if (NOT QORE_MODULES_DIR)
    set(QORE_MODULES_DIR ${MODULE_DIR})
endif()

if (NOT QORE_LIBRARY)
    set(QORE_LIBRARY libqore)
endif()

set(QORE_API_VERSION ${MODULE_API_MAJOR}.${MODULE_API_MINOR})

include(${CMAKE_SOURCE_DIR}/cmake/QoreMacros.cmake)
### end of bootstrap values

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_BINARY_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/include)

set(I18N_QPP_SRC
    src/QC_MessageCatalog.qpp
    src/ql_i18n.qpp
)

set(I18N_CPP_SRC
    src/i18n-catalog-json.cpp
    src/i18n-module.cpp
)

set(QORE_DOX_I18N_TMPL_SRC
    docs/catalogs.dox.tmpl
    docs/cli.dox.tmpl
    docs/locale-formatting.dox.tmpl
    docs/mainpage.dox.tmpl
    docs/plural.dox.tmpl
)

qore_wrap_qpp_value(I18N_QPP_SOURCES DOXLIST _dox_src ${I18N_QPP_SRC})
add_custom_target(I18N_QPP_GENERATED_FILES DEPENDS qpp ${I18N_QPP_SOURCES})

set(i18n_module_name "i18n")

add_library(${i18n_module_name} MODULE ${I18N_CPP_SRC} ${I18N_QPP_SOURCES})
target_include_directories(${i18n_module_name} PRIVATE ${ICU_INCLUDE_DIRS})
if (ICU_LIBRARY_DIRS)
    target_link_directories(${i18n_module_name} PRIVATE ${ICU_LIBRARY_DIRS})
endif()
target_link_libraries(${i18n_module_name} ${ICU_LIBRARIES})
add_dependencies(${i18n_module_name} libqore I18N_QPP_GENERATED_FILES)

if (DOXYGEN_FOUND)
    qore_wrap_dox(QORE_DOX_I18N_SRC ${QORE_DOX_I18N_TMPL_SRC})
    set(MODULE_DOX_INPUT ${QORE_DOX_I18N_SRC} ${_dox_src})
else()
    set(MODULE_DOX_INPUT ${CMAKE_BINARY_DIR}/modules/i18n/mainpage.dox ${_dox_src})
endif()

qore_binary_module_qore(${i18n_module_name} "${VERSION}" "/${VERSION}")

if (DOXYGEN_FOUND)
    add_custom_target(QORE_I18N_MOD_DOX_FILES DEPENDS ${QORE_DOX_I18N_SRC})
    add_dependencies(docs-i18n QORE_I18N_MOD_DOX_FILES)
endif()
