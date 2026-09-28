# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT

# Generate separate build-tree and installed SDK exports. Build-tree paths must
# not be embedded in the installed file, even inside inactive CMake branches.
function(qore_configure_package_configs)
    set(_templates "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")
    configure_file("${_templates}/QoreBuildTree.cmake.in"
        "${CMAKE_BINARY_DIR}/cmake/QoreBuildTree.cmake" @ONLY)
    set(QORE_CONFIG_BUILD_TREE_SETUP
        "include(\"\${CMAKE_CURRENT_LIST_DIR}/QoreBuildTree.cmake\")")
    configure_file("${_templates}/QoreConfig.cmake.in"
        "${CMAKE_BINARY_DIR}/cmake/QoreConfig.cmake" @ONLY)
    set(QORE_CONFIG_BUILD_TREE_SETUP "")
    configure_file("${_templates}/QoreConfig.cmake.in"
        "${CMAKE_BINARY_DIR}/cmake/install/QoreConfig.cmake" @ONLY)
endfunction()
