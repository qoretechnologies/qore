# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT

# Record reusable compiler flags as a C string. Prefix maps describe the build
# machine's paths; they must still reach the compiler, but are not SDK flags.
function(qore_record_cflags output flags)
    if(UNIX)
        set(recorded "")
        # Preserve shell quoting verbatim: separate_arguments also consumes
        # backslashes inside single quotes, changing legitimate macro values.
        set(token_pattern [=[^([^ \t\r\n"'\\]|\\.|'[^']*'|"([^"\\]|\\.)*")+]=])
        string(REPLACE "\\t" "\t" token_pattern "${token_pattern}")
        string(REPLACE "\\r" "\r" token_pattern "${token_pattern}")
        string(REPLACE "\\n" "\n" token_pattern "${token_pattern}")
        while(NOT flags STREQUAL "")
            string(REGEX REPLACE "^[ \t\r\n]+" "" flags "${flags}")
            if(flags STREQUAL "")
                break()
            endif()
            string(REGEX MATCH "${token_pattern}" token "${flags}")
            if(token STREQUAL "")
                message(FATAL_ERROR "Unterminated quote or escape in compiler flags")
            endif()
            string(LENGTH "${token}" length)
            string(SUBSTRING "${flags}" ${length} -1 flags)
            separate_arguments(argument UNIX_COMMAND "${token}")
            if(NOT argument MATCHES "^-f(file|debug|macro)-prefix-map=")
                string(APPEND recorded " ${token}")
            endif()
        endwhile()
        string(STRIP "${recorded}" flags)
    endif()
    # configure_file substitutes into a C string literal, not a shell command.
    string(REPLACE "\\" "\\\\" flags "${flags}")
    string(REPLACE "\"" "\\\"" flags "${flags}")
    string(REPLACE "\n" "\\n" flags "${flags}")
    string(REPLACE "\r" "\\r" flags "${flags}")
    string(REPLACE "\t" "\\t" flags "${flags}")
    set(${output} "${flags}" PARENT_SCOPE)
endfunction()
