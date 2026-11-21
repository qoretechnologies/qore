# Copyright 2026 Qore Technologies, s.r.o.
#
# qore_compile_race_guard(<target>...)
# runs the C and C++ compiles of the targets through tools/compile-race-guard.py, which makes an object out of date
# when one of its inputs was edited while it was compiling (issue #6951; see design/compile-race-guard.md).  The
# launcher of the target (CMAKE_<LANG>_COMPILER_LAUNCHER, ex: ccache) runs inside it.  Does nothing when
# QORE_COMPILE_RACE_GUARD is OFF, on Windows, or without a Python 3 interpreter.

set(QORE_COMPILE_RACE_GUARD_TOOL "${CMAKE_CURRENT_LIST_DIR}/../tools/compile-race-guard.py")
get_filename_component(QORE_COMPILE_RACE_GUARD_TOOL "${QORE_COMPILE_RACE_GUARD_TOOL}" ABSOLUTE)

function(qore_compile_race_guard)
    if (NOT QORE_COMPILE_RACE_GUARD OR WIN32)
        return()
    endif ()
    if (NOT Python3_EXECUTABLE)
        find_package(Python3 COMPONENTS Interpreter QUIET)
        if (NOT Python3_Interpreter_FOUND)
            message(STATUS "compile race guard: disabled (no Python 3 interpreter)")
            return()
        endif ()
    endif ()
    foreach (_target ${ARGN})
        foreach (_lang C CXX)
            get_target_property(_launcher ${_target} ${_lang}_COMPILER_LAUNCHER)
            if (NOT _launcher)
                set(_launcher "")
            endif ()
            list(FIND _launcher "${QORE_COMPILE_RACE_GUARD_TOOL}" _found)
            if (_found EQUAL -1)
                set_property(TARGET ${_target} PROPERTY ${_lang}_COMPILER_LAUNCHER
                    "${Python3_EXECUTABLE}" -I -S "${QORE_COMPILE_RACE_GUARD_TOOL}" ${_launcher})
            endif ()
        endforeach ()
    endforeach ()
endfunction()
