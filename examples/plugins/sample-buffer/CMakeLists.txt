# Sample plugin-type module for plugin authors. This target is intentionally
# excluded from the default build and is not installed.

set(sample_buffer_plugin_name sample-buffer-plugin)

add_library(${sample_buffer_plugin_name} MODULE sample-buffer.cpp)
add_dependencies(${sample_buffer_plugin_name} libqore)
target_include_directories(${sample_buffer_plugin_name} PRIVATE
    ${CMAKE_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/include)
target_compile_definitions(${sample_buffer_plugin_name} PRIVATE
    BUILDING_DLL=1
    HAVE_UNIX_CONFIG_H
    PACKAGE_VERSION="${VERSION}")
set_target_properties(${sample_buffer_plugin_name} PROPERTIES
    OUTPUT_NAME "sample-buffer"
    PREFIX ""
    SUFFIX "-api-${MODULE_API_MAJOR}.${MODULE_API_MINOR}.qmod")

if(CMAKE_HOST_APPLE)
    target_link_options(${sample_buffer_plugin_name} PRIVATE -Wl,-undefined -Wl,dynamic_lookup)
endif()
