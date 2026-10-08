# Copyright (C) 2026 Qore Technologies, s.r.o.
# Standalone CMake script to update git-revision.h with the source revision.
# Runs at build time (not configure time) so the embedded git hash stays in
# sync with the source after pulling new commits without re-running cmake.
#
# Required variables (passed via -D):
#   SOURCE_DIR  — path to the Qore working tree or extracted source archive
#   OUTPUT_FILE — path to the git-revision.h file to write/update

if(NOT DEFINED SOURCE_DIR OR "${SOURCE_DIR}" STREQUAL ""
        OR NOT DEFINED OUTPUT_FILE OR "${OUTPUT_FILE}" STREQUAL "")
    message(FATAL_ERROR "UpdateGitRevision.cmake requires -DSOURCE_DIR=... -DOUTPUT_FILE=...")
endif()

if(NOT IS_DIRECTORY "${SOURCE_DIR}")
    message(FATAL_ERROR "SOURCE_DIR is not a directory: ${SOURCE_DIR}")
endif()

# Never discover a parent repository when building an extracted archive inside
# another checkout. A linked Git worktree has a .git file rather than a directory.
set(GIT_REV "unknown")
set(_has_revision FALSE)
if(EXISTS "${SOURCE_DIR}/.git")
    set(_has_revision TRUE)
    find_package(Git QUIET)
    if(NOT GIT_FOUND)
        message(FATAL_ERROR "Git is needed to read the working tree revision")
    endif()
    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --verify "HEAD^{commit}"
                    WORKING_DIRECTORY "${SOURCE_DIR}"
                    RESULT_VARIABLE _git_result
                    OUTPUT_VARIABLE GIT_REV
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                    ERROR_VARIABLE _git_error)
    if(NOT "${_git_result}" STREQUAL "0")
        message(FATAL_ERROR "Cannot read the working tree revision: ${_git_error}")
    endif()
elseif(EXISTS "${SOURCE_DIR}/.git-archive-revision")
    file(STRINGS "${SOURCE_DIR}/.git-archive-revision" _archive_lines REGEX "^[^#].+")
    list(LENGTH _archive_lines _archive_count)
    if(NOT _archive_count EQUAL 1)
        message(FATAL_ERROR "Invalid .git-archive-revision: expected one revision")
    endif()
    list(GET _archive_lines 0 _archive_revision)
    if(NOT "${_archive_revision}" STREQUAL "$Format:%H$")
        set(GIT_REV "${_archive_revision}")
        set(_has_revision TRUE)
    endif()
endif()

if(_has_revision)
    string(LENGTH "${GIT_REV}" _revision_length)
    if(NOT GIT_REV MATCHES "^[0-9a-f]+$"
            OR NOT (_revision_length EQUAL 40 OR _revision_length EQUAL 64))
        message(FATAL_ERROR "Invalid source revision: ${GIT_REV}")
    endif()
endif()

set(_new_content "#define BUILD \"${GIT_REV}\"\n")

# Only rewrite if content changed — avoids spurious rebuilds of TUs that
# include git-revision.h
set(_needs_write TRUE)
if(EXISTS "${OUTPUT_FILE}")
    file(READ "${OUTPUT_FILE}" _existing_content)
    if("${_existing_content}" STREQUAL "${_new_content}")
        set(_needs_write FALSE)
    endif()
endif()

if(_needs_write)
    file(WRITE "${OUTPUT_FILE}" "${_new_content}")
    message(STATUS "Updated git-revision.h: ${GIT_REV}")
endif()
