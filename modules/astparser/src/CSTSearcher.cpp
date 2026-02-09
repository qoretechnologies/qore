/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  CSTSearcher.cpp

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

#include "CSTSearcher.h"

#include <algorithm>
#include <cctype>
#include <memory>
#include <sstream>

// --------------------------------------------------------------------------
// Node lookup helpers
// --------------------------------------------------------------------------

static TSNode make_null_node() {
    TSNode n = {};
    return n;
}

TSNode CSTSearcher::findNodeAtPosition(const AstParseResult* result,
                                       uint32_t line, uint32_t col) {
    if (!result) {
        return make_null_node();
    }
    TSNode root = result->getRootNode();
    TSPoint point = {line, col};
    TSNode node = ts_node_descendant_for_point_range(root, point, point);
    // Walk up to find a named node if we landed on an anonymous one
    while (!ts_node_is_null(node) && !ts_node_is_named(node)) {
        node = ts_node_parent(node);
    }
    return node;
}

std::vector<TSNode> CSTSearcher::findNodeAndParents(const AstParseResult* result,
                                                    uint32_t line, uint32_t col) {
    std::vector<TSNode> ancestors;
    TSNode node = findNodeAtPosition(result, line, col);
    while (!ts_node_is_null(node)) {
        ancestors.push_back(node);
        node = ts_node_parent(node);
    }
    return ancestors;
}

// --------------------------------------------------------------------------
// Type classification
// --------------------------------------------------------------------------

ASTSymbolKind CSTSearcher::nodeTypeToSymbolKind(const char* type) {
    if (strcmp(type, "class_declaration") == 0) {
        return ASYK_Class;
    }
    if (strcmp(type, "method_declaration") == 0) {
        return ASYK_Method;
    }
    if (strcmp(type, "constructor_declaration") == 0) {
        return ASYK_Constructor;
    }
    if (strcmp(type, "destructor_declaration") == 0) {
        return ASYK_Function;
    }
    if (strcmp(type, "function_declaration") == 0) {
        return ASYK_Function;
    }
    if (strcmp(type, "namespace_declaration") == 0) {
        return ASYK_Namespace;
    }
    if (strcmp(type, "constant_declaration") == 0) {
        return ASYK_Constant;
    }
    if (strcmp(type, "hashdecl_declaration") == 0) {
        return ASYK_Interface;
    }
    if (strcmp(type, "hash_member_declaration") == 0 ||
        strcmp(type, "hashdecl_member") == 0) {
        return ASYK_Field;
    }
    if (strcmp(type, "variable_declaration") == 0) {
        return ASYK_Variable;
    }
    if (strcmp(type, "global_variable_declaration") == 0) {
        return ASYK_Variable;
    }
    if (strcmp(type, "typedef_declaration") == 0) {
        return ASYK_TypeAlias;
    }
    if (strcmp(type, "enum_declaration") == 0) {
        return ASYK_Constant;
    }
    if (strcmp(type, "module_declaration") == 0) {
        return ASYK_Module;
    }
    return ASYK_None;
}

bool CSTSearcher::isDeclarationNode(const char* type) {
    return nodeTypeToSymbolKind(type) != ASYK_None;
}

// --------------------------------------------------------------------------
// Name extraction
// --------------------------------------------------------------------------

std::string CSTSearcher::getNodeName(TSNode node, const AstParseResult* result) {
    const char* type = ts_node_type(node);

    // Constructor and destructor don't have a "name" field — use the keyword
    if (strcmp(type, "constructor_declaration") == 0) {
        return "constructor";
    }
    if (strcmp(type, "destructor_declaration") == 0) {
        return "destructor";
    }

    // Try "name" field
    TSNode nameNode = ts_node_child_by_field_name(node, "name", 4);
    if (!ts_node_is_null(nameNode)) {
        return result->getNodeText(nameNode);
    }
    return std::string();
}

std::string CSTSearcher::getFieldText(TSNode node, const char* fieldName,
                                      const AstParseResult* result) {
    TSNode child = ts_node_child_by_field_name(node, fieldName, strlen(fieldName));
    if (!ts_node_is_null(child)) {
        return result->getNodeText(child);
    }
    return std::string();
}

// --------------------------------------------------------------------------
// Doc comment extraction
// --------------------------------------------------------------------------

//! Check if a comment node is a doc comment (/** or #!).
static bool isDocComment(const std::string& text) {
    if (text.size() >= 3 && text[0] == '/' && text[1] == '*' && text[2] == '*') {
        return true;
    }
    if (text.size() >= 2 && text[0] == '#' && text[1] == '!') {
        return true;
    }
    return false;
}

std::string CSTSearcher::findDocComment(TSNode node, const AstParseResult* result) {
    if (ts_node_is_null(node) || !result) {
        return std::string();
    }

    TSNode parent = ts_node_parent(node);
    if (ts_node_is_null(parent)) {
        return std::string();
    }

    // Find the index of `node` among its parent's children
    uint32_t childCount = ts_node_child_count(parent);
    uint32_t nodeIndex = UINT32_MAX;
    for (uint32_t i = 0; i < childCount; i++) {
        TSNode child = ts_node_child(parent, i);
        if (ts_node_eq(child, node)) {
            nodeIndex = i;
            break;
        }
    }

    if (nodeIndex == UINT32_MAX || nodeIndex == 0) {
        return std::string();
    }

    uint32_t nodeStartLine = ts_node_start_point(node).row;

    // Walk backwards from the node to find doc comments
    // Collect consecutive doc comment lines/blocks immediately preceding the declaration
    std::vector<std::string> docParts;
    for (int i = static_cast<int>(nodeIndex) - 1; i >= 0; i--) {
        TSNode prev = ts_node_child(parent, static_cast<uint32_t>(i));
        const char* prevType = ts_node_type(prev);

        if (strcmp(prevType, "comment") == 0 || strcmp(prevType, "line_comment") == 0) {
            std::string text = result->getNodeText(prev);
            if (!isDocComment(text)) {
                break; // Non-doc comment stops the search
            }

            uint32_t commentEndLine = ts_node_end_point(prev).row;

            // Check that there's no blank line between this comment and the next element
            // (either the next doc comment or the declaration itself)
            uint32_t nextStartLine;
            if (docParts.empty()) {
                nextStartLine = nodeStartLine;
            } else {
                // The previously collected comment (which is actually after this one in source)
                TSNode nextSibling = ts_node_child(parent, static_cast<uint32_t>(i + 1));
                nextStartLine = ts_node_start_point(nextSibling).row;
            }

            if (nextStartLine > commentEndLine + 1) {
                // Blank line between comment and next — only keep comments after the gap
                break;
            }

            docParts.push_back(text);
        } else {
            break; // Non-comment sibling stops the search
        }
    }

    if (docParts.empty()) {
        return std::string();
    }

    // Reverse to get chronological order (we collected bottom-up)
    std::reverse(docParts.begin(), docParts.end());

    // Join with newlines
    std::string docComment;
    for (size_t i = 0; i < docParts.size(); i++) {
        if (i > 0) {
            docComment += "\n";
        }
        docComment += docParts[i];
    }
    return docComment;
}

// --------------------------------------------------------------------------
// Range/location builders
// --------------------------------------------------------------------------

QoreHashNode* CSTSearcher::makeRange(TSNode node, ExceptionSink* xsink) {
    TSPoint start = ts_node_start_point(node);
    TSPoint end = ts_node_end_point(node);

    ReferenceHolder<QoreHashNode> startHash(new QoreHashNode, xsink);
    ReferenceHolder<QoreHashNode> endHash(new QoreHashNode, xsink);
    ReferenceHolder<QoreHashNode> range(new QoreHashNode, xsink);
    if (*xsink) {
        return nullptr;
    }

    startHash->setKeyValue("line", static_cast<int64>(start.row), xsink);
    startHash->setKeyValue("character", static_cast<int64>(start.column), xsink);
    if (*xsink) {
        return nullptr;
    }
    endHash->setKeyValue("line", static_cast<int64>(end.row), xsink);
    endHash->setKeyValue("character", static_cast<int64>(end.column), xsink);
    if (*xsink) {
        return nullptr;
    }
    range->setKeyValue("start", startHash.release(), xsink);
    range->setKeyValue("end", endHash.release(), xsink);
    if (*xsink) {
        return nullptr;
    }
    return range.release();
}

QoreHashNode* CSTSearcher::makeLocation(TSNode node, const std::string& uri,
                                        ExceptionSink* xsink) {
    ReferenceHolder<QoreHashNode> location(new QoreHashNode, xsink);
    if (*xsink) {
        return nullptr;
    }

    QoreHashNode* range = makeRange(node, xsink);
    if (!range || *xsink) {
        return nullptr;
    }

    location->setKeyValue("uri", new QoreStringNode(uri), xsink);
    location->setKeyValue("range", range, xsink);
    if (*xsink) {
        return nullptr;
    }
    return location.release();
}

// --------------------------------------------------------------------------
// Symbol collection
// --------------------------------------------------------------------------

void CSTSearcher::collectSymbolsRecursive(
    TSNode node,
    const AstParseResult* result,
    const std::string& scopePrefix,
    bool fixSymbols,
    bool bareNames,
    std::vector<CSTSymbolInfo>* vec) {

    const char* type = ts_node_type(node);
    ASTSymbolKind kind = nodeTypeToSymbolKind(type);

    if (kind != ASYK_None) {
        std::string name = getNodeName(node, result);
        if (!name.empty()) {
            CSTSymbolInfo si;
            si.kind = kind;

            // For methods, use ASYK_Method (not ASYK_Function)
            // nodeTypeToSymbolKind already returns ASYK_Method for method_declaration

            if (fixSymbols && !scopePrefix.empty() && !bareNames) {
                si.name = scopePrefix + "::" + name;
            } else {
                si.name = name;
            }

            si.docComment = findDocComment(node, result);

            TSPoint start = ts_node_start_point(node);
            TSPoint end = ts_node_end_point(node);
            si.startLine = start.row;
            si.startCol = start.column;
            si.endLine = end.row;
            si.endCol = end.column;

            // Build new scope prefix for nested declarations (before moving si)
            std::string newPrefix;
            if (strcmp(type, "class_declaration") == 0 ||
                strcmp(type, "namespace_declaration") == 0) {
                if (fixSymbols && !bareNames) {
                    newPrefix = si.name;
                } else {
                    newPrefix = scopePrefix.empty() ? name : scopePrefix + "::" + name;
                }
            } else {
                newPrefix = scopePrefix;
            }

            vec->push_back(std::move(si));

            // Recurse into children for nested declarations
            uint32_t childCount = ts_node_named_child_count(node);
            for (uint32_t i = 0; i < childCount; i++) {
                collectSymbolsRecursive(ts_node_named_child(node, i), result,
                                        newPrefix, fixSymbols, bareNames, vec);
            }
            return; // Don't double-recurse
        }
    }

    // Recurse into children
    uint32_t childCount = ts_node_named_child_count(node);
    for (uint32_t i = 0; i < childCount; i++) {
        collectSymbolsRecursive(ts_node_named_child(node, i), result,
                                scopePrefix, fixSymbols, bareNames, vec);
    }
}

std::vector<CSTSymbolInfo>* CSTSearcher::collectSymbols(
    const AstParseResult* result,
    bool fixSymbols,
    bool bareNames) {
    if (!result) {
        return nullptr;
    }

    std::unique_ptr<std::vector<CSTSymbolInfo>> vec(new std::vector<CSTSymbolInfo>);
    TSNode root = result->getRootNode();
    collectSymbolsRecursive(root, result, std::string(), fixSymbols, bareNames, vec.get());
    return vec.release();
}

// --------------------------------------------------------------------------
// Query matching
// --------------------------------------------------------------------------

bool CSTSearcher::matchesQuery(const std::string& name, const std::string& query,
                               bool exactMatch) {
    if (exactMatch) {
        return name == query;
    }
    // Case-insensitive substring match
    std::string nameLower = name;
    std::string queryLower = query;
    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    std::transform(queryLower.begin(), queryLower.end(), queryLower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return nameLower.find(queryLower) != std::string::npos;
}

std::vector<CSTSymbolInfo>* CSTSearcher::findMatchingSymbols(
    const AstParseResult* result,
    const std::string& query,
    bool exactMatch,
    bool fixSymbols,
    bool bareNames) {

    std::unique_ptr<std::vector<CSTSymbolInfo>> allSyms(
        collectSymbols(result, fixSymbols, bareNames));
    if (!allSyms) {
        return nullptr;
    }

    std::unique_ptr<std::vector<CSTSymbolInfo>> filtered(new std::vector<CSTSymbolInfo>);
    for (auto& si : *allSyms) {
        if (matchesQuery(si.name, query, exactMatch)) {
            filtered->push_back(std::move(si));
        }
    }
    return filtered.release();
}

// --------------------------------------------------------------------------
// Symbol info at position
// --------------------------------------------------------------------------

ASTSymbolUsageKind CSTSearcher::determineUsageKind(TSNode node, TSNode parent) {
    if (ts_node_is_null(parent)) {
        return ASUK_None;
    }

    const char* parentType = ts_node_type(parent);

    // Check if node is the "name" field of a declaration
    if (isNameOfDeclaration(node, parent)) {
        if (strcmp(parentType, "class_declaration") == 0) {
            return ASUK_ClassDeclName;
        }
        if (strcmp(parentType, "constant_declaration") == 0) {
            return ASUK_ConstantDeclName;
        }
        if (strcmp(parentType, "function_declaration") == 0 ||
            strcmp(parentType, "method_declaration") == 0 ||
            strcmp(parentType, "constructor_declaration") == 0 ||
            strcmp(parentType, "destructor_declaration") == 0) {
            return ASUK_FuncDeclName;
        }
        if (strcmp(parentType, "namespace_declaration") == 0) {
            return ASUK_NamespaceDeclName;
        }
        if (strcmp(parentType, "variable_declaration") == 0 ||
            strcmp(parentType, "global_variable_declaration") == 0) {
            return ASUK_VarDeclName;
        }
        if (strcmp(parentType, "hashdecl_declaration") == 0) {
            return ASUK_HashDeclName;
        }
        if (strcmp(parentType, "hash_member_declaration") == 0) {
            return ASUK_HashMemberName;
        }
        if (strcmp(parentType, "typedef_declaration") == 0) {
            return ASUK_TypedefDeclName;
        }
        if (strcmp(parentType, "module_declaration") == 0) {
            return ASUK_ModuleDeclName;
        }
    }

    // Check for type annotations
    TSNode typeChild = ts_node_child_by_field_name(parent, "type", 4);
    if (!ts_node_is_null(typeChild) && ts_node_eq(typeChild, node)) {
        if (strcmp(parentType, "variable_declaration") == 0 ||
            strcmp(parentType, "global_variable_declaration") == 0) {
            return ASUK_VarDeclTypeName;
        }
        if (strcmp(parentType, "function_declaration") == 0 ||
            strcmp(parentType, "method_declaration") == 0) {
            return ASUK_FuncReturnType;
        }
    }

    // Check for call targets
    if (strcmp(parentType, "call_expression") == 0) {
        TSNode funcChild = ts_node_child_by_field_name(parent, "function", 8);
        if (!ts_node_is_null(funcChild) && ts_node_eq(funcChild, node)) {
            return ASUK_CallTarget;
        }
        TSNode argsChild = ts_node_child_by_field_name(parent, "arguments", 9);
        if (!ts_node_is_null(argsChild) && ts_node_eq(argsChild, node)) {
            return ASUK_CallArgs;
        }
    }

    // Check for superclass references
    if (strcmp(parentType, "superclass") == 0) {
        return ASUK_SuperclassDeclName;
    }

    // Check for assignment
    if (strcmp(parentType, "assignment_expression") == 0) {
        TSNode left = ts_node_child_by_field_name(parent, "left", 4);
        if (!ts_node_is_null(left) && ts_node_eq(left, node)) {
            return ASUK_AssignmentLeft;
        }
        return ASUK_AssignmentRight;
    }

    // Check for binary expression
    if (strcmp(parentType, "binary_expression") == 0) {
        TSNode left = ts_node_child_by_field_name(parent, "left", 4);
        if (!ts_node_is_null(left) && ts_node_eq(left, node)) {
            return ASUK_BinaryLeft;
        }
        return ASUK_BinaryRight;
    }

    // Check for return statement
    if (strcmp(parentType, "return_statement") == 0) {
        return ASUK_ReturnStmtVal;
    }

    return ASUK_None;
}

bool CSTSearcher::isNameOfDeclaration(TSNode node, TSNode parent) {
    TSNode nameChild = ts_node_child_by_field_name(parent, "name", 4);
    if (ts_node_is_null(nameChild)) {
        return false;
    }
    // Direct match or node is a descendant of the name field
    if (ts_node_eq(nameChild, node)) {
        return true;
    }
    // For scoped identifiers, the identifier may be a child of the name field
    uint32_t start = ts_node_start_byte(node);
    uint32_t end = ts_node_end_byte(node);
    uint32_t nameStart = ts_node_start_byte(nameChild);
    uint32_t nameEnd = ts_node_end_byte(nameChild);
    return start >= nameStart && end <= nameEnd;
}

CSTSymbolInfo CSTSearcher::findSymbolInfo(
    const AstParseResult* result,
    uint32_t line, uint32_t col) {

    CSTSymbolInfo si;
    if (!result) {
        return si;
    }

    std::vector<TSNode> ancestors = findNodeAndParents(result, line, col);
    if (ancestors.empty()) {
        return si;
    }

    TSNode node = ancestors[0];
    std::string text = result->getNodeText(node);

    // Walk up to find the nearest declaration context
    for (size_t i = 0; i < ancestors.size(); i++) {
        const char* type = ts_node_type(ancestors[i]);
        ASTSymbolKind kind = nodeTypeToSymbolKind(type);
        if (kind != ASYK_None) {
            si.kind = kind;
            si.name = text;

            TSPoint start = ts_node_start_point(node);
            TSPoint end = ts_node_end_point(node);
            si.startLine = start.row;
            si.startCol = start.column;
            si.endLine = end.row;
            si.endCol = end.column;

            // Determine usage kind from the innermost node's relationship to its parent
            if (i > 0) {
                si.usage = determineUsageKind(ancestors[0], ancestors[1]);
            }
            // If the node itself is a declaration, get the name from the declaration
            if (i == 0) {
                std::string declName = getNodeName(ancestors[0], result);
                if (!declName.empty()) {
                    si.name = declName;
                }
            }
            return si;
        }
    }

    // No declaration found, but we still have node info
    si.name = text;
    TSPoint start = ts_node_start_point(node);
    TSPoint end = ts_node_end_point(node);
    si.startLine = start.row;
    si.startCol = start.column;
    si.endLine = end.row;
    si.endCol = end.column;

    // Try to determine usage from parent
    if (ancestors.size() >= 2) {
        si.usage = determineUsageKind(ancestors[0], ancestors[1]);
    }

    return si;
}

// --------------------------------------------------------------------------
// Find references
// --------------------------------------------------------------------------

void CSTSearcher::collectIdentifierRefs(
    TSNode node,
    const AstParseResult* result,
    const std::string& name,
    std::vector<TSNode>* vec) {

    const char* type = ts_node_type(node);

    // Check identifier nodes
    if (strcmp(type, "identifier") == 0 ||
        strcmp(type, "scoped_identifier") == 0) {
        std::string text = result->getNodeText(node);
        if (text == name) {
            vec->push_back(node);
        }
        return; // Identifiers have no children
    }

    // Recurse into all children (including unnamed) for thorough coverage
    uint32_t childCount = ts_node_child_count(node);
    for (uint32_t i = 0; i < childCount; i++) {
        collectIdentifierRefs(ts_node_child(node, i), result, name, vec);
    }
}

std::vector<TSNode>* CSTSearcher::findReferences(
    const AstParseResult* result,
    uint32_t line, uint32_t col,
    bool includeDecl) {

    if (!result) {
        return nullptr;
    }

    TSNode node = findNodeAtPosition(result, line, col);
    if (ts_node_is_null(node)) {
        return nullptr;
    }

    std::string targetName = result->getNodeText(node);
    if (targetName.empty()) {
        return nullptr;
    }

    std::unique_ptr<std::vector<TSNode>> refs(new std::vector<TSNode>);
    TSNode root = result->getRootNode();
    collectIdentifierRefs(root, result, targetName, refs.get());

    if (!includeDecl) {
        // Remove the declaration occurrence (the one at the original position)
        refs->erase(
            std::remove_if(refs->begin(), refs->end(),
                [line, col](const TSNode& n) {
                    TSPoint start = ts_node_start_point(n);
                    return start.row == line && start.column == col;
                }),
            refs->end());
    }

    if (refs->empty()) {
        return nullptr;
    }
    return refs.release();
}

// --------------------------------------------------------------------------
// Scope symbols
// --------------------------------------------------------------------------

void CSTSearcher::collectParameters(
    TSNode funcNode,
    const AstParseResult* result,
    int scopeLevel,
    std::vector<CSTScopeSymbolInfo>* vec) {

    TSNode params = ts_node_child_by_field_name(funcNode, "parameters", 10);
    if (ts_node_is_null(params)) {
        return;
    }

    uint32_t childCount = ts_node_named_child_count(params);
    for (uint32_t i = 0; i < childCount; i++) {
        TSNode param = ts_node_named_child(params, i);
        const char* paramType = ts_node_type(param);
        if (strcmp(paramType, "parameter") == 0) {
            std::string name = getFieldText(param, "name", result);
            if (!name.empty()) {
                CSTSymbolInfo si;
                si.kind = ASYK_Variable;
                si.name = name;
                TSPoint start = ts_node_start_point(param);
                TSPoint end = ts_node_end_point(param);
                si.startLine = start.row;
                si.startCol = start.column;
                si.endLine = end.row;
                si.endCol = end.column;
                vec->push_back(CSTScopeSymbolInfo(std::move(si), scopeLevel));
            }
        }
    }
}

void CSTSearcher::collectLocalsBeforePosition(
    TSNode blockNode,
    const AstParseResult* result,
    uint32_t line, uint32_t col,
    int scopeLevel,
    std::vector<CSTScopeSymbolInfo>* vec) {

    uint32_t childCount = ts_node_named_child_count(blockNode);
    for (uint32_t i = 0; i < childCount; i++) {
        TSNode child = ts_node_named_child(blockNode, i);
        TSPoint childStart = ts_node_start_point(child);

        // Only include declarations before the cursor position
        if (childStart.row > line || (childStart.row == line && childStart.column > col)) {
            break;
        }

        const char* childType = ts_node_type(child);
        if (strcmp(childType, "variable_declaration") == 0) {
            std::string name = getNodeName(child, result);
            if (!name.empty()) {
                CSTSymbolInfo si;
                si.kind = ASYK_Variable;
                si.name = name;
                TSPoint start = ts_node_start_point(child);
                TSPoint end = ts_node_end_point(child);
                si.startLine = start.row;
                si.startCol = start.column;
                si.endLine = end.row;
                si.endCol = end.column;
                vec->push_back(CSTScopeSymbolInfo(std::move(si), scopeLevel));
            }
        }
        // Also check expression statements that contain variable declarations
        if (strcmp(childType, "expression_statement") == 0) {
            uint32_t exprChildCount = ts_node_named_child_count(child);
            for (uint32_t j = 0; j < exprChildCount; j++) {
                TSNode exprChild = ts_node_named_child(child, j);
                const char* exprType = ts_node_type(exprChild);
                if (strcmp(exprType, "variable_declaration") == 0) {
                    std::string name = getNodeName(exprChild, result);
                    if (!name.empty()) {
                        CSTSymbolInfo si;
                        si.kind = ASYK_Variable;
                        si.name = name;
                        TSPoint start = ts_node_start_point(exprChild);
                        TSPoint end = ts_node_end_point(exprChild);
                        si.startLine = start.row;
                        si.startCol = start.column;
                        si.endLine = end.row;
                        si.endCol = end.column;
                        vec->push_back(CSTScopeSymbolInfo(std::move(si), scopeLevel));
                    }
                }
            }
        }
    }
}

void CSTSearcher::collectDeclarationsInScope(
    TSNode scopeNode,
    const AstParseResult* result,
    int scopeLevel,
    std::vector<CSTScopeSymbolInfo>* vec) {

    uint32_t childCount = ts_node_named_child_count(scopeNode);
    for (uint32_t i = 0; i < childCount; i++) {
        TSNode child = ts_node_named_child(scopeNode, i);
        const char* childType = ts_node_type(child);
        ASTSymbolKind kind = nodeTypeToSymbolKind(childType);

        if (kind != ASYK_None) {
            std::string name = getNodeName(child, result);
            if (!name.empty()) {
                CSTSymbolInfo si;
                si.kind = kind;
                si.name = name;
                si.docComment = findDocComment(child, result);
                TSPoint start = ts_node_start_point(child);
                TSPoint end = ts_node_end_point(child);
                si.startLine = start.row;
                si.startCol = start.column;
                si.endLine = end.row;
                si.endCol = end.column;
                vec->push_back(CSTScopeSymbolInfo(std::move(si), scopeLevel));
            }
        }

        // Look for declarations inside member blocks (public { }, private { })
        if (strcmp(childType, "member_block") == 0 ||
            strcmp(childType, "access_modifier_block") == 0) {
            collectDeclarationsInScope(child, result, scopeLevel, vec);
        }
    }
}

void CSTSearcher::collectScopeSymbolsFromAncestors(
    const std::vector<TSNode>& ancestors,
    const AstParseResult* result,
    uint32_t line, uint32_t col,
    std::vector<CSTScopeSymbolInfo>* vec) {

    int scopeLevel = 0;
    bool inFunction = false;

    for (size_t i = 0; i < ancestors.size(); i++) {
        const char* type = ts_node_type(ancestors[i]);

        if (strcmp(type, "block") == 0 && !inFunction) {
            // Check if parent is a function/method/constructor
            if (i + 1 < ancestors.size()) {
                const char* parentType = ts_node_type(ancestors[i + 1]);
                if (strcmp(parentType, "function_declaration") == 0 ||
                    strcmp(parentType, "method_declaration") == 0 ||
                    strcmp(parentType, "constructor_declaration") == 0 ||
                    strcmp(parentType, "destructor_declaration") == 0) {
                    // Collect locals from this block
                    collectLocalsBeforePosition(ancestors[i], result, line, col,
                                                scopeLevel, vec);
                    // Collect parameters
                    collectParameters(ancestors[i + 1], result, scopeLevel, vec);
                    inFunction = true;
                    scopeLevel++;
                }
            }
        } else if (strcmp(type, "function_declaration") == 0 ||
                   strcmp(type, "method_declaration") == 0 ||
                   strcmp(type, "constructor_declaration") == 0 ||
                   strcmp(type, "destructor_declaration") == 0) {
            // Already handled via block above, but add the function itself
            // at this scope level if not already in function scope
            if (!inFunction) {
                collectParameters(ancestors[i], result, scopeLevel, vec);
                inFunction = true;
                scopeLevel++;
            }
        } else if (strcmp(type, "class_declaration") == 0) {
            // Collect class members
            collectDeclarationsInScope(ancestors[i], result, scopeLevel, vec);
            scopeLevel++;
        } else if (strcmp(type, "namespace_declaration") == 0) {
            // Collect namespace declarations
            collectDeclarationsInScope(ancestors[i], result, scopeLevel, vec);
            scopeLevel++;
        } else if (strcmp(type, "source_file") == 0) {
            // Collect top-level declarations
            collectDeclarationsInScope(ancestors[i], result, scopeLevel, vec);
        }
    }
}

std::vector<CSTScopeSymbolInfo>* CSTSearcher::findScopeSymbols(
    const AstParseResult* result,
    uint32_t line, uint32_t col) {

    if (!result) {
        return nullptr;
    }

    std::vector<TSNode> ancestors = findNodeAndParents(result, line, col);
    if (ancestors.empty()) {
        return nullptr;
    }

    std::unique_ptr<std::vector<CSTScopeSymbolInfo>> vec(
        new std::vector<CSTScopeSymbolInfo>);
    collectScopeSymbolsFromAncestors(ancestors, result, line, col, vec.get());

    if (vec->empty()) {
        return nullptr;
    }
    return vec.release();
}

// --------------------------------------------------------------------------
// Hover info
// --------------------------------------------------------------------------

std::string CSTSearcher::buildClassSignature(TSNode node, const AstParseResult* result) {
    std::ostringstream ss;

    // Collect modifiers before "class" keyword
    uint32_t childCount = ts_node_child_count(node);
    for (uint32_t i = 0; i < childCount; i++) {
        TSNode child = ts_node_child(node, i);
        const char* childType = ts_node_type(child);
        if (strcmp(childType, "class") == 0) {
            break; // Stop before "class" keyword
        }
        if (strcmp(childType, "access_modifier") == 0 ||
            strcmp(childType, "public") == 0 ||
            strcmp(childType, "private") == 0) {
            std::string text = result->getNodeText(child);
            ss << text << " ";
        }
    }

    ss << "class " << getNodeName(node, result);

    // Superclasses
    TSNode inherits = ts_node_child_by_field_name(node, "superclasses", 12);
    if (!ts_node_is_null(inherits)) {
        ss << " inherits " << result->getNodeText(inherits);
    }

    return ss.str();
}

std::string CSTSearcher::buildFunctionSignature(TSNode node, const AstParseResult* result) {
    std::ostringstream ss;

    const char* nodeType = ts_node_type(node);

    // Collect modifiers
    uint32_t childCount = ts_node_child_count(node);
    for (uint32_t i = 0; i < childCount; i++) {
        TSNode child = ts_node_child(node, i);
        const char* childType = ts_node_type(child);
        // Stop at function name or keyword
        if (strcmp(childType, "identifier") == 0 ||
            strcmp(childType, "scoped_identifier") == 0 ||
            strcmp(childType, "constructor") == 0 ||
            strcmp(childType, "destructor") == 0 ||
            strcmp(childType, "sub") == 0 ||
            strcmp(childType, "parameter_list") == 0) {
            break;
        }
        if (strcmp(childType, "access_modifier") == 0 ||
            strcmp(childType, "static") == 0 ||
            strcmp(childType, "abstract") == 0 ||
            strcmp(childType, "synchronized") == 0 ||
            strcmp(childType, "deprecated") == 0) {
            ss << result->getNodeText(child) << " ";
        }
        // Return type
        if (strcmp(childType, "simple_type") == 0 ||
            strcmp(childType, "complex_type") == 0 ||
            strcmp(childType, "nothing_type") == 0) {
            ss << result->getNodeText(child) << " ";
        }
    }

    if (strcmp(nodeType, "constructor_declaration") == 0) {
        ss << "constructor";
    } else if (strcmp(nodeType, "destructor_declaration") == 0) {
        ss << "destructor";
    } else if (strcmp(nodeType, "function_declaration") == 0) {
        ss << "sub " << getNodeName(node, result);
    } else {
        ss << getNodeName(node, result);
    }

    // Parameters
    TSNode params = ts_node_child_by_field_name(node, "parameters", 10);
    if (!ts_node_is_null(params)) {
        ss << result->getNodeText(params);
    } else {
        ss << "()";
    }

    // Returns clause
    TSNode returns = ts_node_child_by_field_name(node, "returns", 7);
    if (!ts_node_is_null(returns)) {
        ss << " returns " << result->getNodeText(returns);
    }

    return ss.str();
}

std::string CSTSearcher::buildConstantSignature(TSNode node, const AstParseResult* result) {
    std::ostringstream ss;
    ss << "const " << getNodeName(node, result);

    TSNode value = ts_node_child_by_field_name(node, "value", 5);
    if (!ts_node_is_null(value)) {
        std::string valText = result->getNodeText(value);
        // Truncate long values
        if (valText.size() > 60) {
            valText = valText.substr(0, 57) + "...";
        }
        ss << " = " << valText;
    }

    return ss.str();
}

std::string CSTSearcher::buildVariableSignature(TSNode node, const AstParseResult* result) {
    std::ostringstream ss;

    const char* nodeType = ts_node_type(node);
    if (strcmp(nodeType, "global_variable_declaration") == 0) {
        // Check for our/my/thread_local
        uint32_t childCount = ts_node_child_count(node);
        for (uint32_t i = 0; i < childCount; i++) {
            TSNode child = ts_node_child(node, i);
            const char* childType = ts_node_type(child);
            if (strcmp(childType, "our") == 0 || strcmp(childType, "my") == 0 ||
                strcmp(childType, "thread_local") == 0) {
                ss << result->getNodeText(child) << " ";
                break;
            }
        }
    }

    std::string typeText = getFieldText(node, "type", result);
    if (!typeText.empty()) {
        ss << typeText << " ";
    }

    ss << getNodeName(node, result);

    return ss.str();
}

std::string CSTSearcher::buildHashdeclSignature(TSNode node, const AstParseResult* result) {
    std::ostringstream ss;
    ss << "hashdecl " << getNodeName(node, result);

    TSNode inherits = ts_node_child_by_field_name(node, "superclasses", 12);
    if (!ts_node_is_null(inherits)) {
        ss << " inherits " << result->getNodeText(inherits);
    }

    return ss.str();
}

std::string CSTSearcher::buildTypedefSignature(TSNode node, const AstParseResult* result) {
    std::ostringstream ss;
    ss << "typedef " << getNodeName(node, result);

    TSNode typeNode = ts_node_child_by_field_name(node, "type", 4);
    if (!ts_node_is_null(typeNode)) {
        ss << " = " << result->getNodeText(typeNode);
    }

    return ss.str();
}

std::string CSTSearcher::buildHashMemberSignature(TSNode node, const AstParseResult* result) {
    std::ostringstream ss;

    std::string typeText = getFieldText(node, "type", result);
    if (!typeText.empty()) {
        ss << typeText << " ";
    }

    ss << getNodeName(node, result);

    return ss.str();
}

std::string CSTSearcher::buildHoverInfo(
    const AstParseResult* result,
    ASTSymbolKind kind,
    uint32_t line, uint32_t col) {

    if (!result) {
        return std::string();
    }

    // Map Method and Constructor to Function for lookup (matching old behavior)
    if (kind == ASYK_Method) {
        kind = ASYK_Function;
    }
    if (kind == ASYK_Constructor) {
        kind = ASYK_Function;
    }

    std::vector<TSNode> ancestors = findNodeAndParents(result, line, col);
    if (ancestors.empty()) {
        return std::string();
    }

    // Find the nearest ancestor matching the requested kind
    for (const auto& node : ancestors) {
        const char* type = ts_node_type(node);
        ASTSymbolKind nodeKind = nodeTypeToSymbolKind(type);

        // Constructor/destructor/method all match ASYK_Function (after mapping)
        if (kind == ASYK_Function &&
            (nodeKind == ASYK_Function || nodeKind == ASYK_Method ||
             nodeKind == ASYK_Constructor)) {
            return buildFunctionSignature(node, result);
        }

        if (nodeKind == kind) {
            switch (kind) {
                case ASYK_Class:
                    return buildClassSignature(node, result);
                case ASYK_Constant:
                    return buildConstantSignature(node, result);
                case ASYK_Interface:
                    return buildHashdeclSignature(node, result);
                case ASYK_Field:
                    return buildHashMemberSignature(node, result);
                case ASYK_Variable:
                    return buildVariableSignature(node, result);
                case ASYK_TypeAlias:
                    return buildTypedefSignature(node, result);
                case ASYK_Module:
                    return "module " + getNodeName(node, result);
                default:
                    break;
            }
        }
    }

    return std::string();
}
