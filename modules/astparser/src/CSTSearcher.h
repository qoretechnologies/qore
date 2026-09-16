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
#include "CSTWalk.h"
#include "ast/ASTSymbolKind.h"
#include "ast/ASTSymbolUsageKind.h"

//! Reference to another document's parse result for cross-document resolution.
struct DocumentRef {
    const AstParseResult* result;
    std::string uri;
};

//! Result of cross-document definition resolution (node + owning document URI).
struct DefinitionResult {
    TSNode node;
    std::string uri;
};

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
    //! the declaration of the symbol
    TSNode node = {};
    //! the parent of the declaration
    TSNode parent = {};

    CSTScopeSymbolInfo() = default;
    CSTScopeSymbolInfo(CSTSymbolInfo&& sym, int level, TSNode node, TSNode parent)
        : symbol(std::move(sym)), scopeLevel(level), node(node), parent(parent) {}
};

//! Parameter info for detailed symbol data.
struct CSTParamInfo {
    std::string name;
    std::string typeName;    //!< empty if untyped
    std::string defaultVal;  //!< empty if no default
};

//! Detailed symbol info with type, access, parameters, etc.
struct CSTSymbolDetail {
    CSTSymbolInfo symbol;
    int scopeLevel = 0;
    std::string returnType;  //!< for methods/functions
    std::string typeName;    //!< for variables/members/params
    std::string access;      //!< "public", "private", "private:internal", etc.
    bool isStatic = false;
    bool isConstMethod = false;
    std::vector<CSTParamInfo> params;  //!< for callables
};

//! Semantic token classification for syntax highlighting.
struct SemanticToken {
    uint32_t line;
    uint32_t startChar;
    uint32_t length;
    uint32_t tokenType;      //!< index into the legend (see SemanticTokenType)
    uint32_t tokenModifiers;  //!< bitmask (see SemanticTokenModifier)
};

//! Token type indices for semantic tokens legend.
enum SemanticTokenType : uint32_t {
    STT_Namespace = 0,
    STT_Type,
    STT_Class,
    STT_Enum,
    STT_Interface,
    STT_Struct,
    STT_TypeParameter,
    STT_Parameter,
    STT_Variable,
    STT_Property,
    STT_EnumMember,
    STT_Function,
    STT_Method,
    STT_Keyword,
    STT_Comment,
    STT_String,
    STT_Number,
    STT_Regexp,
    STT_Operator,
    STT_Decorator,
};

//! Token modifier bit flags for semantic tokens.
enum SemanticTokenModifier : uint32_t {
    STM_Declaration  = 1 << 0,
    STM_Definition   = 1 << 1,
    STM_Readonly     = 1 << 2,
    STM_Static       = 1 << 3,
    STM_Deprecated   = 1 << 4,
    STM_Abstract     = 1 << 5,
    STM_Async        = 1 << 6,
    STM_Modification = 1 << 7,
    STM_Documentation = 1 << 8,
    STM_DefaultLibrary = 1 << 9,
};

//! Static utility class for searching tree-sitter CST nodes.
/** Each search checks for cancellation with the CSTCancelCheck of its operation, which also ends the search when an
    exception has been raised; the caller checks the exception sink after the search.
*/
class CSTSearcher {
public:
    CSTSearcher() = delete;
    CSTSearcher(const CSTSearcher&) = delete;

    //! Find the most specific named node at position (0-indexed).
    static TSNode findNodeAtPosition(const AstParseResult* result,
                                     uint32_t line, uint32_t col);

    //! Find node + all ancestors up to root (0-indexed position).
    static std::vector<TSNode> findNodeAndParents(const AstParseResult* result,
                                                  uint32_t line, uint32_t col,
                                                  CSTCancelCheck& cancel);

    //! Map tree-sitter node type string to ASTSymbolKind.
    static ASTSymbolKind nodeTypeToSymbolKind(const char* type);

    //! Check if a node type is a declaration type.
    static bool isDeclarationNode(const char* type);

    //! Extract the "name" field from a declaration node.
    static std::string getNodeName(TSNode node, const AstParseResult* result);

    //! Build a QoreHashNode range from a TSNode (0-indexed).
    static QoreHashNode* makeRange(TSNode node, ExceptionSink* xsink);

    //! Build a QoreHashNode location from a TSNode + URI.
    static QoreHashNode* makeLocation(TSNode node, const std::string& uri,
                                      ExceptionSink* xsink);

    //! Collect all declaration symbols from the tree.
    /** Walks the tree collecting declaration nodes.
        @param result parse result
        @param cancel the cancellation check of the operation
        @param fixSymbols if true, prefix names with namespace/class scope
        @param bareNames if true, strip all namespace/class prefixes
        @return vector of symbol info
    */
    static std::vector<CSTSymbolInfo>* collectSymbols(
        const AstParseResult* result,
        CSTCancelCheck& cancel,
        bool fixSymbols = true,
        bool bareNames = false);

    //! Find symbols matching a query string.
    static std::vector<CSTSymbolInfo>* findMatchingSymbols(
        const AstParseResult* result,
        const std::string& query,
        CSTCancelCheck& cancel,
        bool exactMatch = false,
        bool fixSymbols = true,
        bool bareNames = false);

    //! Find symbol info at a position (0-indexed).
    static CSTSymbolInfo findSymbolInfo(
        const AstParseResult* result,
        uint32_t line, uint32_t col,
        CSTCancelCheck& cancel);

    //! Find all references to the identifier at position (0-indexed).
    static std::vector<TSNode>* findReferences(
        const AstParseResult* result,
        uint32_t line, uint32_t col,
        bool includeDecl,
        CSTCancelCheck& cancel);

    //! Find symbols accessible from a specific scope position (0-indexed).
    static std::vector<CSTScopeSymbolInfo>* findScopeSymbols(
        const AstParseResult* result,
        uint32_t line, uint32_t col,
        CSTCancelCheck& cancel);

    //! Find scope symbols with detailed metadata (type, access, params, etc.).
    static std::vector<CSTSymbolDetail>* findScopeSymbolsDetailed(
        const AstParseResult* result,
        uint32_t line, uint32_t col,
        CSTCancelCheck& cancel);

    //! Resolve the type of a symbol at position (0-indexed).
    static std::string getSymbolType(
        const AstParseResult* result,
        uint32_t line, uint32_t col,
        CSTCancelCheck& cancel);

    //! Find members of a named type (class, namespace, hashdecl, enum).
    static std::vector<CSTSymbolDetail>* findTypeMembers(
        const AstParseResult* result,
        const std::string& typeName,
        CSTCancelCheck& cancel,
        bool includeInherited = true);

    //! Get superclass names for a class at position (0-indexed).
    /** @param result parse result
        @param line 0-indexed line
        @param col 0-indexed column
        @param cancel the cancellation check of the operation
        @return vector of superclass name strings, or nullptr if not a class or no parents
    */
    static std::vector<std::string>* getSuperclassNames(
        const AstParseResult* result, uint32_t line, uint32_t col, CSTCancelCheck& cancel);

    //! Find all references to the identifier at position across multiple documents.
    /** @param result parse result of the current document
        @param line 0-indexed line
        @param col 0-indexed column
        @param includeDecl whether to include the declaration itself
        @param otherDocs other documents to search
        @param cancel the cancellation check of the operation
        @return vector of DefinitionResult (node + URI), or nullptr if not found
    */
    static std::vector<DefinitionResult>* findReferencesCrossDoc(
        const AstParseResult* result,
        uint32_t line, uint32_t col,
        bool includeDecl,
        const std::vector<DocumentRef>& otherDocs,
        CSTCancelCheck& cancel);

    //! Collect semantic tokens from the parse tree.
    /** Walks the tree classifying nodes into semantic token types.
        @param result parse result
        @param cancel the cancellation check of the operation
        @param startLine start of range (0-indexed, inclusive), default 0
        @param endLine end of range (0-indexed, inclusive), default UINT32_MAX
        @return vector of SemanticToken sorted by position, or nullptr if no tokens
    */
    static std::vector<SemanticToken>* collectSemanticTokens(
        const AstParseResult* result,
        CSTCancelCheck& cancel,
        uint32_t startLine = 0,
        uint32_t endLine = UINT32_MAX);

    //! Build hover info description for a declaration at position.
    static std::string buildHoverInfo(
        const AstParseResult* result,
        ASTSymbolKind kind,
        uint32_t line, uint32_t col,
        CSTCancelCheck& cancel);

    //! Resolve the definition location for the symbol at position (0-indexed).
    /** Handles local variables, parameters, member access, scoped access,
        and top-level declarations within a single document.
        @param result parse result
        @param line 0-indexed line
        @param col 0-indexed column
        @param cancel the cancellation check of the operation
        @return vector of definition TSNodes, or nullptr if not found
    */
    static std::vector<TSNode>* resolveDefinition(
        const AstParseResult* result,
        uint32_t line, uint32_t col,
        CSTCancelCheck& cancel);

    //! Resolve definition across multiple documents.
    /** Same logic as resolveDefinition but searches other documents when the
        current document doesn't contain the target declaration.
        @param result parse result of the current document
        @param line 0-indexed line
        @param col 0-indexed column
        @param otherDocs other documents to search
        @param cancel the cancellation check of the operation
        @return vector of DefinitionResult (node + URI), or nullptr if not found
    */
    static std::vector<DefinitionResult>* resolveDefinitionCrossDoc(
        const AstParseResult* result,
        uint32_t line, uint32_t col,
        const std::vector<DocumentRef>& otherDocs,
        CSTCancelCheck& cancel);

private:
    //! Collect all identifier nodes matching `name` in the tree.
    /** @param root the node to search
        @param result the parse result owning the node
        @param name the identifier text to find
        @param vec receives the identifier nodes
        @param parents receives the parent of each identifier node (a null node for the root)
        @param cancel the cancellation check of the operation
    */
    static void collectIdentifierRefs(
        TSNode root,
        const AstParseResult* result,
        const std::string& name,
        std::vector<TSNode>* vec,
        std::vector<TSNode>* parents,
        CSTCancelCheck& cancel);

    //! Collect scope symbols from ancestors and their siblings.
    static void collectScopeSymbolsFromAncestors(
        const std::vector<TSNode>& ancestors,
        const AstParseResult* result,
        uint32_t line, uint32_t col,
        std::vector<CSTScopeSymbolInfo>* vec,
        CSTCancelCheck& cancel);

    //! Build signature for class declaration.
    static std::string buildClassSignature(TSNode node, const AstParseResult* result, CSTCancelCheck& cancel);

    //! Build signature for function/method declaration.
    static std::string buildFunctionSignature(TSNode node, const AstParseResult* result, CSTCancelCheck& cancel);

    //! Build signature for constant declaration.
    static std::string buildConstantSignature(TSNode node, const AstParseResult* result);

    //! Build signature for variable declaration.
    static std::string buildVariableSignature(TSNode node, const AstParseResult* result, CSTCancelCheck& cancel);

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
        std::vector<CSTScopeSymbolInfo>* vec,
        CSTCancelCheck& cancel);

    //! Collect function/method parameters as scope symbols.
    static void collectParameters(
        TSNode funcNode,
        const AstParseResult* result,
        int scopeLevel,
        std::vector<CSTScopeSymbolInfo>* vec,
        CSTCancelCheck& cancel);

    //! Collect local variable declarations from statements before position.
    static void collectLocalsBeforePosition(
        TSNode blockNode,
        const AstParseResult* result,
        uint32_t line, uint32_t col,
        int scopeLevel,
        std::vector<CSTScopeSymbolInfo>* vec,
        CSTCancelCheck& cancel);

    //! Case-insensitive substring match.
    static bool matchesQuery(const std::string& name, const std::string& query,
                             bool exactMatch);

    //! Extract access modifier from a declaration node or its parent member_group.
    static std::string extractAccessModifier(TSNode node, TSNode parent, const AstParseResult* result,
                                             CSTCancelCheck& cancel);

    //! Check if a declaration has 'static' modifier.
    static bool hasStaticModifier(TSNode node, const AstParseResult* result, CSTCancelCheck& cancel);

    //! Check if a declaration has trailing 'const' method qualifier.
    static bool hasConstMethodQualifier(TSNode node, const AstParseResult* result, CSTCancelCheck& cancel);

    //! Extract parameter info from a function/method/constructor node.
    static std::vector<CSTParamInfo> extractParameters(TSNode funcNode,
                                                        const AstParseResult* result,
                                                        CSTCancelCheck& cancel);

    //! Extract return type from a function/method node.
    static std::string extractReturnType(TSNode funcNode, const AstParseResult* result);

    //! Extract type name from a variable/member/parameter declaration with a known parent.
    static std::string extractTypeName(TSNode node, TSNode parent, const AstParseResult* result);

    //! Fills in the details of a declaration, as enrichSymbol() does for the nearest declaration of a symbol
    /** @return true if the node is a declaration whose details were filled in
    */
    static bool fillDeclarationDetail(CSTSymbolDetail& detail, TSNode node, TSNode parent,
                                      const AstParseResult* result, CSTCancelCheck& cancel);

    //! Enrich a scope symbol with detailed metadata from its declaration node.
    static CSTSymbolDetail enrichSymbol(const CSTScopeSymbolInfo& ssi,
                                         const AstParseResult* result,
                                         CSTCancelCheck& cancel);

    //! Find the declaration of a local variable or parameter in the scope chain.
    static bool findLocalDeclaration(
        const std::vector<TSNode>& ancestors,
        const AstParseResult* result,
        const std::string& name,
        uint32_t line, uint32_t col,
        TSNode* outNode,
        CSTCancelCheck& cancel);

    //! Find a declaration node by name and type, searching recursively.
    static bool findDeclarationByName(TSNode root, const AstParseResult* result,
                                       const std::string& name, const char* nodeType,
                                       TSNode* outNode, CSTCancelCheck& cancel);

    //! Search a class body for a member matching a name, walking the inheritance chain.
    static bool findMemberInClass(TSNode classNode, const AstParseResult* result,
                                   const std::string& name, TSNode* outNode, CSTCancelCheck& cancel);

    //! Search direct members of a class body (no inheritance walk).
    /** Checks method, constructor, destructor, member, and constant declarations
        directly within the class node and its member_groups.
        @param classNode class declaration node
        @param result parse result owning classNode
        @param name member name to find
        @param outNode receives the found node
        @param cancel the cancellation check of the operation
        @return true if found
    */
    static bool findMemberInClassBody(TSNode classNode, const AstParseResult* result,
                                       const std::string& name, TSNode* outNode, CSTCancelCheck& cancel);

    //! Find a declaration by name, searching the current document first, then other documents.
    /** @param root root node to search first
        @param result parse result for the root node
        @param name declaration name to find
        @param nodeType node type filter (nullptr for any)
        @param outNode receives the found node
        @param outResult receives the parse result owning the found node
        @param otherDocs other documents to search if not found locally
        @param cancel the cancellation check of the operation
        @return true if found
    */
    static bool findDeclarationByNameCrossDoc(
        TSNode root, const AstParseResult* result,
        const std::string& name, const char* nodeType,
        TSNode* outNode, const AstParseResult** outResult,
        const std::vector<DocumentRef>& otherDocs,
        CSTCancelCheck& cancel);

    //! Search class body for a member, walking inheritance across documents.
    /** @param classNode class declaration node
        @param result parse result owning classNode
        @param name member name to find
        @param outNode receives the found node
        @param outResult receives the parse result owning the found node
        @param otherDocs other documents to search for parent classes
        @param cancel the cancellation check of the operation
        @return true if found
    */
    static bool findMemberInClassCrossDoc(
        TSNode classNode, const AstParseResult* result,
        const std::string& name, TSNode* outNode,
        const AstParseResult** outResult,
        const std::vector<DocumentRef>& otherDocs,
        CSTCancelCheck& cancel);

    //! Add the members declared in the body of a class to a detail vector.
    static void addOwnClassMembers(TSNode classNode, const AstParseResult* result,
                                   std::vector<CSTSymbolDetail>* vec, CSTCancelCheck& cancel);

    //! Collect class members into a detail vector.
    static void collectClassMembers(TSNode classNode, const AstParseResult* result,
                                     std::vector<CSTSymbolDetail>* vec,
                                     bool includeInherited,
                                     CSTCancelCheck& cancel);

    //! Collect namespace members into a detail vector.
    static void collectNamespaceMembers(TSNode nsNode, const AstParseResult* result,
                                         std::vector<CSTSymbolDetail>* vec, CSTCancelCheck& cancel);

    //! Collect hashdecl members into a detail vector.
    static void collectHashdeclMembers(TSNode hdNode, const AstParseResult* result,
                                        std::vector<CSTSymbolDetail>* vec, CSTCancelCheck& cancel);

    //! Collect enum members into a detail vector.
    static void collectEnumMembers(TSNode enumNode, const AstParseResult* result,
                                    std::vector<CSTSymbolDetail>* vec, CSTCancelCheck& cancel);

    //! Collect the semantic tokens of a node and its descendants.
    static void collectSemanticTokens(
        TSNode root,
        const AstParseResult* result,
        uint32_t startLine, uint32_t endLine,
        std::vector<SemanticToken>* vec,
        CSTCancelCheck& cancel);

    //! Classify a node type to a semantic token type and modifiers.
    /** @return true if the node should generate a token */
    /** @param node the node to classify
        @param parent the node's parent
        @param isNameField true if the node is the "name" field of its parent
        @param result the parse result owning the node
        @param tokenType receives the token type
        @param tokenModifiers receives the token modifiers
        @param cancel the cancellation check of the operation
    */
    static bool classifyNode(TSNode node, TSNode parent, bool isNameField,
                             const AstParseResult* result,
                             uint32_t& tokenType, uint32_t& tokenModifiers,
                             CSTCancelCheck& cancel);
};

#endif // _QLS_CSTSEARCHER_H
