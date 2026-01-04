find_package(BISON REQUIRED)
find_package(FLEX 2.5.35 REQUIRED)

# start of hacks to simulate installed QoreConfig.cmake
# which cannot be available yet - the QORE_LIBRARY is missing
# as find_library does not work yet
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
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_SOURCE_DIR/build/include})
include_directories(${CMAKE_SOURCE_DIR/include})

# source files
set(ASTPARSER_QPP_SRC
    src/QC_AstParser.qpp
    src/QC_AstTree.qpp
    src/QC_AstTreeSearcher.qpp
    src/ql_ast.qpp
)

set(ASTPARSER_CPP_SRC
    src/astparser-module.cpp
    src/AstParser.cpp
    src/AstParserHolder.cpp
    src/AstPrinter.cpp
    src/AstTreeHolder.cpp
    src/AstTreePrinter.cpp
    src/AstTreeSearcher.cpp
    src/queries/FindMatchingSymbolsQuery.cpp
    src/queries/FindNodeQuery.cpp
    src/queries/FindNodeAndParentsQuery.cpp
    src/queries/FindReferencesQuery.cpp
    src/queries/FindScopeSymbolsQuery.cpp
    src/queries/FindSymbolInfoQuery.cpp
    src/queries/FindSymbolsQuery.cpp
    src/queries/GetNodesInfoQuery.cpp
    src/queries/SymbolInfoFixes.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/ast_parser.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/ast_scanner.cpp
)

set(QORE_DOX_ASTPARSER_TMPL_SRC
    docs/mainpage.dox.tmpl
)

# prepare ast_scanner and ast_parser targets
bison_target(ast_parser src/ast_parser.ypp ${CMAKE_CURRENT_BINARY_DIR}/ast_parser.cpp)
flex_target(ast_scanner src/ast_scanner.lpp ${CMAKE_CURRENT_BINARY_DIR}/ast_scanner.cpp COMPILE_FLAGS --nowarn)
add_flex_bison_dependency(ast_scanner ast_parser)

qore_wrap_qpp_value(ASTPARSER_QPP_SOURCES DOXLIST _dox_src ${ASTPARSER_QPP_SRC})
add_custom_target(ASTPARSER_QPP_GENERATED_FILES DEPENDS qpp ${ASTPARSER_QPP_SOURCES})
add_custom_target(ASTPARSER_BISON_GENERATED_FILES DEPENDS ${BISON_ast_parser_OUTPUTS})
add_custom_target(ASTPARSER_FLEX_GENERATED_FILES DEPENDS ${FLEX_ast_scanner_OUTPUTS})

# prepare astparser module target
set(astparser_module_name "astparser")

if (SINGLE_COMPILATION_UNIT)
    add_library(${astparser_module_name} MODULE src/astparser_scu.cpp)
else (SINGLE_COMPILATION_UNIT)
    add_library(${astparser_module_name} MODULE ${ASTPARSER_CPP_SRC} ${ASTPARSER_QPP_SOURCES})
endif (SINGLE_COMPILATION_UNIT)

add_dependencies(${astparser_module_name} libqore ASTPARSER_QPP_GENERATED_FILES ASTPARSER_BISON_GENERATED_FILES ASTPARSER_FLEX_GENERATED_FILES)
set(MODULE_DOX_INPUT ${CMAKE_BINARY_DIR}/modules/astparser/mainpage.dox ${_dox_src})
qore_binary_module_qore(${astparser_module_name} "${VERSION}" "/${VERSION}")

if (DOXYGEN_FOUND)
    qore_wrap_dox(QORE_DOX_ASTPARSER_SRC ${QORE_DOX_ASTPARSER_TMPL_SRC})
    add_custom_target(QORE_ASTPARSER_MOD_DOX_FILES DEPENDS ${QORE_DOX_ASTPARSER_SRC})
    add_dependencies(docs-astparser QORE_ASTPARSER_MOD_DOX_FILES)
endif()
