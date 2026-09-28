# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
#
# FetchContent package redirects describe targets created in the current configure run. CMake 3.24+
# normally clears their directory before reading CMakeLists.txt, but silently leaves stale redirects
# when it cannot remove the contents (for example after another user created a non-writable directory).
# A stale config can report a dependency as found without creating its targets: Arrow then skips
# building Boost and fails at generation because Boost::headers does not exist.
#
# Include once, before the first project() call or any package discovery. Preserve any surviving
# directory by renaming it, which requires permission on its parent rather than on its contents,
# and recreate the empty directory CMake expects. Do not change package hints or dependency choices.
include_guard(GLOBAL)

function(_qore_reset_package_redirects)
    # An enclosing project may already have registered valid redirects in this run.
    if (NOT CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
        return()
    endif()
    # This variable is only provided in project mode with CMake 3.24 or later.
    if (NOT CMAKE_FIND_PACKAGE_REDIRECTS_DIR)
        return()
    endif()
    file(GLOB _leftovers LIST_DIRECTORIES TRUE "${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}/*")
    if (NOT _leftovers)
        return()
    endif()

    string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef _suffix)
    set(_backup "${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}.stale-${_suffix}")
    if (EXISTS "${_backup}" OR IS_SYMLINK "${_backup}")
        message(FATAL_ERROR "Cannot preserve stale CMake package redirects: '${_backup}' already exists")
    endif()
    file(RENAME "${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}" "${_backup}" RESULT _result)
    if (NOT _result STREQUAL "0")
        message(FATAL_ERROR "Cannot reset stale CMake package redirects in "
            "'${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}': ${_result}. "
            "Make its parent directory writable or use a new build directory.")
    endif()
    file(MAKE_DIRECTORY "${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}")
    message(STATUS "Reset stale CMake package redirects; previous files preserved in ${_backup}")
endfunction()

_qore_reset_package_redirects()
