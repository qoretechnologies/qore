# Allow BISON_EXECUTABLE to be set from environment variable
if(NOT BISON_EXECUTABLE AND DEFINED ENV{BISON_EXECUTABLE})
    set(BISON_EXECUTABLE $ENV{BISON_EXECUTABLE} CACHE FILEPATH "Path to bison executable")
endif()
get_filename_component(BISON_EXECUTABLE_NAME "${BISON_EXECUTABLE}" NAME)
get_filename_component(BISON_REAL_EXECUTABLE_NAME "${BISON_REAL_EXECUTABLE}" NAME)
if(BISON_REAL_EXECUTABLE_NAME STREQUAL "bison-wrapper.sh")
    unset(BISON_REAL_EXECUTABLE CACHE)
    unset(BISON_REAL_EXECUTABLE)
endif()
if(BISON_EXECUTABLE_NAME STREQUAL "bison-wrapper.sh")
    if(BISON_REAL_EXECUTABLE)
        get_filename_component(BISON_REAL_EXECUTABLE_NAME "${BISON_REAL_EXECUTABLE}" NAME)
        if(NOT BISON_REAL_EXECUTABLE_NAME STREQUAL "bison-wrapper.sh")
            set(BISON_EXECUTABLE ${BISON_REAL_EXECUTABLE} CACHE FILEPATH "Path to bison executable" FORCE)
        else()
            unset(BISON_EXECUTABLE CACHE)
            unset(BISON_EXECUTABLE)
        endif()
    else()
        unset(BISON_EXECUTABLE CACHE)
        unset(BISON_EXECUTABLE)
    endif()
endif()
find_package(BISON REQUIRED)

# Wrap bison to handle directive compatibility across versions, avoiding wrapper recursion.
get_filename_component(BISON_EXECUTABLE_NAME "${BISON_EXECUTABLE}" NAME)
if(BISON_EXECUTABLE_NAME STREQUAL "bison-wrapper.sh")
    find_program(BISON_REAL_EXECUTABLE NAMES bison)
else()
    set(BISON_REAL_EXECUTABLE "${BISON_EXECUTABLE}")
endif()
get_filename_component(BISON_REAL_EXECUTABLE_NAME "${BISON_REAL_EXECUTABLE}" NAME)
if(BISON_REAL_EXECUTABLE_NAME STREQUAL "bison-wrapper.sh")
    find_program(BISON_REAL_EXECUTABLE NAMES bison)
endif()
set(BISON_REAL_EXECUTABLE "${BISON_REAL_EXECUTABLE}" CACHE FILEPATH "Path to real bison executable" FORCE)
set(BISON_WRAPPER_EXECUTABLE ${CMAKE_SOURCE_DIR}/tools/bison-wrapper.sh)
set(BISON_WRAPPER_PATH ${CMAKE_CURRENT_BINARY_DIR}/bison-wrapper.sh)
configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/bison-wrapper.sh.in
    ${CMAKE_CURRENT_BINARY_DIR}/bison-wrapper.sh
)
execute_process(COMMAND chmod +x ${CMAKE_CURRENT_BINARY_DIR}/bison-wrapper.sh)
set(BISON_EXECUTABLE ${CMAKE_CURRENT_BINARY_DIR}/bison-wrapper.sh CACHE FILEPATH "Path to bison executable" FORCE)
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
# The astparser grammar intentionally has conflicts due to parse_option handling.
# See the comment in ast_parser.ypp for details. Warnings are suppressed with bison 3.0+.
if(BISON_VERSION VERSION_GREATER_EQUAL "3.0")
    set(ASTPARSER_BISON_FLAGS "-Wno-conflicts-sr -Wno-conflicts-rr -Wno-other")
else()
    set(ASTPARSER_BISON_FLAGS "")
endif()
bison_target(ast_parser src/ast_parser.ypp ${CMAKE_CURRENT_BINARY_DIR}/ast_parser.cpp
    COMPILE_FLAGS "${ASTPARSER_BISON_FLAGS}")
flex_target(ast_scanner src/ast_scanner.lpp ${CMAKE_CURRENT_BINARY_DIR}/ast_scanner.cpp COMPILE_FLAGS --nowarn)
add_flex_bison_dependency(ast_scanner ast_parser)

qore_wrap_qpp_value(ASTPARSER_QPP_SOURCES DOXLIST _dox_src ${ASTPARSER_QPP_SRC})
add_custom_target(ASTPARSER_QPP_GENERATED_FILES DEPENDS qpp ${ASTPARSER_QPP_SOURCES})
add_custom_target(ASTPARSER_BISON_GENERATED_FILES DEPENDS ${BISON_ast_parser_OUTPUTS})
add_custom_target(ASTPARSER_FLEX_GENERATED_FILES DEPENDS ${FLEX_ast_scanner_OUTPUTS})

# prepare astparser module target
set(astparser_module_name "astparser")

if (SINGLE_COMPILATION_UNIT)
    # Generate the SCU dynamically from source lists with dependency tracking.
    # Bison/flex generated files must be included first to avoid macro conflicts.
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/src)
    set(AST_SCU_CONTENT "/* Auto-generated single compilation unit - do not edit */\n")
    set(AST_SCU_DEPS "")
    # First pass: emit bison/flex generated files (must come before other sources)
    foreach(src ${ASTPARSER_CPP_SRC})
        if (IS_ABSOLUTE "${src}")
            set(abs_src "${src}")
        else()
            set(abs_src "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
        endif()
        get_filename_component(basename "${src}" NAME)
        if (basename STREQUAL "ast_parser.cpp")
            list(APPEND AST_SCU_DEPS "${abs_src}")
            string(APPEND AST_SCU_CONTENT
                "#if defined(__GNUC__)\n"
                "#pragma GCC diagnostic push\n"
                "#pragma GCC diagnostic ignored \"-Wfree-nonheap-object\"\n"
                "#endif\n"
                "#include \"${abs_src}\"\n"
                "#if defined(__GNUC__)\n"
                "#pragma GCC diagnostic pop\n"
                "#endif\n")
        elseif (basename STREQUAL "ast_scanner.cpp")
            list(APPEND AST_SCU_DEPS "${abs_src}")
            string(APPEND AST_SCU_CONTENT "#include \"${abs_src}\"\n")
        endif()
    endforeach()
    # Second pass: emit remaining source files
    foreach(src ${ASTPARSER_CPP_SRC})
        if (IS_ABSOLUTE "${src}")
            set(abs_src "${src}")
        else()
            set(abs_src "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
        endif()
        get_filename_component(basename "${src}" NAME)
        if (NOT basename STREQUAL "ast_parser.cpp" AND NOT basename STREQUAL "ast_scanner.cpp")
            list(APPEND AST_SCU_DEPS "${abs_src}")
            string(APPEND AST_SCU_CONTENT "#include \"${abs_src}\"\n")
        endif()
    endforeach()
    foreach(src ${ASTPARSER_QPP_SOURCES})
        string(APPEND AST_SCU_CONTENT "#include \"${src}\"\n")
        list(APPEND AST_SCU_DEPS "${src}")
    endforeach()
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/src/astparser_scu.cpp "${AST_SCU_CONTENT}")
    add_library(${astparser_module_name} MODULE
        ${CMAKE_CURRENT_BINARY_DIR}/src/astparser_scu.cpp)
    set_source_files_properties(
        ${CMAKE_CURRENT_BINARY_DIR}/src/astparser_scu.cpp
        PROPERTIES OBJECT_DEPENDS "${AST_SCU_DEPS}")
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
