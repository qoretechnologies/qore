# Test modules for the module C++ API mechanism (see include/qore/QoreModuleCppApi.h).
#
# "cppapitest" publishes a C++ API; "cppapiuser" consumes it from C++ and exposes the result to
# Qore-language code so that module-cpp-api.qtest can drive the mechanism.  Neither links against
# the other, and neither is installed: both are built into ${CMAKE_BINARY_DIR}/modules/<name> so
# that run_tests.sh puts them on QORE_MODULE_DIR along with the real binary modules.

include(${CMAKE_SOURCE_DIR}/cmake/QoreMacros.cmake)

set(_cppapi_test_module_suffix "-api-${MODULE_API_MAJOR}.${MODULE_API_MINOR}.qmod")

# --- the producer -------------------------------------------------------------------------------

add_library(cppapitest MODULE cppapitest-module.cpp)
add_dependencies(cppapitest libqore)
target_include_directories(cppapitest PRIVATE
    ${CMAKE_BINARY_DIR}
    ${CMAKE_BINARY_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include)
target_compile_definitions(cppapitest PRIVATE
    BUILDING_DLL=1
    HAVE_UNIX_CONFIG_H
    PACKAGE_VERSION="${VERSION}"
    # DLLLOCAL hides symbols as in every other module (see QORE_BINARY_MODULE_INTERN2 in QoreMacros.cmake)
    $<$<BOOL:${HAVE_GCC_VISIBILITY}>:HAVE_GCC_VISIBILITY=1>)
set_target_properties(cppapitest PROPERTIES
    PREFIX ""
    SUFFIX "${_cppapi_test_module_suffix}"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/modules/cppapitest")

# --- the consumer -------------------------------------------------------------------------------

qore_wrap_qpp_value(CPPAPIUSER_QPP_SOURCES ql_cppapiuser.qpp QC_ProgramContextProbe.qpp
    QC_SerializationIndexProbe.qpp QC_ExceptionResultProbe.qpp QC_PollableProbe.qpp QC_MemberWriteProbe.qpp
    QC_MemberAccessorProbe.qpp)
add_custom_target(CPPAPIUSER_QPP_GENERATED_FILES DEPENDS qpp ${CPPAPIUSER_QPP_SOURCES})

add_library(cppapiuser MODULE cppapiuser-module.cpp ${CPPAPIUSER_QPP_SOURCES})
add_dependencies(cppapiuser libqore CPPAPIUSER_QPP_GENERATED_FILES)
target_include_directories(cppapiuser PRIVATE
    ${CMAKE_BINARY_DIR}
    ${CMAKE_BINARY_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include)
target_compile_definitions(cppapiuser PRIVATE
    BUILDING_DLL=1
    HAVE_UNIX_CONFIG_H
    PACKAGE_VERSION="${VERSION}"
    # DLLLOCAL hides symbols as in every other module (see QORE_BINARY_MODULE_INTERN2 in QoreMacros.cmake)
    $<$<BOOL:${HAVE_GCC_VISIBILITY}>:HAVE_GCC_VISIBILITY=1>)
set_target_properties(cppapiuser PROPERTIES
    PREFIX ""
    SUFFIX "${_cppapi_test_module_suffix}"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/modules/cppapiuser")

# the mechanism resolves symbols through the module's own dlopen() handle, so neither module is
# linked against the other or against libqore; macOS needs the same dynamic lookup that
# QORE_BINARY_MODULE_INTERN2 sets up for installed modules
if (CMAKE_HOST_APPLE)
    target_link_options(cppapitest PRIVATE -Wl,-undefined -Wl,dynamic_lookup)
    target_link_options(cppapiuser PRIVATE -Wl,-undefined -Wl,dynamic_lookup)
endif()

# --- the modules with exit-time code that is not safe in a child created with fork() -------------
# (see fork-exit/forkexit-module.cpp): one source built twice, with an atexit() handler (forkexita, whose fork
# handler has a leaf rank) and with a static destructor (forkexitb, whose fork handler has the reset rank)

# --- the modules with a module command handler (issue #5699) ---------------------------------------------------
# one source built twice (module-command/cmdctx-module.cpp): cmdctxaware declares that its handler honors the
# restrictions of the code that issues a command (QoreModuleInfo::cmd_caller_aware), cmdctxlegacy does not

foreach(_cmdctx_module cmdctxaware cmdctxlegacy)
    if (_cmdctx_module STREQUAL "cmdctxaware")
        set(_cmdctx_aware true)
    else()
        set(_cmdctx_aware false)
    endif()
    add_library(${_cmdctx_module} MODULE module-command/cmdctx-module.cpp)
    add_dependencies(${_cmdctx_module} libqore)
    target_include_directories(${_cmdctx_module} PRIVATE
        ${CMAKE_BINARY_DIR}
        ${CMAKE_BINARY_DIR}/include
        ${CMAKE_SOURCE_DIR}/include)
    target_compile_definitions(${_cmdctx_module} PRIVATE
        BUILDING_DLL=1
        HAVE_UNIX_CONFIG_H
        PACKAGE_VERSION="${VERSION}"
        CMDCTX_ID=${_cmdctx_module}
        CMDCTX_NAME="${_cmdctx_module}"
        CMDCTX_AWARE=${_cmdctx_aware}
        $<$<BOOL:${HAVE_GCC_VISIBILITY}>:HAVE_GCC_VISIBILITY=1>)
    set_target_properties(${_cmdctx_module} PROPERTIES
        PREFIX ""
        SUFFIX "${_cppapi_test_module_suffix}"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/modules/${_cmdctx_module}")
    if (CMAKE_HOST_APPLE)
        target_link_options(${_cmdctx_module} PRIVATE -Wl,-undefined -Wl,dynamic_lookup)
    endif()
endforeach()

# --- the module with builtins of the uncontrolled-API functional domain (qore#5741) ----------------------------------
# a builtin class that Qore code can inherit and builtins with a default argument whose evaluation is visible
# (uncontrolled-api/uncontrolledprobe-module.cpp)

add_library(uncontrolledprobe MODULE uncontrolled-api/uncontrolledprobe-module.cpp)
add_dependencies(uncontrolledprobe libqore)
target_include_directories(uncontrolledprobe PRIVATE
    ${CMAKE_BINARY_DIR}
    ${CMAKE_BINARY_DIR}/include
    ${CMAKE_SOURCE_DIR}/include)
target_compile_definitions(uncontrolledprobe PRIVATE
    BUILDING_DLL=1
    HAVE_UNIX_CONFIG_H
    PACKAGE_VERSION="${VERSION}"
    $<$<BOOL:${HAVE_GCC_VISIBILITY}>:HAVE_GCC_VISIBILITY=1>)
set_target_properties(uncontrolledprobe PROPERTIES
    PREFIX ""
    SUFFIX "${_cppapi_test_module_suffix}"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/modules/uncontrolledprobe")
if (CMAKE_HOST_APPLE)
    target_link_options(uncontrolledprobe PRIVATE -Wl,-undefined -Wl,dynamic_lookup)
endif()

# --- the module that runs code of a Program with and without entering it (qore#6185) ---------------------------------
# (program-tlpd/tlpdprobe-module.cpp)

add_library(tlpdprobe MODULE program-tlpd/tlpdprobe-module.cpp)
add_dependencies(tlpdprobe libqore)
target_include_directories(tlpdprobe PRIVATE
    ${CMAKE_BINARY_DIR}
    ${CMAKE_BINARY_DIR}/include
    ${CMAKE_SOURCE_DIR}/include)
target_compile_definitions(tlpdprobe PRIVATE
    BUILDING_DLL=1
    HAVE_UNIX_CONFIG_H
    PACKAGE_VERSION="${VERSION}"
    $<$<BOOL:${HAVE_GCC_VISIBILITY}>:HAVE_GCC_VISIBILITY=1>)
set_target_properties(tlpdprobe PROPERTIES
    PREFIX ""
    SUFFIX "${_cppapi_test_module_suffix}"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/modules/tlpdprobe")
if (CMAKE_HOST_APPLE)
    target_link_options(tlpdprobe PRIVATE -Wl,-undefined -Wl,dynamic_lookup)
endif()

# --- the module with a DBI driver that reports the setters of its options (qore#5741) --------------------------------
# (datasource-option-setter/dsoptprobe-module.cpp)

add_library(dsoptprobe MODULE datasource-option-setter/dsoptprobe-module.cpp)
add_dependencies(dsoptprobe libqore)
target_include_directories(dsoptprobe PRIVATE
    ${CMAKE_BINARY_DIR}
    ${CMAKE_BINARY_DIR}/include
    ${CMAKE_SOURCE_DIR}/include)
target_compile_definitions(dsoptprobe PRIVATE
    BUILDING_DLL=1
    HAVE_UNIX_CONFIG_H
    PACKAGE_VERSION="${VERSION}"
    $<$<BOOL:${HAVE_GCC_VISIBILITY}>:HAVE_GCC_VISIBILITY=1>)
set_target_properties(dsoptprobe PROPERTIES
    PREFIX ""
    SUFFIX "${_cppapi_test_module_suffix}"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/modules/dsoptprobe")
if (CMAKE_HOST_APPLE)
    target_link_options(dsoptprobe PRIVATE -Wl,-undefined -Wl,dynamic_lookup)
endif()

# --- the module with DBI drivers that report the destinations of their connections (qore#5747) ------------------------
# (datasource-sandbox-targets/dbitargetprobe-module.cpp)

add_library(dbitargetprobe MODULE datasource-sandbox-targets/dbitargetprobe-module.cpp)
add_dependencies(dbitargetprobe libqore)
target_include_directories(dbitargetprobe PRIVATE
    ${CMAKE_BINARY_DIR}
    ${CMAKE_BINARY_DIR}/include
    ${CMAKE_SOURCE_DIR}/include)
target_compile_definitions(dbitargetprobe PRIVATE
    BUILDING_DLL=1
    HAVE_UNIX_CONFIG_H
    PACKAGE_VERSION="${VERSION}"
    $<$<BOOL:${HAVE_GCC_VISIBILITY}>:HAVE_GCC_VISIBILITY=1>)
set_target_properties(dbitargetprobe PROPERTIES
    PREFIX ""
    SUFFIX "${_cppapi_test_module_suffix}"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/modules/dbitargetprobe")
if (CMAKE_HOST_APPLE)
    target_link_options(dbitargetprobe PRIVATE -Wl,-undefined -Wl,dynamic_lookup)
endif()

foreach(_forkexit_module forkexita forkexitb)
    add_library(${_forkexit_module} MODULE fork-exit/forkexit-module.cpp)
    add_dependencies(${_forkexit_module} libqore)
    target_include_directories(${_forkexit_module} PRIVATE
        ${CMAKE_BINARY_DIR}
        ${CMAKE_BINARY_DIR}/include
        ${CMAKE_SOURCE_DIR}/include)
    target_compile_definitions(${_forkexit_module} PRIVATE
        BUILDING_DLL=1
        HAVE_UNIX_CONFIG_H
        PACKAGE_VERSION="${VERSION}"
        FORKEXIT_ID=${_forkexit_module}
        FORKEXIT_NAME="${_forkexit_module}"
        $<$<BOOL:${HAVE_GCC_VISIBILITY}>:HAVE_GCC_VISIBILITY=1>)
    set_target_properties(${_forkexit_module} PROPERTIES
        PREFIX ""
        SUFFIX "${_cppapi_test_module_suffix}"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/modules/${_forkexit_module}")
    if (CMAKE_HOST_APPLE)
        target_link_options(${_forkexit_module} PRIVATE -Wl,-undefined -Wl,dynamic_lookup)
    endif()
endforeach()
target_compile_definitions(forkexita PRIVATE FORKEXIT_NS="ForkExitA" FORKEXIT_ATEXIT=1 FORKEXIT_RANK=QFR_MODULE_LEAF)
target_compile_definitions(forkexitb PRIVATE FORKEXIT_NS="ForkExitB" FORKEXIT_ATEXIT=0 FORKEXIT_RANK=QFR_MODULE_RESET)

# --- the plugin modules whose load order changes the process-global ids of their operations ------------------
# (see plugin-order/pluginorder-module.cpp and plugin-order/plugin-order-identity.qtest): one source built twice

foreach(_pluginorder_module pluginordera pluginorderb)
    add_library(${_pluginorder_module} MODULE plugin-order/pluginorder-module.cpp)
    add_dependencies(${_pluginorder_module} libqore)
    target_include_directories(${_pluginorder_module} PRIVATE
        ${CMAKE_BINARY_DIR}
        ${CMAKE_BINARY_DIR}/include
        ${CMAKE_SOURCE_DIR}/include)
    target_compile_definitions(${_pluginorder_module} PRIVATE
        BUILDING_DLL=1
        HAVE_UNIX_CONFIG_H
        PACKAGE_VERSION="${VERSION}"
        PLUGINORDER_ID=${_pluginorder_module}
        PLUGINORDER_NAME="${_pluginorder_module}"
        $<$<BOOL:${HAVE_GCC_VISIBILITY}>:HAVE_GCC_VISIBILITY=1>)
    set_target_properties(${_pluginorder_module} PROPERTIES
        PREFIX ""
        SUFFIX "${_cppapi_test_module_suffix}"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/modules/${_pluginorder_module}")
    if (CMAKE_HOST_APPLE)
        target_link_options(${_pluginorder_module} PRIVATE -Wl,-undefined -Wl,dynamic_lookup)
    endif()
endforeach()
target_compile_definitions(pluginordera PRIVATE PLUGINORDER_NS="PluginOrderA" PLUGINORDER_OFFSET=1000)
target_compile_definitions(pluginorderb PRIVATE PLUGINORDER_NS="PluginOrderB" PLUGINORDER_OFFSET=2000)

# --- the modules that declare a module API version -----------------------------------------------
# (see module-api/apiprobe-module.cpp and module-api/module-api-version.qtest): one source built twice, declaring
# the module API of the headers (apicur) and its preceding minor (apiold); the latter has the file name
# and descriptor of a module compiled with the previous headers.

foreach(_apiprobe_module apicur apiold)
    add_library(${_apiprobe_module} MODULE module-api/apiprobe-module.cpp)
    add_dependencies(${_apiprobe_module} libqore)
    target_include_directories(${_apiprobe_module} PRIVATE
        ${CMAKE_BINARY_DIR}
        ${CMAKE_BINARY_DIR}/include
        ${CMAKE_SOURCE_DIR}/include)
    target_compile_definitions(${_apiprobe_module} PRIVATE
        BUILDING_DLL=1
        HAVE_UNIX_CONFIG_H
        PACKAGE_VERSION="${VERSION}"
        APIPROBE_ID=${_apiprobe_module}
        APIPROBE_NAME="${_apiprobe_module}"
        $<$<BOOL:${HAVE_GCC_VISIBILITY}>:HAVE_GCC_VISIBILITY=1>)
    set_target_properties(${_apiprobe_module} PROPERTIES
        PREFIX ""
        SUFFIX "${_cppapi_test_module_suffix}"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/modules/${_apiprobe_module}")
    if (CMAKE_HOST_APPLE)
        target_link_options(${_apiprobe_module} PRIVATE -Wl,-undefined -Wl,dynamic_lookup)
    endif()
endforeach()
math(EXPR _apiprobe_old_minor "${MODULE_API_MINOR} - 1")
set_target_properties(apiold PROPERTIES SUFFIX "-api-${MODULE_API_MAJOR}.${_apiprobe_old_minor}.qmod")
target_compile_definitions(apicur PRIVATE APIPROBE_MAJOR=QORE_MODULE_API_MAJOR APIPROBE_MINOR=QORE_MODULE_API_MINOR)
target_compile_definitions(apiold PRIVATE APIPROBE_MAJOR=${MODULE_API_MAJOR} APIPROBE_MINOR=${_apiprobe_old_minor})

# --- the module that owns Programs, which it deletes when it is deleted ---------------------------
# (see program-owner/tdpgmowner-module.cpp and program-owner/program-owner-teardown.qtest)

add_library(tdpgmowner MODULE program-owner/tdpgmowner-module.cpp)
add_dependencies(tdpgmowner libqore)
target_include_directories(tdpgmowner PRIVATE
    ${CMAKE_BINARY_DIR}
    ${CMAKE_BINARY_DIR}/include
    ${CMAKE_SOURCE_DIR}/include)
target_compile_definitions(tdpgmowner PRIVATE
    BUILDING_DLL=1
    HAVE_UNIX_CONFIG_H
    PACKAGE_VERSION="${VERSION}"
    $<$<BOOL:${HAVE_GCC_VISIBILITY}>:HAVE_GCC_VISIBILITY=1>)
set_target_properties(tdpgmowner PROPERTIES
    PREFIX ""
    SUFFIX "${_cppapi_test_module_suffix}"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/modules/tdpgmowner")
if (CMAKE_HOST_APPLE)
    target_link_options(tdpgmowner PRIVATE -Wl,-undefined -Wl,dynamic_lookup)
endif()

# --- the modules initialized with the legacy AOT module entry point -------------------------------
# (see aot-v1-init/aotv1init-module.cpp and qore/misc/module-loader/module-load-failure-cleanup.qtest): one source
# built twice, with an initialization that fails (aotv1fail) and one that succeeds (aotv1ok)

foreach(_aotv1_module aotv1fail aotv1ok)
    add_library(${_aotv1_module} MODULE aot-v1-init/aotv1init-module.cpp)
    add_dependencies(${_aotv1_module} libqore)
    target_include_directories(${_aotv1_module} PRIVATE
        ${CMAKE_BINARY_DIR}
        ${CMAKE_BINARY_DIR}/include
        ${CMAKE_SOURCE_DIR}/include)
    target_compile_definitions(${_aotv1_module} PRIVATE
        BUILDING_DLL=1
        HAVE_UNIX_CONFIG_H
        PACKAGE_VERSION="${VERSION}"
        AOTV1_ID=${_aotv1_module}
        AOTV1_NAME="${_aotv1_module}"
        $<$<BOOL:${HAVE_GCC_VISIBILITY}>:HAVE_GCC_VISIBILITY=1>)
    set_target_properties(${_aotv1_module} PROPERTIES
        PREFIX ""
        SUFFIX "${_cppapi_test_module_suffix}"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/modules/${_aotv1_module}")
    if (CMAKE_HOST_APPLE)
        target_link_options(${_aotv1_module} PRIVATE -Wl,-undefined -Wl,dynamic_lookup)
    endif()
endforeach()
target_compile_definitions(aotv1fail PRIVATE AOTV1_FAIL=1)
target_compile_definitions(aotv1ok PRIVATE AOTV1_FAIL=0)

# remove the files of these modules built for another module API (see qore_remove_stale_module_api_files())
foreach(_cppapi_test_module cppapitest cppapiuser cmdctxaware cmdctxlegacy uncontrolledprobe dsoptprobe dbitargetprobe
        tlpdprobe forkexita forkexitb apicur apiold tdpgmowner aotv1fail aotv1ok pluginordera pluginorderb)
    qore_remove_stale_module_api_files(${_cppapi_test_module})
endforeach()
