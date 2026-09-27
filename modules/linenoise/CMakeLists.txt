# start of hacks to simulate installed QoreConfig.cmake
# which cannot be available yet - the QORE_LIBRARY is missing
# as find_library does not work yet
#include(${CMAKE_BINARY_DIR}/cmake/QoreConfig.cmake)
if (NOT QORE_MODULES_DIR)
    set(QORE_MODULES_DIR ${MODULE_DIR})
endif (NOT QORE_MODULES_DIR)

if (NOT QORE_LIBRARY)
    set(QORE_LIBRARY libqore)
endif (NOT QORE_LIBRARY)

set(QORE_API_VERSION ${MODULE_API_MAJOR}.${MODULE_API_MINOR})

include(${CMAKE_SOURCE_DIR}/cmake/QoreMacros.cmake)
### end of hacks

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/linenoise)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(LINENOISE_CPP_SRC
    src/linenoise/linenoise.cpp
    src/linenoise/wcwidth.cpp
    src/linenoise-module.cpp
)

set(LINENOISE_QPP_SRC
    src/QC_AbstractHistoryProvider.qpp
    src/ql_linenoise.qpp
)

add_definitions(-DUSE_UTF8)

qore_wrap_qpp_value(LINENOISE_QPP_SOURCES DOXLIST _dox_src ${LINENOISE_QPP_SRC})
add_custom_target(LINENOISE_QPP_GENERATED_FILES DEPENDS qpp ${LINENOISE_QPP_SOURCES})

set(linenoise_module_name "linenoise")

add_library(${linenoise_module_name} MODULE ${LINENOISE_CPP_SRC} ${LINENOISE_QPP_SOURCES})
target_include_directories(${linenoise_module_name} PRIVATE ${LLVM_INCLUDE_DIRS})
if(LLVM_LIBS)
    target_link_libraries(${linenoise_module_name} ${LLVM_LIBS})
elseif(TARGET LLVM::LLVM)
    target_link_libraries(${linenoise_module_name} LLVM::LLVM)
endif()
add_dependencies(${linenoise_module_name} libqore LINENOISE_QPP_GENERATED_FILES)

# Exercise the editor's conversion helpers without requiring an interactive TTY.
add_executable(linenoise-unicode-test EXCLUDE_FROM_ALL
    test/unicode-conversion.cpp src/linenoise/wcwidth.cpp)
target_include_directories(linenoise-unicode-test PRIVATE ${LLVM_INCLUDE_DIRS})
if(LLVM_LIBS)
    target_link_libraries(linenoise-unicode-test ${LLVM_LIBS})
elseif(TARGET LLVM::LLVM)
    target_link_libraries(linenoise-unicode-test LLVM::LLVM)
endif()

set(MODULE_DOX_INPUT ${CMAKE_BINARY_DIR}/modules/linenoise/mainpage.dox ${_dox_src})
qore_binary_module_qore(${linenoise_module_name} "${VERSION}" "/${VERSION}")

set(QORE_DOX_LINENOISE_TMPL_SRC
    docs/mainpage.dox.tmpl
)

if (DOXYGEN_FOUND)
    qore_wrap_dox(QORE_DOX_LINENOISE_SRC ${QORE_DOX_LINENOISE_TMPL_SRC})
    add_custom_target(QORE_LINENOISE_MOD_DOX_FILES DEPENDS ${QORE_DOX_LINENOISE_SRC})
    add_dependencies(docs-linenoise QORE_LINENOISE_MOD_DOX_FILES)
endif()
