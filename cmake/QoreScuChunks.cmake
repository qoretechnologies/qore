# Copyright 2026 Qore Technologies, s.r.o.
#
# Splits the libqore single-compilation-unit wrapper into several unity translation units ("chunks") that can be
# compiled in parallel.  Used only when SINGLE_COMPILATION_UNIT_CHUNKS > 1; the default (1) keeps the one wrapper.
#
# Assignment rule (deterministic, independent of the number of CPUs and of the host):
#  1. the wrapper text is split into units: one unit per #include line, a #if/#ifdef/#ifndef ... #endif group
#     is one unit, and the "#undef ECHO" line that follows scanner.cpp belongs to the scanner unit; comment lines are dropped
#  2. each unit has a cost: the sum of the entries of cmake/scu-chunk-costs.txt ("<cost> <file name>", cost =
#     compile time in milliseconds of that file alone in a RelWithDebInfo build) for the files it includes; a file
#     that is not listed costs QORE_SCU_DEFAULT_COST
#  3. units are sorted by cost descending, then by file name, and assigned one by one to the chunk with the smallest
#     load so far (lowest chunk number on a tie): the longest-processing-time-first greedy rule
#  4. inside a chunk the units keep their original order of the single wrapper, so any ordering dependency between
#     files that is satisfied in the single wrapper is satisfied inside a chunk
#  5. files listed in QORE_SCU_ISOLATED_FILES are not part of any chunk: they are returned to be compiled as their
#     own translation units (see below)
# Costs are keyed by file base name, which is unique among the sources of the wrapper (a test checks it).
# The assignment is stable while the table and the file set do not change; adding, removing or re-costing a file
# can move many units to other chunks (the greedy rule is not sticky), which recompiles those chunks once.
# Chunks are separate translation units: the order of the dynamic initialization of namespace-scope objects
# between chunks is unspecified (inside a chunk it is the wrapper order), so such initializers must not depend
# on each other across files; tests of a chunked build exercise this.
# The same source therefore always lands in the same chunk for a given N until the cost table (or the file set)
# changes, which keeps incremental builds and ccache effective.  tools/update-scu-chunk-costs.py regenerates the
# table from the per-file timings of an SINGLE_COMPILATION_UNIT=OFF build.

set(QORE_SCU_DEFAULT_COST 2000)

# Files that must not share a translation unit with the rest of libqore.  <termios.h> defines macros (CR1, ECHO,
# ...) that collide with identifiers of the LLVM headers (ConstantRange.h has a parameter named CR1), so a unit that
# includes <termios.h> (lib/QoreTermIOS.cpp, and the <termios.h> constants of lib/QC_TermIOS.qpp, whose generated
# QC_TermIOS.cpp this names) must not precede, in the same translation unit, a unit that includes LLVM headers.  The
# single wrapper works only because earlier units have already included the LLVM headers (include guards) by the
# time it reaches them; a chunk can place them first, so in a chunked build they are compiled separately (like
# lib/qore-main.cpp, for the same reason, in the single-wrapper build).  examples/test/cmake/test_scu_chunks.py
# fails if any other source of the wrapper (or a header) includes <termios.h>.
set(QORE_SCU_ISOLATED_FILES QC_TermIOS.cpp QoreTermIOS.cpp)

# qore_scu_split_chunks(<wrapper text> <chunk count> <cost file> <output file prefix> <output list variable>
#                       <isolated files variable>)
# writes <prefix><n>.cpp for every non-empty chunk (only when the content changes, so that a reconfigure does not
# recompile) and sets <output list variable> in the caller to the written files, and <isolated files variable> to
# the source paths of the QORE_SCU_ISOLATED_FILES that the wrapper includes
function(qore_scu_split_chunks CONTENT CHUNKS COST_FILE OUT_PREFIX OUT_FILES_VAR OUT_ISOLATED_VAR)
    set(_isolated "")
    if (NOT CHUNKS MATCHES "^[1-9][0-9]*$" OR CHUNKS GREATER 256)
        message(FATAL_ERROR "qore_scu_split_chunks: the chunk count must be between 1 and 256, got '${CHUNKS}'")
    endif ()

    # the text is handled as a CMake list of lines, so a character that CMake list handling treats specially
    # (also in the source and build directory paths that the wrapper contains) is rejected rather than mangled
    foreach (_bad ";" "[" "]" "\\")
        string(FIND "${CONTENT}" "${_bad}" _badpos)
        if (NOT _badpos EQUAL -1)
            message(FATAL_ERROR "qore_scu_split_chunks: the wrapper text (including the source and build "
                "directory paths) must not contain '${_bad}'; SINGLE_COMPILATION_UNIT_CHUNKS cannot be used with "
                "such a path")
        endif ()
    endforeach ()

    # cost table
    file(STRINGS ${COST_FILE} _cost_lines)
    foreach (_cl IN LISTS _cost_lines)
        if (_cl STREQUAL "" OR _cl MATCHES "^#")
            continue()
        endif ()
        if (_cl MATCHES "^([0-9]+) ([^ ]+)$")
            set(_cost_${CMAKE_MATCH_2} ${CMAKE_MATCH_1})
        else ()
            # such a line would silently give its file the default cost
            message(WARNING "${COST_FILE}: ignoring malformed line '${_cl}' (expected '<cost> <file name>')")
        endif ()
    endforeach ()

    # split into units
    string(REPLACE "\n" ";" _lines "${CONTENT}")
    set(_n 0)
    set(_in_group OFF)
    foreach (_line IN LISTS _lines)
        if (_line STREQUAL "" OR _line MATCHES "^//")
            continue()
        endif ()
        if (_line MATCHES "^#undef ")
            if (_in_group OR _n EQUAL 0)
                message(FATAL_ERROR "qore_scu_split_chunks: unexpected '${_line}'")
            endif ()
            math(EXPR _last "${_n} - 1")
            string(APPEND _text_${_last} "\n${_line}")
        elseif (_line MATCHES "^#if")
            if (_in_group)
                message(FATAL_ERROR "qore_scu_split_chunks: nested conditional '${_line}'")
            endif ()
            set(_in_group ON)
            set(_gtext "${_line}")
            set(_gcost 0)
            set(_gname "")
        elseif (_line MATCHES "^#endif")
            if (NOT _in_group)
                message(FATAL_ERROR "qore_scu_split_chunks: unmatched '${_line}'")
            endif ()
            set(_in_group OFF)
            set(_text_${_n} "${_gtext}\n${_line}")
            set(_cost_of_${_n} ${_gcost})
            set(_name_${_n} "${_gname}")
            math(EXPR _n "${_n} + 1")
        elseif (_line MATCHES "^#include \"([^\"]+)\"")
            get_filename_component(_fname "${CMAKE_MATCH_1}" NAME)
            if (DEFINED _cost_${_fname})
                set(_c ${_cost_${_fname}})
            else ()
                set(_c ${QORE_SCU_DEFAULT_COST})
            endif ()
            if (_fname IN_LIST QORE_SCU_ISOLATED_FILES)
                if (_in_group)
                    message(FATAL_ERROR "qore_scu_split_chunks: isolated file ${_fname} inside a conditional")
                endif ()
                list(APPEND _isolated "${CMAKE_MATCH_1}")
            elseif (_in_group)
                string(APPEND _gtext "\n${_line}")
                math(EXPR _gcost "${_gcost} + ${_c}")
                string(APPEND _gname "${_fname}")
            else ()
                set(_text_${_n} "${_line}")
                set(_cost_of_${_n} ${_c})
                set(_name_${_n} "${_fname}")
                math(EXPR _n "${_n} + 1")
            endif ()
        elseif (_in_group)
            string(APPEND _gtext "\n${_line}")
        else ()
            message(FATAL_ERROR "qore_scu_split_chunks: unsupported wrapper line '${_line}'")
        endif ()
    endforeach ()
    if (_in_group)
        message(FATAL_ERROR "qore_scu_split_chunks: unterminated conditional")
    endif ()

    # sort by cost descending, then by name
    set(_keys "")
    math(EXPR _end "${_n} - 1")
    foreach (_i RANGE ${_end})
        set(_cc ${_cost_of_${_i}})
        if (_cc GREATER 999999999)
            set(_cc 999999999)
        endif ()
        math(EXPR _inv "999999999 - ${_cc}")
        # zero-pad to 9 digits so that the string sort is the numeric one (string(REPEAT) needs CMake 3.15)
        string(LENGTH "000000000${_inv}" _len)
        math(EXPR _from "${_len} - 9")
        string(SUBSTRING "000000000${_inv}" ${_from} 9 _padded)
        list(APPEND _keys "${_padded}|${_name_${_i}}|${_i}")
    endforeach ()
    list(SORT _keys)

    # longest-processing-time-first assignment
    math(EXPR _cend "${CHUNKS} - 1")
    foreach (_c RANGE ${_cend})
        set(_load_${_c} 0)
    endforeach ()
    foreach (_key IN LISTS _keys)
        string(REGEX MATCH "\\|([0-9]+)$" _m "${_key}")
        set(_idx ${CMAKE_MATCH_1})
        set(_best 0)
        foreach (_c RANGE ${_cend})
            if (_load_${_c} LESS _load_${_best})
                set(_best ${_c})
            endif ()
        endforeach ()
        set(_chunk_of_${_idx} ${_best})
        math(EXPR _load_${_best} "${_load_${_best}} + ${_cost_of_${_idx}}")
    endforeach ()

    # emit, keeping the original order inside a chunk
    set(_out "")
    set(_out_in "")
    foreach (_c RANGE ${_cend})
        set(_ctext "// Auto-generated libqore unity chunk ${_c} of ${CHUNKS} (see cmake/QoreScuChunks.cmake)\n")
        set(_count 0)
        foreach (_i RANGE ${_end})
            if (_chunk_of_${_i} EQUAL _c)
                string(APPEND _ctext "${_text_${_i}}\n")
                math(EXPR _count "${_count} + 1")
            endif ()
        endforeach ()
        if (_count EQUAL 0)
            continue()
        endif ()
        message(STATUS "libqore unity chunk ${_c}: ${_count} units, estimated cost ${_load_${_c}} ms")
        file(WRITE ${OUT_PREFIX}${_c}.cpp.in "${_ctext}")
        # mtime-stable, as for the single wrapper
        configure_file(${OUT_PREFIX}${_c}.cpp.in ${OUT_PREFIX}${_c}.cpp COPYONLY)
        list(APPEND _out ${OUT_PREFIX}${_c}.cpp)
        list(APPEND _out_in ${OUT_PREFIX}${_c}.cpp.in)
    endforeach ()
    # remove the chunk files of an earlier configure with a larger N (or with empty chunks)
    file(GLOB _old ${OUT_PREFIX}*.cpp ${OUT_PREFIX}*.cpp.in)
    foreach (_f IN LISTS _old)
        if (NOT "${_f}" IN_LIST _out AND NOT "${_f}" IN_LIST _out_in)
            file(REMOVE "${_f}")
        endif ()
    endforeach ()
    set(${OUT_FILES_VAR} "${_out}" PARENT_SCOPE)
    set(${OUT_ISOLATED_VAR} "${_isolated}" PARENT_SCOPE)
endfunction ()
