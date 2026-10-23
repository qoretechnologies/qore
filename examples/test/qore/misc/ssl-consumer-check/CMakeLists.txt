# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# Fixture for SslConsumerCheck.qtest: QoreCheckSslConsumers.cmake on modules and libraries linked to another
# OpenSSL.  PHASE=build builds a stand-in for another OpenSSL (a libssl with another version: libssl.so.4 or
# libssl.4.dylib) and libraries that load it or the real OpenSSL; PHASE=check runs the check for CASE on them.
cmake_minimum_required(VERSION 3.16)
project(qore_ssl_consumer_check C)

find_package(OpenSSL)
if (NOT OPENSSL_FOUND)
    message(STATUS "QORE-SSL-CHECK-SKIP: no OpenSSL development files")
    return()
endif()

if (PHASE STREQUAL "build")
    # another OpenSSL: only its file name and version matter to the check
    add_library(other_ssl SHARED other_ssl.c)
    set_target_properties(other_ssl PROPERTIES OUTPUT_NAME ssl VERSION 4 SOVERSION 4)
    # a library that loads the other OpenSSL, and one that loads the real one
    add_library(dep_other SHARED dep.c)
    target_compile_definitions(dep_other PRIVATE OTHER_SSL)
    target_link_libraries(dep_other PRIVATE other_ssl)
    add_library(dep_same SHARED dep.c)
    target_link_libraries(dep_same PRIVATE OpenSSL::SSL)
    # one library name in two directories: with the other OpenSSL in flagdir, with the real one in pathdir
    add_library(dep_choice_other SHARED dep.c)
    target_compile_definitions(dep_choice_other PRIVATE OTHER_SSL)
    target_link_libraries(dep_choice_other PRIVATE other_ssl)
    set_target_properties(dep_choice_other PROPERTIES OUTPUT_NAME dep_choice
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/flagdir")
    add_library(dep_choice_same SHARED dep.c)
    target_link_libraries(dep_choice_same PRIVATE OpenSSL::SSL)
    set_target_properties(dep_choice_same PROPERTIES OUTPUT_NAME dep_choice
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/pathdir")
    return()
endif()

include("${QORE_CMAKE_DIR}/QoreCheckSslConsumers.cmake")
set(_dep_other "${DEPS_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}dep_other${CMAKE_SHARED_LIBRARY_SUFFIX}")
set(_dep_same "${DEPS_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}dep_same${CMAKE_SHARED_LIBRARY_SUFFIX}")
if (CASE STREQUAL "libqore-other")
    qore_check_ssl_consumers("${_dep_other}" ${OPENSSL_LIBRARIES})
elseif (CASE STREQUAL "libqore-same")
    qore_check_ssl_consumers("${_dep_same}" ${OPENSSL_LIBRARIES})
elseif (CASE STREQUAL "module-own-openssl-other")
    add_library(mod_fail MODULE module.c)
    target_link_libraries(mod_fail OpenSSL::Crypto "${_dep_other}")
    qore_check_module_ssl_consumers(mod_fail)
elseif (CASE STREQUAL "module-private-other")
    add_library(mod_info MODULE module.c)
    target_link_libraries(mod_info "${_dep_other}")
    qore_check_module_ssl_consumers(mod_info)
elseif (CASE STREQUAL "module-own-openssl-same")
    add_library(mod_ok MODULE module.c)
    target_link_libraries(mod_ok OpenSSL::SSL "${_dep_same}")
    qore_check_module_ssl_consumers(mod_ok)
elseif (CASE STREQUAL "module-bare-name")
    # a library given by name in the module's link directories, as from pkg-config
    add_library(mod_name MODULE module.c)
    target_link_directories(mod_name PRIVATE "${DEPS_DIR}")
    target_link_libraries(mod_name "${OPENSSL_CRYPTO_LIBRARY}" dep_other)
    qore_check_module_ssl_consumers(mod_name)
elseif (CASE STREQUAL "module-bare-name-search-path")
    # a library given by name outside the module's link directories, found in the project's library search path
    list(APPEND CMAKE_LIBRARY_PATH "${DEPS_DIR}")
    add_library(mod_path MODULE module.c)
    target_link_libraries(mod_path dep_other)
    qore_check_module_ssl_consumers(mod_path)
elseif (CASE STREQUAL "module-linker-flags-dir")
    # the linker finds a library named in the module's link libraries in a -L directory of the linker flags (ex:
    # from LDFLAGS, as a keg-only Homebrew library) before the project's library search path
    list(APPEND CMAKE_LIBRARY_PATH "${DEPS_DIR}/pathdir")
    string(APPEND CMAKE_MODULE_LINKER_FLAGS " -L${DEPS_DIR}/flagdir")
    add_library(mod_flags MODULE module.c)
    target_link_libraries(mod_flags dep_choice)
    qore_check_module_ssl_consumers(mod_flags)
elseif (CASE STREQUAL "module-linker-script")
    # a GNU ld linker script in place of a library, as /usr/lib64/libyaml.so ("INPUT(libyaml-0.so.2)"): the check
    # follows the library that it names in its directory
    set(_script "${CMAKE_BINARY_DIR}/scriptdir/${CMAKE_SHARED_LIBRARY_PREFIX}dep_script${CMAKE_SHARED_LIBRARY_SUFFIX}")
    file(COPY "${_dep_other}" DESTINATION "${CMAKE_BINARY_DIR}/scriptdir")
    get_filename_component(_dep_other_name "${_dep_other}" NAME)
    file(WRITE "${_script}" "/* GNU ld script */\nINPUT(${_dep_other_name})\n")
    add_library(mod_script MODULE module.c)
    target_link_libraries(mod_script "${_script}")
    qore_check_module_ssl_consumers(mod_script)
elseif (CASE STREQUAL "module-subdir-imported")
    set(DEP_OTHER "${_dep_other}")
    add_subdirectory(subdir)
endif()
message(STATUS "QORE-SSL-CHECK-DONE: ${CASE}")
