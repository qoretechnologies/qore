/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  AstTreePrinter.cpp

  Qore AST Parser

  Copyright (C) 2017 - 2026 Qore Technologies, s.r.o.

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

#include "AstTreePrinter.h"

#include "ast/AST.h"

static void printIndent(std::ostream& os, int indent) {
    for (int i = 0; i < indent; i++)
        os << "  ";
}

static void printString(std::ostream& os, const char* str, int indent) {
    printIndent(os, indent);
    os << str;
}

void AstTreePrinter::printDeclaration(std::ostream& os, ASTDeclaration* decl, int indent) {
    if (!decl) {
        printString(os, "<null declaration>\n", indent);
        return;
    }

    switch (decl->getKind()) {
        case ASTDeclarationKind::ADK_Class: {
            ASTClassDeclaration* d = static_cast<ASTClassDeclaration*>(decl);
            printString(os, "ClassDecl ", indent);
            printLocation(os, d->loc, 0);
            printModifiers(os, d->modifiers, indent+1);
            printName(os, d->name, indent+1);
            if (d->inherits.size() > 0) {
                printString(os, "inherits:\n", indent+1);
                for (size_t i = 0, count = d->inherits.size(); i < count; i++)
                    printDeclaration(os, d->inherits[i], indent+2);
            }
            if (d->declarations.size() > 0) {
                printString(os, "declarations:\n", indent+1);
                for (size_t i = 0, count = d->declarations.size(); i < count; i++)
                    printDeclaration(os, d->declarations[i], indent+2);
            }
            break;
        }
        case ASTDeclarationKind::ADK_Closure: {
            ASTClosureDeclaration* d = static_cast<ASTClosureDeclaration*>(decl);
            printString(os, "ClosureDecl ", indent);
            printLocation(os, d->loc, 0);
            printModifiers(os, d->modifiers, indent+1);
            if (d->returnType.get()) {
                printString(os, "returnType:\n", indent+1);
                printExpression(os, d->returnType.get(), indent+2);
            }
            if (d->params.get()) {
                printString(os, "params:\n", indent+1);
                printExpression(os, d->params.get(), indent+2);
            }
            printString(os, "body:\n", indent+1);
            printStatement(os, d->body.get(), indent+2);
            break;
        }
        case ASTDeclarationKind::ADK_Constant: {
            ASTConstantDeclaration* d = static_cast<ASTConstantDeclaration*>(decl);
            printString(os, "ConstantDecl ", indent);
            printLocation(os, d->loc, 0);
            printModifiers(os, d->modifiers, indent+1);
            printName(os, d->name, indent+1);
            printExpression(os, d->value.get(), indent+1);
            break;
        }
        case ASTDeclarationKind::ADK_Function: {
            ASTFunctionDeclaration* d = static_cast<ASTFunctionDeclaration*>(decl);
            printString(os, "FunctionDecl ", indent);
            printLocation(os, d->loc, 0);
            printModifiers(os, d->modifiers, indent+1);
            printName(os, d->name, indent+1);
            if (d->returnType.get()) {
                printString(os, "returnType:\n", indent+1);
                printExpression(os, d->returnType.get(), indent+2);
            }
            if (d->params.get()) {
                printString(os, "params:\n", indent+1);
                printExpression(os, d->params.get(), indent+2);
            }
            if (d->inits.get()) {
                printString(os, "inits:\n", indent+1);
                printExpression(os, d->inits.get(), indent+2);
            }
            if (d->body.get()) {
                printString(os, "body:\n", indent+1);
                printStatement(os, d->body.get(), indent+2);
            }
            break;
        }
        case ASTDeclarationKind::ADK_Hash: {
            ASTHashDeclaration* d = static_cast<ASTHashDeclaration*>(decl);
            printString(os, "HashDecl ", indent);
            printLocation(os, d->loc, 0);
            printModifiers(os, d->modifiers, indent+1);
            printName(os, d->name, indent+1);
            if (d->declarations.size() > 0) {
                printString(os, "declarations:\n", indent+1);
                for (size_t i = 0, count = d->declarations.size(); i < count; i++)
                    printDeclaration(os, d->declarations[i], indent+2);
            }
            break;
        }
        case ASTDeclarationKind::ADK_HashMember: {
            ASTHashMemberDeclaration* d = static_cast<ASTHashMemberDeclaration*>(decl);
            printString(os, "HashMemberDecl ", indent);
            printLocation(os, d->loc, 0);
            printName(os, d->typeName, indent+1, true, true, "typeName: ");
            printName(os, d->name, indent+1);
            if (d->init.get()) {
                printString(os, "init:\n", indent+1);
                printExpression(os, d->init.get(), indent+2);
                printString(os, "constr: ", indent+1);
                os << d->constr << "\n";
            }
            break;
        }
        case ASTDeclarationKind::ADK_MemberGroup: {
            ASTMemberGroupDeclaration* d = static_cast<ASTMemberGroupDeclaration*>(decl);
            printString(os, "MemberGroupDecl ", indent);
            printLocation(os, d->loc, 0);
            printModifiers(os, d->modifiers, indent+1);
            if (d->members.size() > 0) {
                for (size_t i = 0, count = d->members.size(); i < count; i++)
                    printExpression(os, d->members[i], indent+1);
            } else {
                printString(os, "<empty>\n", indent+1);
            }
            break;
        }
        case ASTDeclarationKind::ADK_Namespace: {
            ASTNamespaceDeclaration* d = static_cast<ASTNamespaceDeclaration*>(decl);
            printString(os, "NamespaceDecl ", indent);
            printLocation(os, d->loc, 0);
            printModifiers(os, d->modifiers, indent+1);
            printName(os, d->name, indent+1);
            if (d->declarations.size() > 0) {
                for (size_t i = 0, count = d->declarations.size(); i < count; i++)
                    printDeclaration(os, d->declarations[i], indent+1);
            } else {
                printString(os, "<empty>\n", indent+1);
            }
            break;
        }
        case ASTDeclarationKind::ADK_Superclass: {
            ASTSuperclassDeclaration* d = static_cast<ASTSuperclassDeclaration*>(decl);
            printString(os, "SuperclassDecl ", indent);
            printLocation(os, d->loc, 0);
            printModifiers(os, d->modifiers, indent+1);
            printName(os, d->name, indent+1);
            break;
        }
        case ASTDeclarationKind::ADK_Variable: {
            ASTVariableDeclaration* d = static_cast<ASTVariableDeclaration*>(decl);
            printString(os, "VariableDecl ", indent);
            printLocation(os, d->loc, 0);
            printModifiers(os, d->modifiers, indent+1);
            printName(os, d->typeName, indent+1, true, true, "typeName: ");
            printName(os, d->name, indent+1);
            break;
        }
        case ASTDeclarationKind::ADK_VarList: {
            ASTVarListDeclaration* d = static_cast<ASTVarListDeclaration*>(decl);
            printString(os, "VarListDecl ", indent);
            printLocation(os, d->loc, 0);
            printModifiers(os, d->modifiers, indent+1);
            if (d->variables.get()) {
                printExpression(os, d->variables.get(), indent+1);
            } else {
                printString(os, "<empty>\n", indent+1);
            }
            break;
        }
        case ASTDeclarationKind::ADK_Typedef: {
            ASTTypedefDeclaration* d = static_cast<ASTTypedefDeclaration*>(decl);
            printString(os, "TypedefDecl ", indent);
            printLocation(os, d->loc, 0);
            printModifiers(os, d->modifiers, indent+1);
            printName(os, d->name, indent+1);
            printName(os, d->typeName, indent+1, true, true, "typeName: ");
            break;
        }
        default:
            break;
    }
}

void AstTreePrinter::printExpression(std::ostream& os, ASTExpression* expr, int indent) {
    if (!expr) {
        printString(os, "<null expression>\n", indent);
        return;
    }

    switch (expr->getKind()) {
        case ASTExpressionKind::AEK_Access: {
            ASTAccessExpression* e = static_cast<ASTAccessExpression*>(expr);
            printString(os, "AccessExpr ", indent);
            printLocation(os, e->loc, 0);
            printString(os, "variable:\n", indent+1);
            printExpression(os, e->variable.get(), indent+2);
            printString(os, "member:\n", indent+1);
            printExpression(os, e->member.get(), indent+2);
            break;
        }
        case ASTExpressionKind::AEK_Assignment: {
            ASTAssignmentExpression* e = static_cast<ASTAssignmentExpression*>(expr);
            printString(os, "AssignmentExpr ", indent);
            printLocation(os, e->loc, 0);
            printString(os, "left:\n", indent+1);
            printExpression(os, e->left.get(), indent+2);
            printString(os, "right:\n", indent+1);
            printExpression(os, e->right.get(), indent+2);
            printString(os, "weak: ", indent+1);
            os << e->weak << "\n";
            break;
        }
        case ASTExpressionKind::AEK_Backquote: {
            ASTBackquoteExpression* e = static_cast<ASTBackquoteExpression*>(expr);
            printString(os, "BackquoteExpr ", indent);
            printLocation(os, e->loc, 0, false);
            os << "; cmd=`" << e->command << "`\n";
            break;
        }
        case ASTExpressionKind::AEK_Binary: {
            ASTBinaryExpression* e = static_cast<ASTBinaryExpression*>(expr);
            printString(os, "BinaryExpr ", indent);
            printLocation(os, e->loc, 0, false);
            printString(os, "; op=", 0);
            printOperator(os, e->op, 0, true);
            printString(os, "left:\n", indent+1);
            printExpression(os, e->left.get(), indent+2);
            printString(os, "right:\n", indent+1);
            printExpression(os, e->right.get(), indent+2);
            break;
        }
        case ASTExpressionKind::AEK_Call: {
            ASTCallExpression* e = static_cast<ASTCallExpression*>(expr);
            printString(os, "CallExpr ", indent);
            printLocation(os, e->loc, 0);
            printString(os, "target:\n", indent+1);
            printExpression(os, e->target.get(), indent+2);
            printString(os, "args:\n", indent+1);
            printExpression(os, e->args.get(), indent+2);
            break;
        }
        case ASTExpressionKind::AEK_Case: {
            ASTCaseExpression* e = static_cast<ASTCaseExpression*>(expr);
            printString(os, "CaseExpr ", indent);
            printLocation(os, e->loc, 0);
            if (e->defaultCase) {
                printString(os, "<default case>\n", indent+1);
            } else {
                printString(os, "op: ", indent+1);
                printOperator(os, e->op, 0, true);
                printString(os, "caseExpr:\n", indent+1);
                printExpression(os, e->caseExpr.get(), indent+2);
            }
            if (e->statements.get()) {
                printString(os, "statements:\n", indent+1);
                printStatement(os, e->statements.get(), indent+2);
            }
            break;
        }
        case ASTExpressionKind::AEK_Cast: {
            ASTCastExpression* e = static_cast<ASTCastExpression*>(expr);
            printString(os, "CastExpr ", indent);
            printLocation(os, e->loc, 0);
            printName(os, e->castType, indent+1, true, true, "castType: ");
            printExpression(os, e->obj.get(), indent+1);
            break;
        }
        case ASTExpressionKind::AEK_Closure: {
            ASTClosureExpression* e = static_cast<ASTClosureExpression*>(expr);
            printString(os, "ClosureExpr ", indent);
            printLocation(os, e->loc, 0);
            printDeclaration(os, e->closure.get(), indent+1);
            break;
        }
        case ASTExpressionKind::AEK_ConstrInit: {
            ASTConstrInitExpression* e = static_cast<ASTConstrInitExpression*>(expr);
            printString(os, "ConstrInitExpr ", indent);
            printLocation(os, e->loc, 0);
            if (e->inits.size() > 0) {
                for (size_t i = 0, count = e->inits.size(); i < count; i++)
                    printExpression(os, e->inits[i], indent+1);
            }
            break;
        }
        case ASTExpressionKind::AEK_ContextMod: {
            ASTContextModExpression* e = static_cast<ASTContextModExpression*>(expr);
            printString(os, "ContextModExpr ", indent);
            printLocation(os, e->loc, 0, false);
            if (e->acmeKind == ACMEK_SortBy)
                os << " (SortBy)\n";
            else if (e->acmeKind == ACMEK_SortDescBy)
                os << " (SortDescBy)\n";
            else if (e->acmeKind == ACMEK_Where)
                os << " (Where)\n";
            printExpression(os, e->expression.get(), indent+1);
            break;
        }
        case ASTExpressionKind::AEK_ContextRow: {
            printString(os, "ContextRowExpr ", indent);
            printLocation(os, expr->loc, 0);
            break;
        }
        case ASTExpressionKind::AEK_Decl: {
            ASTDeclExpression* e = static_cast<ASTDeclExpression*>(expr);
            printString(os, "DeclExpr ", indent);
            printLocation(os, e->loc, 0);
            printDeclaration(os, e->declaration.get(), indent+1);
            break;
        }
        case ASTExpressionKind::AEK_Find: {
            ASTFindExpression* e = static_cast<ASTFindExpression*>(expr);
            printString(os, "FindExpr ", indent);
            printLocation(os, e->loc, 0);
            printString(os, "result:\n", indent+1);
            printExpression(os, e->result.get(), indent+2);
            printString(os, "data:\n", indent+1);
            printExpression(os, e->data.get(), indent+2);
            printString(os, "where:\n", indent+1);
            printExpression(os, e->where.get(), indent+2);
            break;
        }
        case ASTExpressionKind::AEK_Hash: {
            ASTHashExpression* e = static_cast<ASTHashExpression*>(expr);
            printString(os, "HashExpr ", indent);
            printLocation(os, e->loc, 0);
            if (e->elements.size() > 0) {
                for (size_t i = 0, count = e->elements.size(); i < count; i++)
                    printExpression(os, e->elements[i], indent+1);
            }
            else {
                printString(os, "<empty>\n", indent+1);
            }
            break;
        }
        case ASTExpressionKind::AEK_HashdeclHash: {
            ASTHashdeclHashExpression* e = static_cast<ASTHashdeclHashExpression*>(expr);
            printString(os, "HashdeclHashExpr ", indent);
            printLocation(os, e->loc, 0);
            printName(os, e->hashdecl, indent+1, true, true, "hashdecl: ");
            printString(os, "hash:\n", indent+1);
            printExpression(os, e->hash.get(), indent+2);
            break;
        }
        case ASTExpressionKind::AEK_HashElement: {
            ASTHashElementExpression* e = static_cast<ASTHashElementExpression*>(expr);
            printString(os, "HashElementExpr ", indent);
            printLocation(os, e->loc, 0);
            printString(os, "key:\n", indent+1);
            printExpression(os, e->key.get(), indent+2);
            printString(os, "value:\n", indent+1);
            printExpression(os, e->value.get(), indent+2);
            break;
        }
        case ASTExpressionKind::AEK_ImplicitArg: {
            ASTImplicitArgExpression* e = static_cast<ASTImplicitArgExpression*>(expr);
            printString(os, "ImplicitArgExpr ", indent);
            printLocation(os, e->loc, 0, false);
            os << "; offset=" << e->offset << "\n";
            break;
        }
        case ASTExpressionKind::AEK_ImplicitElem: {
            printString(os, "ImplicitElemExpr ", indent);
            printLocation(os, expr->loc, 0);
            break;
        }
        case ASTExpressionKind::AEK_Index: {
            ASTIndexExpression* e = static_cast<ASTIndexExpression*>(expr);
            printString(os, "IndexExpr ", indent);
            printLocation(os, e->loc, 0, false);
            if (e->indexKind == ASTIndexExpression::IndexKind::AIE_SquareBrackets)
                os << " (square brackets)\n";
            else
                os << " (curly brackets)\n";
            printString(os, "variable:\n", indent+1);
            printExpression(os, e->variable.get(), indent+2);
            printString(os, "index:\n", indent+1);
            printExpression(os, e->index.get(), indent+2);
            break;
        }
        case ASTExpressionKind::AEK_List: {
            ASTListExpression* e = static_cast<ASTListExpression*>(expr);
            printString(os, "ListExpr ", indent);
            printLocation(os, e->loc, 0);
            if (e->elements.size() > 0) {
                for (size_t i = 0, count = e->elements.size(); i < count; i++)
                    printExpression(os, e->elements[i], indent+1);
            } else {
                printString(os, "<empty>\n", indent+1);
            }
            break;
        }
        case ASTExpressionKind::AEK_Literal: {
            ASTLiteralExpression* e = static_cast<ASTLiteralExpression*>(expr);
            printString(os, "LiteralExpr ", indent);
            printLocation(os, e->loc, 0);
            printIndent(os, indent+1);
            if (e->kind == ALEK_Binary)
                os << "binary: " << e->value.str << "\n";
            else if (e->kind == ALEK_Date)
                os << "date: " << e->value.str << "\n";
            else if (e->kind == ALEK_Float)
                os << "float: " << e->value.d << "\n";
            else if (e->kind == ALEK_Int)
                os << "int: " << e->value.i << "\n";
            else if (e->kind == ALEK_Number)
                os << "number: " << e->value.str << "\n";
            else if (e->kind == ALEK_String) {
                if (e->value.stdstr)
                    os << "string: \"" << e->value.stdstr->c_str() << "\"\n";
                else
                    os << "string: (pointer) " << e->value.stdstr << "\n";
            }
            break;
        }
        case ASTExpressionKind::AEK_Name: {
            ASTNameExpression* e = static_cast<ASTNameExpression*>(expr);
            printString(os, "NameExpr ", indent);
            printLocation(os, e->loc, 0, false);
            printName(os, e->name, 0, true, true, ": ");
            break;
        }
        case ASTExpressionKind::AEK_Range: {
            ASTRangeExpression* e = static_cast<ASTRangeExpression*>(expr);
            printString(os, "RangeExpr ", indent);
            printLocation(os, e->loc, 0, true);
            printString(os, "left:\n", indent+1);
            printExpression(os, e->left.get(), indent+2);
            printString(os, "right:\n", indent+1);
            printExpression(os, e->right.get(), indent+2);
            break;
        }
        case ASTExpressionKind::AEK_Regex: {
            ASTRegexExpression* e = static_cast<ASTRegexExpression*>(expr);
            printString(os, "RegexExpr ", indent);
            printLocation(os, e->loc, 0);
            printString(os, "options:", indent+1);
            if (e->extractRegex)
                os << " extractRegex";
            if (e->caseSensitive)
                os << " caseSensitive";
            if (e->extended)
                os << " extended";
            if (e->dotAll)
                os << " dotAll";
            if (e->multiline)
                os << " multiline";
            if (e->global)
                os << " global";
            os << "\n";
            printString(os, "regex: \"", indent+1);
            os << e->str << "\"\n";
            break;
        }
        case ASTExpressionKind::AEK_RegexSubst: {
            ASTRegexSubstExpression* e = static_cast<ASTRegexSubstExpression*>(expr);
            printString(os, "RegexSubstExpr ", indent);
            printLocation(os, e->loc, 0);
            printString(os, "options:", indent+1);
            if (e->caseSensitive)
                os << " caseSensitive";
            if (e->extended)
                os << " extended";
            if (e->dotAll)
                os << " dotAll";
            if (e->multiline)
                os << " multiline";
            if (e->global)
                os << " global";
            os << "\n";
            printString(os, "source: \"", indent+1);
            os << e->source << "\"\n";
            printString(os, "target: \"", indent+1);
            os << e->target << "\"\n";
            break;
        }
        case ASTExpressionKind::AEK_RegexTrans: {
            ASTRegexTransExpression* e = static_cast<ASTRegexTransExpression*>(expr);
            printString(os, "RegexTransExpr ", indent);
            printLocation(os, e->loc, 0);
            printString(os, "source: \"", indent+1);
            os << e->source << "\"\n";
            printString(os, "target: \"", indent+1);
            os << e->target << "\"\n";
            break;
        }
        case ASTExpressionKind::AEK_Returns: {
            ASTReturnsExpression* e = static_cast<ASTReturnsExpression*>(expr);
            printString(os, "ReturnsExpr ", indent);
            printLocation(os, e->loc, 0);
            printExpression(os, e->typeName.get(), indent+1);
            break;
        }
        case ASTExpressionKind::AEK_SwitchBody: {
            ASTSwitchBodyExpression* e = static_cast<ASTSwitchBodyExpression*>(expr);
            printString(os, "SwitchBodyExpr ", indent);
            printLocation(os, e->loc, 0);
            if (e->cases.size() > 0) {
                for (size_t i = 0, count = e->cases.size(); i < count; i++)
                    printExpression(os, e->cases[i], indent+1);
            } else {
                printString(os, "<empty>\n", indent+1);
            }
            break;
        }
        case ASTExpressionKind::AEK_Ternary: {
            ASTTernaryExpression* e = static_cast<ASTTernaryExpression*>(expr);
            printString(os, "TernaryExpr ", indent);
            printLocation(os, e->loc, 0);
            printString(os, "condition:\n", indent+1);
            printExpression(os, e->condition.get(), indent+2);
            printString(os, "ifTrue:\n", indent+1);
            printExpression(os, e->exprTrue.get(), indent+2);
            printString(os, "ifFalse:\n", indent+1);
            printExpression(os, e->exprFalse.get(), indent+2);
            break;
        }
        case ASTExpressionKind::AEK_Unary: {
            ASTUnaryExpression* e = static_cast<ASTUnaryExpression*>(expr);
            printString(os, "UnaryExpr ", indent);
            printLocation(os, e->loc, 0, false);
            printString(os, "; op=", 0);
            printOperator(os, e->op, 0, true);
            printExpression(os, e->expression.get(), indent+1);
            break;
        }
        default:
            break;
    }
}

void AstTreePrinter::printLocation(std::ostream& os, const ASTParseLocation& loc, int indent, bool newline) {
    printIndent(os, indent);
    os << "<" << loc.firstLine << ":" << loc.firstCol << "-" << loc.lastLine << ":" << loc.lastCol << ">";
    if (newline)
        os << "\n";
}

void AstTreePrinter::printModifiers(std::ostream& os, ASTModifiers mods, int indent, bool modsOnly, bool newline) {
    if (mods.empty())
        return;
    printIndent(os, indent);
    if (!modsOnly)
        os << "modifiers: ";

    bool notFirst = false;
    if (mods.contains(AM_Transient)) {
        if (notFirst)
            os << " ";
        os << "transient";
        notFirst = true;
    }
    if (mods.contains(AM_Abstract)) {
        if (notFirst)
            os << " ";
        os << "abstract";
        notFirst = true;
    }
    if (mods.contains(AM_Our)) {
        if (notFirst)
            os << " ";
        os << "our";
        notFirst = true;
    }
    if (mods.contains(AM_ThreadLocal)) {
        if (notFirst)
            os << " ";
        os << "thread_local";
        notFirst = true;
    }
    if (mods.contains(AM_My)) {
        if (notFirst)
            os << " ";
        os << "my";
        notFirst = true;
    }
    if (mods.contains(AM_Public)) {
        if (notFirst)
            os << " ";
        os << "public";
        notFirst = true;
    }
    if (mods.contains(AM_Private)) {
        if (notFirst)
            os << " ";
        os << "private";
        notFirst = true;
    }
    if (mods.contains(AM_PrivateHierarchy)) {
        if (notFirst)
            os << " ";
        os << "private:hierarchy";
        notFirst = true;
    }
    if (mods.contains(AM_PrivateInternal)) {
        if (notFirst)
            os << " ";
        os << "private:internal";
        notFirst = true;
    }
    if (mods.contains(AM_Static)) {
        if (notFirst)
            os << " ";
        os << "static";
        notFirst = true;
    }
    if (mods.contains(AM_Synchronized)) {
        if (notFirst)
            os << " ";
        os << "synchronized";
        notFirst = true;
    }
    if (mods.contains(AM_Final)) {
        if (notFirst)
            os << " ";
        os << "final";
        notFirst = true;
    }
    if (mods.contains(AM_Deprecated)) {
        if (notFirst)
            os << " ";
        os << "deprecated";
        notFirst = true;
    }

    if (newline)
        os << "\n";
}

void AstTreePrinter::printName(std::ostream& os, ASTName& name, int indent, bool location, bool newline, const char* prefix) {
    printIndent(os, indent);
    os << prefix << "\"" << name.name;
    if (location) {
        os <<  "\" ";
        printLocation(os, name.loc, 0, newline);
    } else {
        os <<  "\"";
        if (newline)
            os << "\n";
    }
}

void AstTreePrinter::printOperator(std::ostream& os, ASTOperator op, int indent, bool newline) {
    printIndent(os, indent);
    switch (op.op) {
        case AOK_None: os << "None"; if (newline) os << "\n"; break;
        case AOK_Background: os << "Background"; if (newline) os << "\n"; break;
        case AOK_Chomp: os << "Chomp"; if (newline) os << "\n"; break;
        case AOK_Delete: os << "Delete"; if (newline) os << "\n"; break;
        case AOK_Elements: os << "Elements"; if (newline) os << "\n"; break;
        case AOK_Exists: os << "Exists"; if (newline) os << "\n"; break;
        case AOK_Extract: os << "Extract"; if (newline) os << "\n"; break;
        case AOK_Foldl: os << "Foldl"; if (newline) os << "\n"; break;
        case AOK_Foldr: os << "Foldr"; if (newline) os << "\n"; break;
        case AOK_Instanceof: os << "Instanceof"; if (newline) os << "\n"; break;
        case AOK_Keys: os << "Keys"; if (newline) os << "\n"; break;
        case AOK_Map: os << "Map"; if (newline) os << "\n"; break;
        case AOK_New: os << "New"; if (newline) os << "\n"; break;
        case AOK_Pop: os << "Pop"; if (newline) os << "\n"; break;
        case AOK_Push: os << "Push"; if (newline) os << "\n"; break;
        case AOK_Remove: os << "Remove"; if (newline) os << "\n"; break;
        case AOK_Select: os << "Select"; if (newline) os << "\n"; break;
        case AOK_Shift: os << "Shift"; if (newline) os << "\n"; break;
        case AOK_Splice: os << "Splice"; if (newline) os << "\n"; break;
        case AOK_Trim: os << "Trim"; if (newline) os << "\n"; break;
        case AOK_Unshift: os << "Unshift"; if (newline) os << "\n"; break;
        case AOK_Reference: os << "Reference"; if (newline) os << "\n"; break;
        case AOK_PreIncrement: os << "PreIncrement"; if (newline) os << "\n"; break;
        case AOK_PostIncrement: os << "PostIncrement"; if (newline) os << "\n"; break;
        case AOK_PreDecrement: os << "PreDecrement"; if (newline) os << "\n"; break;
        case AOK_PostDecrement: os << "PostDecrement"; if (newline) os << "\n"; break;
        case AOK_Plus: os << "Plus"; if (newline) os << "\n"; break;
        case AOK_Minus: os << "Minus"; if (newline) os << "\n"; break;
        case AOK_Multiply: os << "Multiply"; if (newline) os << "\n"; break;
        case AOK_Divide: os << "Divide"; if (newline) os << "\n"; break;
        case AOK_Modulo: os << "Modulo"; if (newline) os << "\n"; break;
        case AOK_UnaryPlus: os << "UnaryPlus"; if (newline) os << "\n"; break;
        case AOK_UnaryMinus: os << "UnaryMinus"; if (newline) os << "\n"; break;
        case AOK_BinaryAnd: os << "BinaryAnd"; if (newline) os << "\n"; break;
        case AOK_BinaryOr: os << "BinaryOr"; if (newline) os << "\n"; break;
        case AOK_BinaryXor: os << "BinaryXor"; if (newline) os << "\n"; break;
        case AOK_BinaryNot: os << "BinaryNot"; if (newline) os << "\n"; break;
        case AOK_ShiftLeft: os << "ShiftLeft"; if (newline) os << "\n"; break;
        case AOK_ShiftRight: os << "ShiftRight"; if (newline) os << "\n"; break;
        case AOK_Assignment: os << "Assignment"; if (newline) os << "\n"; break;
        case AOK_PlusEquals: os << "PlusEquals"; if (newline) os << "\n"; break;
        case AOK_MinusEquals: os << "MinusEquals"; if (newline) os << "\n"; break;
        case AOK_MultiplyEquals: os << "MultiplyEquals"; if (newline) os << "\n"; break;
        case AOK_DivideEquals: os << "DivideEquals"; if (newline) os << "\n"; break;
        case AOK_ModuloEquals: os << "ModuloEquals"; if (newline) os << "\n"; break;
        case AOK_AndEquals: os << "AndEquals"; if (newline) os << "\n"; break;
        case AOK_OrEquals: os << "OrEquals"; if (newline) os << "\n"; break;
        case AOK_XorEquals: os << "XorEquals"; if (newline) os << "\n"; break;
        case AOK_ShiftLeftEquals: os << "ShiftLeftEquals"; if (newline) os << "\n"; break;
        case AOK_ShiftRightEquals: os << "ShiftRightEquals"; if (newline) os << "\n"; break;
        case AOK_Equals: os << "Equals"; if (newline) os << "\n"; break;
        case AOK_NotEquals: os << "NotEquals"; if (newline) os << "\n"; break;
        case AOK_Comparison: os << "Comparison"; if (newline) os << "\n"; break;
        case AOK_GreaterThan: os << "GreaterThan"; if (newline) os << "\n"; break;
        case AOK_GreaterThanOrEquals: os << "GreaterThanOrEquals"; if (newline) os << "\n"; break;
        case AOK_LessThan: os << "LessThan"; if (newline) os << "\n"; break;
        case AOK_LessThanOrEquals: os << "LessThanOrEquals"; if (newline) os << "\n"; break;
        case AOK_AbsoluteEquals: os << "AbsoluteEquals"; if (newline) os << "\n"; break;
        case AOK_AbsoluteNotEquals: os << "AbsoluteNotEquals"; if (newline) os << "\n"; break;
        case AOK_LogicalAnd: os << "LogicalAnd"; if (newline) os << "\n"; break;
        case AOK_LogicalOr: os << "LogicalOr"; if (newline) os << "\n"; break;
        case AOK_LogicalNot: os << "LogicalNot"; if (newline) os << "\n"; break;
        case AOK_NullCoalesce: os << "NullCoalesce"; if (newline) os << "\n"; break;
        case AOK_ValueCoalesce: os << "ValueCoalesce"; if (newline) os << "\n"; break;
        case AOK_RegexMatch: os << "RegexMatch"; if (newline) os << "\n"; break;
        case AOK_RegexNotMatch: os << "RegexNotMatch"; if (newline) os << "\n"; break;
        case AOK_BrokenLogicalOr: os << "BrokenLogicalOr"; if (newline) os << "\n"; break;
        case AOK_BrokenBinaryOr: os << "BrokenBinaryOr"; if (newline) os << "\n"; break;
        case AOK_BrokenBinaryXor: os << "BrokenBinaryXor"; if (newline) os << "\n"; break;
        case AOK_WS_LOGICAL_LE: os << "WS_LOGICAL_LE"; if (newline) os << "\n"; break;
        case AOK_WS_LOGICAL_GE: os << "WS_LOGICAL_GE"; if (newline) os << "\n"; break;
        case AOK_WS_LOGICAL_NE: os << "WS_LOGICAL_NE"; if (newline) os << "\n"; break;
        case AOK_WS_LOGICAL_EQ: os << "WS_LOGICAL_EQ"; if (newline) os << "\n"; break;
        case AOK_WS_ABSOLUTE_EQ: os << "WS_ABSOLUTE_EQ"; if (newline) os << "\n"; break;
        case AOK_WS_ABSOLUTE_NE: os << "WS_ABSOLUTE_NE"; if (newline) os << "\n"; break;
        case AOK_WS_LOGICAL_CMP: os << "WS_LOGICAL_CMP"; if (newline) os << "\n"; break;
        case AOK_WS_LOGICAL_AND: os << "WS_LOGICAL_AND"; if (newline) os << "\n"; break;
        case AOK_WS_LOGICAL_OR: os << "WS_LOGICAL_OR"; if (newline) os << "\n"; break;
        case AOK_WS_BROKEN_LOGICAL_OR: os << "WS_BROKEN_LOGICAL_OR"; if (newline) os << "\n"; break;
        case AOK_WS_SHIFT_RIGHT: os << "WS_SHIFT_RIGHT"; if (newline) os << "\n"; break;
        case AOK_WS_SHIFT_LEFT: os << "WS_SHIFT_LEFT"; if (newline) os << "\n"; break;
        case AOK_WS_PLUS_EQUALS: os << "WS_PLUS_EQUALS"; if (newline) os << "\n"; break;
        case AOK_WS_MINUS_EQUALS: os << "WS_MINUS_EQUALS"; if (newline) os << "\n"; break;
        case AOK_WS_AND_EQUALS: os << "WS_AND_EQUALS"; if (newline) os << "\n"; break;
        case AOK_WS_OR_EQUALS: os << "WS_OR_EQUALS"; if (newline) os << "\n"; break;
        case AOK_WS_MODULO_EQUALS: os << "WS_MODULO_EQUALS"; if (newline) os << "\n"; break;
        case AOK_WS_MULTIPLY_EQUALS: os << "WS_MULTIPLY_EQUALS"; if (newline) os << "\n"; break;
        case AOK_WS_DIVIDE_EQUALS: os << "WS_DIVIDE_EQUALS"; if (newline) os << "\n"; break;
        case AOK_WS_XOR_EQUALS: os << "WS_XOR_EQUALS"; if (newline) os << "\n"; break;
        case AOK_WS_SHIFT_RIGHT_EQUALS: os << "WS_SHIFT_RIGHT_EQUALS"; if (newline) os << "\n"; break;
        case AOK_WS_SHIFT_LEFT_EQUALS: os << "WS_SHIFT_LEFT_EQUALS"; if (newline) os << "\n"; break;
        default: break;
    }
}

void AstTreePrinter::printParseOption(std::ostream& os, ASTParseOption* po, int indent) {
    if (!po)
        return;

    printString(os, "ParseOption ", indent);
    printLocation(os, po->loc, 0, false);

    os << " ";
    printParseOptionString(os, po);

    ASTParseOptionKind kind = po->getKind();
    if (kind == APOK_APPEND_INCLUDE_PATH ||
        kind == APOK_APPEND_MODULE_PATH ||
        kind == APOK_DEFINE ||
        kind == APOK_DISABLE_WARNING ||
        kind == APOK_ENABLE_WARNING ||
        kind == APOK_EXEC_CLASS ||
        kind == APOK_INCLUDE ||
        kind == APOK_MODULE_CMD ||
        kind == APOK_REQUIRES ||
        kind == APOK_TRY_MODULE)
    {
        ASTStringParseOption* spo = static_cast<ASTStringParseOption*>(po);
        os << ": \"" << spo->str << "\"";
    }

    os << "\n";
}

void AstTreePrinter::printParseOptionString(std::ostream& os, ASTParseOption* po) {
    if (!po)
        return;

    ASTParseOptionKind kind = po->getKind();
    switch (kind) {
        case APOK_ALLOW_BARE_REFS: os << "ALLOW_BARE_REFS"; break;
        case APOK_ALLOW_DEBUGGER: os << "ALLOW_DEBUGGER"; break;
        case APOK_ALLOW_INJECTION: os << "ALLOW_INJECTION"; break;
        case APOK_ALLOW_STATEMENT_NO_EFFECT: os << "ALLOW_STATEMENT_NO_EFFECT"; break;
        case APOK_APPEND_INCLUDE_PATH: os << "APPEND_INCLUDE_PATH"; break;
        case APOK_APPEND_MODULE_PATH: os << "APPEND_MODULE_PATH"; break;
        case APOK_ASSUME_GLOBAL: os << "ASSUME_GLOBAL"; break;
        case APOK_ASSUME_LOCAL: os << "ASSUME_LOCAL"; break;
        case APOK_BROKEN_INT_ASSIGNMENTS: os << "BROKEN_INT_ASSIGNMENTS"; break;
        case APOK_BROKEN_LIST_PARSING: os << "BROKEN_LIST_PARSING"; break;
        case APOK_BROKEN_LOGIC_PRECEDENCE: os << "BROKEN_LOGIC_PRECEDENCE"; break;
        case APOK_BROKEN_LOOP_STATEMENT: os << "BROKEN_LOOP_STATEMENT"; break;
        case APOK_BROKEN_OPERATORS: os << "BROKEN_OPERATORS"; break;
        case APOK_BROKEN_NAMESPACE_RESOLUTION: os << "BROKEN_NAMESPACE_RESOLUTION"; break;
        case APOK_BROKEN_REFERENCES: os << "BROKEN_REFERENCES"; break;
        case APOK_DEFINE: os << "DEFINE"; break;
        case APOK_DISABLE_ALL_WARNINGS: os << "DISABLE_ALL_WARNINGS"; break;
        case APOK_DISABLE_WARNING: os << "DISABLE_WARNING"; break;
        case APOK_ENABLE_ALL_WARNINGS: os << "ENABLE_ALL_WARNINGS"; break;
        case APOK_ENABLE_WARNING: os << "ENABLE_WARNING"; break;
        case APOK_ENDTRY: os << "ENDTRY"; break;
        case APOK_EXEC_CLASS: os << "EXEC_CLASS"; break;
        case APOK_INCLUDE: os << "INCLUDE"; break;
        case APOK_LOCKDOWN: os << "LOCKDOWN"; break;
        case APOK_LOCK_OPTIONS: os << "LOCK_OPTIONS"; break;
        case APOK_LOCK_WARNINGS: os << "LOCK_WARNINGS"; break;
        case APOK_MODULE_CMD: os << "MODULE_CMD"; break;
        case APOK_MODERN: os << "MODERN"; break;
        case APOK_NEW_STYLE: os << "NEW_STYLE"; break;
        case APOK_IFDEF: os << "IFDEF"; break;
        case APOK_IFNDEF: os << "IFNDEF"; break;
        case APOK_IF: os << "IF"; break;
        case APOK_ELIF: os << "ELIF"; break;
        case APOK_ELSE: os << "ELSE"; break;
        case APOK_ENDIF: os << "ENDIF"; break;
        case APOK_NO_CHILD_PO_RESTRICTIONS: os << "NO_CHILD_PO_RESTRICTIONS"; break;
        case APOK_NO_CLASS_DEFS: os << "NO_CLASS_DEFS"; break;
        case APOK_NO_CONSTANT_DEFS: os << "NO_CONSTANT_DEFS"; break;
        case APOK_NO_DATABASE: os << "NO_DATABASE"; break;
        case APOK_NO_DEBUGGING: os << "NO_DEBUGGING"; break;
        case APOK_NO_EXTERNAL_PROCESS: os << "NO_EXTERNAL_PROCESS"; break;
        case APOK_NO_EXTERNAL_INFO: os << "NO_EXTERNAL_INFO"; break;
        case APOK_NO_EXTERNAL_ACCESS: os << "NO_EXTERNAL_ACCESS"; break;
        case APOK_NO_FILESYSTEM: os << "NO_FILESYSTEM"; break;
        case APOK_NO_GLOBAL_VARS: os << "NO_GLOBAL_VARS"; break;
        case APOK_NO_GUI: os << "NO_GUI"; break;
        case APOK_NO_IO: os << "NO_IO"; break;
        case APOK_NO_LOCALE_CONTROL: os << "NO_LOCALE_CONTROL"; break;
        case APOK_NO_MODULES: os << "NO_MODULES"; break;
        case APOK_NO_NAMESPACE_DEFS: os << "NO_NAMESPACE_DEFS"; break;
        case APOK_NO_NETWORK: os << "NO_NETWORK"; break;
        case APOK_NO_NEW: os << "NO_NEW"; break;
        case APOK_NO_PROCESS_CONTROL: os << "NO_PROCESS_CONTROL"; break;
        case APOK_NO_REFLECTION: os << "NO_REFLECTION"; break;
        case APOK_NO_SUBROUTINE_DEFS: os << "NO_SUBROUTINE_DEFS"; break;
        case APOK_NO_TERMINAL_IO: os << "NO_TERMINAL_IO"; break;
        case APOK_NO_THREADS: os << "NO_THREADS"; break;
        case APOK_NO_THREAD_CLASSES: os << "NO_THREAD_CLASSES"; break;
        case APOK_NO_THREAD_CONTROL: os << "NO_THREAD_CONTROL"; break;
        case APOK_NO_THREAD_INFO: os << "NO_THREAD_INFO"; break;
        case APOK_NO_TOP_LEVEL_STATEMENTS: os << "NO_TOP_LEVEL_STATEMENTS"; break;
        case APOK_NO_TRANSIENT: os << "NO_TRANSIENT"; break;
        case APOK_NO_UNCONTROLLED_APIS: os << "NO_UNCONTROLLED_APIS"; break;
        case APOK_OLD_STYLE: os << "OLD_STYLE"; break;
        case APOK_PERL_BOOLEAN_EVAL: os << "PERL_BOOLEAN_EVAL"; break;
        case APOK_PUSH_PARSE_OPTIONS: os << "PUSH_PARSE_OPTIONS"; break;
        case APOK_REQUIRES: os << "REQUIRES"; break;
        case APOK_REQUIRE_DOLLAR: os << "REQUIRE_DOLLAR"; break;
        case APOK_REQUIRE_OUR: os << "REQUIRE_OUR"; break;
        case APOK_REQUIRE_PROTOTYPES: os << "REQUIRE_PROTOTYPES"; break;
        case APOK_REQUIRE_TYPES: os << "REQUIRE_TYPES"; break;
        case APOK_SET_TIME_ZONE: os << "SET_TIME_ZONE"; break;
        case APOK_STRICT_ARGS: os << "STRICT_ARGS"; break;
        case APOK_STRICT_BOOLEAN_EVAL: os << "STRICT_BOOLEAN_EVAL"; break;
        case APOK_STRONG_ENCAPSULATION: os << "STRONG_ENCAPSULATION"; break;
        case APOK_TRY_MODULE: os << "TRY_MODULE"; break;
        default: break;
    }
}

void AstTreePrinter::printStatement(std::ostream& os, ASTStatement* stmt, int indent) {
    if (!stmt) {
        printString(os, "<null statement>\n", indent);
        return;
    }

    switch (stmt->getKind()) {
        case ASTStatementKind::ASK_Block: {
            ASTStatementBlock* s = static_cast<ASTStatementBlock*>(stmt);
            printString(os, "StmtBlock ", indent);
            printLocation(os, s->loc, 0);
            if (s->statements.size() > 0) {
                for (size_t i = 0, count = s->statements.size(); i < count; i++)
                    printStatement(os, s->statements[i], indent+1);
            } else {
                printString(os, "<empty>\n", indent+1);
            }
            break;
        }
        case ASTStatementKind::ASK_Break: {
            printString(os, "BreakStmt ", indent);
            printLocation(os, stmt->loc, 0);
            break;
        }
        case ASTStatementKind::ASK_Call: {
            ASTCallStatement* s = static_cast<ASTCallStatement*>(stmt);
            printString(os, "CallStmt ", indent);
            printLocation(os, s->loc, 0);
            printExpression(os, s->call.get(), indent+1);
            break;
        }
        case ASTStatementKind::ASK_Context: {
            ASTContextStatement* s = static_cast<ASTContextStatement*>(stmt);
            printString(os, "ContextStmt ", indent);
            printLocation(os, s->loc, 0, false);
            printString(os, s->subcontext ? " (subcontext)\n" : "\n", 0);
            printString(os, "name:\n", indent+1);
            printExpression(os, s->name.get(), indent+2);
            printString(os, "data:\n", indent+1);
            printExpression(os, s->data.get(), indent+2);
            if (s->contextMods.size() > 0) {
                printString(os, "contextMods:\n", indent+1);
                for (size_t i = 0, count = s->contextMods.size(); i < count; i++)
                    printExpression(os, s->contextMods[i], indent+2);
            }
            printStatement(os, s->statements.get(), indent+1);
            break;
        }
        case ASTStatementKind::ASK_Continue: {
            printString(os, "ContinueStmt ", indent);
            printLocation(os, stmt->loc, 0);
            break;
        }
        case ASTStatementKind::ASK_DoWhile: {
            ASTDoWhileStatement* s = static_cast<ASTDoWhileStatement*>(stmt);
            printString(os, "DoWhileStmt ", indent);
            printLocation(os, s->loc, 0);
            printString(os, "condition:\n", indent+1);
            printExpression(os, s->condition.get(), indent+2);
            printStatement(os, s->statement.get(), indent+1);
            break;
        }
        case ASTStatementKind::ASK_Expression: {
            ASTExpressionStatement* s = static_cast<ASTExpressionStatement*>(stmt);
            printString(os, "ExpressionStmt ", indent);
            printLocation(os, s->loc, 0);
            printExpression(os, s->expression.get(), indent+1);
            break;
        }
        case ASTStatementKind::ASK_For: {
            ASTForStatement* s = static_cast<ASTForStatement*>(stmt);
            printString(os, "ForStmt ", indent);
            printLocation(os, s->loc, 0);
            printString(os, "init:\n", indent+1);
            printExpression(os, s->init.get(), indent+2);
            printString(os, "condition:\n", indent+1);
            printExpression(os, s->condition.get(), indent+2);
            printString(os, "iteration:\n", indent+1);
            printExpression(os, s->iteration.get(), indent+2);
            printStatement(os, s->statement.get(), indent+1);
            break;
        }
        case ASTStatementKind::ASK_Foreach: {
            ASTForeachStatement* s = static_cast<ASTForeachStatement*>(stmt);
            printString(os, "ForeachStmt ", indent);
            printLocation(os, s->loc, 0);
            printString(os, "value:\n", indent+1);
            printExpression(os, s->value.get(), indent+2);
            printString(os, "source:\n", indent+1);
            printExpression(os, s->source.get(), indent+2);
            printStatement(os, s->statement.get(), indent+1);
            break;
        }
        case ASTStatementKind::ASK_If: {
            ASTIfStatement* s = static_cast<ASTIfStatement*>(stmt);
            printString(os, "IfStmt ", indent);
            printLocation(os, s->loc, 0);
            printString(os, "condition:\n", indent+1);
            printExpression(os, s->condition.get(), indent+2);
            printString(os, "then:\n", indent+1);
            printStatement(os, s->stmtThen.get(), indent+2);
            printString(os, "else:\n", indent+1);
            printStatement(os, s->stmtElse.get(), indent+2);
            break;
        }
        case ASTStatementKind::ASK_OnBlockExit: {
            ASTOnBlockExitStatement* s = static_cast<ASTOnBlockExitStatement*>(stmt);
            printString(os, "OnBlockExitStmt ", indent);
            printLocation(os, s->loc, 0, false);
            if (s->condition == ASTOnBlockExitStatement::Condition::AOBEC_Error)
                printString(os, " (on_error)\n", 0);
            else if (s->condition == ASTOnBlockExitStatement::Condition::AOBEC_Exit)
                printString(os, " (on_exit)\n", 0);
            else if (s->condition == ASTOnBlockExitStatement::Condition::AOBEC_Success)
                printString(os, " (on_success)\n", 0);
            printStatement(os, s->statement.get(), indent+1);
            break;
        }
        case ASTStatementKind::ASK_Rethrow: {
            printString(os, "RethrowStmt ", indent);
            printLocation(os, stmt->loc, 0);
            break;
        }
        case ASTStatementKind::ASK_Return: {
            ASTReturnStatement* s = static_cast<ASTReturnStatement*>(stmt);
            printString(os, "ReturnStmt ", indent);
            printLocation(os, s->loc, 0);
            if (s->retval.get())
                printExpression(os, s->retval.get(), indent+1);
            break;
        }
        case ASTStatementKind::ASK_Summarize: {
            ASTSummarizeStatement* s = static_cast<ASTSummarizeStatement*>(stmt);
            printString(os, "SummarizeStmt ", indent);
            printLocation(os, s->loc, 0);
            printString(os, "name:\n", indent+1);
            printExpression(os, s->name.get(), indent+2);
            printString(os, "data:\n", indent+1);
            printExpression(os, s->data.get(), indent+2);
            printString(os, "by:\n", indent+1);
            printExpression(os, s->by.get(), indent+2);
            if (s->contextMods.size() > 0) {
                printString(os, "contextMods:\n", indent+1);
                for (size_t i = 0, count = s->contextMods.size(); i < count; i++)
                    printExpression(os, s->contextMods[i], indent+2);
            }
            printStatement(os, s->statements.get(), indent+1);
            break;
        }
        case ASTStatementKind::ASK_Switch: {
            ASTSwitchStatement* s = static_cast<ASTSwitchStatement*>(stmt);
            printString(os, "SwitchStmt ", indent);
            printLocation(os, s->loc, 0);
            printString(os, "variable:\n", indent+1);
            printExpression(os, s->variable.get(), indent+2);
            printExpression(os, s->body.get(), indent+1);
            break;
        }
        case ASTStatementKind::ASK_ThreadExit: {
            printString(os, "ThreadExitStmt ", indent);
            printLocation(os, stmt->loc, 0);
            break;
        }
        case ASTStatementKind::ASK_Throw: {
            ASTThrowStatement* s = static_cast<ASTThrowStatement*>(stmt);
            printString(os, "ThrowStmt ", indent);
            printLocation(os, s->loc, 0);
            printExpression(os, s->expression.get(), indent+1);
            break;
        }
        case ASTStatementKind::ASK_Try: {
            ASTTryStatement* s = static_cast<ASTTryStatement*>(stmt);
            printString(os, "TryStmt ", indent);
            printLocation(os, s->loc, 0);
            printString(os, "tryBlock:\n", indent+1);
            printStatement(os, s->tryStmt.get(), indent+2);
            if (s->catchVar.get()) {
                printString(os, "catchVar:\n", indent+1);
                printExpression(os, s->catchVar.get(), indent+2);
            }
            printString(os, "catchBlock:\n", indent+1);
            printStatement(os, s->catchStmt.get(), indent+2);
            break;
        }
        case ASTStatementKind::ASK_While: {
            ASTWhileStatement* s = static_cast<ASTWhileStatement*>(stmt);
            printString(os, "WhileStmt ", indent);
            printLocation(os, s->loc, 0);
            printString(os, "condition:\n", indent+1);
            printExpression(os, s->condition.get(), indent+2);
            printStatement(os, s->statement.get(), indent+1);
            break;
        }
        default:
            break;
    }
}

void AstTreePrinter::printNode(std::ostream& os, ASTNode* node, int indent) {
    if (!node)
        return;

    switch (node->getNodeType()) {
        case ANT_Declaration: {
            ASTDeclaration* decl = static_cast<ASTDeclaration*>(node);
            printDeclaration(os, decl, indent);
            break;
        }
        case ANT_Expression: {
            ASTExpression* expr = static_cast<ASTExpression*>(node);
            printExpression(os, expr, indent);
            break;
        }
        case ANT_Name: {
            ASTName* name = static_cast<ASTName*>(node);
            printName(os, *name, indent);
            break;
        }
        case ANT_ParseOption: {
            ASTParseOption* po = static_cast<ASTParseOption*>(node);
            printParseOption(os, po, indent);
            break;
        }
        case ANT_Statement: {
            ASTStatement* stmt = static_cast<ASTStatement*>(node);
            printStatement(os, stmt, indent);
            break;
        }
        case ANT_None:
        default:
            break;
    }
}

void AstTreePrinter::printTree(std::ostream& os, ASTTree* tree) {
    if (!tree) {
        os << "no tree to print out\n";
        return;
    }

    for (size_t i = 0, count = tree->nodes.size(); i < count; i++)
        printNode(os, tree->nodes[i], 0);
    os.flush();
}
