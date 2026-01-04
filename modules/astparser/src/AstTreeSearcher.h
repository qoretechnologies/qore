/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  AstTreeSearcher.h

  Qore AST Parser

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

#ifndef _QLS_ASTTREESEARCHER_H
#define _QLS_ASTTREESEARCHER_H

#include <string>
#include <vector>

#include "ast/ASTParseLocation.h"
#include "ast/ASTSymbolInfo.h"

class ASTNode;
class ASTTree;
struct ASTScopeSymbolInfo;

class AstTreeSearcher {
public:
    AstTreeSearcher() = delete;
    AstTreeSearcher(const AstTreeSearcher& other) = delete;

    static std::vector<ASTSymbolInfo>* findMatchingSymbols(ASTTree* tree, const std::string& query, bool exactMatch = false, bool fixSymbols = true, bool bareNames = false);
    static std::vector<ASTSymbolInfo>* findMatchingSymbols(const std::vector<ASTSymbolInfo>* symbols, const std::string& query, bool exactMatch = false);
    static ASTNode* findNode(ASTTree* tree, ast_loc_t line, ast_loc_t col);
    static std::vector<ASTNode*>* findNodeAndParents(ASTTree* tree, ast_loc_t line, ast_loc_t col);
    static std::vector<ASTNode*>* findReferences(ASTTree* tree, ast_loc_t line, ast_loc_t col, bool includeDecl);
    static ASTSymbolInfo findSymbolInfo(ASTTree* tree, ast_loc_t line, ast_loc_t col);
    static std::vector<ASTSymbolInfo>* findSymbols(ASTTree* tree, bool fixSymbols = true, bool bareNames = false);
    static std::vector<ASTScopeSymbolInfo>* findScopeSymbols(ASTTree* tree, ast_loc_t line, ast_loc_t col);
};

#endif // _QLS_ASTTREESEARCHER_H
