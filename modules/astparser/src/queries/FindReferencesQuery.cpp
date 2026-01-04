/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  FindReferencesQuery.cpp

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

#include "queries/FindReferencesQuery.h"

#include <memory>

#include "ast/AST.h"

void FindReferencesQuery::inDeclaration(std::vector<ASTNode*>* vec, ASTDeclaration* decl, const std::string& name) {
    if (!decl)
        return;

    switch (decl->getKind()) {
        case ASTDeclarationKind::ADK_Class: {
            ASTClassDeclaration* d = static_cast<ASTClassDeclaration*>(decl);
            inName(vec, d->name, name);
            for (size_t i = 0, count = d->inherits.size(); i < count; i++)
                inDeclaration(vec, d->inherits[i], name);
            for (size_t i = 0, count = d->declarations.size(); i < count; i++)
                inDeclaration(vec, d->declarations[i], name);
            break;
        }
        case ASTDeclarationKind::ADK_Closure: {
            ASTClosureDeclaration* d = static_cast<ASTClosureDeclaration*>(decl);
            inExpression(vec, d->returnType.get(), name);
            inExpression(vec, d->params.get(), name);
            inStatement(vec, d->body.get(), name);
            break;
        }
        case ASTDeclarationKind::ADK_Constant: {
            ASTConstantDeclaration* d = static_cast<ASTConstantDeclaration*>(decl);
            inName(vec, d->name, name);
            inExpression(vec, d->value.get(), name);
            break;
        }
        case ASTDeclarationKind::ADK_Function: {
            ASTFunctionDeclaration* d = static_cast<ASTFunctionDeclaration*>(decl);
            inName(vec, d->name, name);
            inExpression(vec, d->returnType.get(), name);
            inExpression(vec, d->params.get(), name);
            inExpression(vec, d->inits.get(), name);
            inStatement(vec, d->body.get(), name);
            break;
        }
        case ASTDeclarationKind::ADK_Hash: {
            ASTHashDeclaration* d = static_cast<ASTHashDeclaration*>(decl);
            inName(vec, d->name, name);
            for (size_t i = 0, count = d->declarations.size(); i < count; i++)
                inDeclaration(vec, d->declarations[i], name);
            break;
        }
        case ASTDeclarationKind::ADK_HashMember: {
            ASTHashMemberDeclaration* d = static_cast<ASTHashMemberDeclaration*>(decl);
            inName(vec, d->typeName, name);
            inName(vec, d->name, name);
            inExpression(vec, d->init.get(), name);
            break;
        }
        case ASTDeclarationKind::ADK_MemberGroup: {
            ASTMemberGroupDeclaration* d = static_cast<ASTMemberGroupDeclaration*>(decl);
            for (size_t i = 0, count = d->members.size(); i < count; i++)
                inExpression(vec, d->members[i], name);
            break;
        }
        case ASTDeclarationKind::ADK_Namespace: {
            ASTNamespaceDeclaration* d = static_cast<ASTNamespaceDeclaration*>(decl);
            for (size_t i = 0, count = d->declarations.size(); i < count; i++)
                inDeclaration(vec, d->declarations[i], name);
            break;
        }
        case ASTDeclarationKind::ADK_Superclass: {
            ASTSuperclassDeclaration* d = static_cast<ASTSuperclassDeclaration*>(decl);
            inName(vec, d->name, name);
            break;
        }
        case ASTDeclarationKind::ADK_Variable: {
            ASTVariableDeclaration* d = static_cast<ASTVariableDeclaration*>(decl);
            inName(vec, d->typeName, name);
            inName(vec, d->name, name);
            break;
        }
        case ASTDeclarationKind::ADK_VarList: {
            ASTVarListDeclaration* d = static_cast<ASTVarListDeclaration*>(decl);
            inExpression(vec, d->variables.get(), name);
            break;
        }
        case ASTDeclarationKind::ADK_Typedef: {
            ASTTypedefDeclaration* d = static_cast<ASTTypedefDeclaration*>(decl);
            inName(vec, d->name, name);
            inName(vec, d->typeName, name);
            break;
        }
        default:
            break;
    }
}

void FindReferencesQuery::inExpression(std::vector<ASTNode*>* vec, ASTExpression* expr, const std::string& name) {
    if (!expr)
        return;

    switch (expr->getKind()) {
        case ASTExpressionKind::AEK_Access: {
            ASTAccessExpression* e = static_cast<ASTAccessExpression*>(expr);
            inExpression(vec, e->variable.get(), name);
            inExpression(vec, e->member.get(), name);
            break;
        }
        case ASTExpressionKind::AEK_Assignment: {
            ASTAssignmentExpression* e = static_cast<ASTAssignmentExpression*>(expr);
            inExpression(vec, e->left.get(), name);
            inExpression(vec, e->right.get(), name);
            break;
        }
        case ASTExpressionKind::AEK_Backquote:
            break;
        case ASTExpressionKind::AEK_Binary: {
            ASTBinaryExpression* e = static_cast<ASTBinaryExpression*>(expr);
            inExpression(vec, e->left.get(), name);
            inExpression(vec, e->right.get(), name);
            break;
        }
        case ASTExpressionKind::AEK_Call: {
            ASTCallExpression* e = static_cast<ASTCallExpression*>(expr);
            inExpression(vec, e->target.get(), name);
            inExpression(vec, e->args.get(), name);
            break;
        }
        case ASTExpressionKind::AEK_Case: {
            ASTCaseExpression* e = static_cast<ASTCaseExpression*>(expr);
            inExpression(vec, e->caseExpr.get(), name);
            inStatement(vec, e->statements.get(), name);
            break;
        }
        case ASTExpressionKind::AEK_Cast: {
            ASTCastExpression* e = static_cast<ASTCastExpression*>(expr);
            inName(vec, e->castType, name);
            inExpression(vec, e->obj.get(), name);
            break;
        }
        case ASTExpressionKind::AEK_Closure: {
            ASTClosureExpression* e = static_cast<ASTClosureExpression*>(expr);
            inDeclaration(vec, e->closure.get(), name);
            break;
        }
        case ASTExpressionKind::AEK_ConstrInit: {
            ASTConstrInitExpression* e = static_cast<ASTConstrInitExpression*>(expr);
            for (size_t i = 0, count = e->inits.size(); i < count; i++)
                inExpression(vec, e->inits[i], name);
            break;
        }
        case ASTExpressionKind::AEK_ContextMod: {
            ASTContextModExpression* e = static_cast<ASTContextModExpression*>(expr);
            inExpression(vec, e->expression.get(), name);
            break;
        }
        case ASTExpressionKind::AEK_ContextRow:
            break;
        case ASTExpressionKind::AEK_Decl: {
            ASTDeclExpression* e = static_cast<ASTDeclExpression*>(expr);
            inDeclaration(vec, e->declaration.get(), name);
            break;
        }
        case ASTExpressionKind::AEK_Find: {
            ASTFindExpression* e = static_cast<ASTFindExpression*>(expr);
            inExpression(vec, e->result.get(), name);
            inExpression(vec, e->data.get(), name);
            inExpression(vec, e->where.get(), name);
            break;
        }
        case ASTExpressionKind::AEK_Hash: {
            ASTHashExpression* e = static_cast<ASTHashExpression*>(expr);
            for (size_t i = 0, count = e->elements.size(); i < count; i++)
                inExpression(vec, e->elements[i], name);
            break;
        }
        case ASTExpressionKind::AEK_HashdeclHash: {
            ASTHashdeclHashExpression* e = static_cast<ASTHashdeclHashExpression*>(expr);
            inName(vec, e->hashdecl, name);
            inExpression(vec, e->hash.get(), name);
            break;
        }
        case ASTExpressionKind::AEK_HashElement: {
            ASTHashElementExpression* e = static_cast<ASTHashElementExpression*>(expr);
            inExpression(vec, e->key.get(), name);
            inExpression(vec, e->value.get(), name);
            break;
        }
        case ASTExpressionKind::AEK_ImplicitArg:
        case ASTExpressionKind::AEK_ImplicitElem:
            break;
        case ASTExpressionKind::AEK_Index: {
            ASTIndexExpression* e = static_cast<ASTIndexExpression*>(expr);
            inExpression(vec, e->variable.get(), name);
            inExpression(vec, e->index.get(), name);
            break;
        }
        case ASTExpressionKind::AEK_List: {
            ASTListExpression* e = static_cast<ASTListExpression*>(expr);
            for (size_t i = 0, count = e->elements.size(); i < count; i++)
                inExpression(vec, e->elements[i], name);
            break;
        }
        case ASTExpressionKind::AEK_Literal:
            break;
        case ASTExpressionKind::AEK_Name: {
            ASTNameExpression* e = static_cast<ASTNameExpression*>(expr);
            inName(vec, e->name, name);
            break;
        }
        case ASTExpressionKind::AEK_Range: {
            ASTRangeExpression* e = static_cast<ASTRangeExpression*>(expr);
            inExpression(vec, e->left.get(), name);
            inExpression(vec, e->right.get(), name);
            break;
        }
        case ASTExpressionKind::AEK_Regex:
        case ASTExpressionKind::AEK_RegexSubst:
        case ASTExpressionKind::AEK_RegexTrans:
            break;
        case ASTExpressionKind::AEK_Returns: {
            ASTReturnsExpression* e = static_cast<ASTReturnsExpression*>(expr);
            inExpression(vec, e->typeName.get(), name);
            break;
        }
        case ASTExpressionKind::AEK_SwitchBody: {
            ASTSwitchBodyExpression* e = static_cast<ASTSwitchBodyExpression*>(expr);
            for (size_t i = 0, count = e->cases.size(); i < count; i++)
                inExpression(vec, e->cases[i], name);
            break;
        }
        case ASTExpressionKind::AEK_Ternary: {
            ASTTernaryExpression* e = static_cast<ASTTernaryExpression*>(expr);
            inExpression(vec, e->condition.get(), name);
            inExpression(vec, e->exprTrue.get(), name);
            inExpression(vec, e->exprFalse.get(), name);
            break;
        }
        case ASTExpressionKind::AEK_Unary: {
            ASTUnaryExpression* e = static_cast<ASTUnaryExpression*>(expr);
            inExpression(vec, e->expression.get(), name);
            break;
        }
        default:
            break;
    }
}

void FindReferencesQuery::inName(std::vector<ASTNode*>* vec, ASTName& n, const std::string& name) {
    if (n.name == name)
        vec->push_back(&n);
}

void FindReferencesQuery::inName(std::vector<ASTNode*>* vec, ASTName* n, const std::string& name) {
    if (!n)
        return;
    if (n->name == name)
        vec->push_back(n);
}

void FindReferencesQuery::inStatement(std::vector<ASTNode*>* vec, ASTStatement* stmt, const std::string& name) {
    if (!stmt)
        return;

    switch (stmt->getKind()) {
        case ASTStatementKind::ASK_Block: {
            ASTStatementBlock* s = static_cast<ASTStatementBlock*>(stmt);
            for (size_t i = 0, count = s->statements.size(); i < count; i++)
                inStatement(vec, s->statements[i], name);
            break;
        }
        case ASTStatementKind::ASK_Break:
            break;
        case ASTStatementKind::ASK_Call: {
            ASTCallStatement* s = static_cast<ASTCallStatement*>(stmt);
            inExpression(vec, s->call.get(), name);
            break;
        }
        case ASTStatementKind::ASK_Context: {
            ASTContextStatement* s = static_cast<ASTContextStatement*>(stmt);
            inExpression(vec, s->name.get(), name);
            inExpression(vec, s->data.get(), name);
            for (size_t i = 0, count = s->contextMods.size(); i < count; i++)
                inExpression(vec, s->contextMods[i], name);
            inStatement(vec, s->statements.get(), name);
            break;
        }
        case ASTStatementKind::ASK_Continue:
            break;
        case ASTStatementKind::ASK_DoWhile: {
            ASTDoWhileStatement* s = static_cast<ASTDoWhileStatement*>(stmt);
            inExpression(vec, s->condition.get(), name);
            inStatement(vec, s->statement.get(), name);
            break;
        }
        case ASTStatementKind::ASK_Expression: {
            ASTExpressionStatement* s = static_cast<ASTExpressionStatement*>(stmt);
            inExpression(vec, s->expression.get(), name);
            break;
        }
        case ASTStatementKind::ASK_For: {
            ASTForStatement* s = static_cast<ASTForStatement*>(stmt);
            inExpression(vec, s->init.get(), name);
            inExpression(vec, s->condition.get(), name);
            inExpression(vec, s->iteration.get(), name);
            inStatement(vec, s->statement.get(), name);
            break;
        }
        case ASTStatementKind::ASK_Foreach: {
            ASTForeachStatement* s = static_cast<ASTForeachStatement*>(stmt);
            inExpression(vec, s->value.get(), name);
            inExpression(vec, s->source.get(), name);
            inStatement(vec, s->statement.get(), name);
            break;
        }
        case ASTStatementKind::ASK_If: {
            ASTIfStatement* s = static_cast<ASTIfStatement*>(stmt);
            inExpression(vec, s->condition.get(), name);
            inStatement(vec, s->stmtThen.get(), name);
            inStatement(vec, s->stmtElse.get(), name);
            break;
        }
        case ASTStatementKind::ASK_OnBlockExit: {
            ASTOnBlockExitStatement* s = static_cast<ASTOnBlockExitStatement*>(stmt);
            inStatement(vec, s->statement.get(), name);
            break;
        }
        case ASTStatementKind::ASK_Rethrow:
            break;
        case ASTStatementKind::ASK_Return: {
            ASTReturnStatement* s = static_cast<ASTReturnStatement*>(stmt);
            inExpression(vec, s->retval.get(), name);
            break;
        }
        case ASTStatementKind::ASK_Summarize: {
            ASTSummarizeStatement* s = static_cast<ASTSummarizeStatement*>(stmt);
            inExpression(vec, s->name.get(), name);
            inExpression(vec, s->data.get(), name);
            inExpression(vec, s->by.get(), name);
            for (size_t i = 0, count = s->contextMods.size(); i < count; i++)
                inExpression(vec, s->contextMods[i], name);
            inStatement(vec, s->statements.get(), name);
            break;
        }
        case ASTStatementKind::ASK_Switch: {
            ASTSwitchStatement* s = static_cast<ASTSwitchStatement*>(stmt);
            inExpression(vec, s->variable.get(), name);
            inExpression(vec, s->body.get(), name);
            break;
        }
        case ASTStatementKind::ASK_ThreadExit:
            break;
        case ASTStatementKind::ASK_Throw: {
            ASTThrowStatement* s = static_cast<ASTThrowStatement*>(stmt);
            inExpression(vec, s->expression.get(), name);
            break;
        }
        case ASTStatementKind::ASK_Try: {
            ASTTryStatement* s = static_cast<ASTTryStatement*>(stmt);
            inStatement(vec, s->tryStmt.get(), name);
            inExpression(vec, s->catchVar.get(), name);
            inStatement(vec, s->catchStmt.get(), name);
            break;
        }
        case ASTStatementKind::ASK_While: {
            ASTWhileStatement* s = static_cast<ASTWhileStatement*>(stmt);
            inExpression(vec, s->condition.get(), name);
            inStatement(vec, s->statement.get(), name);
            break;
        }
        default:
            break;
    }
}

void FindReferencesQuery::findIntern(ASTTree* tree, std::vector<ASTNode*>* vec, const std::string& name) {
    for (size_t i = 0, count = tree->nodes.size(); i < count; i++) {
        ASTNode* node = tree->nodes[i];
        switch (node->getNodeType()) {
            case ANT_Declaration: {
                ASTDeclaration* decl = static_cast<ASTDeclaration*>(node);
                inDeclaration(vec, decl, name);
                break;
            }
            case ANT_Expression: {
                ASTExpression* expr = static_cast<ASTExpression*>(node);
                inExpression(vec, expr, name);
                break;
            }
            case ANT_Name: {
                ASTName* n = static_cast<ASTName*>(node);
                inName(vec, n, name);
                break;
            }
            case ANT_ParseOption:
                break;
            case ANT_Statement: {
                ASTStatement* stmt = static_cast<ASTStatement*>(node);
                inStatement(vec, stmt, name);
                break;
            }
            case ANT_None:
            default:
                break;
        }
    }
}

std::vector<ASTNode*>* FindReferencesQuery::find(ASTTree* tree, const std::string& name) {
    if (!tree)
        return nullptr;

    std::unique_ptr<std::vector<ASTNode*> > vec(new std::vector<ASTNode*>);
    if (!vec)
        return nullptr;
    findIntern(tree, vec.get(), name);
    if (name[0] == '*') {
        std::string nameWithoutAsterisk(name.c_str()+1);
        findIntern(tree, vec.get(), nameWithoutAsterisk);
    }
    return vec.release();
}
