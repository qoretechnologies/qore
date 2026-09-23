if(NOT DEFINED QORE_SOURCE_QLIB_DIR)
    message(FATAL_ERROR "QORE_SOURCE_QLIB_DIR is not defined")
endif()

if(NOT IS_DIRECTORY "${QORE_SOURCE_QLIB_DIR}")
    message(FATAL_ERROR "QORE_SOURCE_QLIB_DIR is not a directory: ${QORE_SOURCE_QLIB_DIR}")
endif()

if(NOT DEFINED QORE_BUILD_QMOD_DIR OR "${QORE_BUILD_QMOD_DIR}" STREQUAL "")
    message(FATAL_ERROR "QORE_BUILD_QMOD_DIR is not defined")
endif()

# Several build trees can share one source tree, and each links the source qlib/ to its own qmods.  A qcc
# format change in one tree invalidates only that tree's qmods, so only links into that tree are removed; a link
# into another build tree belongs to that tree.
get_filename_component(_qore_build_qmod_prefix "${QORE_BUILD_QMOD_DIR}" ABSOLUTE)
set(_qore_build_qmod_prefix "${_qore_build_qmod_prefix}/")

file(GLOB_RECURSE _qore_source_qmods LIST_DIRECTORIES false "${QORE_SOURCE_QLIB_DIR}/*.qmod")
set(_qore_removed_count 0)
foreach(_qore_source_qmod IN LISTS _qore_source_qmods)
    if(NOT IS_SYMLINK "${_qore_source_qmod}")
        continue()
    endif()
    file(READ_SYMLINK "${_qore_source_qmod}" _qore_target)
    if(NOT IS_ABSOLUTE "${_qore_target}")
        get_filename_component(_qore_link_dir "${_qore_source_qmod}" DIRECTORY)
        get_filename_component(_qore_target "${_qore_link_dir}/${_qore_target}" ABSOLUTE)
    endif()
    string(FIND "${_qore_target}" "${_qore_build_qmod_prefix}" _qore_prefix_pos)
    if(_qore_prefix_pos EQUAL 0)
        file(REMOVE "${_qore_source_qmod}")
        math(EXPR _qore_removed_count "${_qore_removed_count} + 1")
    endif()
endforeach()

message(STATUS "Removed ${_qore_removed_count} generated source qmod symlink(s)")
