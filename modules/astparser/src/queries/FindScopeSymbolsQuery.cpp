/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  FindScopeSymbolsQuery.cpp

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

#include "queries/FindScopeSymbolsQuery.h"

#include <memory>

#include "ast/AST.h"
#include "queries/FindNodeAndParentsQuery.h"

void FindScopeSymbolsQuery::collectExpressionSymbols(std::vector<ASTScopeSymbolInfo>* vec,
        ASTExpression* expr, int scopeLevel) {
    if (!expr) {
        return;
    }

    switch (expr->getKind()) {
        case ASTExpressionKind::AEK_Decl: {
            ASTDeclExpression* e = static_cast<ASTDeclExpression*>(expr);
            ASTDeclaration* decl = e->declaration.get();
            if (decl && decl->getKind() == ASTDeclarationKind::ADK_Variable) {
                ASTVariableDeclaration* d = static_cast<ASTVariableDeclaration*>(decl);
                vec->push_back(ASTScopeSymbolInfo(ASTSymbolInfo(ASYK_Variable, &d->name), scopeLevel));
            }
            break;
        }
        case ASTExpressionKind::AEK_List: {
            ASTListExpression* e = static_cast<ASTListExpression*>(expr);
            for (size_t i = 0, count = e->elements.size(); i < count; i++) {
                collectExpressionSymbols(vec, e->elements[i], scopeLevel);
            }
            break;
        }
        case ASTExpressionKind::AEK_Assignment: {
            ASTAssignmentExpression* e = static_cast<ASTAssignmentExpression*>(expr);
            collectExpressionSymbols(vec, e->left.get(), scopeLevel);
            break;
        }
        default:
            break;
    }
}

void FindScopeSymbolsQuery::collectLocalSymbols(std::vector<ASTScopeSymbolInfo>* vec,
        ASTStatement* stmt, ast_loc_t line, ast_loc_t col, int scopeLevel) {
    if (!stmt) {
        return;
    }

    switch (stmt->getKind()) {
        case ASTStatementKind::ASK_Block: {
            ASTStatementBlock* s = static_cast<ASTStatementBlock*>(stmt);
            for (size_t i = 0, count = s->statements.size(); i < count; i++) {
                ASTStatement* inner = s->statements[i];
                // Only collect variables declared before the current position
                if (inner && inner->loc.firstLine <= line) {
                    collectLocalSymbols(vec, inner, line, col, scopeLevel);
                }
            }
            break;
        }
        case ASTStatementKind::ASK_Expression: {
            ASTExpressionStatement* s = static_cast<ASTExpressionStatement*>(stmt);
            collectExpressionSymbols(vec, s->expression.get(), scopeLevel);
            break;
        }
        case ASTStatementKind::ASK_For: {
            ASTForStatement* s = static_cast<ASTForStatement*>(stmt);
            // Collect loop variable from init expression
            collectExpressionSymbols(vec, s->init.get(), scopeLevel);
            if (s->statement && s->statement->loc.inside(line, col)) {
                collectLocalSymbols(vec, s->statement.get(), line, col, scopeLevel);
            }
            break;
        }
        case ASTStatementKind::ASK_Foreach: {
            ASTForeachStatement* s = static_cast<ASTForeachStatement*>(stmt);
            // Collect iteration variable
            collectExpressionSymbols(vec, s->value.get(), scopeLevel);
            if (s->statement && s->statement->loc.inside(line, col)) {
                collectLocalSymbols(vec, s->statement.get(), line, col, scopeLevel);
            }
            break;
        }
        case ASTStatementKind::ASK_If: {
            ASTIfStatement* s = static_cast<ASTIfStatement*>(stmt);
            if (s->stmtThen && s->stmtThen->loc.inside(line, col)) {
                collectLocalSymbols(vec, s->stmtThen.get(), line, col, scopeLevel);
            }
            if (s->stmtElse && s->stmtElse->loc.inside(line, col)) {
                collectLocalSymbols(vec, s->stmtElse.get(), line, col, scopeLevel);
            }
            break;
        }
        case ASTStatementKind::ASK_While: {
            ASTWhileStatement* s = static_cast<ASTWhileStatement*>(stmt);
            if (s->statement && s->statement->loc.inside(line, col)) {
                collectLocalSymbols(vec, s->statement.get(), line, col, scopeLevel);
            }
            break;
        }
        case ASTStatementKind::ASK_DoWhile: {
            ASTDoWhileStatement* s = static_cast<ASTDoWhileStatement*>(stmt);
            if (s->statement && s->statement->loc.inside(line, col)) {
                collectLocalSymbols(vec, s->statement.get(), line, col, scopeLevel);
            }
            break;
        }
        case ASTStatementKind::ASK_Try: {
            ASTTryStatement* s = static_cast<ASTTryStatement*>(stmt);
            if (s->tryStmt && s->tryStmt->loc.inside(line, col)) {
                collectLocalSymbols(vec, s->tryStmt.get(), line, col, scopeLevel);
            }
            if (s->catchStmt && s->catchStmt->loc.inside(line, col)) {
                // Collect the catch variable
                collectExpressionSymbols(vec, s->catchVar.get(), scopeLevel);
                collectLocalSymbols(vec, s->catchStmt.get(), line, col, scopeLevel);
            }
            break;
        }
        case ASTStatementKind::ASK_Switch: {
            ASTSwitchStatement* s = static_cast<ASTSwitchStatement*>(stmt);
            if (s->body) {
                ASTSwitchBodyExpression* body = static_cast<ASTSwitchBodyExpression*>(s->body.get());
                for (size_t i = 0, count = body->cases.size(); i < count; i++) {
                    ASTCaseExpression* caseExpr = static_cast<ASTCaseExpression*>(body->cases[i]);
                    if (caseExpr && caseExpr->statements && caseExpr->statements->loc.inside(line, col)) {
                        collectLocalSymbols(vec, caseExpr->statements.get(), line, col, scopeLevel);
                    }
                }
            }
            break;
        }
        case ASTStatementKind::ASK_Context: {
            ASTContextStatement* s = static_cast<ASTContextStatement*>(stmt);
            if (s->statements && s->statements->loc.inside(line, col)) {
                collectLocalSymbols(vec, s->statements.get(), line, col, scopeLevel);
            }
            break;
        }
        case ASTStatementKind::ASK_Summarize: {
            ASTSummarizeStatement* s = static_cast<ASTSummarizeStatement*>(stmt);
            if (s->statements && s->statements->loc.inside(line, col)) {
                collectLocalSymbols(vec, s->statements.get(), line, col, scopeLevel);
            }
            break;
        }
        default:
            break;
    }
}

void FindScopeSymbolsQuery::collectFunctionParams(std::vector<ASTScopeSymbolInfo>* vec,
        ASTDeclaration* decl, int scopeLevel) {
    if (!decl) {
        return;
    }

    if (decl->getKind() == ASTDeclarationKind::ADK_Function) {
        ASTFunctionDeclaration* d = static_cast<ASTFunctionDeclaration*>(decl);
        // Collect parameters
        collectExpressionSymbols(vec, d->params.get(), scopeLevel);
    } else if (decl->getKind() == ASTDeclarationKind::ADK_Closure) {
        ASTClosureDeclaration* d = static_cast<ASTClosureDeclaration*>(decl);
        collectExpressionSymbols(vec, d->params.get(), scopeLevel);
    }
}

void FindScopeSymbolsQuery::collectClassSymbols(std::vector<ASTScopeSymbolInfo>* vec,
        ASTDeclaration* decl, int scopeLevel) {
    if (!decl || decl->getKind() != ASTDeclarationKind::ADK_Class) {
        return;
    }

    ASTClassDeclaration* d = static_cast<ASTClassDeclaration*>(decl);

    // Collect class members and methods
    for (size_t i = 0, count = d->declarations.size(); i < count; i++) {
        ASTDeclaration* member = d->declarations[i];
        if (!member) {
            continue;
        }

        switch (member->getKind()) {
            case ASTDeclarationKind::ADK_Function: {
                ASTFunctionDeclaration* m = static_cast<ASTFunctionDeclaration*>(member);
                // Use ASYK_Method for class methods, ASYK_Constructor for constructors
                ASTSymbolKind kind = (m->name.name == "constructor" || m->name.name == "destructor" ||
                                      m->name.name == "copy") ? ASYK_Constructor : ASYK_Method;
                vec->push_back(ASTScopeSymbolInfo(ASTSymbolInfo(kind, &m->name), scopeLevel));
                break;
            }
            case ASTDeclarationKind::ADK_MemberGroup: {
                ASTMemberGroupDeclaration* mg = static_cast<ASTMemberGroupDeclaration*>(member);
                for (size_t j = 0, jcount = mg->members.size(); j < jcount; j++) {
                    collectExpressionSymbols(vec, mg->members[j], scopeLevel);
                }
                break;
            }
            case ASTDeclarationKind::ADK_Variable: {
                ASTVariableDeclaration* m = static_cast<ASTVariableDeclaration*>(member);
                vec->push_back(ASTScopeSymbolInfo(ASTSymbolInfo(ASYK_Field, &m->name), scopeLevel));
                break;
            }
            case ASTDeclarationKind::ADK_Constant: {
                ASTConstantDeclaration* m = static_cast<ASTConstantDeclaration*>(member);
                vec->push_back(ASTScopeSymbolInfo(ASTSymbolInfo(ASYK_Constant, &m->name), scopeLevel));
                break;
            }
            default:
                break;
        }
    }
}

void FindScopeSymbolsQuery::collectHashSymbols(std::vector<ASTScopeSymbolInfo>* vec,
        ASTDeclaration* decl, int scopeLevel) {
    if (!decl || decl->getKind() != ASTDeclarationKind::ADK_Hash) {
        return;
    }

    ASTHashDeclaration* d = static_cast<ASTHashDeclaration*>(decl);

    // Collect hash members
    for (size_t i = 0, count = d->declarations.size(); i < count; i++) {
        ASTDeclaration* member = d->declarations[i];
        if (!member) {
            continue;
        }

        if (member->getKind() == ASTDeclarationKind::ADK_HashMember) {
            ASTHashMemberDeclaration* m = static_cast<ASTHashMemberDeclaration*>(member);
            vec->push_back(ASTScopeSymbolInfo(ASTSymbolInfo(ASYK_Field, &m->name), scopeLevel));
        }
    }
}

void FindScopeSymbolsQuery::collectNamespaceSymbols(std::vector<ASTScopeSymbolInfo>* vec,
        ASTDeclaration* decl, int scopeLevel) {
    if (!decl || decl->getKind() != ASTDeclarationKind::ADK_Namespace) {
        return;
    }

    ASTNamespaceDeclaration* d = static_cast<ASTNamespaceDeclaration*>(decl);

    for (size_t i = 0, count = d->declarations.size(); i < count; i++) {
        ASTDeclaration* member = d->declarations[i];
        if (!member) {
            continue;
        }

        switch (member->getKind()) {
            case ASTDeclarationKind::ADK_Class: {
                ASTClassDeclaration* m = static_cast<ASTClassDeclaration*>(member);
                vec->push_back(ASTScopeSymbolInfo(ASTSymbolInfo(ASYK_Class, &m->name), scopeLevel));
                break;
            }
            case ASTDeclarationKind::ADK_Function: {
                ASTFunctionDeclaration* m = static_cast<ASTFunctionDeclaration*>(member);
                vec->push_back(ASTScopeSymbolInfo(ASTSymbolInfo(ASYK_Function, &m->name), scopeLevel));
                break;
            }
            case ASTDeclarationKind::ADK_Constant: {
                ASTConstantDeclaration* m = static_cast<ASTConstantDeclaration*>(member);
                vec->push_back(ASTScopeSymbolInfo(ASTSymbolInfo(ASYK_Constant, &m->name), scopeLevel));
                break;
            }
            case ASTDeclarationKind::ADK_Hash: {
                ASTHashDeclaration* m = static_cast<ASTHashDeclaration*>(member);
                vec->push_back(ASTScopeSymbolInfo(ASTSymbolInfo(ASYK_Interface, &m->name), scopeLevel));
                break;
            }
            case ASTDeclarationKind::ADK_Typedef: {
                ASTTypedefDeclaration* m = static_cast<ASTTypedefDeclaration*>(member);
                vec->push_back(ASTScopeSymbolInfo(ASTSymbolInfo(ASYK_TypeAlias, &m->name), scopeLevel));
                break;
            }
            case ASTDeclarationKind::ADK_Variable: {
                ASTVariableDeclaration* m = static_cast<ASTVariableDeclaration*>(member);
                vec->push_back(ASTScopeSymbolInfo(ASTSymbolInfo(ASYK_Variable, &m->name), scopeLevel));
                break;
            }
            case ASTDeclarationKind::ADK_Namespace: {
                ASTNamespaceDeclaration* m = static_cast<ASTNamespaceDeclaration*>(member);
                vec->push_back(ASTScopeSymbolInfo(ASTSymbolInfo(ASYK_Namespace, &m->name), scopeLevel));
                break;
            }
            default:
                break;
        }
    }
}

std::vector<ASTScopeSymbolInfo>* FindScopeSymbolsQuery::find(ASTTree* tree, ast_loc_t line, ast_loc_t col) {
    if (!tree) {
        return nullptr;
    }

    // Find the node and its parent chain at the given position
    std::unique_ptr<std::vector<ASTNode*>> nodes(FindNodeAndParentsQuery::find(tree, line, col));
    if (!nodes || nodes->empty()) {
        return nullptr;
    }

    std::unique_ptr<std::vector<ASTScopeSymbolInfo>> result(new std::vector<ASTScopeSymbolInfo>);
    result->reserve(32);

    int scopeLevel = 0;

    // Process the scope chain from innermost to outermost
    for (size_t i = 0, count = nodes->size(); i < count; i++) {
        ASTNode* node = nodes->at(i);
        if (!node) {
            continue;
        }

        if (node->getNodeType() == ANT_Declaration) {
            ASTDeclaration* decl = static_cast<ASTDeclaration*>(node);

            switch (decl->getKind()) {
                case ASTDeclarationKind::ADK_Function:
                case ASTDeclarationKind::ADK_Closure: {
                    // Collect function parameters
                    collectFunctionParams(result.get(), decl, scopeLevel);

                    // Collect local variables from the function body
                    if (decl->getKind() == ASTDeclarationKind::ADK_Function) {
                        ASTFunctionDeclaration* d = static_cast<ASTFunctionDeclaration*>(decl);
                        collectLocalSymbols(result.get(), d->body.get(), line, col, scopeLevel);
                    } else {
                        ASTClosureDeclaration* d = static_cast<ASTClosureDeclaration*>(decl);
                        collectLocalSymbols(result.get(), d->body.get(), line, col, scopeLevel);
                    }
                    scopeLevel++;
                    break;
                }
                case ASTDeclarationKind::ADK_Class: {
                    collectClassSymbols(result.get(), decl, scopeLevel);
                    scopeLevel++;
                    break;
                }
                case ASTDeclarationKind::ADK_Hash: {
                    collectHashSymbols(result.get(), decl, scopeLevel);
                    scopeLevel++;
                    break;
                }
                case ASTDeclarationKind::ADK_Namespace: {
                    collectNamespaceSymbols(result.get(), decl, scopeLevel);
                    scopeLevel++;
                    break;
                }
                default:
                    break;
            }
        } else if (node->getNodeType() == ANT_Statement) {
            // Handle statement blocks for local variables
            ASTStatement* stmt = static_cast<ASTStatement*>(node);
            if (stmt->getKind() == ASTStatementKind::ASK_Block) {
                collectLocalSymbols(result.get(), stmt, line, col, scopeLevel);
            }
        }
    }

    // Also collect top-level declarations if we're not already at the file level
    for (size_t i = 0, count = tree->nodes.size(); i < count; i++) {
        ASTNode* node = tree->nodes[i];
        if (!node || node->getNodeType() != ANT_Declaration) {
            continue;
        }

        ASTDeclaration* decl = static_cast<ASTDeclaration*>(node);
        switch (decl->getKind()) {
            case ASTDeclarationKind::ADK_Class: {
                ASTClassDeclaration* d = static_cast<ASTClassDeclaration*>(decl);
                result->push_back(ASTScopeSymbolInfo(ASTSymbolInfo(ASYK_Class, &d->name), scopeLevel));
                break;
            }
            case ASTDeclarationKind::ADK_Function: {
                ASTFunctionDeclaration* d = static_cast<ASTFunctionDeclaration*>(decl);
                result->push_back(ASTScopeSymbolInfo(ASTSymbolInfo(ASYK_Function, &d->name), scopeLevel));
                break;
            }
            case ASTDeclarationKind::ADK_Constant: {
                ASTConstantDeclaration* d = static_cast<ASTConstantDeclaration*>(decl);
                result->push_back(ASTScopeSymbolInfo(ASTSymbolInfo(ASYK_Constant, &d->name), scopeLevel));
                break;
            }
            case ASTDeclarationKind::ADK_Hash: {
                ASTHashDeclaration* d = static_cast<ASTHashDeclaration*>(decl);
                result->push_back(ASTScopeSymbolInfo(ASTSymbolInfo(ASYK_Interface, &d->name), scopeLevel));
                break;
            }
            case ASTDeclarationKind::ADK_Typedef: {
                ASTTypedefDeclaration* d = static_cast<ASTTypedefDeclaration*>(decl);
                result->push_back(ASTScopeSymbolInfo(ASTSymbolInfo(ASYK_TypeAlias, &d->name), scopeLevel));
                break;
            }
            case ASTDeclarationKind::ADK_Namespace: {
                // Only add the namespace name itself at the top level
                // Namespace contents are only accessible with a prefix unless we're inside the namespace
                // (in which case they were already collected from the parent chain)
                ASTNamespaceDeclaration* d = static_cast<ASTNamespaceDeclaration*>(decl);
                result->push_back(ASTScopeSymbolInfo(ASTSymbolInfo(ASYK_Namespace, &d->name), scopeLevel));
                break;
            }
            case ASTDeclarationKind::ADK_Variable: {
                ASTVariableDeclaration* d = static_cast<ASTVariableDeclaration*>(decl);
                result->push_back(ASTScopeSymbolInfo(ASTSymbolInfo(ASYK_Variable, &d->name), scopeLevel));
                break;
            }
            default:
                break;
        }
    }

    return result.release();
}
