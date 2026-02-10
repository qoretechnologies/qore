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
        strcmp(type, "hashdecl_member") == 0 ||
        strcmp(type, "member_declaration") == 0) {
        return ASYK_Field;
    }
    if (strcmp(type, "variable_declaration") == 0 ||
        strcmp(type, "local_variable_declaration") == 0) {
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

    // Find parameter_list child by type (it has no field name in the grammar)
    TSNode params = ts_node_child(funcNode, 0);  // will be overwritten; just need init
    bool foundParams = false;
    uint32_t funcChildCount = ts_node_named_child_count(funcNode);
    for (uint32_t i = 0; i < funcChildCount; i++) {
        TSNode child = ts_node_named_child(funcNode, i);
        if (strcmp(ts_node_type(child), "parameter_list") == 0) {
            params = child;
            foundParams = true;
            break;
        }
    }
    if (!foundParams) {
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
        if (strcmp(childType, "local_variable_declaration") == 0) {
            // Iterate variable_declarator children to get each declared name
            uint32_t declChildCount = ts_node_named_child_count(child);
            for (uint32_t j = 0; j < declChildCount; j++) {
                TSNode declChild = ts_node_named_child(child, j);
                const char* declType = ts_node_type(declChild);
                if (strcmp(declType, "variable_declarator") == 0) {
                    std::string name = getFieldText(declChild, "name", result);
                    if (!name.empty()) {
                        CSTSymbolInfo si;
                        si.kind = ASYK_Variable;
                        si.name = name;
                        TSPoint start = ts_node_start_point(declChild);
                        TSPoint end = ts_node_end_point(declChild);
                        si.startLine = start.row;
                        si.startCol = start.column;
                        si.endLine = end.row;
                        si.endCol = end.column;
                        vec->push_back(CSTScopeSymbolInfo(std::move(si), scopeLevel));
                    }
                }
            }
        }
        // Also check expression statements that contain local variable declarations
        if (strcmp(childType, "expression_statement") == 0) {
            uint32_t exprChildCount = ts_node_named_child_count(child);
            for (uint32_t j = 0; j < exprChildCount; j++) {
                TSNode exprChild = ts_node_named_child(child, j);
                const char* exprType = ts_node_type(exprChild);
                if (strcmp(exprType, "local_variable_declaration") == 0) {
                    uint32_t declChildCount = ts_node_named_child_count(exprChild);
                    for (uint32_t k = 0; k < declChildCount; k++) {
                        TSNode declChild = ts_node_named_child(exprChild, k);
                        const char* declType = ts_node_type(declChild);
                        if (strcmp(declType, "variable_declarator") == 0) {
                            std::string name = getFieldText(declChild, "name", result);
                            if (!name.empty()) {
                                CSTSymbolInfo si;
                                si.kind = ASYK_Variable;
                                si.name = name;
                                TSPoint start = ts_node_start_point(declChild);
                                TSPoint end = ts_node_end_point(declChild);
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

        // Look for declarations inside member groups (public { }, private { })
        if (strcmp(childType, "member_group") == 0) {
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
// Detailed symbol helpers
// --------------------------------------------------------------------------

std::string CSTSearcher::extractAccessModifier(TSNode node, const AstParseResult* result) {
    // Check inline modifiers on the node itself
    // Grammar: modifiers → modifier → access_modifier
    uint32_t childCount = ts_node_child_count(node);
    for (uint32_t i = 0; i < childCount; i++) {
        TSNode child = ts_node_child(node, i);
        const char* childType = ts_node_type(child);
        if (strcmp(childType, "modifiers") == 0) {
            // Walk all children of modifiers (including anonymous)
            uint32_t modCount = ts_node_child_count(child);
            for (uint32_t j = 0; j < modCount; j++) {
                TSNode mod = ts_node_child(child, j);
                const char* modType = ts_node_type(mod);
                if (strcmp(modType, "access_modifier") == 0) {
                    return result->getNodeText(mod);
                }
                // modifier wraps access_modifier — check its children
                if (strcmp(modType, "modifier") == 0) {
                    uint32_t innerCount = ts_node_child_count(mod);
                    for (uint32_t k = 0; k < innerCount; k++) {
                        TSNode inner = ts_node_child(mod, k);
                        if (strcmp(ts_node_type(inner), "access_modifier") == 0) {
                            return result->getNodeText(inner);
                        }
                    }
                }
            }
        }
        if (strcmp(childType, "access_modifier") == 0) {
            return result->getNodeText(child);
        }
    }

    // Check parent member_group for its access modifier
    TSNode parent = ts_node_parent(node);
    if (!ts_node_is_null(parent) && strcmp(ts_node_type(parent), "member_group") == 0) {
        uint32_t parentChildCount = ts_node_child_count(parent);
        for (uint32_t i = 0; i < parentChildCount; i++) {
            TSNode child = ts_node_child(parent, i);
            if (strcmp(ts_node_type(child), "access_modifier") == 0) {
                return result->getNodeText(child);
            }
        }
    }

    return std::string();
}

bool CSTSearcher::hasStaticModifier(TSNode node, const AstParseResult* result) {
    uint32_t childCount = ts_node_child_count(node);
    for (uint32_t i = 0; i < childCount; i++) {
        TSNode child = ts_node_child(node, i);
        const char* childType = ts_node_type(child);
        if (strcmp(childType, "modifiers") == 0) {
            // modifiers → modifier → "static" (anonymous leaf)
            // Check all children (including anonymous) of modifiers
            uint32_t modCount = ts_node_child_count(child);
            for (uint32_t j = 0; j < modCount; j++) {
                TSNode mod = ts_node_child(child, j);
                const char* modType = ts_node_type(mod);
                if (strcmp(modType, "static") == 0) {
                    return true;
                }
                // modifier wraps the keyword — check its text
                if (strcmp(modType, "modifier") == 0) {
                    std::string text = result->getNodeText(mod);
                    if (text == "static") {
                        return true;
                    }
                }
            }
        }
        if (strcmp(childType, "static") == 0) {
            return true;
        }
    }
    return false;
}

std::vector<CSTParamInfo> CSTSearcher::extractParameters(TSNode funcNode,
                                                          const AstParseResult* result) {
    std::vector<CSTParamInfo> params;

    // Find parameter_list child by type
    uint32_t funcChildCount = ts_node_named_child_count(funcNode);
    bool foundParamList = false;
    TSNode paramList = ts_node_child(funcNode, 0);
    for (uint32_t i = 0; i < funcChildCount; i++) {
        TSNode child = ts_node_named_child(funcNode, i);
        if (strcmp(ts_node_type(child), "parameter_list") == 0) {
            paramList = child;
            foundParamList = true;
            break;
        }
    }
    if (!foundParamList) {
        return params;
    }

    uint32_t childCount = ts_node_named_child_count(paramList);
    for (uint32_t i = 0; i < childCount; i++) {
        TSNode param = ts_node_named_child(paramList, i);
        if (strcmp(ts_node_type(param), "parameter") == 0) {
            CSTParamInfo pi;
            pi.name = getFieldText(param, "name", result);
            pi.typeName = getFieldText(param, "type", result);
            pi.defaultVal = getFieldText(param, "default", result);
            params.push_back(std::move(pi));
        }
    }
    return params;
}

std::string CSTSearcher::extractReturnType(TSNode funcNode, const AstParseResult* result) {
    // Check field 'return_type' first (before name)
    std::string rt = getFieldText(funcNode, "return_type", result);
    if (!rt.empty()) {
        return rt;
    }
    // Check 'returns' clause (after parameter_list)
    return getFieldText(funcNode, "returns", result);
}

std::string CSTSearcher::extractTypeName(TSNode node, const AstParseResult* result) {
    const char* type = ts_node_type(node);

    // For variable_declarator, the type is on the parent local/global_variable_declaration
    if (strcmp(type, "variable_declarator") == 0) {
        TSNode parent = ts_node_parent(node);
        if (!ts_node_is_null(parent)) {
            return getFieldText(parent, "type", result);
        }
        return std::string();
    }

    // For parameter, member_declaration, hashdecl_member — 'type' field
    return getFieldText(node, "type", result);
}

CSTSymbolDetail CSTSearcher::enrichSymbol(const CSTScopeSymbolInfo& ssi,
                                           const AstParseResult* result) {
    CSTSymbolDetail detail;
    detail.symbol = ssi.symbol;
    detail.scopeLevel = ssi.scopeLevel;

    // Find the declaration node at the symbol's start position
    std::vector<TSNode> ancestors = findNodeAndParents(
        result, ssi.symbol.startLine, ssi.symbol.startCol);

    if (ancestors.empty()) {
        return detail;
    }

    // Find the nearest declaration/named node
    for (const auto& anc : ancestors) {
        const char* type = ts_node_type(anc);
        ASTSymbolKind kind = nodeTypeToSymbolKind(type);

        if (kind == ASYK_Function || kind == ASYK_Method || kind == ASYK_Constructor) {
            detail.returnType = extractReturnType(anc, result);
            detail.params = extractParameters(anc, result);
            detail.access = extractAccessModifier(anc, result);
            detail.isStatic = hasStaticModifier(anc, result);
            return detail;
        }

        if (strcmp(type, "member_declaration") == 0) {
            detail.typeName = extractTypeName(anc, result);
            detail.access = extractAccessModifier(anc, result);
            detail.isStatic = hasStaticModifier(anc, result);
            return detail;
        }

        if (strcmp(type, "variable_declarator") == 0) {
            detail.typeName = extractTypeName(anc, result);
            return detail;
        }

        if (strcmp(type, "local_variable_declaration") == 0 ||
            strcmp(type, "global_variable_declaration") == 0) {
            detail.typeName = getFieldText(anc, "type", result);
            return detail;
        }

        if (strcmp(type, "parameter") == 0) {
            detail.typeName = getFieldText(anc, "type", result);
            return detail;
        }

        if (strcmp(type, "hashdecl_member") == 0) {
            detail.typeName = extractTypeName(anc, result);
            return detail;
        }

        if (kind == ASYK_Class || kind == ASYK_Namespace || kind == ASYK_Interface ||
            kind == ASYK_Constant || kind == ASYK_TypeAlias) {
            detail.access = extractAccessModifier(anc, result);
            return detail;
        }
    }

    return detail;
}

std::vector<CSTSymbolDetail>* CSTSearcher::findScopeSymbolsDetailed(
    const AstParseResult* result,
    uint32_t line, uint32_t col) {

    if (!result) {
        return nullptr;
    }

    // Get basic scope symbols first
    std::unique_ptr<std::vector<CSTScopeSymbolInfo>> basic(
        findScopeSymbols(result, line, col));
    if (!basic) {
        return nullptr;
    }

    std::unique_ptr<std::vector<CSTSymbolDetail>> vec(
        new std::vector<CSTSymbolDetail>);
    vec->reserve(basic->size());

    for (const auto& ssi : *basic) {
        vec->push_back(enrichSymbol(ssi, result));
    }

    if (vec->empty()) {
        return nullptr;
    }
    return vec.release();
}

// --------------------------------------------------------------------------
// Type resolution and member listing
// --------------------------------------------------------------------------

std::string CSTSearcher::getSymbolType(
    const AstParseResult* result,
    uint32_t line, uint32_t col) {

    if (!result) {
        return std::string();
    }

    std::vector<TSNode> ancestors = findNodeAndParents(result, line, col);
    if (ancestors.empty()) {
        return std::string();
    }

    // Check the leaf node and its ancestors
    for (size_t i = 0; i < ancestors.size(); i++) {
        const char* type = ts_node_type(ancestors[i]);

        // "self" keyword → find enclosing class
        if (strcmp(type, "self") == 0) {
            for (size_t j = i + 1; j < ancestors.size(); j++) {
                if (strcmp(ts_node_type(ancestors[j]), "class_declaration") == 0) {
                    return getNodeName(ancestors[j], result);
                }
            }
            return std::string();
        }

        // Check variable_declarator for type from parent
        if (strcmp(type, "variable_declarator") == 0) {
            return extractTypeName(ancestors[i], result);
        }

        // Check local/global variable declaration
        if (strcmp(type, "local_variable_declaration") == 0 ||
            strcmp(type, "global_variable_declaration") == 0) {
            return getFieldText(ancestors[i], "type", result);
        }

        // Check parameter
        if (strcmp(type, "parameter") == 0) {
            return getFieldText(ancestors[i], "type", result);
        }

        // Check member_declaration
        if (strcmp(type, "member_declaration") == 0) {
            return getFieldText(ancestors[i], "type", result);
        }

        // For identifiers, check if they match a known type name
        if (strcmp(type, "identifier") == 0 || strcmp(type, "scoped_identifier") == 0) {
            // Check if parent is a type node or simple_type
            if (i + 1 < ancestors.size()) {
                const char* parentType = ts_node_type(ancestors[i + 1]);
                if (strcmp(parentType, "simple_type") == 0 ||
                    strcmp(parentType, "type") == 0) {
                    return result->getNodeText(ancestors[i]);
                }
            }
        }
    }

    return std::string();
}

bool CSTSearcher::findDeclarationByName(TSNode root, const AstParseResult* result,
                                          const std::string& name, const char* nodeType,
                                          TSNode* outNode) {
    uint32_t childCount = ts_node_named_child_count(root);
    for (uint32_t i = 0; i < childCount; i++) {
        TSNode child = ts_node_named_child(root, i);
        const char* type = ts_node_type(child);

        if (nodeType == nullptr || strcmp(type, nodeType) == 0) {
            std::string nodeName = getNodeName(child, result);
            if (nodeName == name) {
                *outNode = child;
                return true;
            }
        }

        // Recurse into containers
        if (strcmp(type, "namespace_declaration") == 0 ||
            strcmp(type, "class_declaration") == 0 ||
            strcmp(type, "source_file") == 0 ||
            strcmp(type, "member_group") == 0) {
            if (findDeclarationByName(child, result, name, nodeType, outNode)) {
                return true;
            }
        }
    }
    return false;
}

void CSTSearcher::collectClassMembers(TSNode classNode, const AstParseResult* result,
                                       std::vector<CSTSymbolDetail>* vec,
                                       std::vector<std::string>* visited,
                                       bool includeInherited) {
    std::string className = getNodeName(classNode, result);

    // Prevent infinite recursion in inheritance
    std::vector<std::string> localVisited;
    if (!visited) {
        visited = &localVisited;
    }
    for (const auto& v : *visited) {
        if (v == className) {
            return;
        }
    }
    visited->push_back(className);

    // Iterate all children
    uint32_t childCount = ts_node_named_child_count(classNode);
    for (uint32_t i = 0; i < childCount; i++) {
        TSNode child = ts_node_named_child(classNode, i);
        const char* type = ts_node_type(child);

        if (strcmp(type, "member_group") == 0) {
            // Get access modifier from the group
            std::string groupAccess;
            uint32_t groupChildCount = ts_node_child_count(child);
            for (uint32_t j = 0; j < groupChildCount; j++) {
                TSNode gc = ts_node_child(child, j);
                if (strcmp(ts_node_type(gc), "access_modifier") == 0) {
                    groupAccess = result->getNodeText(gc);
                    break;
                }
            }

            uint32_t memberCount = ts_node_named_child_count(child);
            for (uint32_t j = 0; j < memberCount; j++) {
                TSNode member = ts_node_named_child(child, j);
                const char* mtype = ts_node_type(member);
                ASTSymbolKind kind = nodeTypeToSymbolKind(mtype);

                if (kind != ASYK_None) {
                    CSTSymbolDetail detail;
                    detail.symbol.kind = kind;
                    detail.symbol.name = getNodeName(member, result);
                    detail.symbol.docComment = findDocComment(member, result);
                    TSPoint start = ts_node_start_point(member);
                    TSPoint end = ts_node_end_point(member);
                    detail.symbol.startLine = start.row;
                    detail.symbol.startCol = start.column;
                    detail.symbol.endLine = end.row;
                    detail.symbol.endCol = end.column;
                    detail.access = groupAccess;
                    detail.isStatic = hasStaticModifier(member, result);

                    if (kind == ASYK_Method || kind == ASYK_Function || kind == ASYK_Constructor) {
                        detail.returnType = extractReturnType(member, result);
                        detail.params = extractParameters(member, result);
                    } else if (kind == ASYK_Field) {
                        detail.typeName = extractTypeName(member, result);
                    }

                    vec->push_back(std::move(detail));
                }
            }
        }

        // Direct children (method_declaration, constructor_declaration, etc.)
        ASTSymbolKind kind = nodeTypeToSymbolKind(type);
        if (kind == ASYK_Method || kind == ASYK_Function || kind == ASYK_Constructor) {
            CSTSymbolDetail detail;
            detail.symbol.kind = kind;
            detail.symbol.name = getNodeName(child, result);
            detail.symbol.docComment = findDocComment(child, result);
            TSPoint start = ts_node_start_point(child);
            TSPoint end = ts_node_end_point(child);
            detail.symbol.startLine = start.row;
            detail.symbol.startCol = start.column;
            detail.symbol.endLine = end.row;
            detail.symbol.endCol = end.column;
            detail.access = extractAccessModifier(child, result);
            detail.isStatic = hasStaticModifier(child, result);
            detail.returnType = extractReturnType(child, result);
            detail.params = extractParameters(child, result);
            vec->push_back(std::move(detail));
        } else if (strcmp(type, "constant_declaration") == 0) {
            CSTSymbolDetail detail;
            detail.symbol.kind = ASYK_Constant;
            detail.symbol.name = getNodeName(child, result);
            detail.symbol.docComment = findDocComment(child, result);
            TSPoint start = ts_node_start_point(child);
            TSPoint end = ts_node_end_point(child);
            detail.symbol.startLine = start.row;
            detail.symbol.startCol = start.column;
            detail.symbol.endLine = end.row;
            detail.symbol.endCol = end.column;
            detail.access = extractAccessModifier(child, result);
            vec->push_back(std::move(detail));
        }

        // Handle inherited classes
        if (includeInherited && strcmp(type, "superclass_list") == 0) {
            uint32_t superCount = ts_node_named_child_count(child);
            for (uint32_t j = 0; j < superCount; j++) {
                TSNode superclass = ts_node_named_child(child, j);
                if (strcmp(ts_node_type(superclass), "superclass") != 0) {
                    continue;
                }
                // Get the class name from the superclass node
                uint32_t superChildCount = ts_node_named_child_count(superclass);
                for (uint32_t k = 0; k < superChildCount; k++) {
                    TSNode sc = ts_node_named_child(superclass, k);
                    const char* scType = ts_node_type(sc);
                    if (strcmp(scType, "identifier") == 0 ||
                        strcmp(scType, "scoped_identifier") == 0) {
                        std::string parentName = result->getNodeText(sc);
                        // Find parent class in tree and collect its members
                        TSNode rootNode = ts_tree_root_node(result->getTree());
                        TSNode parentClass = rootNode;  // initialized; overwritten by findDeclarationByName
                        if (findDeclarationByName(rootNode, result, parentName,
                                                   "class_declaration", &parentClass)) {
                            collectClassMembers(parentClass, result, vec, visited, true);
                        }
                        break;
                    }
                }
            }
        }
    }
}

void CSTSearcher::collectNamespaceMembers(TSNode nsNode, const AstParseResult* result,
                                           std::vector<CSTSymbolDetail>* vec) {
    uint32_t childCount = ts_node_named_child_count(nsNode);
    for (uint32_t i = 0; i < childCount; i++) {
        TSNode child = ts_node_named_child(nsNode, i);
        const char* type = ts_node_type(child);
        ASTSymbolKind kind = nodeTypeToSymbolKind(type);

        if (kind != ASYK_None) {
            CSTSymbolDetail detail;
            detail.symbol.kind = kind;
            detail.symbol.name = getNodeName(child, result);
            detail.symbol.docComment = findDocComment(child, result);
            TSPoint start = ts_node_start_point(child);
            TSPoint end = ts_node_end_point(child);
            detail.symbol.startLine = start.row;
            detail.symbol.startCol = start.column;
            detail.symbol.endLine = end.row;
            detail.symbol.endCol = end.column;
            detail.access = extractAccessModifier(child, result);

            if (kind == ASYK_Function) {
                detail.returnType = extractReturnType(child, result);
                detail.params = extractParameters(child, result);
            }

            vec->push_back(std::move(detail));
        }
    }
}

void CSTSearcher::collectHashdeclMembers(TSNode hdNode, const AstParseResult* result,
                                          std::vector<CSTSymbolDetail>* vec) {
    uint32_t childCount = ts_node_named_child_count(hdNode);
    for (uint32_t i = 0; i < childCount; i++) {
        TSNode child = ts_node_named_child(hdNode, i);
        if (strcmp(ts_node_type(child), "hashdecl_member") == 0) {
            CSTSymbolDetail detail;
            detail.symbol.kind = ASYK_Field;
            detail.symbol.name = getFieldText(child, "name", result);
            detail.symbol.docComment = findDocComment(child, result);
            detail.typeName = getFieldText(child, "type", result);
            TSPoint start = ts_node_start_point(child);
            TSPoint end = ts_node_end_point(child);
            detail.symbol.startLine = start.row;
            detail.symbol.startCol = start.column;
            detail.symbol.endLine = end.row;
            detail.symbol.endCol = end.column;
            vec->push_back(std::move(detail));
        }
    }
}

void CSTSearcher::collectEnumMembers(TSNode enumNode, const AstParseResult* result,
                                      std::vector<CSTSymbolDetail>* vec) {
    uint32_t childCount = ts_node_named_child_count(enumNode);
    for (uint32_t i = 0; i < childCount; i++) {
        TSNode child = ts_node_named_child(enumNode, i);
        if (strcmp(ts_node_type(child), "enum_member") == 0) {
            CSTSymbolDetail detail;
            detail.symbol.kind = ASYK_Constant;
            detail.symbol.name = getFieldText(child, "name", result);
            detail.symbol.docComment = findDocComment(child, result);
            TSPoint start = ts_node_start_point(child);
            TSPoint end = ts_node_end_point(child);
            detail.symbol.startLine = start.row;
            detail.symbol.startCol = start.column;
            detail.symbol.endLine = end.row;
            detail.symbol.endCol = end.column;
            // Store enum value if present
            std::string val = getFieldText(child, "value", result);
            if (!val.empty()) {
                detail.typeName = val;
            }
            vec->push_back(std::move(detail));
        }
    }
}

std::vector<CSTSymbolDetail>* CSTSearcher::findTypeMembers(
    const AstParseResult* result,
    const std::string& typeName,
    bool includeInherited) {

    if (!result || typeName.empty()) {
        return nullptr;
    }

    TSNode root = ts_tree_root_node(result->getTree());

    // Try class first
    TSNode found = root;  // initialized; overwritten by findDeclarationByName
    if (findDeclarationByName(root, result, typeName, "class_declaration", &found)) {
        std::unique_ptr<std::vector<CSTSymbolDetail>> vec(
            new std::vector<CSTSymbolDetail>);
        collectClassMembers(found, result, vec.get(), nullptr, includeInherited);
        if (vec->empty()) {
            return nullptr;
        }
        return vec.release();
    }

    // Try namespace
    if (findDeclarationByName(root, result, typeName, "namespace_declaration", &found)) {
        std::unique_ptr<std::vector<CSTSymbolDetail>> vec(
            new std::vector<CSTSymbolDetail>);
        collectNamespaceMembers(found, result, vec.get());
        if (vec->empty()) {
            return nullptr;
        }
        return vec.release();
    }

    // Try hashdecl
    if (findDeclarationByName(root, result, typeName, "hashdecl_declaration", &found)) {
        std::unique_ptr<std::vector<CSTSymbolDetail>> vec(
            new std::vector<CSTSymbolDetail>);
        collectHashdeclMembers(found, result, vec.get());
        if (vec->empty()) {
            return nullptr;
        }
        return vec.release();
    }

    // Try enum
    if (findDeclarationByName(root, result, typeName, "enum_declaration", &found)) {
        std::unique_ptr<std::vector<CSTSymbolDetail>> vec(
            new std::vector<CSTSymbolDetail>);
        collectEnumMembers(found, result, vec.get());
        if (vec->empty()) {
            return nullptr;
        }
        return vec.release();
    }

    return nullptr;
}

// --------------------------------------------------------------------------
// Resolve definition
// --------------------------------------------------------------------------

//! Check if a usage kind is a declaration name (already at definition).
static bool isDeclNameUsage(ASTSymbolUsageKind usage) {
    switch (usage) {
        case ASUK_ClassDeclName:
        case ASUK_ConstantDeclName:
        case ASUK_FuncDeclName:
        case ASUK_NamespaceDeclName:
        case ASUK_VarDeclName:
        case ASUK_HashDeclName:
        case ASUK_HashMemberName:
        case ASUK_TypedefDeclName:
        case ASUK_ModuleDeclName:
            return true;
        default:
            return false;
    }
}

//! Check if a node type is a function-like declaration.
static bool isFunctionLike(const char* type) {
    return strcmp(type, "function_declaration") == 0
        || strcmp(type, "method_declaration") == 0
        || strcmp(type, "constructor_declaration") == 0
        || strcmp(type, "destructor_declaration") == 0;
}

//! Search a class body for a member matching a name.
static bool findMemberInClass(TSNode classNode, const AstParseResult* result,
                               const std::string& name, TSNode* outNode) {
    uint32_t childCount = ts_node_named_child_count(classNode);
    for (uint32_t i = 0; i < childCount; i++) {
        TSNode child = ts_node_named_child(classNode, i);
        const char* type = ts_node_type(child);

        // Direct children: method, constructor, destructor, member, constant
        if (strcmp(type, "method_declaration") == 0 ||
            strcmp(type, "constructor_declaration") == 0 ||
            strcmp(type, "destructor_declaration") == 0 ||
            strcmp(type, "member_declaration") == 0 ||
            strcmp(type, "constant_declaration") == 0) {
            std::string nodeName = CSTSearcher::getNodeName(child, result);
            if (nodeName == name) {
                *outNode = child;
                return true;
            }
        }

        // member_group: walk its children
        if (strcmp(type, "member_group") == 0) {
            uint32_t memberCount = ts_node_named_child_count(child);
            for (uint32_t j = 0; j < memberCount; j++) {
                TSNode member = ts_node_named_child(child, j);
                const char* mtype = ts_node_type(member);
                if (strcmp(mtype, "method_declaration") == 0 ||
                    strcmp(mtype, "constructor_declaration") == 0 ||
                    strcmp(mtype, "destructor_declaration") == 0 ||
                    strcmp(mtype, "member_declaration") == 0 ||
                    strcmp(mtype, "constant_declaration") == 0) {
                    std::string nodeName = CSTSearcher::getNodeName(member, result);
                    if (nodeName == name) {
                        *outNode = member;
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

bool CSTSearcher::findLocalDeclaration(
    const std::vector<TSNode>& ancestors,
    const AstParseResult* result,
    const std::string& name,
    uint32_t line, uint32_t col,
    TSNode* outNode) {

    for (size_t i = 0; i < ancestors.size(); i++) {
        const char* type = ts_node_type(ancestors[i]);

        // Check foreach_statement: iterator variable
        if (strcmp(type, "foreach_statement") == 0) {
            TSNode varNode = ts_node_child_by_field_name(ancestors[i], "variable", 8);
            if (!ts_node_is_null(varNode)) {
                // The variable field might be an identifier or a typed declaration
                // Check if the variable field's text contains the name
                std::string varText = result->getNodeText(varNode);
                // For "int item", the variable field includes the type — find the
                // identifier within it
                if (varText == name) {
                    *outNode = varNode;
                    return true;
                }
                // Try to find the name child within the variable node
                TSNode nameChild = ts_node_child_by_field_name(varNode, "name", 4);
                if (!ts_node_is_null(nameChild)) {
                    std::string nameText = result->getNodeText(nameChild);
                    if (nameText == name) {
                        *outNode = nameChild;
                        return true;
                    }
                }
            }
        }

        // Check any block ancestor: search locals declared before cursor position.
        // This handles locals in function bodies, if/while/for blocks, etc.
        if (strcmp(type, "block") == 0) {
            // Search local_variable_declaration children before position
            uint32_t childCount = ts_node_named_child_count(ancestors[i]);
            for (uint32_t c = 0; c < childCount; c++) {
                TSNode child = ts_node_named_child(ancestors[i], c);
                TSPoint childStart = ts_node_start_point(child);
                if (childStart.row > line ||
                    (childStart.row == line && childStart.column > col)) {
                    break;
                }

                const char* childType = ts_node_type(child);

                // Check local_variable_declaration directly
                if (strcmp(childType, "local_variable_declaration") == 0) {
                    uint32_t declCount = ts_node_named_child_count(child);
                    for (uint32_t d = 0; d < declCount; d++) {
                        TSNode declChild = ts_node_named_child(child, d);
                        if (strcmp(ts_node_type(declChild), "variable_declarator") == 0) {
                            std::string declName = getFieldText(declChild, "name", result);
                            if (declName == name) {
                                TSNode nameNode = ts_node_child_by_field_name(
                                    declChild, "name", 4);
                                if (!ts_node_is_null(nameNode)) {
                                    *outNode = nameNode;
                                } else {
                                    *outNode = declChild;
                                }
                                return true;
                            }
                        }
                    }
                }

                // Also check expression_statement containing local_variable_declaration
                if (strcmp(childType, "expression_statement") == 0) {
                    uint32_t exprCount = ts_node_named_child_count(child);
                    for (uint32_t e = 0; e < exprCount; e++) {
                        TSNode exprChild = ts_node_named_child(child, e);
                        if (strcmp(ts_node_type(exprChild),
                                  "local_variable_declaration") == 0) {
                            uint32_t declCount = ts_node_named_child_count(exprChild);
                            for (uint32_t d = 0; d < declCount; d++) {
                                TSNode declChild = ts_node_named_child(exprChild, d);
                                if (strcmp(ts_node_type(declChild),
                                          "variable_declarator") == 0) {
                                    std::string declName = getFieldText(
                                        declChild, "name", result);
                                    if (declName == name) {
                                        TSNode nameNode = ts_node_child_by_field_name(
                                            declChild, "name", 4);
                                        if (!ts_node_is_null(nameNode)) {
                                            *outNode = nameNode;
                                        } else {
                                            *outNode = declChild;
                                        }
                                        return true;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // If this block's parent is function-like, also search parameters
            if (i + 1 < ancestors.size()) {
                const char* parentType = ts_node_type(ancestors[i + 1]);
                if (isFunctionLike(parentType)) {
                    TSNode funcNode = ancestors[i + 1];
                    uint32_t funcChildCount = ts_node_named_child_count(funcNode);
                    for (uint32_t f = 0; f < funcChildCount; f++) {
                        TSNode funcChild = ts_node_named_child(funcNode, f);
                        if (strcmp(ts_node_type(funcChild), "parameter_list") == 0) {
                            uint32_t paramCount = ts_node_named_child_count(funcChild);
                            for (uint32_t p = 0; p < paramCount; p++) {
                                TSNode param = ts_node_named_child(funcChild, p);
                                if (strcmp(ts_node_type(param), "parameter") == 0) {
                                    std::string paramName = getFieldText(
                                        param, "name", result);
                                    if (paramName == name) {
                                        TSNode nameNode = ts_node_child_by_field_name(
                                            param, "name", 4);
                                        if (!ts_node_is_null(nameNode)) {
                                            *outNode = nameNode;
                                        } else {
                                            *outNode = param;
                                        }
                                        return true;
                                    }
                                }
                            }
                            break;
                        }
                    }
                }
            }
        }
    }

    return false;
}

std::vector<TSNode>* CSTSearcher::resolveDefinition(
    const AstParseResult* result,
    uint32_t line, uint32_t col) {

    if (!result) {
        return nullptr;
    }

    // 1. Get symbol info at position
    CSTSymbolInfo si = findSymbolInfo(result, line, col);

    // 2. Get ancestors for context analysis
    std::vector<TSNode> ancestors = findNodeAndParents(result, line, col);
    if (ancestors.empty()) {
        return nullptr;
    }

    // If findSymbolInfo didn't find a name (no declaration ancestor), fall back
    // to the leaf node text (handles bare identifiers in expression contexts)
    if (si.name.empty()) {
        std::string nodeText = result->getNodeText(ancestors[0]);
        if (nodeText.empty()) {
            return nullptr;
        }
        si.name = nodeText;
    }

    // 3. If already at a declaration name, return nullptr
    if (isDeclNameUsage(si.usage)) {
        return nullptr;
    }

    TSNode root = result->getRootNode();
    TSNode foundNode = make_null_node();

    // 4. MEMBER ACCESS: Check if parent is member_expression
    if (ancestors.size() >= 2) {
        TSNode parentNode = ancestors[1];
        const char* parentType = ts_node_type(parentNode);
        if (strcmp(parentType, "member_expression") == 0) {
            // Check if we're the "member" field (right side of the dot)
            TSNode memberField = ts_node_child_by_field_name(parentNode, "member", 6);
            if (!ts_node_is_null(memberField) && ts_node_eq(memberField, ancestors[0])) {
                // Get the "object" field's type
                TSNode objNode = ts_node_child_by_field_name(parentNode, "object", 6);
                if (!ts_node_is_null(objNode)) {
                    TSPoint objPos = ts_node_start_point(objNode);
                    std::string typeName = getSymbolType(result, objPos.row, objPos.column);
                    if (!typeName.empty()) {
                        // Find the class declaration and search for the member
                        TSNode classNode = make_null_node();
                        if (findDeclarationByName(root, result, typeName,
                                                   "class_declaration", &classNode)) {
                            TSNode memberNode = make_null_node();
                            if (findMemberInClass(classNode, result, si.name, &memberNode)) {
                                std::unique_ptr<std::vector<TSNode>> rv(
                                    new std::vector<TSNode>);
                                rv->push_back(memberNode);
                                return rv.release();
                            }
                        }
                    }
                }
            }
        }
    }

    // 5. SCOPE ACCESS: Check if we're inside a scoped_identifier
    for (size_t i = 1; i < ancestors.size(); i++) {
        const char* ancType = ts_node_type(ancestors[i]);
        if (strcmp(ancType, "scoped_identifier") == 0) {
            // Extract the scope part (everything before the last ::)
            std::string fullText = result->getNodeText(ancestors[i]);
            size_t lastColon = fullText.rfind("::");
            if (lastColon != std::string::npos) {
                std::string scopeName = fullText.substr(0, lastColon);
                // Try to find as a class
                TSNode scopeNode = make_null_node();
                if (findDeclarationByName(root, result, scopeName,
                                           "class_declaration", &scopeNode)) {
                    TSNode memberNode = make_null_node();
                    if (findMemberInClass(scopeNode, result, si.name, &memberNode)) {
                        std::unique_ptr<std::vector<TSNode>> rv(
                            new std::vector<TSNode>);
                        rv->push_back(memberNode);
                        return rv.release();
                    }
                }
                // Try to find as a namespace
                if (findDeclarationByName(root, result, scopeName,
                                           "namespace_declaration", &scopeNode)) {
                    // Search namespace body for the declaration
                    if (findDeclarationByName(scopeNode, result, si.name,
                                               nullptr, &foundNode)) {
                        std::unique_ptr<std::vector<TSNode>> rv(
                            new std::vector<TSNode>);
                        rv->push_back(foundNode);
                        return rv.release();
                    }
                }
            }
            break;
        }
        // Only walk through simple ancestors, not past declarations
        if (CSTSearcher::isDeclarationNode(ancType)) {
            break;
        }
    }

    // 6. LOCAL VARIABLE / PARAMETER
    if (findLocalDeclaration(ancestors, result, si.name, line, col, &foundNode)) {
        std::unique_ptr<std::vector<TSNode>> rv(new std::vector<TSNode>);
        rv->push_back(foundNode);
        return rv.release();
    }

    // 6.5. CLASS MEMBER (bare identifier inside a method)
    // When inside a method body, check the enclosing class for a matching member
    for (size_t i = 0; i < ancestors.size(); i++) {
        const char* atype = ts_node_type(ancestors[i]);
        if (strcmp(atype, "class_declaration") == 0) {
            if (findMemberInClass(ancestors[i], result, si.name, &foundNode)) {
                std::unique_ptr<std::vector<TSNode>> rv(new std::vector<TSNode>);
                rv->push_back(foundNode);
                return rv.release();
            }
            break;
        }
    }

    // 7. TOP-LEVEL DECLARATION (named declarations like class, function, etc.)
    if (findDeclarationByName(root, result, si.name, nullptr, &foundNode)) {
        std::unique_ptr<std::vector<TSNode>> rv(new std::vector<TSNode>);
        rv->push_back(foundNode);
        return rv.release();
    }

    // 7.5. TOP-LEVEL VARIABLE DECLARATION (local_variable_declaration → variable_declarator)
    // findDeclarationByName doesn't find these because getNodeName returns empty for
    // local_variable_declaration (name is in variable_declarator child)
    {
        uint32_t rootChildCount = ts_node_named_child_count(root);
        for (uint32_t i = 0; i < rootChildCount; i++) {
            TSNode child = ts_node_named_child(root, i);
            const char* ctype = ts_node_type(child);
            if (strcmp(ctype, "local_variable_declaration") == 0) {
                uint32_t declChildCount = ts_node_named_child_count(child);
                for (uint32_t j = 0; j < declChildCount; j++) {
                    TSNode declChild = ts_node_named_child(child, j);
                    if (strcmp(ts_node_type(declChild), "variable_declarator") == 0) {
                        std::string declName = getFieldText(declChild, "name", result);
                        if (declName == si.name) {
                            TSNode nameNode = ts_node_child_by_field_name(
                                declChild, "name", 4);
                            if (!ts_node_is_null(nameNode)) {
                                foundNode = nameNode;
                            } else {
                                foundNode = declChild;
                            }
                            std::unique_ptr<std::vector<TSNode>> rv(
                                new std::vector<TSNode>);
                            rv->push_back(foundNode);
                            return rv.release();
                        }
                    }
                }
            }
        }
    }

    // 8. Not found in this document
    return nullptr;
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

    // Parameters — find parameter_list by type (no field name in grammar)
    bool foundParams = false;
    uint32_t namedCount = ts_node_named_child_count(node);
    for (uint32_t i = 0; i < namedCount; i++) {
        TSNode child = ts_node_named_child(node, i);
        if (strcmp(ts_node_type(child), "parameter_list") == 0) {
            ss << result->getNodeText(child);
            foundParams = true;
            break;
        }
    }
    if (!foundParams) {
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
