# Copyright (c) 2026 Qore Technologies, s.r.o.
#
# Updates one content-preserving digest sidecar per source. INPUT_MAP contains
# tab-separated source and digest paths. SUCCESS_STAMP is always touched so the
# grouped mtime-driven rule completes without invalidating unchanged sources.

if (NOT DEFINED INPUT_MAP OR "${INPUT_MAP}" STREQUAL "")
    message(FATAL_ERROR "INPUT_MAP is required")
endif ()
if (NOT DEFINED SUCCESS_STAMP OR "${SUCCESS_STAMP}" STREQUAL "")
    message(FATAL_ERROR "SUCCESS_STAMP is required")
endif ()

file(STRINGS "${INPUT_MAP}" _entries ENCODING UTF-8)

# Mark the beginning of the validation window. If a source changes while or
# after it is hashed, its mtime remains newer than this stamp and the next build
# reruns validation instead of accepting a stale digest as converged.
get_filename_component(_success_stamp_dir "${SUCCESS_STAMP}" DIRECTORY)
if (_success_stamp_dir)
    file(MAKE_DIRECTORY "${_success_stamp_dir}")
endif ()
execute_process(COMMAND "${CMAKE_COMMAND}" -E touch "${SUCCESS_STAMP}")

foreach (_entry IN LISTS _entries)
    string(FIND "${_entry}" "\t" _separator)
    if (_separator LESS 1)
        message(FATAL_ERROR "invalid source-content map entry: ${_entry}")
    endif ()
    string(SUBSTRING "${_entry}" 0 ${_separator} _source)
    math(EXPR _digest_start "${_separator} + 1")
    string(SUBSTRING "${_entry}" ${_digest_start} -1 _output)
    if ("${_output}" STREQUAL "")
        message(FATAL_ERROR "missing digest path for source: ${_source}")
    endif ()
    if (NOT EXISTS "${_source}")
        message(FATAL_ERROR "source does not exist: ${_source}")
    endif ()

    file(SHA256 "${_source}" _hash)
    file(SIZE "${_source}" _size)
    set(_digest "format=1\nsize=${_size}\nsha256=${_hash}\n")

    set(_current)
    if (EXISTS "${_output}")
        file(READ "${_output}" _current)
    endif ()
    if (NOT "${_current}" STREQUAL "${_digest}")
        get_filename_component(_output_dir "${_output}" DIRECTORY)
        if (_output_dir)
            file(MAKE_DIRECTORY "${_output_dir}")
        endif ()
        file(WRITE "${_output}" "${_digest}")
    endif ()
endforeach ()
