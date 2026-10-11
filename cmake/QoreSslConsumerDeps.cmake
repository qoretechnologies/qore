# -*- mode: cmake -*-
#
# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# QoreSslConsumerDeps.cmake: resolves the OpenSSL libraries that a macOS library loads; run in script mode by
# QoreCheckSslConsumers.cmake, since file(GET_RUNTIME_DEPENDENCIES) is meant for scripts, not for configuration
#
# cmake -DQORE_SSL_CONSUMER=<library> -DQORE_SSL_OTOOL=<otool> -DQORE_SSL_OUTPUT=<file> -P QoreSslConsumerDeps.cmake
#
# Writes one line per OpenSSL library to the output file: "resolved <path>" for a library resolved as dyld would,
# including @rpath and @loader_path install names, or "unresolved <install name>".

cmake_minimum_required(VERSION 3.16)

foreach(_var QORE_SSL_CONSUMER QORE_SSL_OTOOL QORE_SSL_OUTPUT)
    if (NOT ${_var})
        message(FATAL_ERROR "${_var} is not set")
    endif()
endforeach()

set(CMAKE_GET_RUNTIME_DEPENDENCIES_PLATFORM "macos+macho")
set(CMAKE_GET_RUNTIME_DEPENDENCIES_TOOL "otool")
set(CMAKE_GET_RUNTIME_DEPENDENCIES_COMMAND "${QORE_SSL_OTOOL}")

# only the OpenSSL libraries are resolved
file(GET_RUNTIME_DEPENDENCIES
    LIBRARIES "${QORE_SSL_CONSUMER}"
    RESOLVED_DEPENDENCIES_VAR _deps
    UNRESOLVED_DEPENDENCIES_VAR _unresolved
    PRE_INCLUDE_REGEXES "^(.*/)?lib(ssl|crypto)[.-][^/]*$"
    PRE_EXCLUDE_REGEXES ".*")

set(_out "")
foreach(_dep IN LISTS _deps)
    string(APPEND _out "resolved ${_dep}\n")
endforeach()
foreach(_dep IN LISTS _unresolved)
    string(APPEND _out "unresolved ${_dep}\n")
endforeach()
file(WRITE "${QORE_SSL_OUTPUT}" "${_out}")
