# Standalone CMake script to update git-revision.h with the current HEAD SHA.
# Runs at build time (not configure time) so the embedded git hash stays in
# sync with the source after pulling new commits without re-running cmake.
#
# Required variables (passed via -D):
#   SOURCE_DIR  — path to the qore git working tree
#   OUTPUT_FILE — path to the git-revision.h file to write/update

if(NOT DEFINED SOURCE_DIR OR NOT DEFINED OUTPUT_FILE)
    message(FATAL_ERROR "UpdateGitRevision.cmake requires -DSOURCE_DIR=... -DOUTPUT_FILE=...")
endif()

find_package(Git QUIET)
if(NOT GIT_FOUND)
    message(FATAL_ERROR "Git is needed to generate git-revision.h")
endif()

execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
                WORKING_DIRECTORY ${SOURCE_DIR}
                OUTPUT_VARIABLE GIT_REV
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET)

if(NOT GIT_REV)
    message(WARNING "git rev-parse HEAD returned no output; leaving git-revision.h unchanged")
    return()
endif()

set(_new_content "#define BUILD \"${GIT_REV}\"\n")

# Only rewrite if content changed — avoids spurious rebuilds of TUs that
# include git-revision.h
set(_needs_write TRUE)
if(EXISTS ${OUTPUT_FILE})
    file(READ ${OUTPUT_FILE} _existing_content)
    if("${_existing_content}" STREQUAL "${_new_content}")
        set(_needs_write FALSE)
    endif()
endif()

if(_needs_write)
    file(WRITE ${OUTPUT_FILE} "${_new_content}")
    message(STATUS "Updated git-revision.h: ${GIT_REV}")
endif()
