# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT

include(${CMAKE_CURRENT_LIST_DIR}/QoreInstallRelativeDir.cmake)

# Generate separate build-tree and installed SDK exports. Build-tree paths must
# not be embedded in the installed file, even inside inactive CMake branches.
# Sets _out_var to the value that the installed QoreConfig.cmake gives an absolute install directory: below the
# configured prefix, the path below the prefix that the installed file computes from its location (qore#7849)
function(_qore_config_install_path _out_var _dir)
    if (QORE_CONFIG_RELOCATABLE)
        qore_install_relative_dir(_qore_rel "${_dir}")
        if (NOT IS_ABSOLUTE "${_qore_rel}")
            set(${_out_var} "\${_qore_install_prefix}/${_qore_rel}" PARENT_SCOPE)
            return()
        endif()
    endif()
    set(${_out_var} "${_dir}" PARENT_SCOPE)
endfunction()

# Sets the QORE_CONFIG_* variables of QoreConfig.cmake.in: the absolute paths for the build tree's file; for the
# installed file (_installed TRUE), paths that follow the location of the installed file (qore#7849)
macro(_qore_config_set_paths _installed)
    set(QORE_CONFIG_PREFIX_SETUP "")
    set(QORE_CONFIG_RELOCATABLE FALSE)
    if (${_installed} AND NOT IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
        # QoreConfig.cmake is installed in <libdir>/cmake/Qore
        file(RELATIVE_PATH _qore_config_to_prefix "${CMAKE_INSTALL_FULL_LIBDIR}/cmake/Qore" "${CMAKE_INSTALL_PREFIX}")
        set(QORE_CONFIG_PREFIX_SETUP
            "get_filename_component(_qore_install_prefix \"\${CMAKE_CURRENT_LIST_DIR}/${_qore_config_to_prefix}\" ABSOLUTE)")
        set(QORE_CONFIG_RELOCATABLE TRUE)
    endif()
    _qore_config_install_path(QORE_CONFIG_INCLUDEDIR "${CMAKE_INSTALL_FULL_INCLUDEDIR}")
    _qore_config_install_path(QORE_CONFIG_LIBDIR "${CMAKE_INSTALL_FULL_LIBDIR}")
    _qore_config_install_path(QORE_CONFIG_DATADIR "${CMAKE_INSTALL_FULL_DATADIR}")
    _qore_config_install_path(QORE_CONFIG_BINDIR "${CMAKE_INSTALL_FULL_BINDIR}")
    _qore_config_install_path(QORE_CONFIG_MODULE_DIR "${MODULE_DIR}")
    _qore_config_install_path(QORE_CONFIG_USER_MODULE_DIR "${USER_MODULE_DIR}")
    _qore_config_install_path(QORE_CONFIG_USER_MODULE_VER_DIR "${USER_MODULE_VER_DIR}")
    _qore_config_install_path(QORE_CONFIG_CATALOG_DIR "${QORE_CATALOG_DIR}")
    if (QORE_CONFIG_RELOCATABLE)
        set(QORE_CONFIG_PREFIX "\${_qore_install_prefix}")
    else()
        set(QORE_CONFIG_PREFIX "${myprefix}")
    endif()
endmacro()

function(qore_configure_package_configs)
    set(_templates "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")
    configure_file("${_templates}/QoreBuildTree.cmake.in"
        "${CMAKE_BINARY_DIR}/cmake/QoreBuildTree.cmake" @ONLY)
    # a build tree loaded from another path is reported as not found, with the reason (see QoreBuildTree.cmake.in)
    set(QORE_CONFIG_BUILD_TREE_SETUP
        "include(\"\${CMAKE_CURRENT_LIST_DIR}/QoreBuildTree.cmake\")
if (_qore_build_tree_moved)
    set(QORE_FOUND 0)
    set(Qore_FOUND FALSE)
    set(Qore_NOT_FOUND_MESSAGE \"\${_qore_build_tree_moved_message}\")
    return()
endif()")
    _qore_config_set_paths(FALSE)
    configure_file("${_templates}/QoreConfig.cmake.in"
        "${CMAKE_BINARY_DIR}/cmake/QoreConfig.cmake" @ONLY)
    set(QORE_CONFIG_BUILD_TREE_SETUP "")
    _qore_config_set_paths(TRUE)
    configure_file("${_templates}/QoreConfig.cmake.in"
        "${CMAKE_BINARY_DIR}/cmake/install/QoreConfig.cmake" @ONLY)
endfunction()
