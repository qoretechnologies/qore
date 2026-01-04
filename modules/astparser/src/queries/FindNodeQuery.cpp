/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  FindNodeQuery.cpp

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

#include "queries/FindNodeQuery.h"

#include "ast/AST.h"

ASTNode* FindNodeQuery::inDeclaration(ASTDeclaration* decl, ast_loc_t line, ast_loc_t col) {
    if (!decl)
        return nullptr;
    if (!decl->loc.inside(line, col))
        return nullptr;

    ASTNode* result = nullptr;
    switch (decl->getKind()) {
        case ASTDeclarationKind::ADK_Class: {
            ASTClassDeclaration* d = static_cast<ASTClassDeclaration*>(decl);
            result = inName(d->name, line, col);
            if (result) return result;
            for (size_t i = 0, count = d->inherits.size(); i < count; i++) {
                result = inDeclaration(d->inherits[i], line, col);
                if (result) return result;
            }
            for (size_t i = 0, count = d->declarations.size(); i < count; i++) {
                result = inDeclaration(d->declarations[i], line, col);
                if (result) return result;
            }
            break;
        }
        case ASTDeclarationKind::ADK_Closure: {
            ASTClosureDeclaration* d = static_cast<ASTClosureDeclaration*>(decl);
            result = inExpression(d->returnType.get(), line, col);
            if (result) return result;
            result = inExpression(d->params.get(), line, col);
            if (result) return result;
            result = inStatement(d->body.get(), line, col);
            if (result) return result;
            break;
        }
        case ASTDeclarationKind::ADK_Constant: {
            ASTConstantDeclaration* d = static_cast<ASTConstantDeclaration*>(decl);
            result = inName(d->name, line, col);
            if (result) return result;
            result = inExpression(d->value.get(), line, col);
            if (result) return result;
            break;
        }
        case ASTDeclarationKind::ADK_Function: {
            ASTFunctionDeclaration* d = static_cast<ASTFunctionDeclaration*>(decl);
            result = inName(d->name, line, col);
            if (result) return result;
            result = inExpression(d->returnType.get(), line, col);
            if (result) return result;
            result = inExpression(d->params.get(), line, col);
            if (result) return result;
            result = inExpression(d->inits.get(), line, col);
            if (result) return result;
            result = inStatement(d->body.get(), line, col);
            if (result) return result;
            break;
        }
        case ASTDeclarationKind::ADK_Hash: {
            ASTHashDeclaration* d = static_cast<ASTHashDeclaration*>(decl);
            result = inName(d->name, line, col);
            if (result) return result;
            for (size_t i = 0, count = d->declarations.size(); i < count; i++) {
                result = inDeclaration(d->declarations[i], line, col);
                if (result) return result;
            }
            break;
        }
        case ASTDeclarationKind::ADK_HashMember: {
            ASTHashMemberDeclaration* d = static_cast<ASTHashMemberDeclaration*>(decl);
            result = inName(d->typeName, line, col);
            if (result) return result;
            result = inName(d->name, line, col);
            if (result) return result;
            result = inExpression(d->init.get(), line, col);
            if (result) return result;
            break;
        }
        case ASTDeclarationKind::ADK_MemberGroup: {
            ASTMemberGroupDeclaration* d = static_cast<ASTMemberGroupDeclaration*>(decl);
            for (size_t i = 0, count = d->members.size(); i < count; i++) {
                result = inExpression(d->members[i], line, col);
                if (result) return result;
            }
            break;
        }
        case ASTDeclarationKind::ADK_Namespace: {
            ASTNamespaceDeclaration* d = static_cast<ASTNamespaceDeclaration*>(decl);
            for (size_t i = 0, count = d->declarations.size(); i < count; i++) {
                result = inDeclaration(d->declarations[i], line, col);
                if (result) return result;
            }
            break;
        }
        case ASTDeclarationKind::ADK_Superclass: {
            ASTSuperclassDeclaration* d = static_cast<ASTSuperclassDeclaration*>(decl);
            result = inName(d->name, line, col);
            if (result) return result;
            break;
        }
        case ASTDeclarationKind::ADK_Variable: {
            ASTVariableDeclaration* d = static_cast<ASTVariableDeclaration*>(decl);
            result = inName(d->typeName, line, col);
            if (result) return result;
            result = inName(d->name, line, col);
            if (result) return result;
            break;
        }
        case ASTDeclarationKind::ADK_VarList: {
            ASTVarListDeclaration* d = static_cast<ASTVarListDeclaration*>(decl);
            result = inExpression(d->variables.get(), line, col);
            if (result) return result;
            break;
        }
        case ASTDeclarationKind::ADK_Typedef: {
            ASTTypedefDeclaration* d = static_cast<ASTTypedefDeclaration*>(decl);
            result = inName(d->name, line, col);
            if (result) return result;
            result = inName(d->typeName, line, col);
            if (result) return result;
            break;
        }
        default:
            break;
    }
    return decl;
}

ASTNode* FindNodeQuery::inExpression(ASTExpression* expr, ast_loc_t line, ast_loc_t col) {
    if (!expr)
        return nullptr;
    if (!expr->loc.inside(line, col))
        return nullptr;

    ASTNode* result = nullptr;
    switch (expr->getKind()) {
        case ASTExpressionKind::AEK_Access: {
            ASTAccessExpression* e = static_cast<ASTAccessExpression*>(expr);
            result = inExpression(e->variable.get(), line, col);
            if (result) return result;
            result = inExpression(e->member.get(), line, col);
            if (result) return result;
            break;
        }
        case ASTExpressionKind::AEK_Assignment: {
            ASTAssignmentExpression* e = static_cast<ASTAssignmentExpression*>(expr);
            result = inExpression(e->left.get(), line, col);
            if (result) return result;
            result = inExpression(e->right.get(), line, col);
            if (result) return result;
            break;
        }
        case ASTExpressionKind::AEK_Backquote:
            return expr;
        case ASTExpressionKind::AEK_Binary: {
            ASTBinaryExpression* e = static_cast<ASTBinaryExpression*>(expr);
            result = inExpression(e->left.get(), line, col);
            if (result) return result;
            result = inExpression(e->right.get(), line, col);
            if (result) return result;
            break;
        }
        case ASTExpressionKind::AEK_Call: {
            ASTCallExpression* e = static_cast<ASTCallExpression*>(expr);
            result = inExpression(e->target.get(), line, col);
            if (result) return result;
            result = inExpression(e->args.get(), line, col);
            if (result) return result;
            break;
        }
        case ASTExpressionKind::AEK_Case: {
            ASTCaseExpression* e = static_cast<ASTCaseExpression*>(expr);
            result = inExpression(e->caseExpr.get(), line, col);
            if (result) return result;
            result = inStatement(e->statements.get(), line, col);
            if (result) return result;
            break;
        }
        case ASTExpressionKind::AEK_Cast: {
            ASTCastExpression* e = static_cast<ASTCastExpression*>(expr);
            result = inName(e->castType, line, col);
            if (result) return result;
            result = inExpression(e->obj.get(), line, col);
            if (result) return result;
            break;
        }
        case ASTExpressionKind::AEK_Closure: {
            ASTClosureExpression* e = static_cast<ASTClosureExpression*>(expr);
            result = inDeclaration(e->closure.get(), line, col);
            if (result) return result;
            break;
        }
        case ASTExpressionKind::AEK_ConstrInit: {
            ASTConstrInitExpression* e = static_cast<ASTConstrInitExpression*>(expr);
            for (size_t i = 0, count = e->inits.size(); i < count; i++) {
                result = inExpression(e->inits[i], line, col);
                if (result) return result;
            }
            break;
        }
        case ASTExpressionKind::AEK_ContextMod: {
            ASTContextModExpression* e = static_cast<ASTContextModExpression*>(expr);
            result = inExpression(e->expression.get(), line, col);
            if (result) return result;
            break;
        }
        case ASTExpressionKind::AEK_ContextRow:
            return expr;
        case ASTExpressionKind::AEK_Decl: {
            ASTDeclExpression* e = static_cast<ASTDeclExpression*>(expr);
            result = inDeclaration(e->declaration.get(), line, col);
            if (result) return result;
            break;
        }
        case ASTExpressionKind::AEK_Find: {
            ASTFindExpression* e = static_cast<ASTFindExpression*>(expr);
            result = inExpression(e->result.get(), line, col);
            if (result) return result;
            result = inExpression(e->data.get(), line, col);
            if (result) return result;
            result = inExpression(e->where.get(), line, col);
            if (result) return result;
            break;
        }
        case ASTExpressionKind::AEK_Hash: {
            ASTHashExpression* e = static_cast<ASTHashExpression*>(expr);
            for (size_t i = 0, count = e->elements.size(); i < count; i++) {
                result = inExpression(e->elements[i], line, col);
                if (result) return result;
            }
            break;
        }
        case ASTExpressionKind::AEK_HashdeclHash: {
            ASTHashdeclHashExpression* e = static_cast<ASTHashdeclHashExpression*>(expr);
            result = inName(e->hashdecl, line, col);
            if (result) return result;
            result = inExpression(e->hash.get(), line, col);
            if (result) return result;
            break;
        }
        case ASTExpressionKind::AEK_HashElement: {
            ASTHashElementExpression* e = static_cast<ASTHashElementExpression*>(expr);
            result = inExpression(e->key.get(), line, col);
            if (result) return result;
            result = inExpression(e->value.get(), line, col);
            if (result) return result;
            break;
        }
        case ASTExpressionKind::AEK_ImplicitArg:
        case ASTExpressionKind::AEK_ImplicitElem:
            return expr;
        case ASTExpressionKind::AEK_Index: {
            ASTIndexExpression* e = static_cast<ASTIndexExpression*>(expr);
            result = inExpression(e->variable.get(), line, col);
            if (result) return result;
            result = inExpression(e->index.get(), line, col);
            if (result) return result;
            break;
        }
        case ASTExpressionKind::AEK_List: {
            ASTListExpression* e = static_cast<ASTListExpression*>(expr);
            for (size_t i = 0, count = e->elements.size(); i < count; i++) {
                result = inExpression(e->elements[i], line, col);
                if (result) return result;
            }
            break;
        }
        case ASTExpressionKind::AEK_Literal:
            return expr;
        case ASTExpressionKind::AEK_Name: {
            ASTNameExpression* e = static_cast<ASTNameExpression*>(expr);
            return &e->name;
        }
        case ASTExpressionKind::AEK_Range: {
            ASTRangeExpression* e = static_cast<ASTRangeExpression*>(expr);
            result = inExpression(e->left.get(), line, col);
            if (result) return result;
            result = inExpression(e->right.get(), line, col);
            if (result) return result;
            break;
        }
        case ASTExpressionKind::AEK_Regex:
        case ASTExpressionKind::AEK_RegexSubst:
        case ASTExpressionKind::AEK_RegexTrans:
            return expr;
        case ASTExpressionKind::AEK_Returns: {
            ASTReturnsExpression* e = static_cast<ASTReturnsExpression*>(expr);
            result = inExpression(e->typeName.get(), line, col);
            if (result) return result;
            break;
        }
        case ASTExpressionKind::AEK_SwitchBody: {
            ASTSwitchBodyExpression* e = static_cast<ASTSwitchBodyExpression*>(expr);
            for (size_t i = 0, count = e->cases.size(); i < count; i++) {
                result = inExpression(e->cases[i], line, col);
                if (result) return result;
            }
            break;
        }
        case ASTExpressionKind::AEK_Ternary: {
            ASTTernaryExpression* e = static_cast<ASTTernaryExpression*>(expr);
            result = inExpression(e->condition.get(), line, col);
            if (result) return result;
            result = inExpression(e->exprTrue.get(), line, col);
            if (result) return result;
            result = inExpression(e->exprFalse.get(), line, col);
            if (result) return result;
            break;
        }
        case ASTExpressionKind::AEK_Unary: {
            ASTUnaryExpression* e = static_cast<ASTUnaryExpression*>(expr);
            result = inExpression(e->expression.get(), line, col);
            if (result) return result;
            break;
        }
        default:
            break;
    }
    return expr;
}

ASTNode* FindNodeQuery::inName(ASTName& name, ast_loc_t line, ast_loc_t col) {
    if (!name.loc.inside(line, col))
        return nullptr;
    return &name;
}

ASTNode* FindNodeQuery::inName(ASTName* name, ast_loc_t line, ast_loc_t col) {
    if (!name)
        return nullptr;
    if (!name->loc.inside(line, col))
        return nullptr;
    return name;
}

ASTNode* FindNodeQuery::inParseOption(ASTParseOption* po, ast_loc_t line, ast_loc_t col) {
    if (!po)
        return nullptr;
    if (!po->loc.inside(line, col))
        return nullptr;
    return po;
}

ASTNode* FindNodeQuery::inStatement(ASTStatement* stmt, ast_loc_t line, ast_loc_t col) {
    if (!stmt)
        return nullptr;
    if (!stmt->loc.inside(line, col))
        return nullptr;

    ASTNode* result = nullptr;
    switch (stmt->getKind()) {
        case ASTStatementKind::ASK_Block: {
            ASTStatementBlock* s = static_cast<ASTStatementBlock*>(stmt);
            for (size_t i = 0, count = s->statements.size(); i < count; i++) {
                result = inStatement(s->statements[i], line, col);
                if (result) return result;
            }
            break;
        }
        case ASTStatementKind::ASK_Break:
            return stmt;
        case ASTStatementKind::ASK_Call: {
            ASTCallStatement* s = static_cast<ASTCallStatement*>(stmt);
            result = inExpression(s->call.get(), line, col);
            if (result) return result;
            break;
        }
        case ASTStatementKind::ASK_Context: {
            ASTContextStatement* s = static_cast<ASTContextStatement*>(stmt);
            result = inExpression(s->name.get(), line, col);
            if (result) return result;
            result = inExpression(s->data.get(), line, col);
            if (result) return result;
            for (size_t i = 0, count = s->contextMods.size(); i < count; i++) {
                result = inExpression(s->contextMods[i], line, col);
                if (result) return result;
            }
            result = inStatement(s->statements.get(), line, col);
            if (result) return result;
            break;
        }
        case ASTStatementKind::ASK_Continue:
            return stmt;
        case ASTStatementKind::ASK_DoWhile: {
            ASTDoWhileStatement* s = static_cast<ASTDoWhileStatement*>(stmt);
            result = inExpression(s->condition.get(), line, col);
            if (result) return result;
            result = inStatement(s->statement.get(), line, col);
            if (result) return result;
            break;
        }
        case ASTStatementKind::ASK_Expression: {
            ASTExpressionStatement* s = static_cast<ASTExpressionStatement*>(stmt);
            result = inExpression(s->expression.get(), line, col);
            if (result) return result;
            break;
        }
        case ASTStatementKind::ASK_For: {
            ASTForStatement* s = static_cast<ASTForStatement*>(stmt);
            result = inExpression(s->init.get(), line, col);
            if (result) return result;
            result = inExpression(s->condition.get(), line, col);
            if (result) return result;
            result = inExpression(s->iteration.get(), line, col);
            if (result) return result;
            result = inStatement(s->statement.get(), line, col);
            if (result) return result;
            break;
        }
        case ASTStatementKind::ASK_Foreach: {
            ASTForeachStatement* s = static_cast<ASTForeachStatement*>(stmt);
            result = inExpression(s->value.get(), line, col);
            if (result) return result;
            result = inExpression(s->source.get(), line, col);
            if (result) return result;
            result = inStatement(s->statement.get(), line, col);
            if (result) return result;
            break;
        }
        case ASTStatementKind::ASK_If: {
            ASTIfStatement* s = static_cast<ASTIfStatement*>(stmt);
            result = inExpression(s->condition.get(), line, col);
            if (result) return result;
            result = inStatement(s->stmtThen.get(), line, col);
            if (result) return result;
            result = inStatement(s->stmtElse.get(), line, col);
            if (result) return result;
            break;
        }
        case ASTStatementKind::ASK_OnBlockExit: {
            ASTOnBlockExitStatement* s = static_cast<ASTOnBlockExitStatement*>(stmt);
            result = inStatement(s->statement.get(), line, col);
            if (result) return result;
            break;
        }
        case ASTStatementKind::ASK_Rethrow:
            return stmt;
        case ASTStatementKind::ASK_Return: {
            ASTReturnStatement* s = static_cast<ASTReturnStatement*>(stmt);
            result = inExpression(s->retval.get(), line, col);
            if (result) return result;
            break;
        }
        case ASTStatementKind::ASK_Summarize: {
            ASTSummarizeStatement* s = static_cast<ASTSummarizeStatement*>(stmt);
            result = inExpression(s->name.get(), line, col);
            if (result) return result;
            result = inExpression(s->data.get(), line, col);
            if (result) return result;
            result = inExpression(s->by.get(), line, col);
            if (result) return result;
            for (size_t i = 0, count = s->contextMods.size(); i < count; i++) {
                result = inExpression(s->contextMods[i], line, col);
                if (result) return result;
            }
            result = inStatement(s->statements.get(), line, col);
            if (result) return result;
            break;
        }
        case ASTStatementKind::ASK_Switch: {
            ASTSwitchStatement* s = static_cast<ASTSwitchStatement*>(stmt);
            result = inExpression(s->variable.get(), line, col);
            if (result) return result;
            result = inExpression(s->body.get(), line, col);
            if (result) return result;
            break;
        }
        case ASTStatementKind::ASK_ThreadExit:
            return stmt;
        case ASTStatementKind::ASK_Throw: {
            ASTThrowStatement* s = static_cast<ASTThrowStatement*>(stmt);
            result = inExpression(s->expression.get(), line, col);
            if (result) return result;
            break;
        }
        case ASTStatementKind::ASK_Try: {
            ASTTryStatement* s = static_cast<ASTTryStatement*>(stmt);
            result = inStatement(s->tryStmt.get(), line, col);
            if (result) return result;
            result = inExpression(s->catchVar.get(), line, col);
            if (result) return result;
            result = inStatement(s->catchStmt.get(), line, col);
            if (result) return result;
            break;
        }
        case ASTStatementKind::ASK_While: {
            ASTWhileStatement* s = static_cast<ASTWhileStatement*>(stmt);
            result = inExpression(s->condition.get(), line, col);
            if (result) return result;
            result = inStatement(s->statement.get(), line, col);
            if (result) return result;
            break;
        }
        default:
            break;
    }
    return stmt;
}

ASTNode* FindNodeQuery::find(ASTTree* tree, ast_loc_t line, ast_loc_t col) {
    if (!tree)
        return nullptr;

    for (size_t i = 0, count = tree->nodes.size(); i < count; i++) {
        ASTNode* result = nullptr;
        ASTNode* node = tree->nodes[i];
        switch (node->getNodeType()) {
            case ANT_Declaration: {
                ASTDeclaration* decl = static_cast<ASTDeclaration*>(node);
                result = inDeclaration(decl, line, col);
                break;
            }
            case ANT_Expression: {
                ASTExpression* expr = static_cast<ASTExpression*>(node);
                result = inExpression(expr, line, col);
                break;
            }
            case ANT_Name: {
                ASTName* name = static_cast<ASTName*>(node);
                result = inName(name, line, col);
                break;
            }
            case ANT_ParseOption: {
                ASTParseOption* po = static_cast<ASTParseOption*>(node);
                result = inParseOption(po, line, col);
                break;
            }
            case ANT_Statement: {
                ASTStatement* stmt = static_cast<ASTStatement*>(node);
                result = inStatement(stmt, line, col);
                break;
            }
            case ANT_None:
            default:
                break;
        }
        if (result)
            return result;
    }
    return nullptr;
}
