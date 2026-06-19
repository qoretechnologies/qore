# Ensure the in-tree source qmod symlinks point at the built AOT qmods.
#
# The in-tree test suite anchors module lookup at the source qlib/ directory
# (QORE_MODULE_DIR=.../qlib or %prepend-module-path).  For the loader to prefer
# the AOT artifact over the .qm source, a symlink qlib/<X>/<X>.qmod (split-dir
# module) or qlib/<X>.qmod (flat module) must point at build/qlib-qmod/<...>.qmod.
#
# Those symlinks are created per-qmod when a qmod target builds, but the
# qcc-format-change cleanup removes them and recreation only happens if the
# corresponding qmod target actually rebuilds.  A partial build (or up-to-date
# qmods) after a format-changing change can therefore leave a module without its
# symlink, silently falling back to slow source parsing at load time.
#
# This script is idempotent and self-healing: it (re)creates any missing/incorrect
# symlink from the already-built qmods and removes orphaned symlinks.  It only ever
# touches symlinks — never real .qm or .qmod files — so it is safe to run on every
# configure and every build.
#
# Inputs:
#   QORE_SOURCE_QLIB_DIR - the source qlib/ directory
#   QORE_BUILD_QMOD_DIR  - the build qlib-qmod/ directory holding the built qmods

if(NOT DEFINED QORE_SOURCE_QLIB_DIR OR NOT DEFINED QORE_BUILD_QMOD_DIR)
    message(FATAL_ERROR "QORE_SOURCE_QLIB_DIR and QORE_BUILD_QMOD_DIR must be defined")
endif()

if(NOT IS_DIRECTORY "${QORE_BUILD_QMOD_DIR}")
    return()  # nothing built yet (e.g. first configure)
endif()

set(_created 0)
set(_fixed 0)
set(_removed 0)

# Map each built qmod to its source-tree symlink (same relative path under qlib/).
file(GLOB_RECURSE _built_qmods LIST_DIRECTORIES false RELATIVE "${QORE_BUILD_QMOD_DIR}"
     "${QORE_BUILD_QMOD_DIR}/*.qmod")
foreach(_rel IN LISTS _built_qmods)
    set(_qmod "${QORE_BUILD_QMOD_DIR}/${_rel}")
    set(_link "${QORE_SOURCE_QLIB_DIR}/${_rel}")
    get_filename_component(_link_dir "${_link}" DIRECTORY)
    # only place a symlink where the source module actually lives
    if(NOT IS_DIRECTORY "${_link_dir}")
        continue()
    endif()
    if(IS_SYMLINK "${_link}")
        file(READ_SYMLINK "${_link}" _cur)
        if(NOT IS_ABSOLUTE "${_cur}")
            get_filename_component(_cur "${_link_dir}/${_cur}" ABSOLUTE)
        endif()
        if(_cur STREQUAL "${_qmod}")
            continue()  # already correct
        endif()
        file(REMOVE "${_link}")
        file(CREATE_LINK "${_qmod}" "${_link}" SYMBOLIC)
        math(EXPR _fixed "${_fixed} + 1")
    elseif(EXISTS "${_link}")
        continue()  # a real file lives here — never clobber source
    else()
        file(CREATE_LINK "${_qmod}" "${_link}" SYMBOLIC)
        math(EXPR _created "${_created} + 1")
    endif()
endforeach()

# Remove orphaned symlinks (target qmod no longer exists).
file(GLOB_RECURSE _src_qmods LIST_DIRECTORIES false "${QORE_SOURCE_QLIB_DIR}/*.qmod")
foreach(_sq IN LISTS _src_qmods)
    if(IS_SYMLINK "${_sq}")
        get_filename_component(_real "${_sq}" REALPATH)
        if(NOT EXISTS "${_real}")
            file(REMOVE "${_sq}")
            math(EXPR _removed "${_removed} + 1")
        endif()
    endif()
endforeach()

if(_created OR _fixed OR _removed)
    message(STATUS "Source qmod symlinks ensured: ${_created} created, ${_fixed} fixed, ${_removed} orphan(s) removed")
endif()
