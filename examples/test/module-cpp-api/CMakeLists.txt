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
    PACKAGE_VERSION="${VERSION}")
set_target_properties(cppapitest PROPERTIES
    PREFIX ""
    SUFFIX "${_cppapi_test_module_suffix}"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/modules/cppapitest")

# --- the consumer -------------------------------------------------------------------------------

qore_wrap_qpp_value(CPPAPIUSER_QPP_SOURCES ql_cppapiuser.qpp)
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
    PACKAGE_VERSION="${VERSION}")
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
