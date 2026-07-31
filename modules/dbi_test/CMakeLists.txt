# mock DBI driver used only by the Qore test suite
#
# This module is NOT installed: it exists so that the datasource mutation observer outcome
# boundaries (driver exception, restartable failover, commit ambiguity, bounded streams) can be
# tested deterministically with no external database.  run_tests.sh adds every directory under
# build/modules to QORE_MODULE_DIR, so the tests find it with no extra harness configuration.

set(QORE_API_VERSION ${MODULE_API_MAJOR}.${MODULE_API_MINOR})

include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_BINARY_DIR})
include_directories(${CMAKE_BINARY_DIR}/include)

set(dbi_test_module_name "dbitest")

add_library(${dbi_test_module_name} MODULE src/dbi_test-module.cpp)
add_dependencies(${dbi_test_module_name} libqore)

target_compile_definitions(${dbi_test_module_name} PRIVATE
    PACKAGE_VERSION="${VERSION}"
    BUILDING_DLL=1)

set_target_properties(${dbi_test_module_name} PROPERTIES
    PREFIX ""
    SUFFIX "-api-${QORE_API_VERSION}.qmod")

# modules resolve libqore symbols dynamically; do not link against the library
if (APPLE)
    set_target_properties(${dbi_test_module_name} PROPERTIES
        LINK_FLAGS "-Wl,-undefined -Wl,dynamic_lookup")
endif (APPLE)
