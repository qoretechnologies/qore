/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  FindSymbolsQuery.h

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

#ifndef _QLS_QUERIES_FINDSYMBOLSQUERY_H
#define _QLS_QUERIES_FINDSYMBOLSQUERY_H

#include <vector>

#include "ast/ASTName.h"
#include "ast/ASTSymbolInfo.h"

class ASTDeclaration;
class ASTExpression;
class ASTStatement;
class ASTTree;

class FindSymbolsQuery {
public:
    FindSymbolsQuery() = delete;
    FindSymbolsQuery(const FindSymbolsQuery& other) = delete;

    //! Find all symbols in the given tree.
    /**
        @param tree tree to search
        @param fixSymbols whether to fix symbol infos
        @param bareNames whether to return bare symbol names (without namespace and class prefixes)

        @return new list of matching symbols
     */
    static std::vector<ASTSymbolInfo>* find(ASTTree* tree, bool fixSymbols = true, bool bareNames = false);

private:
    static void inDeclaration(std::vector<ASTSymbolInfo>* vec, ASTDeclaration* decl);
    static void inExpression(std::vector<ASTSymbolInfo>* vec, ASTExpression* expr);
    static void inName(std::vector<ASTSymbolInfo>* vec, ASTName& name);
    static void inName(std::vector<ASTSymbolInfo>* vec, ASTName* name);
    static void inStatement(std::vector<ASTSymbolInfo>* vec, ASTStatement* stmt);

    static void fixClassInfo(ASTSymbolInfo& si, std::vector<ASTNode*>* nodes, bool bareNames);
    static void fixConstantInfo(ASTSymbolInfo& si, std::vector<ASTNode*>* nodes, bool bareNames);
    static void fixFunctionInfo(ASTSymbolInfo& si, std::vector<ASTNode*>* nodes, bool bareNames);
    static void fixHashDeclInfo(ASTSymbolInfo& si, std::vector<ASTNode*>* nodes, bool bareNames);
    static void fixHashMemberInfo(ASTSymbolInfo& si, std::vector<ASTNode*>* nodes, bool bareNames);
    static void fixVariableInfo(ASTSymbolInfo& si, std::vector<ASTNode*>* nodes, bool bareNames);
    static void fixSymbolInfos(ASTTree* tree, std::vector<ASTSymbolInfo>* vec, bool bareNames);
};

#endif // _QLS_QUERIES_FINDSYMBOLSQUERY_H
