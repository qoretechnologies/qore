# -*- mode: qore; indent-tabs-mode: nil -*-

/*  SymbolInfoKinds.qc Copyright 2017 - 2026 Qore Technologies, s.r.o.

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

public namespace QLS {

#! Describes the kind of symbol.
/**
    As described in https://github.com/Microsoft/language-server-protocol/blob/master/protocol.md
 */
public const ASTSymbolKind = {
    "None": 0,
    "File": 1,
    "Module": 2,
    "Namespace": 3,
    "Package": 4,
    "Class": 5,
    "Method": 6,
    "Property": 7,
    "Field": 8,
    "Constructor": 9,
    "Enum": 10,
    "Interface": 11,
    "Function": 12,
    "Variable": 13,
    "Constant": 14,
    "String": 15,
    "Number": 16,
    "Boolean": 17,
    "Array": 18,
};

#! Describes in what situation a symbol was used.
public const ASTSymbolUsageKind = {
    "None": 0,

    # Declarations.
    "ClassDeclName": 100,
    "ConstantDeclName": 101,
    "FuncDeclName": 102,
    "FuncReturnType": 103,
    "NamespaceDeclName": 104,
    "SuperclassDeclName": 105,
    "VarDeclName": 106,
    "VarDeclTypeName": 107,
    "HashDeclName": 108,
    "HashMemberName": 109,

    # Expressions.
    "AccessVariable": 200,
    "AccessMember": 201,
    "AssignmentLeft": 202,
    "AssignmentRight": 203,
    "BinaryLeft": 204,
    "BinaryRight": 205,
    "CallTarget": 206,
    "CallArgs": 207,
    "CaseExpr": 208,
    "CastType": 209,
    "CastObject": 210,
    "FindData": 211,
    "HashValue": 212,
    "IndexVariable": 213,
    "IndexIndex": 214,
    "ListElement": 215,
    "ReturnsType": 216,
    "TernaryCond": 217,
    "TernaryTrue": 218,
    "TernaryFalse": 219,
    "Unary": 220,
    "RangeLeft": 221,
    "RangeRight": 222,
    "HashdeclHashHashdecl": 223,

    # Statements.
    "ContextStmtName": 300,
    "ContextStmtData": 301,
    "DoWhileStmtCond": 302,
    "ExprStmtExpr": 303,
    "ForeachStmtSrc": 304,
    "ForeachStmtVal": 305,
    "ForStmtCond": 306,
    "ForStmtInit": 307,
    "ForStmtIter": 308,
    "IfStmtCond": 309,
    "ReturnStmtVal": 310,
    "SummarizeStmtName": 311,
    "SummarizeStmtData": 312,
    "SwitchStmtVar": 313,
    "ThrowStmtExpr": 314,
    "TryStmtCatchVar": 315,
    "WhileStmtCond": 316,
};

#! Map describing possible symbol kinds for symbol usage kinds.
public const SymbolUsageStringMap = {
    "None": (),

    # Declarations.
    "ClassDeclName": (ASTSymbolKind.Class,),
    "ConstantDeclName": (ASTSymbolKind.Constant,),
    "FuncDeclName": (ASTSymbolKind.Function, ASTSymbolKind.Method, ASTSymbolKind.Constructor),
    "FuncReturnType": (ASTSymbolKind.Class,),
    "NamespaceDeclName": (ASTSymbolKind.Namespace,),
    "SuperclassDeclName": (ASTSymbolKind.Class,),
    "VarDeclName": (ASTSymbolKind.Variable,),
    "VarDeclTypeName": (ASTSymbolKind.Class,),
    "HashDeclName": (ASTSymbolKind.Interface, ASTSymbolKind.Class),
    "HashMemberName": (ASTSymbolKind.Field, ASTSymbolKind.Variable),

    # Expressions.
    "AccessVariable": (ASTSymbolKind.Variable, ASTSymbolKind.Constant, ),
    "AccessMember": (ASTSymbolKind.Variable, ASTSymbolKind.Field, ASTSymbolKind.Constructor, ASTSymbolKind.Method),
    "AssignmentLeft": (ASTSymbolKind.Variable,),
    "AssignmentRight": (ASTSymbolKind.Variable, ASTSymbolKind.Constant, ASTSymbolKind.Function, ASTSymbolKind.Method, ASTSymbolKind.Constructor),
    "BinaryLeft": (ASTSymbolKind.Variable, ASTSymbolKind.Constant),
    "BinaryRight": (ASTSymbolKind.Variable, ASTSymbolKind.Constant),
    "CallTarget": (ASTSymbolKind.Function, ASTSymbolKind.Method, ASTSymbolKind.Class, ASTSymbolKind.Variable),
    "CallArgs": (ASTSymbolKind.Variable, ASTSymbolKind.Constant),
    "CaseExpr": (ASTSymbolKind.Constant,),
    "CastType": (ASTSymbolKind.Class,),
    "CastObject": (ASTSymbolKind.Variable, ASTSymbolKind.Constant),
    "FindData": (ASTSymbolKind.Variable,),
    "HashValue": (ASTSymbolKind.Variable, ASTSymbolKind.Constant),
    "IndexVariable": (ASTSymbolKind.Variable, ASTSymbolKind.Constant),
    "IndexIndex": (ASTSymbolKind.Variable, ASTSymbolKind.Constant),
    "ListElement": (ASTSymbolKind.Variable, ASTSymbolKind.Constant),
    "ReturnsType": (ASTSymbolKind.Class,),
    "TernaryCond": (ASTSymbolKind.Variable, ASTSymbolKind.Constant),
    "TernaryTrue": (ASTSymbolKind.Variable, ASTSymbolKind.Constant),
    "TernaryFalse": (ASTSymbolKind.Variable, ASTSymbolKind.Constant),
    "Unary": (ASTSymbolKind.Variable, ASTSymbolKind.Constant, ASTSymbolKind.Function, ASTSymbolKind.Method, ASTSymbolKind.Constructor),
    "RangeLeft": list(),
    "RangeRight": list(),
    "HashdeclHashHashdecl": (ASTSymbolKind.Interface,),

    # Statements.
    "ContextStmtName": (ASTSymbolKind.Variable, ASTSymbolKind.Constant),
    "ContextStmtData": (ASTSymbolKind.Variable, ASTSymbolKind.Constant),
    "DoWhileStmtCond": (ASTSymbolKind.Variable, ASTSymbolKind.Constant),
    "ExprStmtExpr": (ASTSymbolKind.Class, ASTSymbolKind.Constant, ASTSymbolKind.Function, ASTSymbolKind.Constructor, ASTSymbolKind.Method, ASTSymbolKind.Variable, ASTSymbolKind.Namespace),
    "ForeachStmtSrc": (ASTSymbolKind.Variable,),
    "ForeachStmtVal": (ASTSymbolKind.Variable,),
    "ForStmtCond": (ASTSymbolKind.Variable, ASTSymbolKind.Constant),
    "ForStmtInit": (ASTSymbolKind.Variable, ASTSymbolKind.Constant),
    "ForStmtIter": (ASTSymbolKind.Variable, ASTSymbolKind.Constant),
    "IfStmtCond": (ASTSymbolKind.Variable, ASTSymbolKind.Constant),
    "ReturnStmtVal": (ASTSymbolKind.Variable, ASTSymbolKind.Constant),
    "SummarizeStmtName": (ASTSymbolKind.Variable, ASTSymbolKind.Constant),
    "SummarizeStmtData": (ASTSymbolKind.Variable, ASTSymbolKind.Constant),
    "SwitchStmtVar": (ASTSymbolKind.Variable, ASTSymbolKind.Constant),
    "ThrowStmtExpr": (ASTSymbolKind.Variable, ASTSymbolKind.Constant),
    "TryStmtCatchVar": (ASTSymbolKind.Variable,),
    "WhileStmtCond": (ASTSymbolKind.Variable, ASTSymbolKind.Constant),
};

#! Map describing possible symbol kinds for symbol usage kinds.
public const SymbolUsageFastMap = map {string($1.value): SymbolUsageStringMap{$1.key} }, ASTSymbolUsageKind.pairIterator();

} # namespace QLS
