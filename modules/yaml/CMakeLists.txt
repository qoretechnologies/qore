# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT

if (NOT QORE_MODULES_DIR)
    set(QORE_MODULES_DIR ${MODULE_DIR})
endif()
if (NOT QORE_LIBRARY)
    set(QORE_LIBRARY libqore)
endif()
set(QORE_API_VERSION ${MODULE_API_MAJOR}.${MODULE_API_MINOR})
include(${CMAKE_SOURCE_DIR}/cmake/QoreMacros.cmake)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_BINARY_DIR}/include
    ${CMAKE_SOURCE_DIR}/include ${LIBYAML_INCLUDE_DIR})

set(YAML_QPP_SRC
    src/ql_yaml.qpp
    src/QC_YamlSaxParser.qpp
    src/QC_YamlDocumentIterator.qpp
    src/QC_YamlStreamWriter.qpp)
set(YAML_CPP_SRC
    src/QoreYamlEmitter.cpp
    src/QoreYamlParser.cpp
    src/QoreYamlSaxParser.cpp
    src/QoreYamlDocumentIterator.cpp
    src/QoreYamlStreamWriter.cpp
    src/yaml-module.cpp
    src/yaml-scalar-util.cpp
    src/YamlStreamReadHandler.cpp)

qore_wrap_qpp_value(YAML_QPP_SOURCES DOXLIST _dox_src ${YAML_QPP_SRC})
add_custom_target(YAML_QPP_GENERATED_FILES DEPENDS qpp ${YAML_QPP_SOURCES})
add_library(yaml MODULE ${YAML_CPP_SRC} ${YAML_QPP_SOURCES})
target_link_libraries(yaml ${LIBYAML_LIBRARY})
add_dependencies(yaml libqore YAML_QPP_GENERATED_FILES)

set(MODULE_DOX_INPUT ${CMAKE_CURRENT_BINARY_DIR}/mainpage.dox ${_dox_src})
qore_binary_module_qore(yaml "${VERSION}" "/${VERSION}")
if (DOXYGEN_FOUND)
    qore_wrap_dox(QORE_DOX_YAML_SRC docs/mainpage.dox.tmpl)
    add_custom_target(QORE_YAML_MOD_DOX_FILES DEPENDS ${QORE_DOX_YAML_SRC})
    add_dependencies(docs-yaml QORE_YAML_MOD_DOX_FILES)
endif()
