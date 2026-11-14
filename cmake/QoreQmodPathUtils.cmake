# -*- mode: cmake -*-
#
# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# QoreQmodPathUtils.cmake: path helpers of the scripts that maintain the source tree's qmod links
# (RemoveSourceQmodSymlinks.cmake, EnsureSourceQmodSymlinks.cmake), which include it from their own directory

include_guard(GLOBAL)

# Sets <var> to the physical path of <path> (relative to <base>): the real path of its longest existing ancestor
# followed by the rest of it, so that a target that no longer exists is named like one that does
#
# Limits, which the links that the build writes do not reach (it writes absolute, normalized links):
#   - get_filename_component(ABSOLUTE) collapses ".." as text before any symbolic link is resolved, so a target such
#     as <alias>/../x is judged by its text, not by the directory that <alias>/.. names on the file system
#   - the part of the path beyond a dangling symbolic link (or any other missing component) is kept as text, not
#     resolved; such a target does not exist, so it cannot name an existing qmod of the tree, and a link to a qmod
#     that no longer exists is judged by the real path of its existing ancestors, which is where the tree's qmod
#     was, or it is removed as dangling by EnsureSourceQmodSymlinks.cmake
function(_qore_qmod_physical_path path base var)
    get_filename_component(_path "${path}" ABSOLUTE BASE_DIR "${base}")
    set(_rest "")
    while(NOT EXISTS "${_path}")
        get_filename_component(_parent "${_path}" DIRECTORY)
        if("${_parent}" STREQUAL "${_path}" OR "${_parent}" STREQUAL "")
            break()
        endif()
        get_filename_component(_name "${_path}" NAME)
        set(_rest "/${_name}${_rest}")
        set(_path "${_parent}")
    endwhile()
    get_filename_component(_path "${_path}" REALPATH)
    if("${_path}" STREQUAL "/")
        set(_path "")
    endif()
    set(${var} "${_path}${_rest}" PARENT_SCOPE)
endfunction()
