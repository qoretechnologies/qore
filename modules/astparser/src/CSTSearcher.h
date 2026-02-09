/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  CSTSearcher.h

  Qore AST Parser — tree-sitter-based symbol search

  Copyright (C) 2023 - 2026 Qore Technologies, s.r.o.

  Permission is hereby granted, free of charge, to any person obtaining a
  copy of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom the
  Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
  DEALINGS IN THE SOFTWARE.
*/

#ifndef _QLS_CSTSEARCHER_H
#define _QLS_CSTSEARCHER_H

#include <cstring>
#include <string>
#include <vector>

#include <tree_sitter/api.h>

#include "qore/Qore.h"

#include "AstParser.h"
#include "ast/ASTSymbolKind.h"
#include "ast/ASTSymbolUsageKind.h"

//! Symbol info for CST-based search results.
struct CSTSymbolInfo {
    ASTSymbolKind kind = ASYK_None;
    ASTSymbolUsageKind usage = ASUK_None;
    std::string name;
    std::string docComment;
    uint32_t startLine = 0;
    uint32_t startCol = 0;
    uint32_t endLine = 0;
    uint32_t endCol = 0;
};

//! Scope symbol info with scope level.
struct CSTScopeSymbolInfo {
    CSTSymbolInfo symbol;
    int scopeLevel = 0;

    CSTScopeSymbolInfo() = default;
    CSTScopeSymbolInfo(CSTSymbolInfo&& sym, int level)
        : symbol(std::move(sym)), scopeLevel(level) {}
    CSTScopeSymbolInfo(const CSTSymbolInfo& sym, int level)
        : symbol(sym), scopeLevel(level) {}
};

//! Static utility class for searching tree-sitter CST nodes.
class CSTSearcher {
public:
    CSTSearcher() = delete;
    CSTSearcher(const CSTSearcher&) = delete;

    //! Find the most specific named node at position (0-indexed).
    static TSNode findNodeAtPosition(const AstParseResult* result,
                                     uint32_t line, uint32_t col);

    //! Find node + all ancestors up to root (0-indexed position).
    static std::vector<TSNode> findNodeAndParents(const AstParseResult* result,
                                                  uint32_t line, uint32_t col);

    //! Map tree-sitter node type string to ASTSymbolKind.
    static ASTSymbolKind nodeTypeToSymbolKind(const char* type);

    //! Check if a node type is a declaration type.
    static bool isDeclarationNode(const char* type);

    //! Extract the "name" field from a declaration node.
    static std::string getNodeName(TSNode node, const AstParseResult* result);

    //! Find preceding doc comment (/** or #!) for a node.
    static std::string findDocComment(TSNode node, const AstParseResult* result);

    //! Build a QoreHashNode range from a TSNode (0-indexed).
    static QoreHashNode* makeRange(TSNode node, ExceptionSink* xsink);

    //! Build a QoreHashNode location from a TSNode + URI.
    static QoreHashNode* makeLocation(TSNode node, const std::string& uri,
                                      ExceptionSink* xsink);

    //! Collect all declaration symbols from the tree.
    /** Recursively walks the tree collecting declaration nodes.
        @param result parse result
        @param fixSymbols if true, prefix names with namespace/class scope
        @param bareNames if true, strip all namespace/class prefixes
        @return vector of symbol info
    */
    static std::vector<CSTSymbolInfo>* collectSymbols(
        const AstParseResult* result,
        bool fixSymbols = true,
        bool bareNames = false);

    //! Find symbols matching a query string.
    static std::vector<CSTSymbolInfo>* findMatchingSymbols(
        const AstParseResult* result,
        const std::string& query,
        bool exactMatch = false,
        bool fixSymbols = true,
        bool bareNames = false);

    //! Find symbol info at a position (0-indexed).
    static CSTSymbolInfo findSymbolInfo(
        const AstParseResult* result,
        uint32_t line, uint32_t col);

    //! Find all references to the identifier at position (0-indexed).
    static std::vector<TSNode>* findReferences(
        const AstParseResult* result,
        uint32_t line, uint32_t col,
        bool includeDecl);

    //! Find symbols accessible from a specific scope position (0-indexed).
    static std::vector<CSTScopeSymbolInfo>* findScopeSymbols(
        const AstParseResult* result,
        uint32_t line, uint32_t col);

    //! Build hover info description for a declaration at position.
    static std::string buildHoverInfo(
        const AstParseResult* result,
        ASTSymbolKind kind,
        uint32_t line, uint32_t col);

private:
    //! Recursively collect symbols into vec, tracking scope prefix.
    static void collectSymbolsRecursive(
        TSNode node,
        const AstParseResult* result,
        const std::string& scopePrefix,
        bool fixSymbols,
        bool bareNames,
        std::vector<CSTSymbolInfo>* vec);

    //! Collect all identifier nodes matching `name` in the tree.
    static void collectIdentifierRefs(
        TSNode node,
        const AstParseResult* result,
        const std::string& name,
        std::vector<TSNode>* vec);

    //! Collect scope symbols from ancestors and their siblings.
    static void collectScopeSymbolsFromAncestors(
        const std::vector<TSNode>& ancestors,
        const AstParseResult* result,
        uint32_t line, uint32_t col,
        std::vector<CSTScopeSymbolInfo>* vec);

    //! Build signature for class declaration.
    static std::string buildClassSignature(TSNode node, const AstParseResult* result);

    //! Build signature for function/method declaration.
    static std::string buildFunctionSignature(TSNode node, const AstParseResult* result);

    //! Build signature for constant declaration.
    static std::string buildConstantSignature(TSNode node, const AstParseResult* result);

    //! Build signature for variable declaration.
    static std::string buildVariableSignature(TSNode node, const AstParseResult* result);

    //! Build signature for hashdecl declaration.
    static std::string buildHashdeclSignature(TSNode node, const AstParseResult* result);

    //! Build signature for typedef declaration.
    static std::string buildTypedefSignature(TSNode node, const AstParseResult* result);

    //! Build signature for hash member declaration.
    static std::string buildHashMemberSignature(TSNode node, const AstParseResult* result);

    //! Get the field text from a child node by field name.
    static std::string getFieldText(TSNode node, const char* fieldName,
                                    const AstParseResult* result);

    //! Determine usage kind from node context.
    static ASTSymbolUsageKind determineUsageKind(TSNode node, TSNode parent);

    //! Check if a node is inside the "name" field of its parent declaration.
    static bool isNameOfDeclaration(TSNode node, TSNode parent);

    //! Collect declarations from a block/body node at given scope level.
    static void collectDeclarationsInScope(
        TSNode scopeNode,
        const AstParseResult* result,
        int scopeLevel,
        std::vector<CSTScopeSymbolInfo>* vec);

    //! Collect function/method parameters as scope symbols.
    static void collectParameters(
        TSNode funcNode,
        const AstParseResult* result,
        int scopeLevel,
        std::vector<CSTScopeSymbolInfo>* vec);

    //! Collect local variable declarations from statements before position.
    static void collectLocalsBeforePosition(
        TSNode blockNode,
        const AstParseResult* result,
        uint32_t line, uint32_t col,
        int scopeLevel,
        std::vector<CSTScopeSymbolInfo>* vec);

    //! Case-insensitive substring match.
    static bool matchesQuery(const std::string& name, const std::string& query,
                             bool exactMatch);
};

#endif // _QLS_CSTSEARCHER_H
