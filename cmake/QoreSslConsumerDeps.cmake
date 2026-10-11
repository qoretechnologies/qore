# -*- mode: cmake -*-
#
# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# QoreSslConsumerDeps.cmake: lists the OpenSSL libraries that a library loads, directly or through its dependencies;
# run in script mode by QoreCheckSslConsumers.cmake, since file(GET_RUNTIME_DEPENDENCIES) is meant for scripts
#
# cmake -DQORE_SSL_CONSUMER=<library> -DQORE_SSL_PLATFORM=<macos+macho|linux+elf> -DQORE_SSL_TOOL=<otool|objdump>
#     -DQORE_SSL_COMMAND=<tool path> -DQORE_SSL_OUTPUT=<file> [-DQORE_SSL_DIRS=<directories>]
#     -P QoreSslConsumerDeps.cmake
#
# Writes one line per OpenSSL library to the output file: "resolved <path>" for a library resolved as the dynamic
# loader would (including @rpath and @loader_path install names on macOS), or "unresolved <name>".

cmake_minimum_required(VERSION 3.16)

foreach(_var QORE_SSL_CONSUMER QORE_SSL_PLATFORM QORE_SSL_TOOL QORE_SSL_COMMAND QORE_SSL_OUTPUT)
    if (NOT ${_var})
        message(FATAL_ERROR "${_var} is not set")
    endif()
endforeach()

set(CMAKE_GET_RUNTIME_DEPENDENCIES_PLATFORM "${QORE_SSL_PLATFORM}")
set(CMAKE_GET_RUNTIME_DEPENDENCIES_TOOL "${QORE_SSL_TOOL}")
set(CMAKE_GET_RUNTIME_DEPENDENCIES_COMMAND "${QORE_SSL_COMMAND}")

# the libraries of the consumer and of its dependencies, recursively; macOS system libraries are in the dyld cache
get_filename_component(_consumer_dir "${QORE_SSL_CONSUMER}" DIRECTORY)
set(_exclude)
if (QORE_SSL_PLATFORM STREQUAL "macos+macho")
    set(_exclude "^/usr/lib/" "^/System/")
endif()
set(_args)
if (_exclude)
    list(APPEND _args PRE_EXCLUDE_REGEXES ${_exclude})
endif()
file(GET_RUNTIME_DEPENDENCIES
    LIBRARIES "${QORE_SSL_CONSUMER}"
    RESOLVED_DEPENDENCIES_VAR _deps
    UNRESOLVED_DEPENDENCIES_VAR _unresolved
    DIRECTORIES "${_consumer_dir}" ${QORE_SSL_DIRS}
    ${_args})

set(_out "")
foreach(_dep IN LISTS _deps)
    if (_dep MATCHES "(^|/)lib(ssl|crypto)[.-][^/]*$")
        string(APPEND _out "resolved ${_dep}\n")
    endif()
endforeach()
foreach(_dep IN LISTS _unresolved)
    if (_dep MATCHES "(^|/)lib(ssl|crypto)[.-][^/]*$")
        string(APPEND _out "unresolved ${_dep}\n")
    endif()
endforeach()
file(WRITE "${QORE_SSL_OUTPUT}" "${_out}")
