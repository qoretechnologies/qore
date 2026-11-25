# Copyright (c) 2026 Qore Technologies, s.r.o.
#
# Makes a depfile with one rule that names its target and no prerequisite, when there is none (qore#7189).
#
# A custom command whose depfile another rule rewrites also depends on its depfile with Ninja, and this script, the
# command of a rule of its own, makes it when it is missing (see _QORE_DEPFILE_REWRITTEN_ELSEWHERE_DEPENDS() in
# QoreMacros.cmake): a removed object directory or a first build then never fails for a missing input.  It writes a
# rule rather than an empty file: Ninja takes an empty depfile for a missing one, so the edge that reads it would run
# in every build until the command that writes it replaced it.  An existing depfile is left as it is.
#
# Inputs:
#   DEPFILE - the depfile
#   TARGET  - the target that its rule names: the output of the custom command whose depfile it is

if (NOT DEFINED DEPFILE OR "${DEPFILE}" STREQUAL "")
    message(FATAL_ERROR "DEPFILE is required")
endif ()
if (NOT DEFINED TARGET OR "${TARGET}" STREQUAL "")
    message(FATAL_ERROR "TARGET is required")
endif ()

if (NOT EXISTS "${DEPFILE}")
    # make depfile syntax, as tools/qore-qo-source-order writes it (makeDepfilePath()): $ is doubled, # and spaces
    # are escaped
    string(REPLACE "$" "$$" _target "${TARGET}")
    string(REPLACE "#" "\\#" _target "${_target}")
    string(REPLACE " " "\\ " _target "${_target}")
    file(WRITE "${DEPFILE}" "${_target}:\n")
endif ()
