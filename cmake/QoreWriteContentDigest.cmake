# Copyright (c) 2026 Qore Technologies, s.r.o.
#
# Writes a deterministic digest file for a newline-separated list of inputs.
# The digest output is updated only when the input content summary changes;
# SUCCESS_STAMP is always touched so mtime-driven build tools can complete the
# rule without propagating unchanged content to downstream targets.

if (NOT DEFINED INPUT_LIST OR "${INPUT_LIST}" STREQUAL "")
    message(FATAL_ERROR "INPUT_LIST is required")
endif ()
if (NOT DEFINED OUTPUT OR "${OUTPUT}" STREQUAL "")
    message(FATAL_ERROR "OUTPUT is required")
endif ()
if (NOT DEFINED SUCCESS_STAMP OR "${SUCCESS_STAMP}" STREQUAL "")
    message(FATAL_ERROR "SUCCESS_STAMP is required")
endif ()

file(READ "${INPUT_LIST}" _input_text)
string(REPLACE "\n" ";" _inputs "${_input_text}")

get_filename_component(_output_dir "${OUTPUT}" DIRECTORY)
if (_output_dir)
    file(MAKE_DIRECTORY "${_output_dir}")
endif ()
get_filename_component(_success_stamp_dir "${SUCCESS_STAMP}" DIRECTORY)
if (_success_stamp_dir)
    file(MAKE_DIRECTORY "${_success_stamp_dir}")
endif ()

set(_digest "format=1\n")
foreach (_path IN LISTS _inputs)
    if ("${_path}" STREQUAL "")
        continue()
    endif ()

    if (EXISTS "${_path}")
        file(SHA256 "${_path}" _hash)
        file(SIZE "${_path}" _size)
        string(APPEND _digest "${_path}\t${_size}\t${_hash}\n")
    else ()
        string(APPEND _digest "${_path}\tMISSING\n")
    endif ()
endforeach ()

set(_current)
if (EXISTS "${OUTPUT}")
    file(READ "${OUTPUT}" _current)
endif ()
if (NOT "${_current}" STREQUAL "${_digest}")
    file(WRITE "${OUTPUT}" "${_digest}")
endif ()

execute_process(COMMAND "${CMAKE_COMMAND}" -E touch "${SUCCESS_STAMP}")
