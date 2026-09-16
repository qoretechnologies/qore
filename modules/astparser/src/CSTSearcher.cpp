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
#include "CSTWalk.h"

#include <algorithm>
#include <cctype>
#include <memory>
#include <sstream>
#include <unordered_set>

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
                                                    uint32_t line, uint32_t col,
                                                    CSTCancelCheck& cancel) {
    std::vector<TSNode> ancestors;
    TSNode node = findNodeAtPosition(result, line, col);
    if (ts_node_is_null(node)) {
        return ancestors;
    }
    // ts_node_parent() searches from the root for each ancestor, so the path is built from the root down
    TSNode current = result->getRootNode();
    while (!ts_node_is_null(current)) {
        if (cancel()) {
            return std::vector<TSNode>();
        }
        ancestors.push_back(current);
        if (current.id == node.id) {
            break;
        }
        current = ts_node_child_with_descendant(current, node);
    }
    std::reverse(ancestors.begin(), ancestors.end());
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

std::vector<CSTSymbolInfo>* CSTSearcher::collectSymbols(
    const AstParseResult* result,
    CSTCancelCheck& cancel,
    bool fixSymbols,
    bool bareNames) {
    if (!result) {
        return nullptr;
    }

    std::unique_ptr<std::vector<CSTSymbolInfo>> vec(new std::vector<CSTSymbolInfo>);

    // the scope prefix for the named children of the node at each depth of the walk, and the doc comments of its
    // children
    struct PathEntry {
        std::string childPrefix;
        CSTDocComments docComments;
    };
    std::vector<PathEntry> path;
    bool ok = cst_walk(result->getRootNode(), cancel, [&](const TSTreeCursor*, TSNode node, uint32_t depth) {
        if (depth && (path[depth - 1].docComments.add(node) || !ts_node_is_named(node))) {
            // comments and anonymous nodes declare nothing
            return CSTWalkAction::Skip;
        }
        const std::string scopePrefix = depth ? path[depth - 1].childPrefix : std::string();
        std::string childPrefix = scopePrefix;

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

                si.docComment = depth ? path[depth - 1].docComments.get(result) : std::string();

                TSPoint start = ts_node_start_point(node);
                TSPoint end = ts_node_end_point(node);
                si.startLine = start.row;
                si.startCol = start.column;
                si.endLine = end.row;
                si.endCol = end.column;

                // Build new scope prefix for nested declarations
                if (strcmp(type, "class_declaration") == 0 ||
                    strcmp(type, "namespace_declaration") == 0) {
                    if (fixSymbols && !bareNames) {
                        childPrefix = si.name;
                    } else {
                        childPrefix = scopePrefix.empty() ? name : scopePrefix + "::" + name;
                    }
                }

                vec->push_back(std::move(si));
            }
        }

        if (path.size() <= depth) {
            path.resize(depth + 1);
        }
        path[depth].childPrefix = std::move(childPrefix);
        path[depth].docComments.reset();
        return CSTWalkAction::Descend;
    });
    if (!ok) {
        return nullptr;
    }
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
    CSTCancelCheck& cancel,
    bool exactMatch,
    bool fixSymbols,
    bool bareNames) {

    std::unique_ptr<std::vector<CSTSymbolInfo>> allSyms(
        collectSymbols(result, cancel, fixSymbols, bareNames));
    if (!allSyms) {
        return nullptr;
    }

    std::unique_ptr<std::vector<CSTSymbolInfo>> filtered(new std::vector<CSTSymbolInfo>);
    for (auto& si : *allSyms) {
        if (cancel()) {
            return nullptr;
        }
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
    uint32_t line, uint32_t col,
    CSTCancelCheck& cancel) {

    CSTSymbolInfo si;
    if (!result) {
        return si;
    }

    std::vector<TSNode> ancestors = findNodeAndParents(result, line, col, cancel);
    if (ancestors.empty()) {
        return si;
    }

    TSNode node = ancestors[0];
    std::string text = result->getNodeText(node);

    // Walk up to find the nearest declaration context
    for (size_t i = 0; i < ancestors.size(); i++) {
        if (cancel()) {
            return CSTSymbolInfo();
        }
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

static bool isIdentifierLikeNode(const char* type) {
    return strcmp(type, "identifier") == 0
        || strcmp(type, "scoped_identifier") == 0
        || strcmp(type, "streaming_keyword_identifier") == 0;
}

void CSTSearcher::collectIdentifierRefs(
    TSNode root,
    const AstParseResult* result,
    const std::string& name,
    std::vector<TSNode>* vec,
    std::vector<TSNode>* parents,
    CSTCancelCheck& cancel) {

    std::vector<TSNode> path;
    cst_walk(root, cancel, [&](const TSTreeCursor*, TSNode node, uint32_t depth) {
        // Check identifier nodes
        if (isIdentifierLikeNode(ts_node_type(node))) {
            if (result->getNodeText(node) == name) {
                vec->push_back(node);
                if (parents) {
                    parents->push_back(depth ? path[depth - 1] : TSNode{});
                }
            }
            return CSTWalkAction::Skip; // Identifiers have no children
        }

        // Walk all children (including unnamed) for thorough coverage
        if (path.size() <= depth) {
            path.resize(depth + 1);
        }
        path[depth] = node;
        return CSTWalkAction::Descend;
    });
}

std::vector<TSNode>* CSTSearcher::findReferences(
    const AstParseResult* result,
    uint32_t line, uint32_t col,
    bool includeDecl,
    CSTCancelCheck& cancel) {

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
    collectIdentifierRefs(root, result, targetName, refs.get(), nullptr, cancel);
    if (cancel.failed()) {
        return nullptr;
    }

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

//! Returns the location of a node as symbol info of the given kind and name
static CSTSymbolInfo make_symbol_info(TSNode node, ASTSymbolKind kind, std::string name) {
    CSTSymbolInfo si;
    si.kind = kind;
    si.name = std::move(name);
    TSPoint start = ts_node_start_point(node);
    TSPoint end = ts_node_end_point(node);
    si.startLine = start.row;
    si.startCol = start.column;
    si.endLine = end.row;
    si.endCol = end.column;
    return si;
}

void CSTSearcher::collectParameters(
    TSNode funcNode,
    const AstParseResult* result,
    int scopeLevel,
    std::vector<CSTScopeSymbolInfo>* vec,
    CSTCancelCheck& cancel) {

    // Find parameter_list child by type (it has no field name in the grammar)
    TSNode params = cst_find_named_child(funcNode, "parameter_list", cancel);
    if (ts_node_is_null(params)) {
        return;
    }

    cst_for_each_child(params, true, cancel, [&](TSNode param) {
        if (strcmp(ts_node_type(param), "parameter") == 0) {
            std::string name = getFieldText(param, "name", result);
            if (!name.empty()) {
                vec->push_back(CSTScopeSymbolInfo(make_symbol_info(param, ASYK_Variable, std::move(name)),
                    scopeLevel, param, params));
            }
        }
        return true;
    });
}

void CSTSearcher::collectLocalsBeforePosition(
    TSNode blockNode,
    const AstParseResult* result,
    uint32_t line, uint32_t col,
    int scopeLevel,
    std::vector<CSTScopeSymbolInfo>* vec,
    CSTCancelCheck& cancel) {

    // Adds each name declared by the variable_declarator children of a local_variable_declaration
    auto addDeclarators = [&](TSNode decl) {
        return cst_for_each_child(decl, true, cancel, [&](TSNode declChild) {
            if (strcmp(ts_node_type(declChild), "variable_declarator") == 0) {
                std::string name = getFieldText(declChild, "name", result);
                if (!name.empty()) {
                    vec->push_back(CSTScopeSymbolInfo(make_symbol_info(declChild, ASYK_Variable, std::move(name)),
                        scopeLevel, declChild, decl));
                }
            }
            return true;
        });
    };

    cst_for_each_child(blockNode, true, cancel, [&](TSNode child) {
        TSPoint childStart = ts_node_start_point(child);

        // Only include declarations before the cursor position
        if (childStart.row > line || (childStart.row == line && childStart.column > col)) {
            return false;
        }

        const char* childType = ts_node_type(child);
        if (strcmp(childType, "local_variable_declaration") == 0 && !addDeclarators(child)) {
            return false;
        }
        // Also check expression statements that contain local variable declarations
        if (strcmp(childType, "expression_statement") == 0) {
            return cst_for_each_child(child, true, cancel, [&](TSNode exprChild) {
                return strcmp(ts_node_type(exprChild), "local_variable_declaration") != 0
                    || addDeclarators(exprChild);
            });
        }
        return true;
    });
}

void CSTSearcher::collectDeclarationsInScope(
    TSNode scopeNode,
    const AstParseResult* result,
    int scopeLevel,
    std::vector<CSTScopeSymbolInfo>* vec,
    CSTCancelCheck& cancel) {

    CSTDocComments docComments;
    cst_for_each_child(scopeNode, false, cancel, [&](TSNode child) {
        if (docComments.add(child) || !ts_node_is_named(child)) {
            return true;
        }
        const char* childType = ts_node_type(child);
        ASTSymbolKind kind = nodeTypeToSymbolKind(childType);

        if (kind != ASYK_None) {
            std::string name = getNodeName(child, result);
            if (!name.empty()) {
                CSTSymbolInfo si = make_symbol_info(child, kind, std::move(name));
                si.docComment = docComments.get(result);
                vec->push_back(CSTScopeSymbolInfo(std::move(si), scopeLevel, child, scopeNode));
            }
        }

        // Look for declarations inside member groups (public { }, private { })
        if (strcmp(childType, "member_group") == 0) {
            collectDeclarationsInScope(child, result, scopeLevel, vec, cancel);
        }
        return !cancel.failed();
    });
}

void CSTSearcher::collectScopeSymbolsFromAncestors(
    const std::vector<TSNode>& ancestors,
    const AstParseResult* result,
    uint32_t line, uint32_t col,
    std::vector<CSTScopeSymbolInfo>* vec,
    CSTCancelCheck& cancel) {

    int scopeLevel = 0;
    bool inFunction = false;

    for (size_t i = 0; i < ancestors.size(); i++) {
        if (cancel()) {
            return;
        }
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
                                                scopeLevel, vec, cancel);
                    // Collect parameters
                    collectParameters(ancestors[i + 1], result, scopeLevel, vec, cancel);
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
                collectParameters(ancestors[i], result, scopeLevel, vec, cancel);
                inFunction = true;
                scopeLevel++;
            }
        } else if (strcmp(type, "class_declaration") == 0) {
            // Collect class members
            collectDeclarationsInScope(ancestors[i], result, scopeLevel, vec, cancel);
            scopeLevel++;
        } else if (strcmp(type, "namespace_declaration") == 0) {
            // Collect namespace declarations
            collectDeclarationsInScope(ancestors[i], result, scopeLevel, vec, cancel);
            scopeLevel++;
        } else if (strcmp(type, "source_file") == 0) {
            // Collect top-level declarations
            collectDeclarationsInScope(ancestors[i], result, scopeLevel, vec, cancel);
        }
    }
}

std::vector<CSTScopeSymbolInfo>* CSTSearcher::findScopeSymbols(
    const AstParseResult* result,
    uint32_t line, uint32_t col,
    CSTCancelCheck& cancel) {

    if (!result) {
        return nullptr;
    }

    std::vector<TSNode> ancestors = findNodeAndParents(result, line, col, cancel);
    if (ancestors.empty()) {
        return nullptr;
    }

    std::unique_ptr<std::vector<CSTScopeSymbolInfo>> vec(
        new std::vector<CSTScopeSymbolInfo>);
    collectScopeSymbolsFromAncestors(ancestors, result, line, col, vec.get(), cancel);

    if (vec->empty() || cancel.failed()) {
        return nullptr;
    }
    return vec.release();
}

// --------------------------------------------------------------------------
// Detailed symbol helpers
// --------------------------------------------------------------------------

std::string CSTSearcher::extractAccessModifier(TSNode node, TSNode parent, const AstParseResult* result,
        CSTCancelCheck& cancel) {
    std::string access;
    bool found = false;
    // Check inline modifiers on the node itself
    // Grammar: modifiers → modifier → access_modifier
    cst_for_each_child(node, false, cancel, [&](TSNode child) {
        const char* childType = ts_node_type(child);
        if (strcmp(childType, "modifiers") == 0) {
            // Walk all children of modifiers (including anonymous)
            cst_for_each_child(child, false, cancel, [&](TSNode mod) {
                const char* modType = ts_node_type(mod);
                if (strcmp(modType, "access_modifier") == 0) {
                    access = result->getNodeText(mod);
                    found = true;
                    return false;
                }
                // modifier wraps access_modifier — check its children
                if (strcmp(modType, "modifier") == 0) {
                    TSNode inner = cst_find_named_child(mod, "access_modifier", cancel);
                    if (!ts_node_is_null(inner)) {
                        access = result->getNodeText(inner);
                        found = true;
                        return false;
                    }
                }
                return true;
            });
            if (found) {
                return false;
            }
        }
        if (strcmp(childType, "access_modifier") == 0) {
            access = result->getNodeText(child);
            found = true;
            return false;
        }
        return !cancel.failed();
    });
    if (found || cancel.failed()) {
        return access;
    }

    // Check parent member_group for its access modifier
    if (!ts_node_is_null(parent) && strcmp(ts_node_type(parent), "member_group") == 0) {
        TSNode groupAccess = cst_find_named_child(parent, "access_modifier", cancel);
        if (!ts_node_is_null(groupAccess)) {
            access = result->getNodeText(groupAccess);
        }
    }

    return access;
}

bool CSTSearcher::hasStaticModifier(TSNode node, const AstParseResult* result, CSTCancelCheck& cancel) {
    bool found = false;
    cst_for_each_child(node, false, cancel, [&](TSNode child) {
        const char* childType = ts_node_type(child);
        if (strcmp(childType, "modifiers") == 0) {
            // modifiers → modifier → "static" (anonymous leaf)
            // Check all children (including anonymous) of modifiers
            cst_for_each_child(child, false, cancel, [&](TSNode mod) {
                const char* modType = ts_node_type(mod);
                // modifier wraps the keyword — check its text
                found = strcmp(modType, "static") == 0
                    || (strcmp(modType, "modifier") == 0 && result->getNodeText(mod) == "static");
                return !found;
            });
        }
        if (strcmp(childType, "static") == 0) {
            found = true;
        }
        return !found;
    });
    return found && !cancel.failed();
}

bool CSTSearcher::hasConstMethodQualifier(TSNode node, const AstParseResult* result, CSTCancelCheck& cancel) {
    TSNode qualifier = cst_find_named_child(node, "method_qualifier", cancel);
    return !ts_node_is_null(qualifier) && result->getNodeText(qualifier) == "const";
}

std::vector<CSTParamInfo> CSTSearcher::extractParameters(TSNode funcNode,
                                                          const AstParseResult* result,
                                                          CSTCancelCheck& cancel) {
    std::vector<CSTParamInfo> params;

    // Find parameter_list child by type
    TSNode paramList = cst_find_named_child(funcNode, "parameter_list", cancel);
    if (ts_node_is_null(paramList)) {
        return params;
    }

    cst_for_each_child(paramList, true, cancel, [&](TSNode param) {
        if (strcmp(ts_node_type(param), "parameter") == 0) {
            CSTParamInfo pi;
            pi.name = getFieldText(param, "name", result);
            pi.typeName = getFieldText(param, "type", result);
            pi.defaultVal = getFieldText(param, "default", result);
            params.push_back(std::move(pi));
        }
        return true;
    });
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

std::string CSTSearcher::extractTypeName(TSNode node, TSNode parent, const AstParseResult* result) {
    const char* type = ts_node_type(node);

    // For variable_declarator, the type is on the parent local/global_variable_declaration
    if (strcmp(type, "variable_declarator") == 0) {
        if (!ts_node_is_null(parent)) {
            return getFieldText(parent, "type", result);
        }
        return std::string();
    }

    // For parameter, member_declaration, hashdecl_member — 'type' field
    return getFieldText(node, "type", result);
}

bool CSTSearcher::fillDeclarationDetail(CSTSymbolDetail& detail, TSNode node, TSNode parent,
        const AstParseResult* result, CSTCancelCheck& cancel) {
    const char* type = ts_node_type(node);
    ASTSymbolKind kind = nodeTypeToSymbolKind(type);

    if (kind == ASYK_Function || kind == ASYK_Method || kind == ASYK_Constructor) {
        detail.returnType = extractReturnType(node, result);
        detail.params = extractParameters(node, result, cancel);
        detail.access = extractAccessModifier(node, parent, result, cancel);
        detail.isStatic = hasStaticModifier(node, result, cancel);
        detail.isConstMethod = hasConstMethodQualifier(node, result, cancel);
        return true;
    }

    if (strcmp(type, "member_declaration") == 0) {
        detail.typeName = extractTypeName(node, parent, result);
        detail.access = extractAccessModifier(node, parent, result, cancel);
        detail.isStatic = hasStaticModifier(node, result, cancel);
        return true;
    }

    if (strcmp(type, "variable_declarator") == 0 || strcmp(type, "hashdecl_member") == 0) {
        detail.typeName = extractTypeName(node, parent, result);
        return true;
    }

    if (strcmp(type, "local_variable_declaration") == 0 ||
        strcmp(type, "global_variable_declaration") == 0 ||
        strcmp(type, "parameter") == 0) {
        detail.typeName = getFieldText(node, "type", result);
        return true;
    }

    if (kind == ASYK_Class || kind == ASYK_Namespace || kind == ASYK_Interface ||
        kind == ASYK_Constant || kind == ASYK_TypeAlias) {
        detail.access = extractAccessModifier(node, parent, result, cancel);
        return true;
    }

    return false;
}

CSTSymbolDetail CSTSearcher::enrichSymbol(const CSTScopeSymbolInfo& ssi,
                                           const AstParseResult* result,
                                           CSTCancelCheck& cancel) {
    CSTSymbolDetail detail;
    detail.symbol = ssi.symbol;
    detail.scopeLevel = ssi.scopeLevel;

    // The details are those of the nearest declaration that contains the start of the symbol, which is the
    // declaration of the symbol itself if it has details
    if (!ts_node_is_null(ssi.node) && fillDeclarationDetail(detail, ssi.node, ssi.parent, result, cancel)) {
        return detail;
    }

    // Find the declaration node at the symbol's start position
    std::vector<TSNode> ancestors = findNodeAndParents(
        result, ssi.symbol.startLine, ssi.symbol.startCol, cancel);

    // Find the nearest declaration/named node
    for (size_t i = 0; i < ancestors.size(); ++i) {
        if (cancel()) {
            break;
        }
        TSNode parent = i + 1 < ancestors.size() ? ancestors[i + 1] : TSNode{};
        if (fillDeclarationDetail(detail, ancestors[i], parent, result, cancel)) {
            break;
        }
    }

    return detail;
}

std::vector<CSTSymbolDetail>* CSTSearcher::findScopeSymbolsDetailed(
    const AstParseResult* result,
    uint32_t line, uint32_t col,
    CSTCancelCheck& cancel) {

    if (!result) {
        return nullptr;
    }

    // Get basic scope symbols first
    std::unique_ptr<std::vector<CSTScopeSymbolInfo>> basic(
        findScopeSymbols(result, line, col, cancel));
    if (!basic) {
        return nullptr;
    }

    std::unique_ptr<std::vector<CSTSymbolDetail>> vec(
        new std::vector<CSTSymbolDetail>);
    vec->reserve(basic->size());

    for (const auto& ssi : *basic) {
        if (cancel()) {
            return nullptr;
        }
        vec->push_back(enrichSymbol(ssi, result, cancel));
    }

    if (vec->empty() || cancel.failed()) {
        return nullptr;
    }
    return vec.release();
}

// --------------------------------------------------------------------------
// Type resolution and member listing
// --------------------------------------------------------------------------

std::string CSTSearcher::getSymbolType(
    const AstParseResult* result,
    uint32_t line, uint32_t col,
    CSTCancelCheck& cancel) {

    if (!result) {
        return std::string();
    }

    std::vector<TSNode> ancestors = findNodeAndParents(result, line, col, cancel);
    if (ancestors.empty()) {
        return std::string();
    }

    // Resolve symbol use sites through the lexical scope before considering
    // enclosing declarations.  For example, the `obj` in
    // `int value = obj.getValue()` is nested under the declaration of
    // `value`; the enclosing declaration's type must not mask `obj`'s type.
    CSTSymbolInfo symbolInfo = findSymbolInfo(result, line, col, cancel);
    if (!symbolInfo.name.empty()) {
        std::unique_ptr<std::vector<CSTScopeSymbolInfo>> symbols(
            findScopeSymbols(result, line, col, cancel));
        if (symbols) {
            for (const auto& symbol : *symbols) {
                if (cancel()) {
                    return std::string();
                }
                if (symbol.symbol.name == symbolInfo.name) {
                    // Enrich only the matching symbol instead of every symbol
                    // in scope; large scopes otherwise make a single type
                    // lookup unnecessarily expensive.
                    CSTSymbolDetail detail = enrichSymbol(symbol, result, cancel);
                    if (!detail.typeName.empty()) {
                        return detail.typeName;
                    }
                }
            }
        }
    }
    if (cancel.failed()) {
        return std::string();
    }

    // Check the leaf node and its ancestors
    for (size_t i = 0; i < ancestors.size(); i++) {
        if (cancel()) {
            return std::string();
        }
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
            std::string typeName = extractTypeName(ancestors[i],
                i + 1 < ancestors.size() ? ancestors[i + 1] : TSNode{}, result);
            if (!typeName.empty()) {
                return typeName;
            }
            // A bare identifier expression can be represented by a
            // variable_declarator during error recovery.  Continue inspecting
            // the remaining ancestors when the node has no declaration type.
            continue;
        }

        // Check local/global variable declaration
        if (strcmp(type, "local_variable_declaration") == 0 ||
            strcmp(type, "global_variable_declaration") == 0) {
            std::string typeName = getFieldText(ancestors[i], "type", result);
            if (!typeName.empty()) {
                return typeName;
            }
            continue;
        }

        // Check parameter
        if (strcmp(type, "parameter") == 0) {
            std::string typeName = getFieldText(ancestors[i], "type", result);
            if (!typeName.empty()) {
                return typeName;
            }
            continue;
        }

        // Check member_declaration
        if (strcmp(type, "member_declaration") == 0) {
            std::string typeName = getFieldText(ancestors[i], "type", result);
            if (!typeName.empty()) {
                return typeName;
            }
            continue;
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
                                          TSNode* outNode, CSTCancelCheck& cancel) {
    bool found = false;
    cst_walk(root, cancel, [&](const TSTreeCursor*, TSNode node, uint32_t depth) {
        if (!depth) {
            return CSTWalkAction::Descend;
        }
        if (!ts_node_is_named(node)) {
            return CSTWalkAction::Skip;
        }

        const char* type = ts_node_type(node);
        if ((nodeType == nullptr || strcmp(type, nodeType) == 0) && getNodeName(node, result) == name) {
            *outNode = node;
            found = true;
            return CSTWalkAction::Stop;
        }

        // Search containers
        if (strcmp(type, "namespace_declaration") == 0 ||
            strcmp(type, "class_declaration") == 0 ||
            strcmp(type, "source_file") == 0 ||
            strcmp(type, "member_group") == 0) {
            return CSTWalkAction::Descend;
        }
        return CSTWalkAction::Skip;
    });
    return found;
}

//! Returns the names of the parent classes of a class declaration in declaration order
static std::vector<std::string> getParentClassNames(TSNode classNode, const AstParseResult* result,
        CSTCancelCheck& cancel) {
    std::vector<std::string> names;
    cst_for_each_child(classNode, true, cancel, [&](TSNode child) {
        if (strcmp(ts_node_type(child), "superclass_list") != 0) {
            return true;
        }

        return cst_for_each_child(child, true, cancel, [&](TSNode superclass) {
            if (strcmp(ts_node_type(superclass), "superclass") == 0) {
                // the parent class name is the first identifier or scoped_identifier
                cst_for_each_child(superclass, true, cancel, [&](TSNode sc) {
                    const char* scType = ts_node_type(sc);
                    if (strcmp(scType, "identifier") == 0 || strcmp(scType, "scoped_identifier") == 0) {
                        names.push_back(result->getNodeText(sc));
                        return false;
                    }
                    return true;
                });
            }
            return !cancel.failed();
        });
    });
    return names;
}

//! Returns true if the class has already been visited and otherwise records it
/** A class without a name is never recorded.
*/
static bool checkVisitedClass(TSNode classNode, const AstParseResult* result, std::vector<std::string>& visited) {
    std::string className = CSTSearcher::getNodeName(classNode, result);
    if (className.empty()) {
        return false;
    }
    if (std::find(visited.begin(), visited.end(), className) != visited.end()) {
        return true;
    }
    visited.push_back(className);
    return false;
}

void CSTSearcher::addOwnClassMembers(TSNode classNode, const AstParseResult* result,
                                      std::vector<CSTSymbolDetail>* vec, CSTCancelCheck& cancel) {
    // Iterate all children
    CSTDocComments docComments;
    cst_for_each_child(classNode, false, cancel, [&](TSNode child) {
        if (docComments.add(child) || !ts_node_is_named(child)) {
            return true;
        }
        const char* type = ts_node_type(child);

        if (strcmp(type, "member_group") == 0) {
            // Get access modifier from the group
            std::string groupAccess;
            TSNode groupModifier = cst_find_named_child(child, "access_modifier", cancel);
            if (!ts_node_is_null(groupModifier)) {
                groupAccess = result->getNodeText(groupModifier);
            }

            CSTDocComments memberDocComments;
            cst_for_each_child(child, false, cancel, [&](TSNode member) {
                if (memberDocComments.add(member) || !ts_node_is_named(member)) {
                    return true;
                }
                const char* mtype = ts_node_type(member);
                ASTSymbolKind kind = nodeTypeToSymbolKind(mtype);

                if (kind != ASYK_None) {
                    CSTSymbolDetail detail;
                    detail.symbol = make_symbol_info(member, kind, getNodeName(member, result));
                    detail.symbol.docComment = memberDocComments.get(result);
                    detail.access = groupAccess;
                    detail.isStatic = hasStaticModifier(member, result, cancel);

                    if (kind == ASYK_Method || kind == ASYK_Function || kind == ASYK_Constructor) {
                        detail.returnType = extractReturnType(member, result);
                        detail.params = extractParameters(member, result, cancel);
                        detail.isConstMethod = hasConstMethodQualifier(member, result, cancel);
                    } else if (kind == ASYK_Field) {
                        detail.typeName = extractTypeName(member, child, result);
                    }

                    vec->push_back(std::move(detail));
                }
                return !cancel.failed();
            });
        }

        // Direct children (method_declaration, constructor_declaration, etc.)
        ASTSymbolKind kind = nodeTypeToSymbolKind(type);
        if (kind == ASYK_Method || kind == ASYK_Function || kind == ASYK_Constructor) {
            CSTSymbolDetail detail;
            detail.symbol = make_symbol_info(child, kind, getNodeName(child, result));
            detail.symbol.docComment = docComments.get(result);
            detail.access = extractAccessModifier(child, classNode, result, cancel);
            detail.isStatic = hasStaticModifier(child, result, cancel);
            detail.returnType = extractReturnType(child, result);
            detail.params = extractParameters(child, result, cancel);
            detail.isConstMethod = hasConstMethodQualifier(child, result, cancel);
            vec->push_back(std::move(detail));
        } else if (strcmp(type, "constant_declaration") == 0) {
            CSTSymbolDetail detail;
            detail.symbol = make_symbol_info(child, ASYK_Constant, getNodeName(child, result));
            detail.symbol.docComment = docComments.get(result);
            detail.access = extractAccessModifier(child, classNode, result, cancel);
            vec->push_back(std::move(detail));
        }

        return !cancel.failed();
    });
}

void CSTSearcher::collectClassMembers(TSNode classNode, const AstParseResult* result,
                                       std::vector<CSTSymbolDetail>* vec,
                                       bool includeInherited,
                                       CSTCancelCheck& cancel) {
    // Prevent infinite loops in inheritance; the name of each class is recorded before its parents are visited
    std::vector<std::string> visited;
    auto enterClass = [&](TSNode cls) -> bool {
        std::string className = getNodeName(cls, result);
        if (std::find(visited.begin(), visited.end(), className) != visited.end()) {
            return false;
        }
        visited.push_back(std::move(className));
        return true;
    };
    enterClass(classNode);

    // The members of the parent classes, depth-first in declaration order, precede the members of each class,
    // whose superclass_list precedes its body; the stack holds each class with the parent classes still to visit
    struct ClassFrame {
        TSNode cls;
        std::vector<std::string> parents;
        size_t nextParent;
    };
    std::vector<ClassFrame> stack;
    stack.push_back({classNode, includeInherited ? getParentClassNames(classNode, result, cancel)
        : std::vector<std::string>(), 0});
    TSNode rootNode = ts_tree_root_node(result->getTree());
    while (!stack.empty()) {
        if (cancel()) {
            return;
        }
        ClassFrame& frame = stack.back();
        if (frame.nextParent < frame.parents.size()) {
            // Find parent class in tree and collect its members
            std::string parentName = frame.parents[frame.nextParent++];
            TSNode parentClass = rootNode;  // initialized; overwritten by findDeclarationByName
            if (findDeclarationByName(rootNode, result, parentName, "class_declaration", &parentClass, cancel)
                && enterClass(parentClass)) {
                stack.push_back({parentClass, getParentClassNames(parentClass, result, cancel), 0});
            }
            continue;
        }
        addOwnClassMembers(frame.cls, result, vec, cancel);
        stack.pop_back();
    }
}

void CSTSearcher::collectNamespaceMembers(TSNode nsNode, const AstParseResult* result,
                                           std::vector<CSTSymbolDetail>* vec, CSTCancelCheck& cancel) {
    CSTDocComments docComments;
    cst_for_each_child(nsNode, false, cancel, [&](TSNode child) {
        if (docComments.add(child) || !ts_node_is_named(child)) {
            return true;
        }
        ASTSymbolKind kind = nodeTypeToSymbolKind(ts_node_type(child));

        if (kind != ASYK_None) {
            CSTSymbolDetail detail;
            detail.symbol = make_symbol_info(child, kind, getNodeName(child, result));
            detail.symbol.docComment = docComments.get(result);
            detail.access = extractAccessModifier(child, nsNode, result, cancel);

            if (kind == ASYK_Function) {
                detail.returnType = extractReturnType(child, result);
                detail.params = extractParameters(child, result, cancel);
                detail.isConstMethod = hasConstMethodQualifier(child, result, cancel);
            }

            vec->push_back(std::move(detail));
        }
        return !cancel.failed();
    });
}

void CSTSearcher::collectHashdeclMembers(TSNode hdNode, const AstParseResult* result,
                                          std::vector<CSTSymbolDetail>* vec, CSTCancelCheck& cancel) {
    CSTDocComments docComments;
    cst_for_each_child(hdNode, false, cancel, [&](TSNode child) {
        if (docComments.add(child) || strcmp(ts_node_type(child), "hashdecl_member") != 0) {
            return true;
        }
        CSTSymbolDetail detail;
        detail.symbol = make_symbol_info(child, ASYK_Field, getFieldText(child, "name", result));
        detail.symbol.docComment = docComments.get(result);
        detail.typeName = getFieldText(child, "type", result);
        vec->push_back(std::move(detail));
        return true;
    });
}

void CSTSearcher::collectEnumMembers(TSNode enumNode, const AstParseResult* result,
                                      std::vector<CSTSymbolDetail>* vec, CSTCancelCheck& cancel) {
    CSTDocComments docComments;
    cst_for_each_child(enumNode, false, cancel, [&](TSNode child) {
        if (docComments.add(child) || strcmp(ts_node_type(child), "enum_member") != 0) {
            return true;
        }
        CSTSymbolDetail detail;
        detail.symbol = make_symbol_info(child, ASYK_Constant, getFieldText(child, "name", result));
        detail.symbol.docComment = docComments.get(result);
        // Store enum value if present
        std::string val = getFieldText(child, "value", result);
        if (!val.empty()) {
            detail.typeName = val;
        }
        vec->push_back(std::move(detail));
        return true;
    });
}

std::vector<CSTSymbolDetail>* CSTSearcher::findTypeMembers(
    const AstParseResult* result,
    const std::string& typeName,
    CSTCancelCheck& cancel,
    bool includeInherited) {

    if (!result || typeName.empty()) {
        return nullptr;
    }

    TSNode root = ts_tree_root_node(result->getTree());
    std::unique_ptr<std::vector<CSTSymbolDetail>> vec(new std::vector<CSTSymbolDetail>);

    TSNode found = root;  // initialized; overwritten by findDeclarationByName
    if (findDeclarationByName(root, result, typeName, "class_declaration", &found, cancel)) {
        collectClassMembers(found, result, vec.get(), includeInherited, cancel);
    } else if (findDeclarationByName(root, result, typeName, "namespace_declaration", &found, cancel)) {
        collectNamespaceMembers(found, result, vec.get(), cancel);
    } else if (findDeclarationByName(root, result, typeName, "hashdecl_declaration", &found, cancel)) {
        collectHashdeclMembers(found, result, vec.get(), cancel);
    } else if (findDeclarationByName(root, result, typeName, "enum_declaration", &found, cancel)) {
        collectEnumMembers(found, result, vec.get(), cancel);
    }

    if (vec->empty() || cancel.failed()) {
        return nullptr;
    }
    return vec.release();
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

//! Returns true if a node declares a class member that definitions resolve to
static bool isClassMemberDeclaration(const char* type) {
    return strcmp(type, "method_declaration") == 0 ||
        strcmp(type, "constructor_declaration") == 0 ||
        strcmp(type, "destructor_declaration") == 0 ||
        strcmp(type, "member_declaration") == 0 ||
        strcmp(type, "constant_declaration") == 0;
}

//! Search direct members of a class body (no inheritance walk).
bool CSTSearcher::findMemberInClassBody(TSNode classNode, const AstParseResult* result,
                                         const std::string& name, TSNode* outNode, CSTCancelCheck& cancel) {
    bool found = false;
    // returns false when the member is found
    auto check = [&](TSNode member) {
        if (isClassMemberDeclaration(ts_node_type(member)) && CSTSearcher::getNodeName(member, result) == name) {
            *outNode = member;
            found = true;
            return false;
        }
        return true;
    };
    cst_for_each_child(classNode, true, cancel, [&](TSNode child) {
        // Direct children: method, constructor, destructor, member, constant
        if (!check(child)) {
            return false;
        }
        // member_group: walk its children
        if (strcmp(ts_node_type(child), "member_group") == 0) {
            cst_for_each_child(child, true, cancel, check);
        }
        return !found && !cancel.failed();
    });
    return found;
}

//! Search a class body for a member matching a name, walking the inheritance chain.
bool CSTSearcher::findMemberInClass(TSNode classNode, const AstParseResult* result,
                                     const std::string& name, TSNode* outNode, CSTCancelCheck& cancel) {
    std::vector<std::string> visited;
    // the classes whose parent classes are still to be searched, depth-first in declaration order
    struct ClassFrame {
        std::vector<std::string> parents;
        size_t nextParent;
    };
    std::vector<ClassFrame> stack;
    TSNode root = ts_tree_root_node(result->getTree());
    TSNode cls = classNode;
    while (true) {
        // Cycle detection
        if (!checkVisitedClass(cls, result, visited)) {
            // Search direct members
            if (findMemberInClassBody(cls, result, name, outNode, cancel)) {
                return true;
            }
            stack.push_back({getParentClassNames(cls, result, cancel), 0});
        }

        // Find the next parent class to search
        bool next = false;
        while (!next && !stack.empty()) {
            if (cancel()) {
                return false;
            }
            ClassFrame& frame = stack.back();
            if (frame.nextParent == frame.parents.size()) {
                stack.pop_back();
                continue;
            }
            const std::string& parentName = frame.parents[frame.nextParent++];
            TSNode parentClass = root;
            if (CSTSearcher::findDeclarationByName(root, result, parentName, "class_declaration", &parentClass,
                    cancel)) {
                cls = parentClass;
                next = true;
            }
        }
        if (!next) {
            return false;
        }
    }
}

//! Returns the name node of a variable_declarator or parameter, or the node itself if it has no name node
static TSNode getNameNode(TSNode node) {
    TSNode nameNode = ts_node_child_by_field_name(node, "name", 4);
    return ts_node_is_null(nameNode) ? node : nameNode;
}

//! Finds a variable declared by the variable_declarator children of a local_variable_declaration
/** @return true if the variable was found or the search was cancelled
*/
static bool findDeclarator(TSNode decl, const AstParseResult* result, const std::string& name, TSNode* outNode,
        bool& found, CSTCancelCheck& cancel) {
    return !cst_for_each_child(decl, true, cancel, [&](TSNode declChild) {
        if (strcmp(ts_node_type(declChild), "variable_declarator") == 0) {
            TSNode nameNode = ts_node_child_by_field_name(declChild, "name", 4);
            if (!ts_node_is_null(nameNode) && result->getNodeText(nameNode) == name) {
                *outNode = nameNode;
                found = true;
                return false;
            }
        }
        return true;
    });
}

bool CSTSearcher::findLocalDeclaration(
    const std::vector<TSNode>& ancestors,
    const AstParseResult* result,
    const std::string& name,
    uint32_t line, uint32_t col,
    TSNode* outNode,
    CSTCancelCheck& cancel) {

    bool found = false;
    for (size_t i = 0; i < ancestors.size(); i++) {
        if (cancel()) {
            return false;
        }
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
            cst_for_each_child(ancestors[i], true, cancel, [&](TSNode child) {
                TSPoint childStart = ts_node_start_point(child);
                if (childStart.row > line ||
                    (childStart.row == line && childStart.column > col)) {
                    return false;
                }

                const char* childType = ts_node_type(child);

                // Check local_variable_declaration directly
                if (strcmp(childType, "local_variable_declaration") == 0
                    && findDeclarator(child, result, name, outNode, found, cancel)) {
                    return false;
                }

                // Also check expression_statement containing local_variable_declaration
                if (strcmp(childType, "expression_statement") == 0) {
                    return cst_for_each_child(child, true, cancel, [&](TSNode exprChild) {
                        return strcmp(ts_node_type(exprChild), "local_variable_declaration") != 0
                            || !findDeclarator(exprChild, result, name, outNode, found, cancel);
                    });
                }
                return true;
            });
            if (found || cancel.failed()) {
                return found;
            }

            // If this block's parent is function-like, also search parameters
            if (i + 1 < ancestors.size()) {
                const char* parentType = ts_node_type(ancestors[i + 1]);
                if (isFunctionLike(parentType)) {
                    TSNode params = cst_find_named_child(ancestors[i + 1], "parameter_list", cancel);
                    if (!ts_node_is_null(params)) {
                        cst_for_each_child(params, true, cancel, [&](TSNode param) {
                            if (strcmp(ts_node_type(param), "parameter") == 0
                                && getFieldText(param, "name", result) == name) {
                                *outNode = getNameNode(param);
                                found = true;
                                return false;
                            }
                            return true;
                        });
                        if (found || cancel.failed()) {
                            return found;
                        }
                    }
                }
            }
        }
    }

    return false;
}

//! Finds a variable declared at the top level of a document
static bool findTopLevelVariable(TSNode root, const AstParseResult* result, const std::string& name,
        TSNode* outNode, CSTCancelCheck& cancel) {
    bool found = false;
    cst_for_each_child(root, true, cancel, [&](TSNode child) {
        return strcmp(ts_node_type(child), "local_variable_declaration") != 0
            || !findDeclarator(child, result, name, outNode, found, cancel);
    });
    return found;
}

std::vector<TSNode>* CSTSearcher::resolveDefinition(
    const AstParseResult* result,
    uint32_t line, uint32_t col,
    CSTCancelCheck& cancel) {

    if (!result) {
        return nullptr;
    }

    // 1. Get symbol info at position
    CSTSymbolInfo si = findSymbolInfo(result, line, col, cancel);

    // 2. Get ancestors for context analysis
    std::vector<TSNode> ancestors = findNodeAndParents(result, line, col, cancel);
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
    // returns the definition that was found, unless the search was cancelled
    auto found = [&](TSNode node) -> std::vector<TSNode>* {
        if (cancel.failed()) {
            return nullptr;
        }
        std::unique_ptr<std::vector<TSNode>> rv(new std::vector<TSNode>);
        rv->push_back(node);
        return rv.release();
    };

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
                    std::string typeName = getSymbolType(result, objPos.row, objPos.column, cancel);
                    if (!typeName.empty()) {
                        // Find the class declaration and search for the member
                        TSNode classNode = make_null_node();
                        if (findDeclarationByName(root, result, typeName,
                                                   "class_declaration", &classNode, cancel)) {
                            TSNode memberNode = make_null_node();
                            if (findMemberInClass(classNode, result, si.name, &memberNode, cancel)) {
                                return found(memberNode);
                            }
                        }
                    }
                }
            }
        }
    }

    // 5. SCOPE ACCESS: Check if we're inside a scoped_identifier
    for (size_t i = 1; i < ancestors.size(); i++) {
        if (cancel()) {
            return nullptr;
        }
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
                                           "class_declaration", &scopeNode, cancel)) {
                    TSNode memberNode = make_null_node();
                    if (findMemberInClass(scopeNode, result, si.name, &memberNode, cancel)) {
                        return found(memberNode);
                    }
                }
                // Try to find as a namespace
                if (findDeclarationByName(root, result, scopeName,
                                           "namespace_declaration", &scopeNode, cancel)) {
                    // Search namespace body for the declaration
                    if (findDeclarationByName(scopeNode, result, si.name,
                                               nullptr, &foundNode, cancel)) {
                        return found(foundNode);
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
    if (findLocalDeclaration(ancestors, result, si.name, line, col, &foundNode, cancel)) {
        return found(foundNode);
    }

    // 6.5. CLASS MEMBER (bare identifier inside a method)
    // When inside a method body, check the enclosing class for a matching member
    for (size_t i = 0; i < ancestors.size(); i++) {
        if (cancel()) {
            return nullptr;
        }
        const char* atype = ts_node_type(ancestors[i]);
        if (strcmp(atype, "class_declaration") == 0) {
            if (findMemberInClass(ancestors[i], result, si.name, &foundNode, cancel)) {
                return found(foundNode);
            }
            break;
        }
    }

    // 7. TOP-LEVEL DECLARATION (named declarations like class, function, etc.)
    if (findDeclarationByName(root, result, si.name, nullptr, &foundNode, cancel)) {
        return found(foundNode);
    }

    // 7.5. TOP-LEVEL VARIABLE DECLARATION (local_variable_declaration → variable_declarator)
    // findDeclarationByName doesn't find these because getNodeName returns empty for
    // local_variable_declaration (name is in variable_declarator child)
    if (findTopLevelVariable(root, result, si.name, &foundNode, cancel)) {
        return found(foundNode);
    }

    // 8. Not found in this document
    return nullptr;
}

// --------------------------------------------------------------------------
// Cross-document definition resolution
// --------------------------------------------------------------------------

bool CSTSearcher::findDeclarationByNameCrossDoc(
    TSNode root, const AstParseResult* result,
    const std::string& name, const char* nodeType,
    TSNode* outNode, const AstParseResult** outResult,
    const std::vector<DocumentRef>& otherDocs,
    CSTCancelCheck& cancel) {

    // Try current document first
    if (findDeclarationByName(root, result, name, nodeType, outNode, cancel)) {
        *outResult = result;
        return true;
    }

    // Search other documents
    for (const auto& doc : otherDocs) {
        if (cancel()) {
            return false;
        }
        if (!doc.result) {
            continue;
        }
        TSNode otherRoot = doc.result->getRootNode();
        if (findDeclarationByName(otherRoot, doc.result, name, nodeType, outNode, cancel)) {
            *outResult = doc.result;
            return true;
        }
    }

    return false;
}

bool CSTSearcher::findMemberInClassCrossDoc(
    TSNode classNode, const AstParseResult* result,
    const std::string& name, TSNode* outNode,
    const AstParseResult** outResult,
    const std::vector<DocumentRef>& otherDocs,
    CSTCancelCheck& cancel) {

    std::vector<std::string> visited;
    // the classes whose parent classes are still to be searched, depth-first in declaration order; parent classes
    // are looked up from the document of the class that names them
    struct ClassFrame {
        const AstParseResult* result;
        std::vector<std::string> parents;
        size_t nextParent;
    };
    std::vector<ClassFrame> stack;
    TSNode cls = classNode;
    const AstParseResult* clsResult = result;
    while (true) {
        // Cycle detection
        if (!checkVisitedClass(cls, clsResult, visited)) {
            // Search direct members using shared helper
            if (findMemberInClassBody(cls, clsResult, name, outNode, cancel)) {
                *outResult = clsResult;
                return true;
            }
            stack.push_back({clsResult, getParentClassNames(cls, clsResult, cancel), 0});
        }

        // Find the next parent class to search across all documents
        bool next = false;
        while (!next && !stack.empty()) {
            if (cancel()) {
                return false;
            }
            ClassFrame& frame = stack.back();
            if (frame.nextParent == frame.parents.size()) {
                stack.pop_back();
                continue;
            }
            const std::string& parentName = frame.parents[frame.nextParent++];
            TSNode parentClass = make_null_node();
            const AstParseResult* parentResult = nullptr;
            if (findDeclarationByNameCrossDoc(frame.result->getRootNode(), frame.result, parentName,
                    "class_declaration", &parentClass, &parentResult, otherDocs, cancel)) {
                cls = parentClass;
                clsResult = parentResult;
                next = true;
            }
        }
        if (!next) {
            return false;
        }
    }
}

//! Look up the URI for a found result — empty string means current document.
static std::string lookupUri(const AstParseResult* foundResult,
                             const AstParseResult* currentResult,
                             const std::vector<DocumentRef>& otherDocs) {
    if (foundResult == currentResult) {
        return "";
    }
    for (const auto& doc : otherDocs) {
        if (doc.result == foundResult) {
            return doc.uri;
        }
    }
    return "";
}

std::vector<DefinitionResult>* CSTSearcher::resolveDefinitionCrossDoc(
    const AstParseResult* result,
    uint32_t line, uint32_t col,
    const std::vector<DocumentRef>& otherDocs,
    CSTCancelCheck& cancel) {

    if (!result) {
        return nullptr;
    }

    // 1. Get symbol info at position
    CSTSymbolInfo si = findSymbolInfo(result, line, col, cancel);

    // 2. Get ancestors for context analysis
    std::vector<TSNode> ancestors = findNodeAndParents(result, line, col, cancel);
    if (ancestors.empty()) {
        return nullptr;
    }

    // Fall back to leaf node text if findSymbolInfo didn't find a name
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
    const AstParseResult* foundResult = nullptr;
    // returns the definition that was found, unless the search was cancelled
    auto found = [&](TSNode node, const AstParseResult* nodeResult) -> std::vector<DefinitionResult>* {
        if (cancel.failed()) {
            return nullptr;
        }
        std::unique_ptr<std::vector<DefinitionResult>> rv(new std::vector<DefinitionResult>);
        rv->push_back({node, lookupUri(nodeResult, result, otherDocs)});
        return rv.release();
    };

    // 4. MEMBER ACCESS: Check if parent is member_expression
    if (ancestors.size() >= 2) {
        TSNode parentNode = ancestors[1];
        const char* parentType = ts_node_type(parentNode);
        if (strcmp(parentType, "member_expression") == 0) {
            TSNode memberField = ts_node_child_by_field_name(parentNode, "member", 6);
            if (!ts_node_is_null(memberField) && ts_node_eq(memberField, ancestors[0])) {
                TSNode objNode = ts_node_child_by_field_name(parentNode, "object", 6);
                if (!ts_node_is_null(objNode)) {
                    TSPoint objPos = ts_node_start_point(objNode);
                    std::string typeName = getSymbolType(result, objPos.row, objPos.column, cancel);
                    if (!typeName.empty()) {
                        TSNode classNode = make_null_node();
                        const AstParseResult* classResult = nullptr;
                        if (findDeclarationByNameCrossDoc(root, result, typeName,
                                "class_declaration", &classNode, &classResult, otherDocs, cancel)) {
                            if (findMemberInClassCrossDoc(classNode, classResult,
                                    si.name, &foundNode, &foundResult, otherDocs, cancel)) {
                                return found(foundNode, foundResult);
                            }
                        }
                    }
                }
            }
        }
    }

    // 5. SCOPE ACCESS: Check if we're inside a scoped_identifier
    for (size_t i = 1; i < ancestors.size(); i++) {
        if (cancel()) {
            return nullptr;
        }
        const char* ancType = ts_node_type(ancestors[i]);
        if (strcmp(ancType, "scoped_identifier") == 0) {
            std::string fullText = result->getNodeText(ancestors[i]);
            size_t lastColon = fullText.rfind("::");
            if (lastColon != std::string::npos) {
                std::string scopeName = fullText.substr(0, lastColon);
                // Try as a class
                TSNode scopeNode = make_null_node();
                const AstParseResult* scopeResult = nullptr;
                if (findDeclarationByNameCrossDoc(root, result, scopeName,
                        "class_declaration", &scopeNode, &scopeResult, otherDocs, cancel)) {
                    if (findMemberInClassCrossDoc(scopeNode, scopeResult,
                            si.name, &foundNode, &foundResult, otherDocs, cancel)) {
                        return found(foundNode, foundResult);
                    }
                }
                // Try as a namespace — search only within the namespace node
                if (findDeclarationByNameCrossDoc(root, result, scopeName,
                        "namespace_declaration", &scopeNode, &scopeResult, otherDocs, cancel)) {
                    if (findDeclarationByName(scopeNode, scopeResult,
                            si.name, nullptr, &foundNode, cancel)) {
                        return found(foundNode, scopeResult);
                    }
                }
            }
            break;
        }
        if (CSTSearcher::isDeclarationNode(ancType)) {
            break;
        }
    }

    // 6. LOCAL VARIABLE / PARAMETER (always local — no cross-doc needed)
    if (findLocalDeclaration(ancestors, result, si.name, line, col, &foundNode, cancel)) {
        return found(foundNode, result);
    }

    // 6.5. CLASS MEMBER (bare identifier inside a method)
    for (size_t i = 0; i < ancestors.size(); i++) {
        if (cancel()) {
            return nullptr;
        }
        const char* atype = ts_node_type(ancestors[i]);
        if (strcmp(atype, "class_declaration") == 0) {
            if (findMemberInClassCrossDoc(ancestors[i], result,
                    si.name, &foundNode, &foundResult, otherDocs, cancel)) {
                return found(foundNode, foundResult);
            }
            break;
        }
    }

    // 7. TOP-LEVEL DECLARATION (cross-doc aware)
    {
        const AstParseResult* topResult = nullptr;
        if (findDeclarationByNameCrossDoc(root, result, si.name, nullptr,
                &foundNode, &topResult, otherDocs, cancel)) {
            return found(foundNode, topResult);
        }
    }

    // 7.5. TOP-LEVEL VARIABLE DECLARATION (always local)
    if (findTopLevelVariable(root, result, si.name, &foundNode, cancel)) {
        return found(foundNode, result);
    }

    // 8. Not found
    return nullptr;
}

// --------------------------------------------------------------------------
// Hover info
// --------------------------------------------------------------------------

std::string CSTSearcher::buildClassSignature(TSNode node, const AstParseResult* result, CSTCancelCheck& cancel) {
    std::ostringstream ss;

    // Collect modifiers before "class" keyword
    cst_for_each_child(node, false, cancel, [&](TSNode child) {
        const char* childType = ts_node_type(child);
        if (strcmp(childType, "class") == 0) {
            return false; // Stop before "class" keyword
        }
        if (strcmp(childType, "access_modifier") == 0 ||
            strcmp(childType, "public") == 0 ||
            strcmp(childType, "private") == 0) {
            std::string text = result->getNodeText(child);
            ss << text << " ";
        }
        return true;
    });

    ss << "class " << getNodeName(node, result);

    // Superclasses
    TSNode inherits = ts_node_child_by_field_name(node, "superclasses", 12);
    if (!ts_node_is_null(inherits)) {
        ss << " inherits " << result->getNodeText(inherits);
    }

    return ss.str();
}

std::string CSTSearcher::buildFunctionSignature(TSNode node, const AstParseResult* result, CSTCancelCheck& cancel) {
    std::ostringstream ss;

    const char* nodeType = ts_node_type(node);

    // Collect modifiers
    cst_for_each_child(node, false, cancel, [&](TSNode child) {
        const char* childType = ts_node_type(child);
        // Stop at function name or keyword
        if (strcmp(childType, "identifier") == 0 ||
            strcmp(childType, "scoped_identifier") == 0 ||
            strcmp(childType, "constructor") == 0 ||
            strcmp(childType, "destructor") == 0 ||
            strcmp(childType, "sub") == 0 ||
            strcmp(childType, "parameter_list") == 0) {
            return false;
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
        return true;
    });

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
    TSNode params = cst_find_named_child(node, "parameter_list", cancel);
    if (!ts_node_is_null(params)) {
        ss << result->getNodeText(params);
    } else {
        ss << "()";
    }

    if (hasConstMethodQualifier(node, result, cancel)) {
        ss << " const";
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

std::string CSTSearcher::buildVariableSignature(TSNode node, const AstParseResult* result, CSTCancelCheck& cancel) {
    std::ostringstream ss;

    const char* nodeType = ts_node_type(node);
    if (strcmp(nodeType, "global_variable_declaration") == 0) {
        // Check for our/my/thread_local
        cst_for_each_child(node, false, cancel, [&](TSNode child) {
            const char* childType = ts_node_type(child);
            if (strcmp(childType, "our") == 0 || strcmp(childType, "my") == 0 ||
                strcmp(childType, "thread_local") == 0) {
                ss << result->getNodeText(child) << " ";
                return false;
            }
            return true;
        });
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
    uint32_t line, uint32_t col,
    CSTCancelCheck& cancel) {

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

    std::vector<TSNode> ancestors = findNodeAndParents(result, line, col, cancel);

    // Find the nearest ancestor matching the requested kind
    for (const auto& node : ancestors) {
        if (cancel()) {
            return std::string();
        }
        const char* type = ts_node_type(node);
        ASTSymbolKind nodeKind = nodeTypeToSymbolKind(type);

        // Constructor/destructor/method all match ASYK_Function (after mapping)
        if (kind == ASYK_Function &&
            (nodeKind == ASYK_Function || nodeKind == ASYK_Method ||
             nodeKind == ASYK_Constructor)) {
            return buildFunctionSignature(node, result, cancel);
        }

        if (nodeKind == kind) {
            switch (kind) {
                case ASYK_Class:
                    return buildClassSignature(node, result, cancel);
                case ASYK_Constant:
                    return buildConstantSignature(node, result);
                case ASYK_Interface:
                    return buildHashdeclSignature(node, result);
                case ASYK_Field:
                    return buildHashMemberSignature(node, result);
                case ASYK_Variable:
                    return buildVariableSignature(node, result, cancel);
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

// --------------------------------------------------------------------------
// findReferencesCrossDoc
// --------------------------------------------------------------------------

std::vector<DefinitionResult>* CSTSearcher::findReferencesCrossDoc(
    const AstParseResult* result,
    uint32_t line, uint32_t col,
    bool includeDecl,
    const std::vector<DocumentRef>& otherDocs,
    CSTCancelCheck& cancel) {

    if (!result) {
        return nullptr;
    }

    // Find the identifier at position to get the name
    TSNode node = findNodeAtPosition(result, line, col);
    if (ts_node_is_null(node)) {
        return nullptr;
    }

    const char* nodeType = ts_node_type(node);
    if (!isIdentifierLikeNode(nodeType)) {
        return nullptr;
    }

    uint32_t nameStart = ts_node_start_byte(node);
    uint32_t nameEnd = ts_node_end_byte(node);
    const std::string& src = result->getSource();
    if (nameStart >= nameEnd || nameEnd > src.size()) {
        return nullptr;
    }

    std::string name = src.substr(nameStart, nameEnd - nameStart);
    if (name.empty()) {
        return nullptr;
    }

    std::unique_ptr<std::vector<DefinitionResult>> vec(new std::vector<DefinitionResult>());

    // Adds the references in a document
    auto addReferences = [&](const AstParseResult* docResult, const std::string& uri) {
        std::vector<TSNode> refs;
        std::vector<TSNode> parents;
        collectIdentifierRefs(docResult->getRootNode(), docResult, name, &refs, &parents, cancel);
        for (size_t i = 0; i < refs.size(); ++i) {
            if (cancel()) {
                return;
            }
            const TSNode& ref = refs[i];
            if (!includeDecl) {
                const TSNode& parent = parents[i];
                if (!ts_node_is_null(parent) && isNameOfDeclaration(ref, parent)) {
                    continue;
                }
            }
            vec->push_back({ref, uri});
        }
    };

    // Collect references in current document
    addReferences(result, "");

    // Collect references in other documents
    for (const auto& docRef : otherDocs) {
        if (cancel.failed()) {
            break;
        }
        if (docRef.result) {
            addReferences(docRef.result, docRef.uri);
        }
    }

    if (vec->empty() || cancel.failed()) {
        return nullptr;
    }
    return vec.release();
}

// --------------------------------------------------------------------------
// getSuperclassNames
// --------------------------------------------------------------------------

std::vector<std::string>* CSTSearcher::getSuperclassNames(
    const AstParseResult* result,
    uint32_t line, uint32_t col,
    CSTCancelCheck& cancel) {

    if (!result) {
        return nullptr;
    }

    // Find the node at position and walk up to find a class_declaration
    std::vector<TSNode> ancestors = findNodeAndParents(result, line, col, cancel);
    TSNode classNode = make_null_node();
    for (const auto& node : ancestors) {
        const char* type = ts_node_type(node);
        if (strcmp(type, "class_declaration") == 0) {
            classNode = node;
            break;
        }
    }

    if (ts_node_is_null(classNode)) {
        return nullptr;
    }

    // Find the superclass_list child
    TSNode superList = cst_find_named_child(classNode, "superclass_list", cancel);
    if (ts_node_is_null(superList)) {
        return nullptr;
    }

    // Collect superclass names
    std::unique_ptr<std::vector<std::string>> vec(new std::vector<std::string>());
    cst_for_each_child(superList, true, cancel, [&](TSNode superNode) {
        if (strcmp(ts_node_type(superNode), "superclass") != 0) {
            return true;
        }
        // The superclass has optional access_modifier + identifier/scoped_identifier
        // Find the identifier or scoped_identifier child
        cst_for_each_child(superNode, true, cancel, [&](TSNode scChild) {
            const char* scType = ts_node_type(scChild);
            if (strcmp(scType, "identifier") == 0 || strcmp(scType, "scoped_identifier") == 0) {
                uint32_t start = ts_node_start_byte(scChild);
                uint32_t end = ts_node_end_byte(scChild);
                const std::string& src = result->getSource();
                if (start < end && end <= src.size()) {
                    vec->push_back(src.substr(start, end - start));
                }
                return false;
            }
            return true;
        });
        return !cancel.failed();
    });

    if (vec->empty() || cancel.failed()) {
        return nullptr;
    }
    return vec.release();
}

// --------------------------------------------------------------------------
// Semantic tokens
// --------------------------------------------------------------------------

//! Set of Qore keywords for semantic token classification.
static const std::unordered_set<std::string>& getQoreKeywords() {
    static const std::unordered_set<std::string> kw = {
        "abstract", "background", "break", "case", "catch", "class", "const",
        "constructor", "continue", "default", "delete", "deprecated", "destructor",
        "do", "else", "enum", "exists", "final", "foldl", "foldr", "for",
        "foreach", "hashdecl", "if", "in", "inherits", "instanceof", "keys",
        "map", "module", "my", "namespace", "new", "on_error", "on_exit",
        "on_success", "our", "pop", "private", "private:hierarchy",
        "private:internal", "public", "push", "rethrow", "return", "returns",
        "select", "shift", "splice", "static", "sub", "switch", "synchronized",
        "thread_exit", "thread_local", "throw", "trim", "try", "typedef",
        "unshift", "where", "while",
    };
    return kw;
}

//! Check if a node type string is a Qore keyword.
static bool isKeyword(const char* type) {
    return getQoreKeywords().count(type) > 0;
}

//! Emit semantic token(s) for a potentially multi-line node.
static void emitToken(TSNode node, uint32_t tokenType, uint32_t tokenModifiers,
                       uint32_t startLine, uint32_t endLine,
                       const AstParseResult* result,
                       std::vector<SemanticToken>* vec) {
    TSPoint start = ts_node_start_point(node);
    TSPoint end = ts_node_end_point(node);

    if (start.row > endLine || end.row < startLine) {
        return;
    }

    if (start.row == end.row) {
        // Single-line token
        uint32_t len = end.column - start.column;
        if (len > 0) {
            vec->push_back({start.row, start.column, len, tokenType, tokenModifiers});
        }
    } else {
        // Multi-line token: emit one token per line
        const std::string& src = result->getSource();
        uint32_t byteStart = ts_node_start_byte(node);
        uint32_t byteEnd = ts_node_end_byte(node);

        if (byteEnd > src.size()) {
            byteEnd = (uint32_t)src.size();
        }

        // Find line boundaries
        uint32_t lineStart = start.row;
        uint32_t col = start.column;
        uint32_t pos = byteStart;

        while (pos < byteEnd && lineStart <= endLine) {
            // Find end of this line
            uint32_t lineEnd = pos;
            while (lineEnd < byteEnd && src[lineEnd] != '\n') {
                ++lineEnd;
            }

            if (lineStart >= startLine) {
                uint32_t len = lineEnd - pos;
                if (len > 0) {
                    vec->push_back({lineStart, col, len, tokenType, tokenModifiers});
                }
            }

            // Move to next line
            if (lineEnd < byteEnd && src[lineEnd] == '\n') {
                ++lineEnd;
            }
            pos = lineEnd;
            col = 0;
            ++lineStart;
        }
    }
}

std::vector<SemanticToken>* CSTSearcher::collectSemanticTokens(
        const AstParseResult* result,
        CSTCancelCheck& cancel,
        uint32_t startLine, uint32_t endLine) {
    if (!result) {
        return nullptr;
    }
    TSTree* tree = result->getTree();
    if (!tree) {
        return nullptr;
    }

    std::unique_ptr<std::vector<SemanticToken>> vec(new std::vector<SemanticToken>());
    TSNode root = ts_tree_root_node(tree);
    collectSemanticTokens(root, result, startLine, endLine, vec.get(), cancel);

    // Sort by position
    std::sort(vec->begin(), vec->end(),
        [](const SemanticToken& a, const SemanticToken& b) {
            if (a.line != b.line) {
                return a.line < b.line;
            }
            return a.startChar < b.startChar;
        });

    if (vec->empty() || cancel.failed()) {
        return nullptr;
    }
    return vec.release();
}

void CSTSearcher::collectSemanticTokens(
        TSNode root, const AstParseResult* result,
        uint32_t startLine, uint32_t endLine,
        std::vector<SemanticToken>* vec,
        CSTCancelCheck& cancel) {
    // the node at each depth of the walk, and whether a child of the node with the "name" field has been entered;
    // only the first such child is the name of its parent
    struct PathEntry {
        TSNode node;
        bool nameSeen;
    };
    std::vector<PathEntry> path;
    cst_walk(root, cancel, [&](const TSTreeCursor* cursor, TSNode node, uint32_t depth) {
        if (depth) {
            bool isNameField = false;
            const char* field = ts_tree_cursor_current_field_name(cursor);
            if (field && strcmp(field, "name") == 0 && !path[depth - 1].nameSeen) {
                path[depth - 1].nameSeen = true;
                isNameField = true;
            }
            uint32_t tokenType = 0;
            uint32_t tokenModifiers = 0;
            if (classifyNode(node, path[depth - 1].node, isNameField, result, tokenType, tokenModifiers, cancel)) {
                emitToken(node, tokenType, tokenModifiers, startLine, endLine, result, vec);
                // Don't walk into classified leaf tokens
                return CSTWalkAction::Skip;
            }
        }

        // Quick range check for structural nodes
        if (ts_node_end_point(node).row < startLine || ts_node_start_point(node).row > endLine) {
            return CSTWalkAction::Skip;
        }
        if (path.size() <= depth) {
            path.resize(depth + 1);
        }
        path[depth] = {node, false};
        return CSTWalkAction::Descend;
    });
}

//! Returns the last named child of a node, or a null node
static TSNode last_named_child(TSNode node, CSTCancelCheck& cancel) {
    TSNode last = {};
    cst_for_each_child(node, true, cancel, [&](TSNode child) {
        last = child;
        return true;
    });
    return last;
}

bool CSTSearcher::classifyNode(TSNode node, TSNode parent, bool isNameField,
                                const AstParseResult* result,
                                uint32_t& tokenType,
                                uint32_t& tokenModifiers,
                                CSTCancelCheck& cancel) {
    const char* type = ts_node_type(node);
    bool named = ts_node_is_named(node);

    if (!named) {
        // Anonymous node — check if it's a keyword
        if (isKeyword(type)) {
            tokenType = STT_Keyword;
            tokenModifiers = 0;
            return true;
        }
        // Operators
        if (strcmp(type, "+") == 0 || strcmp(type, "-") == 0
                || strcmp(type, "*") == 0 || strcmp(type, "/") == 0
                || strcmp(type, "%") == 0 || strcmp(type, "=") == 0
                || strcmp(type, "==") == 0 || strcmp(type, "!=") == 0
                || strcmp(type, "<") == 0 || strcmp(type, ">") == 0
                || strcmp(type, "<=") == 0 || strcmp(type, ">=") == 0
                || strcmp(type, "&&") == 0 || strcmp(type, "||") == 0
                || strcmp(type, "!") == 0 || strcmp(type, "&") == 0
                || strcmp(type, "|") == 0 || strcmp(type, "^") == 0
                || strcmp(type, "~") == 0 || strcmp(type, "<<") == 0
                || strcmp(type, ">>") == 0 || strcmp(type, ">>>") == 0
                || strcmp(type, "+=") == 0 || strcmp(type, "-=") == 0
                || strcmp(type, "*=") == 0 || strcmp(type, "/=") == 0
                || strcmp(type, "++") == 0 || strcmp(type, "--") == 0
                || strcmp(type, "?") == 0 || strcmp(type, ":") == 0
                || strcmp(type, "<=>") == 0 || strcmp(type, "??") == 0
                || strcmp(type, "=~") == 0 || strcmp(type, "!~") == 0
                || strcmp(type, "..") == 0 || strcmp(type, ".") == 0) {
            tokenType = STT_Operator;
            tokenModifiers = 0;
            return true;
        }
        return false;
    }

    // Named nodes
    // Comments (tree-sitter extras)
    if (strcmp(type, "comment") == 0 || strcmp(type, "line_comment") == 0
            || strcmp(type, "block_comment") == 0) {
        tokenType = STT_Comment;
        tokenModifiers = 0;
        return true;
    }

    // String literals
    if (strcmp(type, "string_literal") == 0
            || strcmp(type, "double_quoted_string") == 0
            || strcmp(type, "single_quoted_string") == 0
            || strcmp(type, "char_literal") == 0
            || strcmp(type, "backquote_string") == 0) {
        tokenType = STT_String;
        tokenModifiers = 0;
        return true;
    }

    // Number literals
    if (strcmp(type, "integer_literal") == 0
            || strcmp(type, "float_literal") == 0
            || strcmp(type, "number_literal") == 0) {
        tokenType = STT_Number;
        tokenModifiers = 0;
        return true;
    }

    // Regex
    if (strcmp(type, "regex_literal") == 0) {
        tokenType = STT_Regexp;
        tokenModifiers = 0;
        return true;
    }

    // Boolean/null constants
    if (strcmp(type, "true") == 0 || strcmp(type, "false") == 0
            || strcmp(type, "True") == 0 || strcmp(type, "False") == 0
            || strcmp(type, "NOTHING") == 0 || strcmp(type, "NULL") == 0) {
        tokenType = STT_Keyword;
        tokenModifiers = 0;
        return true;
    }

    // Type annotations
    if (strcmp(type, "simple_type") == 0 || strcmp(type, "scoped_type") == 0) {
        tokenType = STT_Type;
        tokenModifiers = 0;
        return true;
    }

    // Access modifier
    if (strcmp(type, "access_modifier") == 0) {
        tokenType = STT_Keyword;
        tokenModifiers = 0;
        return true;
    }

    // Parse directives (%modern, %requires, etc.)
    if (strcmp(type, "parse_directive") == 0) {
        tokenType = STT_Decorator;
        tokenModifiers = 0;
        return true;
    }

    // Relative date literals (P1D, PT5H, etc.)
    if (strcmp(type, "relative_date") == 0) {
        tokenType = STT_Number;
        tokenModifiers = 0;
        return true;
    }

    // Implicit arguments ($1, etc.)
    if (strcmp(type, "implicit_argument") == 0) {
        tokenType = STT_Variable;
        tokenModifiers = 0;
        return true;
    }

    // Variable name (e.g., $. prefixed)
    if (strcmp(type, "variable_name") == 0) {
        tokenType = STT_Variable;
        tokenModifiers = 0;
        return true;
    }

    // Identifiers — classify based on parent context
    if (isIdentifierLikeNode(type)) {
        const char* parentType = ts_node_type(parent);

        // A declaration's "name" field is given by the walk, which knows the field of each child
        if (isNameField) {
            // Declaration name
            if (strcmp(parentType, "class_declaration") == 0) {
                tokenType = STT_Class;
                tokenModifiers = STM_Declaration;
                return true;
            }
            if (strcmp(parentType, "function_declaration") == 0) {
                tokenType = STT_Function;
                tokenModifiers = STM_Declaration;
                return true;
            }
            if (strcmp(parentType, "method_declaration") == 0) {
                tokenType = STT_Method;
                tokenModifiers = STM_Declaration;
                return true;
            }
            if (strcmp(parentType, "namespace_declaration") == 0) {
                tokenType = STT_Namespace;
                tokenModifiers = STM_Declaration;
                return true;
            }
            if (strcmp(parentType, "enum_declaration") == 0) {
                tokenType = STT_Enum;
                tokenModifiers = STM_Declaration;
                return true;
            }
            if (strcmp(parentType, "hashdecl_declaration") == 0) {
                tokenType = STT_Struct;
                tokenModifiers = STM_Declaration;
                return true;
            }
            if (strcmp(parentType, "typedef_declaration") == 0) {
                tokenType = STT_Type;
                tokenModifiers = STM_Declaration;
                return true;
            }
            if (strcmp(parentType, "constant_declaration") == 0) {
                tokenType = STT_Variable;
                tokenModifiers = STM_Declaration | STM_Readonly;
                return true;
            }
            if (strcmp(parentType, "variable_declarator") == 0) {
                tokenType = STT_Variable;
                tokenModifiers = STM_Declaration;
                return true;
            }
            if (strcmp(parentType, "enum_member") == 0) {
                tokenType = STT_EnumMember;
                tokenModifiers = STM_Declaration;
                return true;
            }
        }

        // Parameter name
        if (strcmp(parentType, "parameter") == 0) {
            tokenType = STT_Parameter;
            tokenModifiers = STM_Declaration;
            return true;
        }

        // Member declaration name
        if (strcmp(parentType, "member_declaration") == 0) {
            // Check if this is the member name (not the type)
            TSNode typeChild = ts_node_child_by_field_name(parent, "type", 4);
            if (!ts_node_is_null(typeChild)
                    && ts_node_start_byte(typeChild) != ts_node_start_byte(node)) {
                tokenType = STT_Property;
                tokenModifiers = STM_Declaration;
                return true;
            }
            // If no type field, last identifier child is the name
            TSNode lastChild = last_named_child(parent, cancel);
            if (!ts_node_is_null(lastChild)) {
                if (ts_node_start_byte(lastChild) == ts_node_start_byte(node)) {
                    tokenType = STT_Property;
                    tokenModifiers = STM_Declaration;
                    return true;
                }
            }
        }

        // Call expression — function/method name
        if (strcmp(parentType, "call_expression") == 0) {
            tokenType = STT_Function;
            tokenModifiers = 0;
            return true;
        }

        // Member expression — method/property access
        if (strcmp(parentType, "member_expression") == 0) {
            // Last child is the member name
            TSNode lastNamed = last_named_child(parent, cancel);
            if (!ts_node_is_null(lastNamed)) {
                if (ts_node_start_byte(lastNamed) == ts_node_start_byte(node)) {
                    tokenType = STT_Property;
                    tokenModifiers = 0;
                    return true;
                }
            }
        }

        // Superclass name
        if (strcmp(parentType, "superclass") == 0) {
            tokenType = STT_Class;
            tokenModifiers = 0;
            return true;
        }

        // Hash member name (in hashdecl body)
        if (strcmp(parentType, "hash_member") == 0) {
            tokenType = STT_Property;
            tokenModifiers = STM_Declaration;
            return true;
        }

        // Default: treat as variable reference
        tokenType = STT_Variable;
        tokenModifiers = 0;
        return true;
    }

    // Constructor/destructor — don't emit for the structural node, let children emit
    if (strcmp(type, "constructor_declaration") == 0
            || strcmp(type, "destructor_declaration") == 0) {
        return false;
    }

    return false;
}
