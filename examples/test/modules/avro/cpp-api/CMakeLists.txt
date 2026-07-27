# Test module for the avro C++ API (see include/qore/QoreAvroApi.h).
#
# "avrocppapi" consumes the avro module's C++ API through q_get_module_cpp_api() exactly as an
# out-of-tree consumer does: it includes only the installed <qore/QoreAvroApi.h> and links against
# neither libqore nor the avro module.  It is built into ${CMAKE_BINARY_DIR}/modules/avrocppapi so
# that run_tests.sh puts it on QORE_MODULE_DIR, and it is never installed.

include(${CMAKE_SOURCE_DIR}/cmake/QoreMacros.cmake)

qore_wrap_qpp_value(AVROCPPAPI_QPP_SOURCES ql_avrocppapi.qpp)
add_custom_target(AVROCPPAPI_QPP_GENERATED_FILES DEPENDS qpp ${AVROCPPAPI_QPP_SOURCES})

add_library(avrocppapi MODULE avrocppapi-module.cpp ${AVROCPPAPI_QPP_SOURCES})
add_dependencies(avrocppapi libqore avro AVROCPPAPI_QPP_GENERATED_FILES)
target_include_directories(avrocppapi PRIVATE
    ${CMAKE_BINARY_DIR}
    ${CMAKE_BINARY_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include)
target_compile_definitions(avrocppapi PRIVATE
    BUILDING_DLL=1
    HAVE_UNIX_CONFIG_H
    PACKAGE_VERSION="${VERSION}")
set_target_properties(avrocppapi PROPERTIES
    PREFIX ""
    SUFFIX "-api-${MODULE_API_MAJOR}.${MODULE_API_MINOR}.qmod"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/modules/avrocppapi")

if (CMAKE_HOST_APPLE)
    target_link_options(avrocppapi PRIVATE -Wl,-undefined -Wl,dynamic_lookup)
endif()
