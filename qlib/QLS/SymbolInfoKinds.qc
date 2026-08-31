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

#! Describes the kind of symbol returned by the astparser module.
/** The values are aliases of the astparser constants so QLS cannot drift from
    the parser ABI.  Conversion to Language Server Protocol symbol kinds is
    performed explicitly at the protocol boundary.
 */
public const ASTSymbolKind = {
    "None": astparser::ASYK_None,
    "File": astparser::ASYK_File,
    "Module": astparser::ASYK_Module,
    "Namespace": astparser::ASYK_Namespace,
    "Package": astparser::ASYK_Package,
    "Class": astparser::ASYK_Class,
    "Method": astparser::ASYK_Method,
    "Property": astparser::ASYK_Property,
    "Field": astparser::ASYK_Field,
    "Constructor": astparser::ASYK_Constructor,
    "Enum": astparser::ASYK_Enum,
    "Interface": astparser::ASYK_Interface,
    "Function": astparser::ASYK_Function,
    "Variable": astparser::ASYK_Variable,
    "Constant": astparser::ASYK_Constant,
    "String": astparser::ASYK_String,
    "Number": astparser::ASYK_Number,
    "Boolean": astparser::ASYK_Boolean,
    "Array": astparser::ASYK_Array,
    "TypeAlias": astparser::ASYK_TypeAlias,
};

#! Describes in what situation a symbol was used.
public const ASTSymbolUsageKind = {
    "None": astparser::ASUK_None,

    # Declarations.
    "ClassDeclName": astparser::ASUK_ClassDeclName,
    "ConstantDeclName": astparser::ASUK_ConstantDeclName,
    "FuncDeclName": astparser::ASUK_FuncDeclName,
    "FuncReturnType": astparser::ASUK_FuncReturnType,
    "NamespaceDeclName": astparser::ASUK_NamespaceDeclName,
    "SuperclassDeclName": astparser::ASUK_SuperclassDeclName,
    "VarDeclName": astparser::ASUK_VarDeclName,
    "VarDeclTypeName": astparser::ASUK_VarDeclTypeName,
    "HashDeclName": astparser::ASUK_HashDeclName,
    "HashMemberName": astparser::ASUK_HashMemberName,
    "TypedefDeclName": astparser::ASUK_TypedefDeclName,
    "TypedefTypeName": astparser::ASUK_TypedefTypeName,
    "ModuleDeclName": astparser::ASUK_ModuleDeclName,

    # Expressions.
    "AccessVariable": astparser::ASUK_AccessVariable,
    "AccessMember": astparser::ASUK_AccessMember,
    "AssignmentLeft": astparser::ASUK_AssignmentLeft,
    "AssignmentRight": astparser::ASUK_AssignmentRight,
    "BinaryLeft": astparser::ASUK_BinaryLeft,
    "BinaryRight": astparser::ASUK_BinaryRight,
    "CallTarget": astparser::ASUK_CallTarget,
    "CallArgs": astparser::ASUK_CallArgs,
    "CaseExpr": astparser::ASUK_CaseExpr,
    "CastType": astparser::ASUK_CastType,
    "CastObject": astparser::ASUK_CastObject,
    "FindData": astparser::ASUK_FindData,
    "HashElement": astparser::ASUK_HashElement,
    "IndexVariable": astparser::ASUK_IndexVariable,
    "IndexIndex": astparser::ASUK_IndexIndex,
    "ListElement": astparser::ASUK_ListElement,
    "ReturnsType": astparser::ASUK_ReturnsType,
    "TernaryCond": astparser::ASUK_TernaryCond,
    "TernaryTrue": astparser::ASUK_TernaryTrue,
    "TernaryFalse": astparser::ASUK_TernaryFalse,
    "Unary": astparser::ASUK_Unary,
    "RangeLeft": astparser::ASUK_RangeLeft,
    "RangeRight": astparser::ASUK_RangeRight,
    "HashdeclHashHashdecl": astparser::ASUK_HashdeclHashHashdecl,

    # Statements.
    "ContextStmtName": astparser::ASUK_ContextStmtName,
    "ContextStmtData": astparser::ASUK_ContextStmtData,
    "DoWhileStmtCond": astparser::ASUK_DoWhileStmtCond,
    "ExprStmtExpr": astparser::ASUK_ExprStmtExpr,
    "ForeachStmtSrc": astparser::ASUK_ForeachStmtSrc,
    "ForeachStmtVal": astparser::ASUK_ForeachStmtVal,
    "ForStmtCond": astparser::ASUK_ForStmtCond,
    "ForStmtInit": astparser::ASUK_ForStmtInit,
    "ForStmtIter": astparser::ASUK_ForStmtIter,
    "IfStmtCond": astparser::ASUK_IfStmtCond,
    "ReturnStmtVal": astparser::ASUK_ReturnStmtVal,
    "SummarizeStmtName": astparser::ASUK_SummarizeStmtName,
    "SummarizeStmtData": astparser::ASUK_SummarizeStmtData,
    "SwitchStmtVar": astparser::ASUK_SwitchStmtVar,
    "ThrowStmtExpr": astparser::ASUK_ThrowStmtExpr,
    "TryStmtCatchVar": astparser::ASUK_TryStmtCatchVar,
    "WhileStmtCond": astparser::ASUK_WhileStmtCond,
    "AssertStmtExpr": astparser::ASUK_AssertStmtExpr,
    "DebugStmtExpr": astparser::ASUK_DebugStmtExpr,
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
    "TypedefDeclName": (ASTSymbolKind.TypeAlias,),
    "TypedefTypeName": (ASTSymbolKind.Class, ASTSymbolKind.Interface, ASTSymbolKind.TypeAlias),
    "ModuleDeclName": (ASTSymbolKind.Module,),

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
    "HashElement": (ASTSymbolKind.Variable, ASTSymbolKind.Constant),
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
    "AssertStmtExpr": (ASTSymbolKind.Variable, ASTSymbolKind.Constant, ASTSymbolKind.Function,
        ASTSymbolKind.Method),
    "DebugStmtExpr": (ASTSymbolKind.Variable, ASTSymbolKind.Constant, ASTSymbolKind.Function,
        ASTSymbolKind.Method),
};

#! Map describing possible symbol kinds for symbol usage kinds.
public const SymbolUsageFastMap = map {string($1.value): SymbolUsageStringMap{$1.key} }, ASTSymbolUsageKind.pairIterator();

} # namespace QLS
