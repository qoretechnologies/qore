# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
#
# Removes the files of a binary module that were built for another module API from its output directory.
#
# A binary module is built as <name>-api-<major>.<minor>.qmod, with the module API of the Qore headers that it is
# compiled with (design/module-api-version.md).  When the API changes, CMake writes the module under the new name but
# never removes the file with the old one, which stays in an incrementally built tree: tools that look for the
# module's file by a pattern then find two files (or the old one), and Qore refuses the old one.  This script runs
# after each build of the module (qore_remove_stale_module_api_files() in QoreMacros.cmake) and removes every
# <name>-api-*.qmod file in the directory except the one just built.
#
# Arguments:
#   -DDIR=<the directory of the module file>
#   -DNAME=<the module file name without the -api-* suffix>
#   -DKEEP=<the file name of the module just built>

foreach (_arg DIR NAME KEEP)
    if ("${${_arg}}" STREQUAL "")
        message(FATAL_ERROR "QoreRemoveStaleModuleApiFiles.cmake: -D${_arg}=... is required")
    endif()
endforeach()

file(GLOB _qore_stale_candidates LIST_DIRECTORIES false "${DIR}/${NAME}-api-*.qmod")
foreach (_file IN LISTS _qore_stale_candidates)
    get_filename_component(_name "${_file}" NAME)
    # the name must be <NAME>-api-<major>.<minor>.qmod exactly: the glob also matches another module whose name
    # starts with <NAME>-api- (ex: "foo-api-x" for module "foo")
    string(LENGTH "${NAME}-api-" _prefix_len)
    string(SUBSTRING "${_name}" ${_prefix_len} -1 _rest)
    if (NOT _name STREQUAL KEEP AND _rest MATCHES "^[0-9]+\\.[0-9]+\\.qmod$")
        message(STATUS "Removing ${_file}: built for another module API than ${KEEP}")
        file(REMOVE "${_file}")
    endif()
endforeach()
