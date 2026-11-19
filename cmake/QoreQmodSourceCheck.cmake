# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# Checks the AOT modules (qmods) of a build against the contents of their dependencies before they are compiled
# (qore#6598): runs `qcc --check-module-sources @<list>` in the environment of the compile, which rewrites the sources
# stamp of each qmod that the module loader would find out of date (see _QORE_REGISTER_QMOD_SOURCE_CHECK() in
# QoreMacros.cmake).  qcc gets none of the settings of the qmod build cache (QORE_QMOD_CACHE_*), as when the cache
# runs it (design/qmod-build-cache.md).  A qmod that cannot be checked fails the build.
#
# -DQCC=<qcc> -DLIST=<file with one qmod per line> -DENV=<'|'-separated NAME=VALUE entries of the compile>

cmake_policy(SET CMP0007 NEW)

if (NOT DEFINED QCC OR "${QCC}" STREQUAL "" OR NOT DEFINED LIST OR "${LIST}" STREQUAL "")
    message(FATAL_ERROR "QoreQmodSourceCheck.cmake: QCC and LIST are required")
endif()

# the settings of the qmod build cache are taken out of the environment of qcc
execute_process(COMMAND "${CMAKE_COMMAND}" -E environment OUTPUT_VARIABLE _environment)
string(REGEX MATCHALL "(^|\n)QORE_QMOD_CACHE_[A-Za-z0-9_]*=" _names "${_environment}")
set(_unset "")
foreach (_name IN LISTS _names)
    string(REGEX REPLACE "^\n?([^=]*)=$" "\\1" _name "${_name}")
    list(APPEND _unset "--unset=${_name}")
endforeach()

string(REPLACE "|" ";" _env "${ENV}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env ${_unset} ${_env} "${QCC}" --check-module-sources "@${LIST}"
    RESULT_VARIABLE _rc
)
if (NOT _rc EQUAL 0)
    message(FATAL_ERROR "the AOT modules of the build cannot be checked against their dependencies (qcc "
        "--check-module-sources exited with ${_rc}); see the error above for the module and the dependency")
endif()
