# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# Rewrites the optional module stamp of an AOT module (.qmod) when a module of one of its failed %try-module
# directives has been installed since the module was compiled (qore#6145).
#
# qcc writes the paths where those modules could be installed, and are not, to <qmod>.d.optional; the .qmod depends
# on the stamp, and this script runs before it in every build: the stamp is rewritten only when one of the paths
# exists now, so that the module is rebuilt then, and is left unchanged otherwise, also by a file added beside the
# paths.  After the rebuild the paths of the module that became available are no longer listed, and the stamp is
# left as it is.
#
# -DCANDIDATES=<qmod>.d.optional  -DSTAMP=<qmod>.optional.stamp

if (NOT DEFINED CANDIDATES OR NOT DEFINED STAMP)
    message(FATAL_ERROR "QoreOptionalModuleStamp.cmake: CANDIDATES and STAMP are required")
endif()

set(_present "")
if (EXISTS "${CANDIDATES}")
    file(STRINGS "${CANDIDATES}" _candidates)
    foreach (_path IN LISTS _candidates)
        if (_path AND EXISTS "${_path}" AND NOT IS_DIRECTORY "${_path}")
            string(APPEND _present "${_path}\n")
        endif()
    endforeach()
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
