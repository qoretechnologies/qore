# -*- mode: cmake -*-
#
# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# QoreCheckThirdPartySources.cmake: checks that every download in the build takes its source from the manifest
# (third-party-sources.cmake) and, with a mirror, that the mirror has every archive of the manifest
#
# cmake [-DQORE_SOURCE_DIR=<source tree>] [-DQORE_THIRD_PARTY_SOURCES_FILE=<manifest>]
#       [-DQORE_THIRD_PARTY_MIRROR=<mirror base URL>] -P QoreCheckThirdPartySources.cmake
#
# It fails when, in a CMakeLists.txt or *.cmake file of the source tree:
#   - a FetchContent_Declare(), FetchContent_Populate() or ExternalProject_Add() call names its source itself (URL,
#     URL_HASH, URL_MD5, GIT_REPOSITORY, SVN_REPOSITORY, HG_REPOSITORY, CVS_REPOSITORY or DOWNLOAD_COMMAND) instead of
#     taking it from qore_third_party_source_args()
#   - file(DOWNLOAD) is called
#   - a string names the URL of an archive file outside the manifest
#   - qore_third_party_source_args() or qore_third_party_version() names an archive that is not in the manifest
# when the upstream URL of an archive of the manifest does not use https, when an archive of the manifest is not used by the build, and, with a mirror, when the mirror does not provide an
# archive of the manifest with its pinned SHA-256.  QORE_THIRD_PARTY_MIRROR is taken from the environment when it is
# not set with -D; set with -D, it must not be empty.  Directories with a CMakeCache.txt (build trees), _deps and .git
# directories are not checked.

cmake_minimum_required(VERSION 3.14)

if (DEFINED QORE_THIRD_PARTY_MIRROR AND "${QORE_THIRD_PARTY_MIRROR}" STREQUAL "")
    message(FATAL_ERROR "QORE_THIRD_PARTY_MIRROR is set but empty")
endif()
if (NOT DEFINED QORE_SOURCE_DIR)
    get_filename_component(QORE_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif()

include("${CMAKE_CURRENT_LIST_DIR}/QoreThirdPartySources.cmake")
get_filename_component(_manifest "${QORE_THIRD_PARTY_SOURCES_FILE}" ABSOLUTE)
# the manifest as it is named in messages
file(RELATIVE_PATH _manifest_name "${QORE_SOURCE_DIR}" "${_manifest}")
if (_manifest_name MATCHES "^[.][.]/")
    set(_manifest_name "${_manifest}")
endif()
set(_module "${CMAKE_CURRENT_LIST_DIR}/QoreThirdPartySources.cmake")
set(_self "${CMAKE_CURRENT_LIST_FILE}")

set(_errors)
set(_used)
set(_n_calls 0)

# Adds an error message made of the arguments to the caller's _errors (a function, not a macro: a macro would expand
# variable references in the quoted source text)
function(_qore_tp_error)
    string(CONCAT _msg ${ARGN})
    string(ASCII 1 _semi)
    string(REPLACE ";" "${_semi}" _msg "${_msg}")
    set(_list "${_errors}")
    list(APPEND _list "${_msg}")
    set(_errors "${_list}" PARENT_SCOPE)
endfunction()

# Adds the CMake files below <dir> to <var>
function(_qore_tp_collect dir var)
    set(_files "${${var}}")
    file(GLOB _children LIST_DIRECTORIES true "${dir}/*")
    foreach(_child IN LISTS _children)
        get_filename_component(_leaf "${_child}" NAME)
        if (IS_SYMLINK "${_child}")
            continue()
        elseif (IS_DIRECTORY "${_child}")
            if (_leaf STREQUAL ".git" OR _leaf STREQUAL "_deps" OR EXISTS "${_child}/CMakeCache.txt")
                continue()
            endif()
            _qore_tp_collect("${_child}" _files)
        elseif (_leaf STREQUAL "CMakeLists.txt" OR _leaf MATCHES "\\.cmake$")
            list(APPEND _files "${_child}")
        endif()
    endforeach()
    set(${var} "${_files}" PARENT_SCOPE)
endfunction()

# Checks the calls of one file
function(_qore_tp_check_file file)
    set(_errors "${_errors}")
    set(_used "${_used}")
    set(_n_calls "${_n_calls}")
    file(READ "${file}" _text)
    file(RELATIVE_PATH _rel "${QORE_SOURCE_DIR}" "${file}")
    # characters that CMake lists treat specially are replaced so that each token is one list element
    string(ASCII 1 _semi)
    string(ASCII 2 _open)
    string(ASCII 3 _close)
    string(REPLACE ";" "${_semi}" _text "${_text}")
    string(REPLACE "[" "${_open}" _text "${_text}")
    string(REPLACE "]" "${_close}" _text "${_text}")
    # escape sequences are replaced by spaces, keeping the lines and the extent of the quoted strings
    string(REGEX REPLACE "\\\\[^\n]" "  " _text "${_text}")
    # tokens: quoted strings, comments, parentheses and everything else; only repeats of single characters, which
    # CMake's regular expressions match without recursion (a long quoted string would otherwise exhaust the stack)
    string(REGEX MATCHALL "\"[^\"]*\"|#[^\n]*|[()]|[^\"#()]+" _tokens "${_text}")
    set(_line 1)
    set(_prev "")
    set(_call "")
    set(_depth 0)
    set(_args "")
    set(_strings "")
    foreach(_tok IN LISTS _tokens)
        string(REGEX MATCHALL "\n" _nls "${_tok}")
        list(LENGTH _nls _n_nl)
        if (_tok MATCHES "^#")
            continue()
        endif()
        if (_tok MATCHES "^\"")
            # an archive URL outside the manifest
            if (NOT file STREQUAL _manifest AND NOT file STREQUAL _module AND NOT file STREQUAL _self
                    AND _tok MATCHES "(https?|ftp)://[^\" ]+\\.(tar\\.gz|tgz|tar\\.xz|txz|tar\\.bz2|tbz2|tar\\.zst|tar|zip|7z)\"$")
                _qore_tp_error("${_rel}:${_line}: archive URL outside ${_manifest_name}: ${_tok}")
            endif()
            if (_call)
                string(APPEND _args " \"\" ")
            endif()
        elseif (_call)
            if (_tok STREQUAL "(")
                math(EXPR _depth "${_depth} + 1")
                string(APPEND _args " ( ")
            elseif (_tok STREQUAL ")")
                math(EXPR _depth "${_depth} - 1")
                if (_depth EQUAL 0)
                    string(STRIP "${_args}" _args)
                    if (_call MATCHES "^(fetchcontent_declare|fetchcontent_populate|externalproject_add)$")
                        math(EXPR _n_calls "${_n_calls} + 1")
                        if (_args MATCHES "(^|[ \t\r\n])(URL|URL_HASH|URL_MD5|GIT_REPOSITORY|SVN_REPOSITORY|HG_REPOSITORY|CVS_REPOSITORY|DOWNLOAD_COMMAND)([ \t\r\n]|$)")
                            _qore_tp_error("${_rel}:${_call_line}: ${_call_name}() names its source (${CMAKE_MATCH_2}): "
                                "take it from qore_third_party_source_args()")
                        endif()
                    elseif (_call STREQUAL "skip")
                    elseif (_call STREQUAL "file")
                        if (_args MATCHES "^DOWNLOAD([ \t\r\n]|$)")
                            _qore_tp_error("${_rel}:${_call_line}: file(DOWNLOAD) outside "
                                "QoreThirdPartySources.cmake: add the archive to ${_manifest_name}")
                        endif()
                    else()
                        # qore_third_party_source_args() or qore_third_party_version(): the first argument is a name
                        string(REGEX MATCH "^[^ \t\r\n]+" _name "${_args}")
                        get_property(_entry GLOBAL PROPERTY "QORE_THIRD_PARTY_SOURCE_${_name}")
                        if (NOT _entry)
                            _qore_tp_error("${_rel}:${_call_line}: ${_call_name}(${_name}): ${_name} is not "
                                "listed in ${_manifest_name}")
                        elseif (_call STREQUAL "qore_third_party_source_args")
                            list(APPEND _used "${_name}")
                        endif()
                    endif()
                    set(_call "")
                else()
                    string(APPEND _args " ) ")
                endif()
            else()
                string(APPEND _args "${_tok}")
            endif()
        elseif (_tok STREQUAL "(")
            string(TOLOWER "${_prev}" _lprev)
            if (_lprev MATCHES "(^|[^a-z0-9_])(fetchcontent_declare|fetchcontent_populate|externalproject_add|file|qore_third_party_source_args|qore_third_party_version)[ \t]*$")
                set(_call "${CMAKE_MATCH_2}")
                string(REGEX MATCH "[A-Za-z0-9_]+[ \t]*$" _call_name "${_prev}")
                string(STRIP "${_call_name}" _call_name)
                set(_call_line ${_line})
                set(_depth 1)
                set(_args "")
                # the module itself defines the commands and downloads from the mirror; the manifest registers
                if ((file STREQUAL _module OR file STREQUAL _self)
                        AND NOT _call MATCHES "^(fetchcontent_declare|fetchcontent_populate|externalproject_add)$")
                    set(_call "skip")
                endif()
            endif()
        endif()
        if (NOT _tok MATCHES "^\"")
            set(_prev "${_tok}")
        endif()
        math(EXPR _line "${_line} + ${_n_nl}")
    endforeach()
    set(_errors "${_errors}" PARENT_SCOPE)
    set(_used "${_used}" PARENT_SCOPE)
    set(_n_calls "${_n_calls}" PARENT_SCOPE)
endfunction()

set(_files)
_qore_tp_collect("${QORE_SOURCE_DIR}" _files)
list(SORT _files)
foreach(_file IN LISTS _files)
    _qore_tp_check_file("${_file}")
endforeach()

get_property(_names GLOBAL PROPERTY QORE_THIRD_PARTY_SOURCES)
list(LENGTH _names _n_entries)
foreach(_name IN LISTS _names)
    _qore_third_party_entry("${_name}" _tp)
    if (NOT _tp_URL MATCHES "^https://")
        _qore_tp_error("${_name} ${_tp_VERSION}: the upstream URL must use https: ${_tp_URL}")
    endif()
    if (NOT _name IN_LIST _used)
        _qore_tp_error("${_manifest_name}: ${_name} is not used by the build, remove it")
    endif()
endforeach()

if (QORE_THIRD_PARTY_MIRROR)
    string(RANDOM LENGTH 12 _tag)
    set(_tmp "${CMAKE_CURRENT_BINARY_DIR}/qore-third-party-check-${_tag}")
    foreach(_name IN LISTS _names)
        _qore_third_party_entry("${_name}" _tp)
        _qore_third_party_mirror_download("${_tp_MIRROR_URL}" "${_tmp}/${_tp_FILE}" "${_tp_SHA256}" _result)
        file(REMOVE "${_tmp}/${_tp_FILE}")
        if (_result STREQUAL "ok")
            message(STATUS "${_name} ${_tp_VERSION}: the mirror has ${_tp_FILE} with the pinned SHA-256")
        elseif (_result MATCHES "^mismatch:(.*)$")
            _qore_tp_error("${_name} ${_tp_VERSION}: the mirror's ${_tp_MIRROR_URL} has SHA-256 ${CMAKE_MATCH_1}, "
                "${_manifest_name} pins ${_tp_SHA256}")
        else()
            string(REGEX REPLACE "^failed:" "" _reason "${_result}")
            _qore_tp_error("${_name} ${_tp_VERSION}: not available from the mirror (${_reason}: no entry, or no "
                "access): ${_tp_MIRROR_URL}: upload it as described in ${_manifest_name}")
        endif()
    endforeach()
    file(REMOVE_RECURSE "${_tmp}")
endif()

if (_errors)
    string(ASCII 1 _semi)
    string(ASCII 2 _open)
    string(ASCII 3 _close)
    set(_msg "")
    foreach(_error IN LISTS _errors)
        string(REPLACE "${_semi}" ";" _error "${_error}")
        string(REPLACE "${_open}" "[" _error "${_error}")
        string(REPLACE "${_close}" "]" _error "${_error}")
        string(APPEND _msg "\n  ${_error}")
    endforeach()
    message(FATAL_ERROR "third-party sources:${_msg}")
endif()
list(LENGTH _files _n_files)
set(_with_mirror "")
if (QORE_THIRD_PARTY_MIRROR)
    set(_with_mirror ", all in the mirror")
endif()
message(STATUS "third-party sources: ${_n_entries} archives in the manifest${_with_mirror}; "
    "${_n_calls} download declarations in ${_n_files} files take their source from it")
