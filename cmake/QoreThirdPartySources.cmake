# -*- mode: cmake -*-
#
# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# QoreThirdPartySources.cmake: the sources of the third-party archives listed in third-party-sources.cmake
#
# qore_third_party_version(<name> <var>)
#   sets <var> to the pinned version of <name>
#
# qore_third_party_source_args(<name> <var> [OVERRIDE <url or path>])
#   sets <var> to the source arguments of FetchContent_Declare() or ExternalProject_Add() for <name>:
#     URL <url> URL_HASH SHA256=<sha256>
#   The URL is the override, when OVERRIDE is given and not empty; otherwise the archive fetched from the mirror, when
#   QORE_THIRD_PARTY_MIRROR is set and the mirror provides it; otherwise the upstream URL.  The SHA-256 is verified
#   whichever URL is used.
#
# QORE_THIRD_PARTY_MIRROR: the base URL of the mirror (<mirror>/<name>/<version>/<file>); when it is not set with -D,
# it is taken from the environment.  The mirror is read with the header "JOB-TOKEN: $CI_JOB_TOKEN" in a CI job, or
# "PRIVATE-TOKEN: $QORE_THIRD_PARTY_MIRROR_TOKEN" when that is set.  The token goes only to the mirror: the archive is
# fetched at configuration time into <build>/third-party-sources/, the build uses that verified copy, and the upstream
# URL is never given a token.  An archive with a different SHA-256 on the mirror fails the configuration; when the
# mirror cannot provide the archive at all, the upstream URL is used.

include_guard(GLOBAL)

# an empty OVERRIDE is no override: keep it an empty string in cmake_parse_arguments(); include() gives this file its own
# policy scope, and the functions defined here record it
if (POLICY CMP0174)
    cmake_policy(SET CMP0174 NEW)
endif()

set(_QORE_THIRD_PARTY_MODULE_DIR "${CMAKE_CURRENT_LIST_DIR}")

if (NOT DEFINED QORE_THIRD_PARTY_SOURCES_FILE)
    set(QORE_THIRD_PARTY_SOURCES_FILE "${_QORE_THIRD_PARTY_MODULE_DIR}/third-party-sources.cmake")
endif()

if (NOT DEFINED QORE_THIRD_PARTY_MIRROR AND DEFINED ENV{QORE_THIRD_PARTY_MIRROR})
    set(QORE_THIRD_PARTY_MIRROR "$ENV{QORE_THIRD_PARTY_MIRROR}")
endif()
if (QORE_THIRD_PARTY_MIRROR)
    string(REGEX REPLACE "/+$" "" QORE_THIRD_PARTY_MIRROR "${QORE_THIRD_PARTY_MIRROR}")
endif()

# Registers an archive; called by the manifest
function(qore_third_party_source name version)
    cmake_parse_arguments(PARSE_ARGV 2 _tp "" "SHA256;URL" "")
    set(_where "${QORE_THIRD_PARTY_SOURCES_FILE}: ${name}")
    if (DEFINED _tp_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "${_where}: unexpected arguments: ${_tp_UNPARSED_ARGUMENTS}")
    endif()
    if (NOT name MATCHES "^[A-Za-z0-9][A-Za-z0-9._-]*$")
        message(FATAL_ERROR "${_where}: invalid name")
    endif()
    if (NOT version MATCHES "^[A-Za-z0-9][A-Za-z0-9._-]*$")
        message(FATAL_ERROR "${_where}: invalid version \"${version}\"")
    endif()
    if (NOT _tp_SHA256 MATCHES "^[0-9a-f]+$")
        message(FATAL_ERROR "${_where}: SHA256 must be 64 lowercase hex digits: \"${_tp_SHA256}\"")
    endif()
    string(LENGTH "${_tp_SHA256}" _len)
    if (NOT _len EQUAL 64)
        message(FATAL_ERROR "${_where}: SHA256 must be 64 lowercase hex digits: \"${_tp_SHA256}\"")
    endif()
    string(REPLACE "{version}" "${version}" _url "${_tp_URL}")
    if (NOT _url MATCHES "^[a-z][a-z0-9+.-]*://[^ ;?#]+/[^/ ;?#]+$")
        message(FATAL_ERROR "${_where}: URL must be the URL of an archive file, without a query: \"${_url}\"")
    endif()
    get_property(_names GLOBAL PROPERTY QORE_THIRD_PARTY_SOURCES)
    if (name IN_LIST _names)
        message(FATAL_ERROR "${_where}: listed twice")
    endif()
    set_property(GLOBAL APPEND PROPERTY QORE_THIRD_PARTY_SOURCES "${name}")
    set_property(GLOBAL PROPERTY "QORE_THIRD_PARTY_SOURCE_${name}" "${version}" "${_tp_SHA256}" "${_url}")
endfunction()

# Sets <prefix>_VERSION, <prefix>_SHA256, <prefix>_URL, <prefix>_FILE and <prefix>_MIRROR_URL (with a mirror) for a
# manifest entry in the caller's scope
macro(_qore_third_party_entry name prefix)
    get_property(_qore_tp_entry GLOBAL PROPERTY "QORE_THIRD_PARTY_SOURCE_${name}")
    if (NOT _qore_tp_entry)
        message(FATAL_ERROR "${name} is not listed in ${QORE_THIRD_PARTY_SOURCES_FILE}")
    endif()
    list(GET _qore_tp_entry 0 ${prefix}_VERSION)
    list(GET _qore_tp_entry 1 ${prefix}_SHA256)
    list(GET _qore_tp_entry 2 ${prefix}_URL)
    string(REGEX REPLACE "^.*/" "" ${prefix}_FILE "${${prefix}_URL}")
    set(${prefix}_MIRROR_URL)
    if (QORE_THIRD_PARTY_MIRROR)
        set(${prefix}_MIRROR_URL "${QORE_THIRD_PARTY_MIRROR}/${name}/${${prefix}_VERSION}/${${prefix}_FILE}")
    endif()
endmacro()

# Downloads <url> from the mirror to <path> and verifies its SHA-256; <result_var> is set to "ok", "mismatch:<sha256>"
# or "failed:<reason>".  The mirror's token is passed only here and is never written to the output.
function(_qore_third_party_mirror_download url path sha256 result_var)
    set(_auth)
    if (NOT "$ENV{CI_JOB_TOKEN}" STREQUAL "")
        set(_auth HTTPHEADER "JOB-TOKEN: $ENV{CI_JOB_TOKEN}")
    elseif (NOT "$ENV{QORE_THIRD_PARTY_MIRROR_TOKEN}" STREQUAL "")
        set(_auth HTTPHEADER "PRIVATE-TOKEN: $ENV{QORE_THIRD_PARTY_MIRROR_TOKEN}")
    endif()
    get_filename_component(_dir "${path}" DIRECTORY)
    file(MAKE_DIRECTORY "${_dir}")
    file(REMOVE "${path}.part")
    # no LOG and no SHOW_PROGRESS: curl's verbose log would contain the request headers
    file(DOWNLOAD "${url}" "${path}.part" ${_auth} TLS_VERIFY ON INACTIVITY_TIMEOUT 60 STATUS _status)
    list(GET _status 0 _code)
    if (NOT _code EQUAL 0)
        list(GET _status 1 _reason)
        string(REPLACE "\"" "" _reason "${_reason}")
        file(REMOVE "${path}.part")
        set(${result_var} "failed:${_reason}" PARENT_SCOPE)
        return()
    endif()
    file(SHA256 "${path}.part" _have)
    if (NOT _have STREQUAL sha256)
        file(REMOVE "${path}.part")
        set(${result_var} "mismatch:${_have}" PARENT_SCOPE)
        return()
    endif()
    file(RENAME "${path}.part" "${path}")
    set(${result_var} "ok" PARENT_SCOPE)
endfunction()

function(qore_third_party_version name var)
    _qore_third_party_entry("${name}" _tp)
    set(${var} "${_tp_VERSION}" PARENT_SCOPE)
endfunction()

function(qore_third_party_source_args name var)
    cmake_parse_arguments(PARSE_ARGV 2 _arg "" "OVERRIDE" "")
    if (DEFINED _arg_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "qore_third_party_source_args(${name}): unexpected arguments: ${_arg_UNPARSED_ARGUMENTS}")
    endif()
    _qore_third_party_entry("${name}" _tp)
    if (NOT "${_arg_OVERRIDE}" STREQUAL "")
        set(_url "${_arg_OVERRIDE}")
    else()
        set(_url "${_tp_URL}")
        if (_tp_MIRROR_URL)
            set(_path "${CMAKE_BINARY_DIR}/third-party-sources/${name}/${_tp_VERSION}/${_tp_FILE}")
            set(_result "ok")
            set(_have)
            if (EXISTS "${_path}")
                file(SHA256 "${_path}" _have)
            endif()
            if (NOT _have STREQUAL _tp_SHA256)
                file(REMOVE "${_path}")
                _qore_third_party_mirror_download("${_tp_MIRROR_URL}" "${_path}" "${_tp_SHA256}" _result)
            endif()
            if (_result STREQUAL "ok")
                message(STATUS "${name} ${_tp_VERSION}: source fetched from the mirror (SHA-256 verified): "
                    "${_tp_MIRROR_URL}")
                set(_url "${_path}")
            elseif (_result MATCHES "^mismatch:(.*)$")
                message(FATAL_ERROR "${name} ${_tp_VERSION}: the mirror's ${_tp_MIRROR_URL} has SHA-256 "
                    "${CMAKE_MATCH_1}; ${QORE_THIRD_PARTY_SOURCES_FILE} pins ${_tp_SHA256}")
            else()
                string(REGEX REPLACE "^failed:" "" _reason "${_result}")
                message(STATUS "${name} ${_tp_VERSION}: not available from the mirror (${_reason}): "
                    "${_tp_MIRROR_URL}; using the upstream source")
            endif()
        endif()
    endif()
    set(${var} URL "${_url}" URL_HASH "SHA256=${_tp_SHA256}" PARENT_SCOPE)
endfunction()

include("${QORE_THIRD_PARTY_SOURCES_FILE}")
