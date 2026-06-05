if(NOT DEFINED QORE_SOURCE_QLIB_DIR)
    message(FATAL_ERROR "QORE_SOURCE_QLIB_DIR is not defined")
endif()

if(NOT IS_DIRECTORY "${QORE_SOURCE_QLIB_DIR}")
    message(FATAL_ERROR "QORE_SOURCE_QLIB_DIR is not a directory: ${QORE_SOURCE_QLIB_DIR}")
endif()

file(GLOB_RECURSE _qore_source_qmods LIST_DIRECTORIES false "${QORE_SOURCE_QLIB_DIR}/*.qmod")
set(_qore_removed_count 0)
foreach(_qore_source_qmod IN LISTS _qore_source_qmods)
    if(IS_SYMLINK "${_qore_source_qmod}")
        file(REMOVE "${_qore_source_qmod}")
        math(EXPR _qore_removed_count "${_qore_removed_count} + 1")
    endif()
endforeach()

message(STATUS "Removed ${_qore_removed_count} generated source qmod symlink(s)")
