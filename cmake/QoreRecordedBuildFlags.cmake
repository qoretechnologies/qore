# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT

# Normalize checkout/build roots in recorded compiler settings. This must never
# change the flags passed to the compiler: it only makes the compiler fingerprint
# independent of where equivalent source and build trees happen to live.
function(qore_recorded_build_flags _out _value)
    string(LENGTH "${CMAKE_SOURCE_DIR}" _source_length)
    string(LENGTH "${CMAKE_BINARY_DIR}" _build_length)
    if (_build_length GREATER _source_length)
        set(_roots BINARY SOURCE)
    else()
        set(_roots SOURCE BINARY)
    endif()
    foreach(_kind IN LISTS _roots)
        set(_root "${CMAKE_${_kind}_DIR}")
        if (NOT "${_root}" STREQUAL "")
            # Treat the root literally and require a path/token boundary. A sibling
            # such as /checkout-extra is a distinct input and must remain distinct.
            string(REGEX REPLACE "([][+.*()^$?\\\\|])" "\\\\\\1" _escaped "${_root}")
            string(REGEX REPLACE "${_escaped}(/|=|[ \t\"']|$)" "<${_kind}>\\1" _value "${_value}")
        endif()
    endforeach()
    set(${_out} "${_value}" PARENT_SCOPE)
endfunction()
