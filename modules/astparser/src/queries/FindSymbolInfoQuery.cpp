/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  FindSymbolInfoQuery.cpp

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

#include "queries/FindSymbolInfoQuery.h"

#include <memory>
#include <string>
#include <sstream>

#include "AstPrinter.h"
#include "AstTreePrinter.h"
#include "ast/AST.h"
#include "queries/FindNodeAndParentsQuery.h"

static bool exprMatches(ASTExpression* expr, ast_loc_t line, ast_loc_t col) {
    return expr && expr->loc.inside(line, col) && expr->getKind() == ASTExpressionKind::AEK_Name;
}

static ASTSymbolInfo exprHoverInfo(ASTExpression* expr, ASTSymbolKind sk, ASTSymbolUsageKind suk) {
    ASTNameExpression* name = static_cast<ASTNameExpression*>(expr);
    return ASTSymbolInfo(sk, suk, name->loc, name->name.name);
}

ASTSymbolInfo FindSymbolInfoQuery::nextRound(std::vector<ASTNode*>* nodes, ast_loc_t line, ast_loc_t col) {
    if (nodes->size() > 0) {
        nodes->erase(nodes->begin());
        if (nodes->size() > 0) {
            ASTNodeType nt = nodes->front()->getNodeType();
            if (nt == ANT_Declaration)
                return inDeclaration(nodes, line, col);
            else if (nt == ANT_Expression)
                return inExpression(nodes, line, col);
            else if (nt == ANT_Statement)
                return inStatement(nodes, line, col);
        }
    }
    return ASTSymbolInfo();
}

ASTSymbolInfo FindSymbolInfoQuery::inDeclaration(std::vector<ASTNode*>* nodes, ast_loc_t line, ast_loc_t col) {
    ASTDeclaration* decl = static_cast<ASTDeclaration*>(nodes->front());
    if (!decl)
        return ASTSymbolInfo();
    switch (decl->getKind()) {
        case ASTDeclarationKind::ADK_Class: {
            ASTClassDeclaration* d = static_cast<ASTClassDeclaration*>(decl);
            //classHoverInfo(hi, d);
            if (d->name.loc.inside(line, col))
                return ASTSymbolInfo(ASYK_Class, ASUK_ClassDeclName, d->name.loc, d->name.name);
            break;
        }
        case ASTDeclarationKind::ADK_Closure:
            break;
        case ASTDeclarationKind::ADK_Constant: {
            ASTConstantDeclaration* d = static_cast<ASTConstantDeclaration*>(decl);
            if (d->name.loc.inside(line, col))
                return ASTSymbolInfo(ASYK_Constant, ASUK_ConstantDeclName, d->name.loc, d->name.name);
            break;
        }
        case ASTDeclarationKind::ADK_Function: {
            ASTFunctionDeclaration* d = static_cast<ASTFunctionDeclaration*>(decl);
            if (d->name.loc.inside(line, col))
                return ASTSymbolInfo(ASYK_Function, ASUK_FuncDeclName, d->name.loc, d->name.name);
            if (d->returnType && d->returnType->loc.inside(line, col)) {
                if (d->returnType->getKind() == ASTExpressionKind::AEK_Name) {
                    ASTNameExpression* name = static_cast<ASTNameExpression*>(d->returnType.get());
                    return ASTSymbolInfo(ASYK_Class, ASUK_FuncReturnType, d->returnType->loc, name->name.name);
                }
            }
            break;
        }
        case ASTDeclarationKind::ADK_Hash: {
            ASTHashDeclaration* d = static_cast<ASTHashDeclaration*>(decl);
            if (d->name.loc.inside(line, col))
                return ASTSymbolInfo(ASYK_Interface, ASUK_HashDeclName, d->name.loc, d->name.name);
            break;
        }
        case ASTDeclarationKind::ADK_HashMember: {
            ASTHashMemberDeclaration* d = static_cast<ASTHashMemberDeclaration*>(decl);
            if (d->typeName.loc.inside(line, col))
                return ASTSymbolInfo(ASYK_Class, ASUK_VarDeclTypeName, d->typeName.loc, d->typeName.name);
            if (d->name.loc.inside(line, col))
                return ASTSymbolInfo(ASYK_Field, ASUK_HashMemberName, d->name.loc, d->name.name);
            break;
        }
        case ASTDeclarationKind::ADK_MemberGroup:
            break;
        case ASTDeclarationKind::ADK_Namespace: {
            ASTNamespaceDeclaration* d = static_cast<ASTNamespaceDeclaration*>(decl);
            if (d->name.loc.inside(line, col))
                return ASTSymbolInfo(ASYK_Namespace, ASUK_NamespaceDeclName, d->name.loc, d->name.name);
            break;
        }
        case ASTDeclarationKind::ADK_Superclass: {
            ASTSuperclassDeclaration* d = static_cast<ASTSuperclassDeclaration*>(decl);
            if (d->name.loc.inside(line, col))
                return ASTSymbolInfo(ASYK_Class, ASUK_SuperclassDeclName, d->name.loc, d->name.name);
            break;
        }
        case ASTDeclarationKind::ADK_Variable: {
            ASTVariableDeclaration* d = static_cast<ASTVariableDeclaration*>(decl);
            if (d->typeName.loc.inside(line, col))
                return ASTSymbolInfo(ASYK_Class, ASUK_VarDeclTypeName, d->typeName.loc, d->typeName.name);
            if (d->name.loc.inside(line, col))
                return ASTSymbolInfo(ASYK_Variable, ASUK_VarDeclName, d->name.loc, d->name.name);
            break;
        }
        case ASTDeclarationKind::ADK_VarList:
            break;
        case ASTDeclarationKind::ADK_Typedef: {
            ASTTypedefDeclaration* d = static_cast<ASTTypedefDeclaration*>(decl);
            if (d->name.loc.inside(line, col))
                return ASTSymbolInfo(ASYK_TypeAlias, ASUK_TypedefDeclName, d->name.loc, d->name.name);
            if (d->typeName.loc.inside(line, col))
                return ASTSymbolInfo(ASYK_Class, ASUK_TypedefTypeName, d->typeName.loc, d->typeName.name);
            break;
        }
        default:
            break;
    }
    return ASTSymbolInfo();
}

ASTSymbolInfo FindSymbolInfoQuery::inExpression(std::vector<ASTNode*>* nodes, ast_loc_t line, ast_loc_t col) {
    ASTExpression* expr = static_cast<ASTExpression*>(nodes->front());
    if (!expr)
        return ASTSymbolInfo();
    switch (expr->getKind()) {
        case ASTExpressionKind::AEK_Access: {
            ASTAccessExpression* e = static_cast<ASTAccessExpression*>(expr);
            if (exprMatches(e->variable.get(), line, col))
                return exprHoverInfo(e->variable.get(), ASYK_Variable, ASUK_AccessVariable);
            if (exprMatches(e->member.get(), line, col))
                return exprHoverInfo(e->member.get(), ASYK_Variable, ASUK_AccessMember);
            break;
        }
        case ASTExpressionKind::AEK_Assignment: {
            ASTAssignmentExpression* e = static_cast<ASTAssignmentExpression*>(expr);
            if (exprMatches(e->left.get(), line, col))
                return exprHoverInfo(e->left.get(), ASYK_Variable, ASUK_AssignmentLeft);
            if (exprMatches(e->right.get(), line, col))
                return exprHoverInfo(e->right.get(), ASYK_Variable, ASUK_AssignmentRight);
            break;
        }
        case ASTExpressionKind::AEK_Backquote:
            break;
        case ASTExpressionKind::AEK_Binary: {
            ASTBinaryExpression* e = static_cast<ASTBinaryExpression*>(expr);
            if (exprMatches(e->left.get(), line, col))
                return exprHoverInfo(e->left.get(), ASYK_Variable, ASUK_BinaryLeft);
            if (exprMatches(e->right.get(), line, col))
                return exprHoverInfo(e->right.get(), ASYK_Variable, ASUK_BinaryRight);
            break;
        }
        case ASTExpressionKind::AEK_Call: {
            ASTCallExpression* e = static_cast<ASTCallExpression*>(expr);
            if (exprMatches(e->target.get(), line, col))
                return exprHoverInfo(e->target.get(), ASYK_Function, ASUK_CallTarget);
            if (exprMatches(e->args.get(), line, col))
                return exprHoverInfo(e->args.get(), ASYK_Variable, ASUK_CallArgs);
            break;
        }
        case ASTExpressionKind::AEK_Case: {
            ASTCaseExpression* e = static_cast<ASTCaseExpression*>(expr);
            if (exprMatches(e->caseExpr.get(), line, col))
                return exprHoverInfo(e->caseExpr.get(), ASYK_Constant, ASUK_CaseExpr);
            break;
        }
        case ASTExpressionKind::AEK_Cast: {
            ASTCastExpression* e = static_cast<ASTCastExpression*>(expr);
            if (e->castType.loc.inside(line, col))
                return ASTSymbolInfo(ASYK_Class, ASUK_CastType, e->castType.loc, e->castType.name);
            if (exprMatches(e->obj.get(), line, col))
                return exprHoverInfo(e->obj.get(), ASYK_Variable, ASUK_CastObject);
            break;
        }
        case ASTExpressionKind::AEK_Closure:
        case ASTExpressionKind::AEK_ConstrInit:
        case ASTExpressionKind::AEK_ContextMod:
        case ASTExpressionKind::AEK_ContextRow:
            break;
        case ASTExpressionKind::AEK_Decl:
            return nextRound(nodes, line, col);
        case ASTExpressionKind::AEK_Find: {
            ASTFindExpression* e = static_cast<ASTFindExpression*>(expr);
            if (exprMatches(e->data.get(), line, col))
                return exprHoverInfo(e->data.get(), ASYK_Variable, ASUK_FindData);
            break;
        }
        case ASTExpressionKind::AEK_Hash:
            break;
        case ASTExpressionKind::AEK_HashdeclHash: {
            ASTHashdeclHashExpression* e = static_cast<ASTHashdeclHashExpression*>(expr);
            if (e->hashdecl.loc.inside(line, col))
                return ASTSymbolInfo(ASYK_Interface, ASUK_CastType, e->hashdecl.loc, e->hashdecl.name);
            break;
        }
        case ASTExpressionKind::AEK_HashElement: {
            ASTHashElementExpression* e = static_cast<ASTHashElementExpression*>(expr);
            if (exprMatches(e->value.get(), line, col))
                return exprHoverInfo(e->value.get(), ASYK_Variable, ASUK_HashElement);
            break;
        }
        case ASTExpressionKind::AEK_ImplicitArg:
        case ASTExpressionKind::AEK_ImplicitElem:
            break;
        case ASTExpressionKind::AEK_Index: {
            ASTIndexExpression* e = static_cast<ASTIndexExpression*>(expr);
            if (exprMatches(e->variable.get(), line, col))
                return exprHoverInfo(e->variable.get(), ASYK_Variable, ASUK_IndexVariable);
            if (exprMatches(e->index.get(), line, col))
                return exprHoverInfo(e->index.get(), ASYK_Variable, ASUK_IndexIndex);
            break;
        }
        case ASTExpressionKind::AEK_List: {
            ASTListExpression* e = static_cast<ASTListExpression*>(expr);
            for (size_t i = 0, count = e->elements.size(); i < count; i++) {
                ASTExpression* le = e->elements[i];
                if (exprMatches(le, line, col))
                    return exprHoverInfo(le, ASYK_Variable, ASUK_ListElement);
            }
            break;
        }
        case ASTExpressionKind::AEK_Literal:
            break;
        case ASTExpressionKind::AEK_Name:
            return nextRound(nodes, line, col);
        case ASTExpressionKind::AEK_Range: {
            ASTRangeExpression* e = static_cast<ASTRangeExpression*>(expr);
            if (exprMatches(e->left.get(), line, col))
                return exprHoverInfo(e->left.get(), ASYK_Variable, ASUK_RangeLeft);
            if (exprMatches(e->right.get(), line, col))
                return exprHoverInfo(e->right.get(), ASYK_Variable, ASUK_RangeRight);
            break;
        }
        case ASTExpressionKind::AEK_Regex:
        case ASTExpressionKind::AEK_RegexSubst:
        case ASTExpressionKind::AEK_RegexTrans:
            break;
        case ASTExpressionKind::AEK_Returns: {
            ASTReturnsExpression* e = static_cast<ASTReturnsExpression*>(expr);
            if (exprMatches(e->typeName.get(), line, col))
                return ASTSymbolInfo(ASYK_Class, ASUK_ReturnsType, e->typeName->loc, e->typeName->name.name);
            break;
        }
        case ASTExpressionKind::AEK_SwitchBody:
            break;
        case ASTExpressionKind::AEK_Ternary: {
            ASTTernaryExpression* e = static_cast<ASTTernaryExpression*>(expr);
            if (exprMatches(e->condition.get(), line, col))
                return exprHoverInfo(e->condition.get(), ASYK_Variable, ASUK_TernaryCond);
            if (exprMatches(e->exprTrue.get(), line, col))
                return exprHoverInfo(e->exprTrue.get(), ASYK_Variable, ASUK_TernaryTrue);
            if (exprMatches(e->exprFalse.get(), line, col))
                return exprHoverInfo(e->exprFalse.get(), ASYK_Variable, ASUK_TernaryFalse);
            break;
        }
        case ASTExpressionKind::AEK_Unary: {
            ASTUnaryExpression* e = static_cast<ASTUnaryExpression*>(expr);
            if (exprMatches(e->expression.get(), line, col))
                return exprHoverInfo(e->expression.get(), ASYK_Variable, ASUK_Unary);
            break;
        }
        default:
            break;
    }
    return ASTSymbolInfo();
}

ASTSymbolInfo FindSymbolInfoQuery::inStatement(std::vector<ASTNode*>* nodes, ast_loc_t line, ast_loc_t col) {
    ASTStatement* stmt = static_cast<ASTStatement*>(nodes->front());
    if (!stmt)
        return ASTSymbolInfo();
    switch (stmt->getKind()) {
        case ASTStatementKind::ASK_Block:
        case ASTStatementKind::ASK_Break:
        case ASTStatementKind::ASK_Call:
            break;
        case ASTStatementKind::ASK_Context: {
            ASTContextStatement* s = static_cast<ASTContextStatement*>(stmt);
            if (exprMatches(s->name.get(), line, col))
                return exprHoverInfo(s->name.get(), ASYK_Variable, ASUK_ContextStmtName);
            if (exprMatches(s->data.get(), line, col))
                return exprHoverInfo(s->data.get(), ASYK_Variable, ASUK_ContextStmtData);
            break;
        }
        case ASTStatementKind::ASK_Continue:
            break;
        case ASTStatementKind::ASK_DoWhile: {
            ASTDoWhileStatement* s = static_cast<ASTDoWhileStatement*>(stmt);
            if (exprMatches(s->condition.get(), line, col))
                return exprHoverInfo(s->condition.get(), ASYK_Variable, ASUK_DoWhileStmtCond);
            break;
        }
        case ASTStatementKind::ASK_Expression: {
            ASTExpressionStatement* s = static_cast<ASTExpressionStatement*>(stmt);
            if (exprMatches(s->expression.get(), line, col))
                return exprHoverInfo(s->expression.get(), ASYK_Variable, ASUK_ExprStmtExpr);
            break;
        }
        case ASTStatementKind::ASK_For: {
            ASTForStatement* s = static_cast<ASTForStatement*>(stmt);
            if (exprMatches(s->init.get(), line, col))
                return exprHoverInfo(s->init.get(), ASYK_Variable, ASUK_ForStmtInit);
            if (exprMatches(s->condition.get(), line, col))
                return exprHoverInfo(s->condition.get(), ASYK_Variable, ASUK_ForStmtCond);
            if (exprMatches(s->iteration.get(), line, col))
                return exprHoverInfo(s->iteration.get(), ASYK_Variable, ASUK_ForStmtIter);
            break;
        }
        case ASTStatementKind::ASK_Foreach: {
            ASTForeachStatement* s = static_cast<ASTForeachStatement*>(stmt);
            if (exprMatches(s->value.get(), line, col))
                return exprHoverInfo(s->value.get(), ASYK_Variable, ASUK_ForeachStmtVal);
            if (exprMatches(s->source.get(), line, col))
                return exprHoverInfo(s->source.get(), ASYK_Variable, ASUK_ForeachStmtSrc);
            break;
        }
        case ASTStatementKind::ASK_If: {
            ASTIfStatement* s = static_cast<ASTIfStatement*>(stmt);
            if (exprMatches(s->condition.get(), line, col))
                return exprHoverInfo(s->condition.get(), ASYK_Variable, ASUK_IfStmtCond);
            break;
        }
        case ASTStatementKind::ASK_OnBlockExit:
        case ASTStatementKind::ASK_Rethrow:
            break;
        case ASTStatementKind::ASK_Return: {
            ASTReturnStatement* s = static_cast<ASTReturnStatement*>(stmt);
            if (exprMatches(s->retval.get(), line, col))
                return exprHoverInfo(s->retval.get(), ASYK_Variable, ASUK_ReturnStmtVal);
            break;
        }
        case ASTStatementKind::ASK_Summarize: {
            ASTSummarizeStatement* s = static_cast<ASTSummarizeStatement*>(stmt);
            if (exprMatches(s->name.get(), line, col))
                return exprHoverInfo(s->name.get(), ASYK_Variable, ASUK_SummarizeStmtName);
            if (exprMatches(s->data.get(), line, col))
                return exprHoverInfo(s->data.get(), ASYK_Variable, ASUK_SummarizeStmtData);
            break;
        }
        case ASTStatementKind::ASK_Switch: {
            ASTSwitchStatement* s = static_cast<ASTSwitchStatement*>(stmt);
            if (exprMatches(s->variable.get(), line, col))
                return exprHoverInfo(s->variable.get(), ASYK_Variable, ASUK_SwitchStmtVar);
            break;
        }
        case ASTStatementKind::ASK_ThreadExit:
            break;
        case ASTStatementKind::ASK_Throw: {
            ASTThrowStatement* s = static_cast<ASTThrowStatement*>(stmt);
            if (exprMatches(s->expression.get(), line, col))
                return exprHoverInfo(s->expression.get(), ASYK_Variable, ASUK_ThrowStmtExpr);
            break;
        }
        case ASTStatementKind::ASK_Try: {
            ASTTryStatement* s = static_cast<ASTTryStatement*>(stmt);
            if (exprMatches(s->catchVar.get(), line, col))
                return exprHoverInfo(s->catchVar.get(), ASYK_Variable, ASUK_TryStmtCatchVar);
            break;
        }
        case ASTStatementKind::ASK_While: {
            ASTWhileStatement* s = static_cast<ASTWhileStatement*>(stmt);
            if (exprMatches(s->condition.get(), line, col))
                return exprHoverInfo(s->condition.get(), ASYK_Variable, ASUK_WhileStmtCond);
            break;
        }
        default:
            break;
    }
    return ASTSymbolInfo();
}

ASTSymbolInfo FindSymbolInfoQuery::find(ASTTree* tree, ast_loc_t line, ast_loc_t col) {
    if (!tree)
        return ASTSymbolInfo();

    // Target node is first, all the parents follow.
    std::unique_ptr<std::vector<ASTNode*> > nodes(FindNodeAndParentsQuery::find(tree, line, col));
    if (!nodes)
        return ASTSymbolInfo();

    // Get rid of ASTName at the beginning.
    if (nodes->at(0)->getNodeType() == ANT_Name)
        nodes->erase(nodes->begin());

    ASTNode* node = nodes->front();
    switch(node->getNodeType()) {
        case ANT_Declaration:
            return inDeclaration(nodes.get(), line, col);
        case ANT_Expression:
            return inExpression(nodes.get(), line, col);
        case ANT_ParseError:
        case ANT_ParseOption:
            break;
        case ANT_Statement:
            return inStatement(nodes.get(), line, col);
        default:
            break;
    }

    return ASTSymbolInfo();
}
