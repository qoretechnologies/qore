/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  FindScopeSymbolsQuery.h

  Qore AST Parser

  Copyright (C) 2026 Qore Technologies, s.r.o.

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

#ifndef _QLS_QUERIES_FINDSCOPESYMBOLSQUERY_H
#define _QLS_QUERIES_FINDSCOPESYMBOLSQUERY_H

#include <vector>

#include "ast/ASTName.h"
#include "ast/ASTSymbolInfo.h"

class ASTDeclaration;
class ASTExpression;
class ASTStatement;
class ASTTree;

//! Symbol info with scope level for completion.
/**
    @since %Qore 2.2
 */
struct ASTScopeSymbolInfo {
    //! Symbol information.
    ASTSymbolInfo symbol;

    //! Scope level (0 = innermost/local, higher = outer scope).
    int scopeLevel;

    //! Constructor.
    ASTScopeSymbolInfo(ASTSymbolInfo&& sym, int level) :
        symbol(std::move(sym)),
        scopeLevel(level) {}

    //! Copy constructor.
    ASTScopeSymbolInfo(const ASTScopeSymbolInfo& other) :
        symbol(other.symbol),
        scopeLevel(other.scopeLevel) {}

    //! Move constructor.
    ASTScopeSymbolInfo(ASTScopeSymbolInfo&& other) :
        symbol(std::move(other.symbol)),
        scopeLevel(other.scopeLevel) {}
};

//! Query to find symbols accessible from a specific scope position.
/**
    This query returns symbols that are accessible from a given position in the
    source code, ordered by scope level. Symbols from the innermost scope have
    scope_level 0, and outer scopes have increasing levels.

    Scope levels:
    - 0: Local variables and parameters in the current function/method
    - 1: Class members (when inside a class method)
    - 2: Namespace-level symbols
    - 3+: Outer namespaces

    @since %Qore 2.2
 */
class FindScopeSymbolsQuery {
public:
    FindScopeSymbolsQuery() = delete;
    FindScopeSymbolsQuery(const FindScopeSymbolsQuery& other) = delete;

    //! Find all symbols accessible from the given position.
    /**
        @param tree tree to search
        @param line line number (1-indexed)
        @param col column number (1-indexed)

        @return new list of symbols with scope levels
     */
    static std::vector<ASTScopeSymbolInfo>* find(ASTTree* tree, ast_loc_t line, ast_loc_t col);

private:
    //! Collect symbols from a function/method body.
    static void collectLocalSymbols(std::vector<ASTScopeSymbolInfo>* vec,
        ASTStatement* stmt, ast_loc_t line, ast_loc_t col, int scopeLevel);

    //! Collect symbols from a function/method declaration (parameters).
    static void collectFunctionParams(std::vector<ASTScopeSymbolInfo>* vec,
        ASTDeclaration* decl, int scopeLevel);

    //! Collect symbols from a class declaration (members, methods).
    static void collectClassSymbols(std::vector<ASTScopeSymbolInfo>* vec,
        ASTDeclaration* decl, int scopeLevel);

    //! Collect symbols from a namespace declaration.
    static void collectNamespaceSymbols(std::vector<ASTScopeSymbolInfo>* vec,
        ASTDeclaration* decl, int scopeLevel);

    //! Collect symbols from a hash declaration (members).
    static void collectHashSymbols(std::vector<ASTScopeSymbolInfo>* vec,
        ASTDeclaration* decl, int scopeLevel);

    //! Collect symbols from an expression (e.g., variable declarations).
    static void collectExpressionSymbols(std::vector<ASTScopeSymbolInfo>* vec,
        ASTExpression* expr, int scopeLevel);
};

#endif // _QLS_QUERIES_FINDSCOPESYMBOLSQUERY_H
