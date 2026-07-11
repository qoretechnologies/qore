# Copyright (C) 2026 Qore Technologies, s.r.o.

# Ensures that in-tree qmod symlinks point to the built AOT modules.
#
# The in-tree test suite anchors module lookup at the source qlib/ directory.
# Per-qmod build rules normally create these links, but format cleanup and CI
# artifact unpacking can leave built qmods without their source-tree links.
# This script is idempotent and only replaces symlinks, never real files.
#
# Inputs:
#   QORE_SOURCE_QLIB_DIR - source qlib directory
#   QORE_BUILD_QMOD_DIR  - build directory containing the AOT qmods

if (NOT DEFINED QORE_SOURCE_QLIB_DIR OR NOT DEFINED QORE_BUILD_QMOD_DIR)
    message(FATAL_ERROR "QORE_SOURCE_QLIB_DIR and QORE_BUILD_QMOD_DIR must be defined")
endif()

if (NOT IS_DIRECTORY "${QORE_SOURCE_QLIB_DIR}")
    message(FATAL_ERROR "QORE_SOURCE_QLIB_DIR is not a directory: ${QORE_SOURCE_QLIB_DIR}")
endif()

if (NOT IS_DIRECTORY "${QORE_BUILD_QMOD_DIR}")
    return()
endif()

set(_created 0)
set(_fixed 0)
set(_removed 0)

# Flat modules have the same relative path in qlib and qlib-qmod. Split
# modules likewise use <name>/<name>.qmod in both trees.
file(GLOB_RECURSE _built_qmods LIST_DIRECTORIES false
    RELATIVE "${QORE_BUILD_QMOD_DIR}" "${QORE_BUILD_QMOD_DIR}/*.qmod")
foreach(_rel IN LISTS _built_qmods)
    set(_qmod "${QORE_BUILD_QMOD_DIR}/${_rel}")
    set(_link "${QORE_SOURCE_QLIB_DIR}/${_rel}")
    get_filename_component(_link_dir "${_link}" DIRECTORY)
    get_filename_component(_name "${_link}" NAME_WE)

    # Do not create links for qmods built from sources outside qlib.
    if (NOT EXISTS "${_link_dir}/${_name}.qm")
        continue()
    endif()

    if (IS_SYMLINK "${_link}")
        file(READ_SYMLINK "${_link}" _current)
        if (NOT IS_ABSOLUTE "${_current}")
            get_filename_component(_current "${_link_dir}/${_current}" ABSOLUTE)
        endif()
        if ("${_current}" STREQUAL "${_qmod}")
            continue()
        endif()
        file(REMOVE "${_link}")
        file(CREATE_LINK "${_qmod}" "${_link}" SYMBOLIC)
        math(EXPR _fixed "${_fixed} + 1")
    elseif (EXISTS "${_link}")
        # Never replace a real file.
        continue()
    else()
        file(CREATE_LINK "${_qmod}" "${_link}" SYMBOLIC)
        math(EXPR _created "${_created} + 1")
    endif()
endforeach()

# Remove generated links whose build artifact no longer exists. Real files are
# never removed.
file(GLOB_RECURSE _source_qmods LIST_DIRECTORIES false
    "${QORE_SOURCE_QLIB_DIR}/*.qmod")
foreach(_link IN LISTS _source_qmods)
    if (IS_SYMLINK "${_link}")
        get_filename_component(_target "${_link}" REALPATH)
        if (NOT EXISTS "${_target}")
            file(REMOVE "${_link}")
            math(EXPR _removed "${_removed} + 1")
        endif()
    endif()
endforeach()

if (_created OR _fixed OR _removed)
    message(STATUS
        "Source qmod symlinks ensured: ${_created} created, ${_fixed} fixed, ${_removed} removed")
endif()
