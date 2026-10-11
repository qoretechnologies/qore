# -*- mode: cmake -*-
#
# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# QoreCheckSslConsumers.cmake: fails the configuration when a library that can receive Qore's OpenSSL objects uses
# another OpenSSL than the one that Qore found
#
# With two OpenSSL libraries in one process, an object created by one and used by the other crashes (ex: SIGSEGV in
# SSL_set0_rbio when the TLS session of a QUIC connection created by libqore's libssl was configured by the libssl of
# a system libngtcp2_crypto_ossl).
#
# - every shared library linked into libqore must load only Qore's OpenSSL (qore_check_ssl_consumers())
# - a binary module that links OpenSSL itself can exchange OpenSSL objects with libqore and its own libraries, so
#   everything it loads must use Qore's OpenSSL too; a module that does not link OpenSSL cannot hand OpenSSL objects
#   to its libraries, so another OpenSSL that they load privately is reported as information
#   (qore_check_module_ssl_consumers())
#
# On macOS, every install name is loaded separately, so the libraries must be the same files; on ELF platforms, the
# dynamic loader loads one library per SONAME, so the SONAMEs must be the same.  The dependencies of a library are
# examined recursively.  A static library uses the OpenSSL that it is linked with and is not checked.

# the directory of this module and of its dependency script
set(_QORE_SSL_CHECK_DIR "${CMAKE_CURRENT_LIST_DIR}")

# Returns the files of the shared libraries that a link item stands for; link directories resolve bare names
function(_qore_ssl_consumer_files item link_dirs out_var)
    set(_files)
    if (TARGET "${item}")
        get_target_property(_imported "${item}" IMPORTED)
        if (_imported)
            get_target_property(_type "${item}" TYPE)
            if (_type STREQUAL "INTERFACE_LIBRARY")
                # pkg-config imported targets carry the resolved library paths
                get_target_property(_libs "${item}" INTERFACE_LINK_LIBRARIES)
                foreach(_lib IN LISTS _libs)
                    if (NOT TARGET "${_lib}" AND IS_ABSOLUTE "${_lib}")
                        list(APPEND _files "${_lib}")
                    endif()
                endforeach()
            else()
                foreach(_prop IMPORTED_LOCATION IMPORTED_LOCATION_RELEASE IMPORTED_LOCATION_RELWITHDEBINFO
                        IMPORTED_LOCATION_DEBUG IMPORTED_LOCATION_NOCONFIG)
                    get_target_property(_loc "${item}" ${_prop})
                    if (_loc)
                        list(APPEND _files "${_loc}")
                        break()
                    endif()
                endforeach()
            endif()
        endif()
        # a target built in this project (ex: a static library fetched with FetchContent) is not checked
    elseif (IS_ABSOLUTE "${item}")
        list(APPEND _files "${item}")
    elseif (item MATCHES "^(-l)?[A-Za-z0-9_.+-]+$")
        # a bare library name (ex: from pkg-config's LIBRARIES): the target's link directories first, then the
        # project's library search path, as the linker finds it
        string(REGEX REPLACE "^-l" "" _name "${item}")
        unset(_qore_ssl_found CACHE)
        find_library(_qore_ssl_found NAMES "${_name}" HINTS ${link_dirs})
        if (_qore_ssl_found)
            list(APPEND _files "${_qore_ssl_found}")
        endif()
        unset(_qore_ssl_found CACHE)
    endif()
    set(_shared)
    foreach(_file IN LISTS _files)
        # frameworks and SDK stubs (.tbd) are system libraries, and static libraries are not checked
        if (EXISTS "${_file}" AND NOT IS_DIRECTORY "${_file}" AND NOT _file MATCHES "\\.(a|lib|tbd)$")
            list(APPEND _shared "${_file}")
        endif()
    endforeach()
    set(${out_var} "${_shared}" PARENT_SCOPE)
endfunction()

# Returns the OpenSSL library ("ssl" or "crypto") that a library file name is, if any
function(_qore_ssl_kind name out_var)
    get_filename_component(_base "${name}" NAME)
    if (_base MATCHES "^libssl[.-]")
        set(${out_var} ssl PARENT_SCOPE)
    elseif (_base MATCHES "^libcrypto[.-]")
        set(${out_var} crypto PARENT_SCOPE)
    else()
        set(${out_var} "" PARENT_SCOPE)
    endif()
endfunction()

# Returns the SONAME of an ELF shared library, if it has one
function(_qore_elf_soname objdump lib out_var)
    execute_process(COMMAND "${objdump}" -p "${lib}" OUTPUT_VARIABLE _out ERROR_QUIET RESULT_VARIABLE _rc)
    set(_soname "")
    if (_rc EQUAL 0 AND _out MATCHES "SONAME[ \t]+([^ \t\r\n]+)")
        set(_soname "${CMAKE_MATCH_1}")
    endif()
    set(${out_var} "${_soname}" PARENT_SCOPE)
endfunction()

# Sets up the tool and Qore's OpenSSL identities in the caller's scope; sets _qore_ssl_ok to FALSE if the check
# cannot be made here
macro(_qore_ssl_check_setup)
    set(_qore_ssl_ok TRUE)
    if (NOT OPENSSL_SSL_LIBRARY OR NOT OPENSSL_CRYPTO_LIBRARY OR CMAKE_VERSION VERSION_LESS 3.16)
        set(_qore_ssl_ok FALSE)
    else()
        get_filename_component(_qore_ssl_real "${OPENSSL_SSL_LIBRARY}" REALPATH)
        get_filename_component(_qore_crypto_real "${OPENSSL_CRYPTO_LIBRARY}" REALPATH)
        if (APPLE)
            set(_qore_ssl_platform "macos+macho")
            set(_qore_ssl_tool "otool")
            set(_qore_ssl_command "${CMAKE_OTOOL}")
            if (NOT _qore_ssl_command)
                find_program(QORE_SSL_CHECK_OTOOL NAMES otool llvm-otool)
                set(_qore_ssl_command "${QORE_SSL_CHECK_OTOOL}")
            endif()
        else()
            set(_qore_ssl_platform "linux+elf")
            set(_qore_ssl_tool "objdump")
            set(_qore_ssl_command "${CMAKE_OBJDUMP}")
            if (NOT _qore_ssl_command)
                find_program(QORE_SSL_CHECK_OBJDUMP NAMES objdump llvm-objdump)
                set(_qore_ssl_command "${QORE_SSL_CHECK_OBJDUMP}")
            endif()
            if (_qore_ssl_command)
                foreach(_kind ssl crypto)
                    if (_qore_${_kind}_real MATCHES "\\.a$")
                        # a static OpenSSL is in libqore itself
                        set(_qore_${_kind}_id "")
                    else()
                        _qore_elf_soname("${_qore_ssl_command}" "${_qore_${_kind}_real}" _qore_${_kind}_id)
                        if (NOT _qore_${_kind}_id)
                            get_filename_component(_qore_${_kind}_id "${_qore_${_kind}_real}" NAME)
                        endif()
                    endif()
                endforeach()
            endif()
        endif()
        if (NOT _qore_ssl_command)
            set(_qore_ssl_ok FALSE)
        endif()
    endif()
endmacro()

# Returns the OpenSSL libraries that a library loads, directly or through its dependencies, as "resolved <path>" or
# "unresolved <name>" entries, and the error if they cannot be read; the dependencies are resolved in a script (see
# QoreSslConsumerDeps.cmake)
function(_qore_ssl_closure consumer out_var err_var)
    set(_deps_file "${CMAKE_BINARY_DIR}/CMakeFiles/qore-ssl-consumer-deps.txt")
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DQORE_SSL_CONSUMER=${consumer}"
            "-DQORE_SSL_PLATFORM=${_qore_ssl_platform}" "-DQORE_SSL_TOOL=${_qore_ssl_tool}"
            "-DQORE_SSL_COMMAND=${_qore_ssl_command}" "-DQORE_SSL_OUTPUT=${_deps_file}"
            "-DQORE_SSL_DIRS=${CMAKE_C_IMPLICIT_LINK_DIRECTORIES}"
            -P "${_QORE_SSL_CHECK_DIR}/QoreSslConsumerDeps.cmake"
        RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
    set(_lines)
    if (_rc EQUAL 0)
        file(STRINGS "${_deps_file}" _lines)
        set(_err "")
    else()
        string(STRIP "${_err}" _err)
        string(REGEX REPLACE "[ \t\r\n]+" " " _err "${_err}")
    endif()
    file(REMOVE "${_deps_file}")
    set(${out_var} "${_lines}" PARENT_SCOPE)
    set(${err_var} "${_err}" PARENT_SCOPE)
endfunction()

# Checks the OpenSSL libraries that the consumer files load; another OpenSSL is a configuration error if strict,
# otherwise information
function(_qore_check_ssl_files label strict)
    foreach(_consumer IN LISTS ARGN)
        _qore_ssl_closure("${_consumer}" _entries _closure_err)
        if (_closure_err)
            if (strict)
                message(FATAL_ERROR "${label}: cannot verify the OpenSSL libraries that ${_consumer} loads: "
                    "${_closure_err}")
            endif()
            message(STATUS "${label}: cannot read the libraries that ${_consumer} loads; the module does not link "
                "OpenSSL, so it passes it no OpenSSL objects: ${_closure_err}")
            continue()
        endif()
        foreach(_entry IN LISTS _entries)
            if (NOT _entry MATCHES "^(resolved|unresolved) (.*)$")
                continue()
            endif()
            set(_state "${CMAKE_MATCH_1}")
            set(_dep "${CMAKE_MATCH_2}")
            _qore_ssl_kind("${_dep}" _kind)
            if (NOT _kind)
                continue()
            endif()
            set(_other "")
            if (APPLE)
                set(_qore_desc "${_qore_${_kind}_real}")
                if (_state STREQUAL "unresolved")
                    set(_other "${_dep}, which cannot be resolved,")
                else()
                    get_filename_component(_dep_real "${_dep}" REALPATH)
                    if (NOT _dep_real STREQUAL _qore_${_kind}_real)
                        set(_other "${_dep_real}")
                    endif()
                endif()
            else()
                # the loader loads one library per SONAME; an unresolved dependency is named by its SONAME too
                get_filename_component(_dep_name "${_dep}" NAME)
                if (_qore_${_kind}_id)
                    set(_qore_desc "${_qore_${_kind}_real} (${_qore_${_kind}_id})")
                else()
                    set(_qore_desc "${_qore_${_kind}_real} (a static library linked into libqore)")
                endif()
                if (NOT _dep_name STREQUAL _qore_${_kind}_id)
                    set(_other "${_dep_name}")
                endif()
            endif()
            if (NOT _other)
                continue()
            endif()
            if (strict)
                message(FATAL_ERROR "${label}: ${_consumer} uses ${_other}, but Qore uses ${_qore_desc} (OpenSSL "
                    "${OPENSSL_VERSION}).  Two OpenSSL libraries in one process crash TLS and QUIC connections (ex: "
                    "SIGSEGV in SSL_set0_rbio): build Qore with the OpenSSL that the library uses (set "
                    "OPENSSL_ROOT_DIR to it), or install a build of the library that uses Qore's OpenSSL")
            endif()
            message(STATUS "${label}: ${_consumer} loads ${_other} privately; the module does not link OpenSSL, so "
                "it passes it no OpenSSL objects (Qore uses ${_qore_desc})")
        endforeach()
    endforeach()
endfunction()

# Checks the shared libraries of the given link items of libqore against the OpenSSL that Qore found
#
# qore_check_ssl_consumers(<link item>...)
#
# A link item is a library file, an imported target, or a name; OpenSSL's own libraries are skipped.
function(qore_check_ssl_consumers)
    _qore_ssl_check_setup()
    if (NOT _qore_ssl_ok)
        message(STATUS "cannot check the OpenSSL libraries used by the libraries linked into libqore here")
        return()
    endif()
    set(_consumers)
    foreach(_item IN LISTS ARGN)
        _qore_ssl_consumer_files("${_item}" "" _files)
        foreach(_file IN LISTS _files)
            _qore_ssl_kind("${_file}" _own_kind)
            if (NOT _own_kind)
                list(APPEND _consumers "${_file}")
            endif()
        endforeach()
    endforeach()
    if (_consumers)
        list(REMOVE_DUPLICATES _consumers)
        _qore_check_ssl_files("libqore" TRUE ${_consumers})
    endif()
endfunction()

# Checks the libraries of a binary module target against the OpenSSL that Qore found
#
# qore_check_module_ssl_consumers(<target>)
#
# A module that links OpenSSL itself must load only Qore's OpenSSL; another OpenSSL loaded by the libraries of a
# module that does not link OpenSSL is reported as information.  Call it in the module's directory, after its
# libraries are linked: imported library targets are visible only there (QORE_BINARY_MODULE_QORE() defers it to the
# end of the directory).
function(qore_check_module_ssl_consumers target)
    if (NOT TARGET "${target}")
        return()
    endif()
    _qore_ssl_check_setup()
    if (NOT _qore_ssl_ok)
        return()
    endif()
    get_target_property(_items "${target}" LINK_LIBRARIES)
    get_target_property(_dirs "${target}" LINK_DIRECTORIES)
    if (NOT _items)
        return()
    endif()
    if (NOT _dirs)
        set(_dirs "")
    endif()
    set(_links_openssl FALSE)
    set(_consumers)
    foreach(_item IN LISTS _items)
        if (_item MATCHES "^OpenSSL::(SSL|Crypto)$")
            set(_links_openssl TRUE)
            continue()
        endif()
        _qore_ssl_consumer_files("${_item}" "${_dirs}" _files)
        foreach(_file IN LISTS _files)
            _qore_ssl_kind("${_file}" _own_kind)
            if (_own_kind)
                set(_links_openssl TRUE)
            else()
                list(APPEND _consumers "${_file}")
            endif()
        endforeach()
    endforeach()
    if (_consumers)
        list(REMOVE_DUPLICATES _consumers)
        _qore_check_ssl_files("module ${target}" ${_links_openssl} ${_consumers})
    endif()
endfunction()
