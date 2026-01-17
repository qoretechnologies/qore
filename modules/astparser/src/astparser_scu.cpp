// astparser module single compilation unit

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfree-nonheap-object"
#endif
#include "ast_parser.cpp"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "ast_scanner.cpp"

#include "AstParser.cpp"
#include "AstParserHolder.cpp"
#include "astparser-module.cpp"
#include "AstPrinter.cpp"
#include "AstTreeHolder.cpp"
#include "AstTreePrinter.cpp"
#include "AstTreeSearcher.cpp"

#include "QC_AstParser.cpp"
#include "QC_AstTree.cpp"
#include "QC_AstTreeSearcher.cpp"
#include "ql_ast.cpp"

#include "queries/FindMatchingSymbolsQuery.cpp"
#include "queries/FindNodeAndParentsQuery.cpp"
#include "queries/FindNodeQuery.cpp"
#include "queries/FindReferencesQuery.cpp"
#include "queries/FindScopeSymbolsQuery.cpp"
#include "queries/FindSymbolInfoQuery.cpp"
#include "queries/FindSymbolsQuery.cpp"
#include "queries/GetNodesInfoQuery.cpp"
#include "queries/SymbolInfoFixes.cpp"
