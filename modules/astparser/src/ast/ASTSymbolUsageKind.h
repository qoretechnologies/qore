/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  ASTSymbolUsageKind.h

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

  Note that the Qore library is released under a choice of three open-source
  licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
  information.
*/

#ifndef _QLS_AST_ASTSYMBOLUSAGEKIND_H
#define _QLS_AST_ASTSYMBOLUSAGEKIND_H

//! Describes in what situation a symbol was used.
enum ASTSymbolUsageKind {
    ASUK_None = 0,

    // Declarations.
    ASUK_ClassDeclName = 100,
    ASUK_ConstantDeclName = 101,
    ASUK_FuncDeclName = 102,
    ASUK_FuncReturnType = 103,
    ASUK_NamespaceDeclName = 104,
    ASUK_SuperclassDeclName = 105,
    ASUK_VarDeclName = 106,
    ASUK_VarDeclTypeName = 107,
    ASUK_HashDeclName = 108,
    ASUK_HashMemberName = 109,
    ASUK_TypedefDeclName = 110,
    ASUK_TypedefTypeName = 111,
    ASUK_ModuleDeclName = 112,

    // Expressions.
    ASUK_AccessVariable = 200,
    ASUK_AccessMember = 201,
    ASUK_AssignmentLeft = 202,
    ASUK_AssignmentRight = 203,
    ASUK_BinaryLeft = 204,
    ASUK_BinaryRight = 205,
    ASUK_CallTarget = 206,
    ASUK_CallArgs = 207,
    ASUK_CaseExpr = 208,
    ASUK_CastType = 209,
    ASUK_CastObject = 210,
    ASUK_FindData = 211,
    ASUK_HashElement = 212,
    ASUK_IndexVariable = 213,
    ASUK_IndexIndex = 214,
    ASUK_ListElement = 215,
    ASUK_ReturnsType = 216,
    ASUK_TernaryCond = 217,
    ASUK_TernaryTrue = 218,
    ASUK_TernaryFalse = 219,
    ASUK_Unary = 220,
    ASUK_RangeLeft = 221,
    ASUK_RangeRight = 222,
    ASUK_HashdeclHashHashdecl = 223,

    // Statements.
    ASUK_ContextStmtName = 300,
    ASUK_ContextStmtData = 301,
    ASUK_DoWhileStmtCond = 302,
    ASUK_ExprStmtExpr = 303,
    ASUK_ForeachStmtSrc = 304,
    ASUK_ForeachStmtVal = 305,
    ASUK_ForStmtCond = 306,
    ASUK_ForStmtInit = 307,
    ASUK_ForStmtIter = 308,
    ASUK_IfStmtCond = 309,
    ASUK_ReturnStmtVal = 310,
    ASUK_SummarizeStmtName = 311,
    ASUK_SummarizeStmtData = 312,
    ASUK_SwitchStmtVar = 313,
    ASUK_ThrowStmtExpr = 314,
    ASUK_TryStmtCatchVar = 315,
    ASUK_WhileStmtCond = 316,
    ASUK_AssertStmtExpr = 317,
    ASUK_DebugStmtExpr = 318,
};

#endif // _QLS_AST_ASTSYMBOLUSAGEKIND_H
