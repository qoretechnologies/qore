# Test module for process teardown (see static-program-exit.qtest).
#
# "staticpgm" does nothing but hold a QoreProgram in an object with static storage duration, the
# shape the jni module uses for its global Java context Program.  It is not installed: it is built
# into ${CMAKE_BINARY_DIR}/modules/staticpgm so that run_tests.sh puts it on QORE_MODULE_DIR along
# with the real binary modules.

include(${CMAKE_SOURCE_DIR}/cmake/QoreMacros.cmake)

add_library(staticpgm MODULE staticpgm-module.cpp)
add_dependencies(staticpgm libqore)
target_include_directories(staticpgm PRIVATE
    ${CMAKE_BINARY_DIR}
    ${CMAKE_BINARY_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include)
target_compile_definitions(staticpgm PRIVATE
    BUILDING_DLL=1
    HAVE_UNIX_CONFIG_H
    PACKAGE_VERSION="${VERSION}")
set_target_properties(staticpgm PROPERTIES
    PREFIX ""
    SUFFIX "-api-${MODULE_API_MAJOR}.${MODULE_API_MINOR}.qmod"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/modules/staticpgm")
target_link_libraries(staticpgm PRIVATE libqore)
