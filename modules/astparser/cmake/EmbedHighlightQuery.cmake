# Generates a C++ header with the text of the highlight query of the Qore grammar
#
# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# usage: cmake -DINPUT=<queries/highlights.scm> -DOUTPUT=<header> -P EmbedHighlightQuery.cmake
#
# The header defines astparser_highlight_query, a NUL-terminated char array; its size minus one is the length of the
# query.  The bytes are written as numbers, so the query text needs no escaping.

if(NOT INPUT OR NOT OUTPUT)
    message(FATAL_ERROR "EmbedHighlightQuery.cmake requires INPUT and OUTPUT")
endif()

file(READ "${INPUT}" _hex HEX)
# 16 bytes per line
string(REGEX REPLACE "(................................)" "\\1\n" _hex "${_hex}")
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," _bytes "${_hex}")

set(_content "// generated from queries/highlights.scm by cmake/EmbedHighlightQuery.cmake; do not edit\n")
string(APPEND _content "static const char astparser_highlight_query[] = {\n${_bytes}\n0x00\n};\n")

# write the header only if it changed, so a reconfiguration does not rebuild the module
if(EXISTS "${OUTPUT}")
    file(READ "${OUTPUT}" _old)
    if(_old STREQUAL _content)
        return()
    endif()
endif()
file(WRITE "${OUTPUT}" "${_content}")
