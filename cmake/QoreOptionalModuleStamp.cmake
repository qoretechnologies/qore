# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# Rewrites the optional module stamp of an AOT module (.qmod) when a module of one of its failed %try-module
# directives has become available since the module was compiled (qore#6145, qore#6598).
#
# qcc writes the paths where those modules could be installed, and are not, to <qmod>.d.optional; the .qmod depends
# on the stamp, and this script runs before it in every build: the stamp is rewritten only when the module is out of
# date for one of those modules, so that it is rebuilt then, and is left unchanged otherwise, also by a file added
# beside the paths.  The module is out of date
# - when one of the listed paths exists now (qore#6145), or
# - when qcc --check-optional-modules says so (qore#6598): it makes the module loader's own checks, with the module
#   path of the compile (this script runs in the environment of the compile): a candidate file of one of the modules
#   that is not the file the compile saw (its size and content hash differ from its fingerprint, as when a module is
#   installed again with the modification time of its build, older than the AOT module's), and a module that can be
#   loaded now (a module that existed then but could not be loaded, and can be now).  It runs only for a module with
#   failed %try-module directives (a non-empty list) that exists.
# After the rebuild the paths of the module that became available are no longer listed, and the stamp is left as it
# is.
#
# -DCANDIDATES=<qmod>.d.optional  -DSTAMP=<qmod>.optional.stamp  [-DQMOD=<qmod> -DQCC=<qcc>]

if (NOT DEFINED CANDIDATES OR NOT DEFINED STAMP)
    message(FATAL_ERROR "QoreOptionalModuleStamp.cmake: CANDIDATES and STAMP are required")
endif()

set(_present "")
set(_candidates "")
if (EXISTS "${CANDIDATES}")
    file(STRINGS "${CANDIDATES}" _candidates)
    foreach (_path IN LISTS _candidates)
        if (_path AND EXISTS "${_path}" AND NOT IS_DIRECTORY "${_path}")
            string(APPEND _present "${_path}\n")
        endif()
    endforeach()
endif()

if (NOT _present AND _candidates AND QCC AND QMOD AND EXISTS "${QMOD}")
    execute_process(
        COMMAND "${QCC}" --check-optional-modules "${QMOD}"
        RESULT_VARIABLE _rc
        OUTPUT_VARIABLE _out
        ERROR_VARIABLE _err
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if (_rc EQUAL 0 AND _out MATCHES "^stale: ")
        string(APPEND _present "${_out}\n")
    elseif (NOT _rc EQUAL 0)
        # the module is left as it is: the loader makes the same checks when it loads it
        message(WARNING "cannot check the optional modules of '${QMOD}': ${_err}")
    endif()
endif()

if (NOT EXISTS "${STAMP}")
    file(WRITE "${STAMP}" "")
endif()
if (_present)
    # the time of the candidate list tells this install from one that a rebuild already took into account (the
    # list is rewritten by each compile)
    file(TIMESTAMP "${CANDIDATES}" _listed "%s" UTC)
    set(_content "${_listed}\n${_present}")
    file(READ "${STAMP}" _old)
    if (NOT _old STREQUAL _content)
        file(WRITE "${STAMP}" "${_content}")
    endif()
endif()
