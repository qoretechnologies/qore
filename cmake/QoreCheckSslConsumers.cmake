# -*- mode: cmake -*-
#
# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# QoreCheckSslConsumers.cmake: fails the configuration when a library linked into libqore uses another OpenSSL
# than the one that Qore found
#
# A shared library that libqore links and that uses OpenSSL itself (ex: libngtcp2_crypto_ossl from a system
# package) must use the same libssl and libcrypto as Qore: with two OpenSSL libraries in one process, an object
# created by one is used by the other, which crashes (ex: SIGSEGV in SSL_set0_rbio when the TLS session of a QUIC
# connection created by libqore's libssl is configured by ngtcp2's libssl).
#
# - on macOS, every install name is loaded separately, so the libraries must be the same files
# - on ELF platforms, the dynamic loader loads one library per SONAME, so the SONAMEs must be the same
#
# A static library uses the OpenSSL that libqore links and is not checked.

# the directory of this module and of its dependency script
set(_QORE_SSL_CHECK_DIR "${CMAKE_CURRENT_LIST_DIR}")

# Returns the files of the shared libraries that a link item stands for
function(_qore_ssl_consumer_files item out_var)
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
    endif()
    set(_shared)
    foreach(_file IN LISTS _files)
        if (EXISTS "${_file}" AND NOT _file MATCHES "\\.(a|lib)$")
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

# Raises the configuration error for a library that uses another OpenSSL library than Qore
function(_qore_ssl_consumer_mismatch consumer dep qore_lib)
    message(FATAL_ERROR "${consumer} uses ${dep}, but Qore uses ${qore_lib} (OpenSSL ${OPENSSL_VERSION}).  Two "
        "OpenSSL libraries in one process crash TLS and QUIC connections (ex: SIGSEGV in SSL_set0_rbio): build Qore "
        "with the OpenSSL that the library uses (set OPENSSL_ROOT_DIR to it), or install a build of the library "
        "that uses Qore's OpenSSL")
endfunction()

# Checks the shared libraries of the given link items against the OpenSSL that Qore found
#
# qore_check_ssl_consumers(<link item>...)
#
# A link item is a library file, an imported target, or a name; OpenSSL's own libraries are skipped.
function(qore_check_ssl_consumers)
    set(_qore_ssl "${OPENSSL_SSL_LIBRARY}")
    set(_qore_crypto "${OPENSSL_CRYPTO_LIBRARY}")
    if (NOT _qore_ssl OR NOT _qore_crypto)
        return()
    endif()
    get_filename_component(_qore_ssl_real "${_qore_ssl}" REALPATH)
    get_filename_component(_qore_crypto_real "${_qore_crypto}" REALPATH)

    set(_consumers)
    foreach(_item IN LISTS ARGN)
        _qore_ssl_consumer_files("${_item}" _files)
        foreach(_file IN LISTS _files)
            get_filename_component(_real "${_file}" REALPATH)
            _qore_ssl_kind("${_real}" _own_kind)
            if (NOT _own_kind AND NOT _real STREQUAL _qore_ssl_real AND NOT _real STREQUAL _qore_crypto_real)
                list(APPEND _consumers "${_file}")
            endif()
        endforeach()
    endforeach()
    if (NOT _consumers)
        return()
    endif()
    list(REMOVE_DUPLICATES _consumers)

    if (APPLE)
        if (CMAKE_VERSION VERSION_LESS 3.16)
            message(STATUS "cannot check the OpenSSL libraries used by linked libraries with CMake ${CMAKE_VERSION} "
                "(3.16 or later is needed)")
            return()
        endif()
        set(_otool "${CMAKE_OTOOL}")
        if (NOT _otool)
            find_program(QORE_SSL_CHECK_OTOOL NAMES otool llvm-otool)
            set(_otool "${QORE_SSL_CHECK_OTOOL}")
        endif()
        if (NOT _otool)
            message(STATUS "cannot check the OpenSSL libraries used by linked libraries: no otool")
            return()
        endif()
        set(_deps_file "${CMAKE_BINARY_DIR}/CMakeFiles/qore-ssl-consumer-deps.txt")
        foreach(_consumer IN LISTS _consumers)
            # the install names, including @rpath ones, are resolved as dyld would, in a script (see
            # QoreSslConsumerDeps.cmake)
            execute_process(COMMAND "${CMAKE_COMMAND}" "-DQORE_SSL_CONSUMER=${_consumer}"
                    "-DQORE_SSL_OTOOL=${_otool}" "-DQORE_SSL_OUTPUT=${_deps_file}"
                    -P "${_QORE_SSL_CHECK_DIR}/QoreSslConsumerDeps.cmake"
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
            if (NOT _rc EQUAL 0)
                message(FATAL_ERROR "cannot read the libraries that ${_consumer} loads: ${_err}")
            endif()
            file(STRINGS "${_deps_file}" _lines)
            file(REMOVE "${_deps_file}")
            foreach(_line IN LISTS _lines)
                if (_line MATCHES "^resolved (.*)$")
                    set(_dep "${CMAKE_MATCH_1}")
                    _qore_ssl_kind("${_dep}" _kind)
                    if (NOT _kind)
                        continue()
                    endif()
                    get_filename_component(_dep_real "${_dep}" REALPATH)
                    if (NOT _dep_real STREQUAL _qore_${_kind}_real)
                        _qore_ssl_consumer_mismatch("${_consumer}" "${_dep_real}" "${_qore_${_kind}_real}")
                    endif()
                elseif (_line MATCHES "^unresolved (.*)$")
                    message(WARNING "cannot verify that ${_consumer} uses Qore's OpenSSL: its dependency "
                        "${CMAKE_MATCH_1} cannot be resolved")
                endif()
            endforeach()
        endforeach()
    else()
        set(_objdump "${CMAKE_OBJDUMP}")
        if (NOT _objdump)
            find_program(QORE_SSL_CHECK_OBJDUMP NAMES objdump llvm-objdump)
            set(_objdump "${QORE_SSL_CHECK_OBJDUMP}")
        endif()
        if (NOT _objdump)
            message(STATUS "cannot check the OpenSSL libraries used by linked libraries: no objdump")
            return()
        endif()
        foreach(_kind ssl crypto)
            if (_qore_${_kind}_real MATCHES "\\.a$")
                # a static OpenSSL is in libqore itself
                set(_qore_${_kind}_soname "")
            else()
                _qore_elf_soname("${_objdump}" "${_qore_${_kind}_real}" _qore_${_kind}_soname)
                if (NOT _qore_${_kind}_soname)
                    get_filename_component(_qore_${_kind}_soname "${_qore_${_kind}_real}" NAME)
                endif()
            endif()
        endforeach()
        foreach(_consumer IN LISTS _consumers)
            execute_process(COMMAND "${_objdump}" -p "${_consumer}" OUTPUT_VARIABLE _out ERROR_QUIET
                RESULT_VARIABLE _rc)
            if (NOT _rc EQUAL 0)
                continue()
            endif()
            string(REGEX MATCHALL "NEEDED[ \t]+[^ \t\r\n]+" _needed "${_out}")
            foreach(_entry IN LISTS _needed)
                string(REGEX REPLACE "^NEEDED[ \t]+" "" _dep "${_entry}")
                _qore_ssl_kind("${_dep}" _kind)
                if (NOT _kind)
                    continue()
                endif()
                if (NOT _dep STREQUAL _qore_${_kind}_soname)
                    if (_qore_${_kind}_soname)
                        set(_qore_desc "${_qore_${_kind}_real} (${_qore_${_kind}_soname})")
                    else()
                        set(_qore_desc "${_qore_${_kind}_real} (a static library linked into libqore)")
                    endif()
                    _qore_ssl_consumer_mismatch("${_consumer}" "${_dep}" "${_qore_desc}")
                endif()
            endforeach()
        endforeach()
    endif()
endfunction()
