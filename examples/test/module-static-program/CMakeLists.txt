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
# DLLLOCAL hides symbols as in every other module (see QORE_BINARY_MODULE_INTERN2 in QoreMacros.cmake): libqore's
# usage requirements give this target _QORE_LIB_INTERN, so <qore/Qore.h> includes the library's configuration
# header, which defines HAVE_GCC_VISIBILITY where the compiler supports it; defining it here as well redefines it
set_target_properties(staticpgm PROPERTIES
    PREFIX ""
    SUFFIX "-api-${MODULE_API_MAJOR}.${MODULE_API_MINOR}.qmod"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/modules/staticpgm")
target_link_libraries(staticpgm PRIVATE libqore)
