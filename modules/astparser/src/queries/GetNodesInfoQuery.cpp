/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  GetNodesInfoQuery.cpp

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

#include "queries/GetNodesInfoQuery.h"

#include <sstream>

#include "qore/Qore.h"

#include "AstTreePrinter.h"
#include "ast/AST.h"
#include "queries/SymbolInfoFixes.h"

QoreHashNode* GetNodesInfoQuery::getDeclaration(ASTTree* tree, ASTDeclaration* decl, ExceptionSink* xsink) {
    if (!decl)
        return nullptr;

    ReferenceHolder<QoreHashNode> nodeInfo(new QoreHashNode, xsink);
    if (*xsink)
        return nullptr;

    nodeInfo->setKeyValue("nodetype", static_cast<int64>(ANT_Declaration), xsink);
    nodeInfo->setKeyValue("kind", static_cast<int64>(decl->getKind()), xsink);
    nodeInfo->setKeyValue("loc", getLocation(decl->loc, xsink), xsink);
    switch (decl->getKind()) {
        case ASTDeclarationKind::ADK_Class: {
            ASTClassDeclaration* d = static_cast<ASTClassDeclaration*>(decl);
            nodeInfo->setKeyValue("modifiers", getModifiers(d->modifiers), xsink);
            nodeInfo->setKeyValue("name", getName(tree, d->name, xsink), xsink);
            ReferenceHolder<QoreListNode> inherits(new QoreListNode, xsink);
            ReferenceHolder<QoreListNode> declarations(new QoreListNode, xsink);
            if (*xsink)
                return nullptr;
            for (size_t i = 0, count = d->inherits.size(); i < count; i++)
                inherits->push(getDeclaration(tree, d->inherits[i], xsink), xsink);
            for (size_t i = 0, count = d->declarations.size(); i < count; i++)
                declarations->push(getDeclaration(tree, d->declarations[i], xsink), xsink);
            nodeInfo->setKeyValue("inherits", inherits.release(), xsink);
            nodeInfo->setKeyValue("declarations", declarations.release(), xsink);
            break;
        }
        case ASTDeclarationKind::ADK_Closure: {
            ASTClosureDeclaration* d = static_cast<ASTClosureDeclaration*>(decl);
            nodeInfo->setKeyValue("modifiers", getModifiers(d->modifiers), xsink);
            nodeInfo->setKeyValue("returnType", getExpression(tree, d->returnType.get(), xsink), xsink);
            nodeInfo->setKeyValue("params", getExpression(tree, d->params.get(), xsink), xsink);
            nodeInfo->setKeyValue("body", getStatement(tree, d->body.get(), xsink), xsink);
            break;
        }
        case ASTDeclarationKind::ADK_Constant: {
            ASTConstantDeclaration* d = static_cast<ASTConstantDeclaration*>(decl);
            nodeInfo->setKeyValue("modifiers", getModifiers(d->modifiers), xsink);
            nodeInfo->setKeyValue("name", getName(tree, d->name, xsink), xsink);
            nodeInfo->setKeyValue("value", getExpression(tree, d->value.get(), xsink), xsink);
            break;
        }
        case ASTDeclarationKind::ADK_Function: {
            ASTFunctionDeclaration* d = static_cast<ASTFunctionDeclaration*>(decl);
            ASTSymbolInfo si(ASYK_Function, ASUK_FuncDeclName, d->name.loc, d->name.name);
            SymbolInfoFixes::fixFunctionInfo(tree, si, true);
            nodeInfo->setKeyValue("siKind", static_cast<int64>(si.kind), xsink);
            nodeInfo->setKeyValue("modifiers", getModifiers(d->modifiers), xsink);
            nodeInfo->setKeyValue("name", getName(tree, d->name, xsink), xsink);
            nodeInfo->setKeyValue("afdKind", static_cast<int64>(d->afdKind), xsink);
            nodeInfo->setKeyValue("returnType", getExpression(tree, d->returnType.get(), xsink), xsink);
            nodeInfo->setKeyValue("params", getExpression(tree, d->params.get(), xsink), xsink);
            nodeInfo->setKeyValue("inits", getExpression(tree, d->inits.get(), xsink), xsink);
            nodeInfo->setKeyValue("body", getStatement(tree, d->body.get(), xsink), xsink);
            break;
        }
        case ASTDeclarationKind::ADK_Hash: {
            ASTHashDeclaration* d = static_cast<ASTHashDeclaration*>(decl);
            nodeInfo->setKeyValue("modifiers", getModifiers(d->modifiers), xsink);
            nodeInfo->setKeyValue("name", getName(tree, d->name, xsink), xsink);
            ReferenceHolder<QoreListNode> declarations(new QoreListNode, xsink);
            if (*xsink)
                return nullptr;
            for (size_t i = 0, count = d->declarations.size(); i < count; i++)
                declarations->push(getDeclaration(tree, d->declarations[i], xsink), xsink);
            nodeInfo->setKeyValue("declarations", declarations.release(), xsink);
            break;
        }
        case ASTDeclarationKind::ADK_HashMember: {
            ASTHashMemberDeclaration* d = static_cast<ASTHashMemberDeclaration*>(decl);
            nodeInfo->setKeyValue("name", getName(tree, d->name, xsink), xsink);
            nodeInfo->setKeyValue("typeName", getName(tree, d->typeName, xsink), xsink);
            nodeInfo->setKeyValue("init", getExpression(tree, d->init.get(), xsink), xsink);
            nodeInfo->setKeyValue("constr", d->constr, xsink);
            break;
        }
        case ASTDeclarationKind::ADK_MemberGroup: {
            ASTMemberGroupDeclaration* d = static_cast<ASTMemberGroupDeclaration*>(decl);
            nodeInfo->setKeyValue("modifiers", getModifiers(d->modifiers), xsink);
            ReferenceHolder<QoreListNode> members(new QoreListNode, xsink);
            if (*xsink)
                return nullptr;
            for (size_t i = 0, count = d->members.size(); i < count; i++)
                members->push(getExpression(tree, d->members[i], xsink), xsink);
            nodeInfo->setKeyValue("members", members.release(), xsink);
            break;
        }
        case ASTDeclarationKind::ADK_Namespace: {
            ASTNamespaceDeclaration* d = static_cast<ASTNamespaceDeclaration*>(decl);
            nodeInfo->setKeyValue("modifiers", getModifiers(d->modifiers), xsink);
            nodeInfo->setKeyValue("name", getName(tree, d->name, xsink), xsink);
            ReferenceHolder<QoreListNode> declarations(new QoreListNode, xsink);
            if (*xsink)
                return nullptr;
            for (size_t i = 0, count = d->declarations.size(); i < count; i++)
                declarations->push(getDeclaration(tree, d->declarations[i], xsink), xsink);
            nodeInfo->setKeyValue("declarations", declarations.release(), xsink);
            break;
        }
        case ASTDeclarationKind::ADK_Superclass: {
            ASTSuperclassDeclaration* d = static_cast<ASTSuperclassDeclaration*>(decl);
            nodeInfo->setKeyValue("modifiers", getModifiers(d->modifiers), xsink);
            nodeInfo->setKeyValue("name", getName(tree, d->name, xsink), xsink);
            break;
        }
        case ASTDeclarationKind::ADK_Variable: {
            ASTVariableDeclaration* d = static_cast<ASTVariableDeclaration*>(decl);
            nodeInfo->setKeyValue("modifiers", getModifiers(d->modifiers), xsink);
            nodeInfo->setKeyValue("name", getName(tree, d->name, xsink), xsink);
            nodeInfo->setKeyValue("typeName", getName(tree, d->typeName, xsink), xsink);
            break;
        }
        case ASTDeclarationKind::ADK_VarList: {
            ASTVarListDeclaration* d = static_cast<ASTVarListDeclaration*>(decl);
            nodeInfo->setKeyValue("modifiers", getModifiers(d->modifiers), xsink);
            nodeInfo->setKeyValue("variables", getExpression(tree, d->variables.get(), xsink), xsink);
            break;
        }
        case ASTDeclarationKind::ADK_Typedef: {
            ASTTypedefDeclaration* d = static_cast<ASTTypedefDeclaration*>(decl);
            nodeInfo->setKeyValue("modifiers", getModifiers(d->modifiers), xsink);
            nodeInfo->setKeyValue("name", getName(tree, d->name, xsink), xsink);
            nodeInfo->setKeyValue("typeName", getName(tree, d->typeName, xsink), xsink);
            break;
        }
        default:
            break;
    }
    if (*xsink)
        return nullptr;
    return nodeInfo.release();
}

QoreHashNode* GetNodesInfoQuery::getExpression(ASTTree* tree, ASTExpression* expr, ExceptionSink* xsink) {
    if (!expr)
        return nullptr;

    ReferenceHolder<QoreHashNode> nodeInfo(new QoreHashNode, xsink);
    if (*xsink)
        return nullptr;

    nodeInfo->setKeyValue("nodetype", static_cast<int64>(ANT_Expression), xsink);
    nodeInfo->setKeyValue("kind", static_cast<int64>(expr->getKind()), xsink);
    nodeInfo->setKeyValue("loc", getLocation(expr->loc, xsink), xsink);
    switch (expr->getKind()) {
        case ASTExpressionKind::AEK_Access: {
            ASTAccessExpression* e = static_cast<ASTAccessExpression*>(expr);
            nodeInfo->setKeyValue("variable", getExpression(tree, e->variable.get(), xsink), xsink);
            nodeInfo->setKeyValue("member", getExpression(tree, e->member.get(), xsink), xsink);
            break;
        }
        case ASTExpressionKind::AEK_Assignment: {
            ASTAssignmentExpression* e = static_cast<ASTAssignmentExpression*>(expr);
            nodeInfo->setKeyValue("left", getExpression(tree, e->left.get(), xsink), xsink);
            nodeInfo->setKeyValue("right", getExpression(tree, e->right.get(), xsink), xsink);
            break;
        }
        case ASTExpressionKind::AEK_Backquote: {
            ASTBackquoteExpression* e = static_cast<ASTBackquoteExpression*>(expr);
            nodeInfo->setKeyValue("command", new QoreStringNode(e->command), xsink);
            break;
        }
        case ASTExpressionKind::AEK_Binary: {
            ASTBinaryExpression* e = static_cast<ASTBinaryExpression*>(expr);
            nodeInfo->setKeyValue("op", getOperator(e->op), xsink);
            nodeInfo->setKeyValue("left", getExpression(tree, e->left.get(), xsink), xsink);
            nodeInfo->setKeyValue("right", getExpression(tree, e->right.get(), xsink), xsink);
            break;
        }
        case ASTExpressionKind::AEK_Call: {
            ASTCallExpression* e = static_cast<ASTCallExpression*>(expr);
            nodeInfo->setKeyValue("target", getExpression(tree, e->target.get(), xsink), xsink);
            nodeInfo->setKeyValue("args", getExpression(tree, e->args.get(), xsink), xsink);
            break;
        }
        case ASTExpressionKind::AEK_Case: {
            ASTCaseExpression* e = static_cast<ASTCaseExpression*>(expr);
            nodeInfo->setKeyValue("op", getOperator(e->op), xsink);
            nodeInfo->setKeyValue("caseExpr", getExpression(tree, e->caseExpr.get(), xsink), xsink);
            nodeInfo->setKeyValue("statements", getStatement(tree, e->statements.get(), xsink), xsink);
            nodeInfo->setKeyValue("defaultCase", e->defaultCase, xsink);
            break;
        }
        case ASTExpressionKind::AEK_Cast: {
            ASTCastExpression* e = static_cast<ASTCastExpression*>(expr);
            nodeInfo->setKeyValue("castType", getName(tree, e->castType, xsink), xsink);
            nodeInfo->setKeyValue("obj", getExpression(tree, e->obj.get(), xsink), xsink);
            break;
        }
        case ASTExpressionKind::AEK_Closure: {
            ASTClosureExpression* e = static_cast<ASTClosureExpression*>(expr);
            nodeInfo->setKeyValue("closure", getDeclaration(tree, e->closure.get(), xsink), xsink);
            break;
        }
        case ASTExpressionKind::AEK_ConstrInit: {
            ASTConstrInitExpression* e = static_cast<ASTConstrInitExpression*>(expr);
            ReferenceHolder<QoreListNode> inits(new QoreListNode, xsink);
            if (*xsink)
                return nullptr;
            for (size_t i = 0, count = e->inits.size(); i < count; i++)
                inits->push(getExpression(tree, e->inits[i], xsink), xsink);
            nodeInfo->setKeyValue("inits", inits.release(), xsink);
            break;
        }
        case ASTExpressionKind::AEK_ContextMod: {
            ASTContextModExpression* e = static_cast<ASTContextModExpression*>(expr);
            nodeInfo->setKeyValue("acmeKind", static_cast<int64>(e->acmeKind), xsink);
            nodeInfo->setKeyValue("expression", getExpression(tree, e->expression.get(), xsink), xsink);
            break;
        }
        case ASTExpressionKind::AEK_ContextRow:
            break;
        case ASTExpressionKind::AEK_Decl: {
            ASTDeclExpression* e = static_cast<ASTDeclExpression*>(expr);
            nodeInfo->setKeyValue("declaration", getDeclaration(tree, e->declaration.get(), xsink), xsink);
            break;
        }
        case ASTExpressionKind::AEK_Find: {
            ASTFindExpression* e = static_cast<ASTFindExpression*>(expr);
            nodeInfo->setKeyValue("result", getExpression(tree, e->result.get(), xsink), xsink);
            nodeInfo->setKeyValue("data", getExpression(tree, e->data.get(), xsink), xsink);
            nodeInfo->setKeyValue("where", getExpression(tree, e->where.get(), xsink), xsink);
            break;
        }
        case ASTExpressionKind::AEK_Hash: {
            ASTHashExpression* e = static_cast<ASTHashExpression*>(expr);
            ReferenceHolder<QoreListNode> elements(new QoreListNode, xsink);
            if (*xsink)
                return nullptr;
            for (size_t i = 0, count = e->elements.size(); i < count; i++)
                elements->push(getExpression(tree, e->elements[i], xsink), xsink);
            nodeInfo->setKeyValue("elements", elements.release(), xsink);
            break;
        }
        case ASTExpressionKind::AEK_HashdeclHash: {
            ASTHashdeclHashExpression* e = static_cast<ASTHashdeclHashExpression*>(expr);
            nodeInfo->setKeyValue("hashdecl", getName(tree, e->hashdecl, xsink), xsink);
            nodeInfo->setKeyValue("hash", getExpression(tree, e->hash.get(), xsink), xsink);
            break;
        }
        case ASTExpressionKind::AEK_HashElement: {
            ASTHashElementExpression* e = static_cast<ASTHashElementExpression*>(expr);
            nodeInfo->setKeyValue("key", getExpression(tree, e->key.get(), xsink), xsink);
            nodeInfo->setKeyValue("value", getExpression(tree, e->value.get(), xsink), xsink);
            break;
        }
        case ASTExpressionKind::AEK_ImplicitArg: {
            ASTImplicitArgExpression* e = static_cast<ASTImplicitArgExpression*>(expr);
            nodeInfo->setKeyValue("offset", static_cast<int64>(e->offset), xsink);
            break;
        }
        case ASTExpressionKind::AEK_ImplicitElem:
            break;
        case ASTExpressionKind::AEK_Index: {
            ASTIndexExpression* e = static_cast<ASTIndexExpression*>(expr);
            nodeInfo->setKeyValue("variable", getExpression(tree, e->variable.get(), xsink), xsink);
            nodeInfo->setKeyValue("index", getExpression(tree, e->index.get(), xsink), xsink);
            nodeInfo->setKeyValue("indexKind", static_cast<int64>(e->indexKind), xsink);
            break;
        }
        case ASTExpressionKind::AEK_List: {
            ASTListExpression* e = static_cast<ASTListExpression*>(expr);
            ReferenceHolder<QoreListNode> elements(new QoreListNode, xsink);
            if (*xsink)
                return nullptr;
            for (size_t i = 0, count = e->elements.size(); i < count; i++)
                elements->push(getExpression(tree, e->elements[i], xsink), xsink);
            nodeInfo->setKeyValue("elements", elements.release(), xsink);
            break;
        }
        case ASTExpressionKind::AEK_Literal: {
            ASTLiteralExpression* e = static_cast<ASTLiteralExpression*>(expr);
            switch (e->kind) {
                case ALEK_Binary:
                    nodeInfo->setKeyValue("literalKind", new QoreStringNode("binary"), xsink);
                    nodeInfo->setKeyValue("value", new QoreStringNode(e->value.str), xsink);
                    break;
                case ALEK_Date:
                    nodeInfo->setKeyValue("literalKind", new QoreStringNode("date"), xsink);
                    nodeInfo->setKeyValue("value", new DateTimeNode(e->value.str), xsink);
                    break;
                case ALEK_Float:
                    nodeInfo->setKeyValue("literalKind", new QoreStringNode("float"), xsink);
                    nodeInfo->setKeyValue("value", e->value.d, xsink);
                    break;
                case ALEK_Int:
                    nodeInfo->setKeyValue("literalKind", new QoreStringNode("int"), xsink);
                    nodeInfo->setKeyValue("value", e->value.i, xsink);
                    break;
                case ALEK_Number:
                    nodeInfo->setKeyValue("literalKind", new QoreStringNode("number"), xsink);
                    nodeInfo->setKeyValue("value", new QoreNumberNode(e->value.str), xsink);
                    break;
                case ALEK_String:
                    nodeInfo->setKeyValue("literalKind", new QoreStringNode("string"), xsink);
                    nodeInfo->setKeyValue("value", new QoreStringNode(e->value.stdstr), xsink);
                    break;
                default:
                    break;
            }
            break;
        }
        case ASTExpressionKind::AEK_Name: {
            ASTNameExpression* e = static_cast<ASTNameExpression*>(expr);
            nodeInfo->setKeyValue("name", getName(tree, e->name, xsink), xsink);
            break;
        }
        case ASTExpressionKind::AEK_Range: {
            ASTRangeExpression* e = static_cast<ASTRangeExpression*>(expr);
            nodeInfo->setKeyValue("left", getExpression(tree, e->left.get(), xsink), xsink);
            nodeInfo->setKeyValue("right", getExpression(tree, e->right.get(), xsink), xsink);
            break;
        }
        case ASTExpressionKind::AEK_Regex: {
            ASTRegexExpression* e = static_cast<ASTRegexExpression*>(expr);
            nodeInfo->setKeyValue("str", new QoreStringNode(e->str), xsink);
            nodeInfo->setKeyValue("extractRegex", e->extractRegex, xsink);
            nodeInfo->setKeyValue("caseSensitive", e->caseSensitive, xsink);
            nodeInfo->setKeyValue("extended", e->extended, xsink);
            nodeInfo->setKeyValue("dotAll", e->dotAll, xsink);
            nodeInfo->setKeyValue("multiline", e->multiline, xsink);
            nodeInfo->setKeyValue("global", e->global, xsink);
            break;
        }
        case ASTExpressionKind::AEK_RegexSubst: {
            ASTRegexSubstExpression* e = static_cast<ASTRegexSubstExpression*>(expr);
            nodeInfo->setKeyValue("source", new QoreStringNode(e->source), xsink);
            nodeInfo->setKeyValue("target", new QoreStringNode(e->target), xsink);
            nodeInfo->setKeyValue("caseSensitive", e->caseSensitive, xsink);
            nodeInfo->setKeyValue("extended", e->extended, xsink);
            nodeInfo->setKeyValue("dotAll", e->dotAll, xsink);
            nodeInfo->setKeyValue("multiline", e->multiline, xsink);
            nodeInfo->setKeyValue("global", e->global, xsink);
            break;
        }
        case ASTExpressionKind::AEK_RegexTrans: {
            ASTRegexTransExpression* e = static_cast<ASTRegexTransExpression*>(expr);
            nodeInfo->setKeyValue("source", new QoreStringNode(e->source), xsink);
            nodeInfo->setKeyValue("target", new QoreStringNode(e->target), xsink);
            break;
        }
        case ASTExpressionKind::AEK_Returns: {
            ASTReturnsExpression* e = static_cast<ASTReturnsExpression*>(expr);
            nodeInfo->setKeyValue("typeName", getExpression(tree, e->typeName.get(), xsink), xsink);
            break;
        }
        case ASTExpressionKind::AEK_SwitchBody: {
            ASTSwitchBodyExpression* e = static_cast<ASTSwitchBodyExpression*>(expr);
            ReferenceHolder<QoreListNode> cases(new QoreListNode, xsink);
            if (*xsink)
                return nullptr;
            for (size_t i = 0, count = e->cases.size(); i < count; i++)
                cases->push(getExpression(tree, e->cases[i], xsink), xsink);
            nodeInfo->setKeyValue("cases", cases.release(), xsink);
            break;
        }
        case ASTExpressionKind::AEK_Ternary: {
            ASTTernaryExpression* e = static_cast<ASTTernaryExpression*>(expr);
            nodeInfo->setKeyValue("condition", getExpression(tree, e->condition.get(), xsink), xsink);
            nodeInfo->setKeyValue("exprTrue", getExpression(tree, e->exprTrue.get(), xsink), xsink);
            nodeInfo->setKeyValue("exprFalse", getExpression(tree, e->exprFalse.get(), xsink), xsink);
            break;
        }
        case ASTExpressionKind::AEK_Unary: {
            ASTUnaryExpression* e = static_cast<ASTUnaryExpression*>(expr);
            nodeInfo->setKeyValue("op", getOperator(e->op), xsink);
            nodeInfo->setKeyValue("expression", getExpression(tree, e->expression.get(), xsink), xsink);
            break;
        }
        default:
            break;
    }
    if (*xsink)
        return nullptr;
    return nodeInfo.release();
}

QoreHashNode* GetNodesInfoQuery::getLocation(const ASTParseLocation& loc, ExceptionSink* xsink) {
    ReferenceHolder<QoreHashNode> nodeInfo(new QoreHashNode, xsink);
    if (*xsink)
        return nullptr;

    // Convert to 0-indexed (LSP convention) - internal AST uses 1-indexed
    nodeInfo->setKeyValue("start_line", static_cast<int64>(loc.firstLine - 1), xsink);
    nodeInfo->setKeyValue("start_column", static_cast<int64>(loc.firstCol - 1), xsink);
    nodeInfo->setKeyValue("end_line", static_cast<int64>(loc.lastLine - 1), xsink);
    nodeInfo->setKeyValue("end_column", static_cast<int64>(loc.lastCol - 1), xsink);
    if (*xsink)
        return nullptr;
    return nodeInfo.release();
}

QoreStringNode* GetNodesInfoQuery::getModifiers(ASTModifiers& mods) {
    std::ostringstream oss;
    AstTreePrinter::printModifiers(oss, mods, 0, true, false);
    return new QoreStringNode(oss.str());
}

QoreHashNode* GetNodesInfoQuery::getName(ASTTree* tree, ASTName& name, ExceptionSink* xsink) {
    ReferenceHolder<QoreHashNode> nodeInfo(new QoreHashNode, xsink);
    if (*xsink)
        return nullptr;

    nodeInfo->setKeyValue("nodetype", static_cast<int64>(ANT_Name), xsink);
    nodeInfo->setKeyValue("loc", getLocation(name.loc, xsink), xsink);
    nodeInfo->setKeyValue("name", new QoreStringNode(name.name), xsink);
    if (*xsink)
        return nullptr;
    return nodeInfo.release();
}

QoreHashNode* GetNodesInfoQuery::getName(ASTTree* tree, ASTName* name, ExceptionSink* xsink) {
    if (!name)
        return nullptr;
    return getName(tree, *name, xsink);
}

QoreStringNode* GetNodesInfoQuery::getOperator(ASTOperator& op) {
    std::ostringstream oss;
    AstTreePrinter::printOperator(oss, op, 0, false);
    return new QoreStringNode(oss.str());
}

QoreHashNode* GetNodesInfoQuery::getParseOption(ASTTree* tree, ASTParseOption* po, ExceptionSink* xsink) {
    if (!po)
        return nullptr;

    ReferenceHolder<QoreHashNode> nodeInfo(new QoreHashNode, xsink);
    if (*xsink)
        return nullptr;

    nodeInfo->setKeyValue("nodetype", static_cast<int64>(ANT_ParseOption), xsink);
    nodeInfo->setKeyValue("loc", getLocation(po->loc, xsink), xsink);
    std::ostringstream oss;
    AstTreePrinter::printParseOptionString(oss, po);
    nodeInfo->setKeyValue("option", new QoreStringNode(oss.str()), xsink);
    if (*xsink)
        return nullptr;
    return nodeInfo.release();
}

QoreHashNode* GetNodesInfoQuery::getStatement(ASTTree* tree, ASTStatement* stmt, ExceptionSink* xsink) {
    if (!stmt)
        return nullptr;

    ReferenceHolder<QoreHashNode> nodeInfo(new QoreHashNode, xsink);
    if (*xsink)
        return nullptr;

    nodeInfo->setKeyValue("nodetype", static_cast<int64>(ANT_Statement), xsink);
    nodeInfo->setKeyValue("kind", static_cast<int64>(stmt->getKind()), xsink);
    nodeInfo->setKeyValue("loc", getLocation(stmt->loc, xsink), xsink);
    switch (stmt->getKind()) {
        case ASTStatementKind::ASK_Block: {
            ASTStatementBlock* s = static_cast<ASTStatementBlock*>(stmt);
            ReferenceHolder<QoreListNode> statements(new QoreListNode, xsink);
            for (size_t i = 0, count = s->statements.size(); i < count; i++)
                statements->push(getStatement(tree, s->statements[i], xsink), xsink);
            nodeInfo->setKeyValue("statements", statements.release(), xsink);
            break;
        }
        case ASTStatementKind::ASK_Break:
            break;
        case ASTStatementKind::ASK_Call: {
            ASTCallStatement* s = static_cast<ASTCallStatement*>(stmt);
            nodeInfo->setKeyValue("call", getExpression(tree, s->call.get(), xsink), xsink);
            break;
        }
        case ASTStatementKind::ASK_Context: {
            ASTContextStatement* s = static_cast<ASTContextStatement*>(stmt);
            nodeInfo->setKeyValue("name", getExpression(tree, s->name.get(), xsink), xsink);
            nodeInfo->setKeyValue("data", getExpression(tree, s->data.get(), xsink), xsink);
            ReferenceHolder<QoreListNode> contextMods(new QoreListNode, xsink);
            if (*xsink)
                return nullptr;
            for (size_t i = 0, count = s->contextMods.size(); i < count; i++)
                contextMods->push(getExpression(tree, s->contextMods[i], xsink), xsink);
            nodeInfo->setKeyValue("contextMods", contextMods.release(), xsink);
            nodeInfo->setKeyValue("statements", getStatement(tree, s->statements.get(), xsink), xsink);
            nodeInfo->setKeyValue("subcontext", s->subcontext, xsink);
            break;
        }
        case ASTStatementKind::ASK_Continue:
            break;
        case ASTStatementKind::ASK_DoWhile: {
            ASTDoWhileStatement* s = static_cast<ASTDoWhileStatement*>(stmt);
            nodeInfo->setKeyValue("condition", getExpression(tree, s->condition.get(), xsink), xsink);
            nodeInfo->setKeyValue("statement", getStatement(tree, s->statement.get(), xsink), xsink);
            break;
        }
        case ASTStatementKind::ASK_Expression: {
            ASTExpressionStatement* s = static_cast<ASTExpressionStatement*>(stmt);
            nodeInfo->setKeyValue("expression", getExpression(tree, s->expression.get(), xsink), xsink);
            break;
        }
        case ASTStatementKind::ASK_For: {
            ASTForStatement* s = static_cast<ASTForStatement*>(stmt);
            nodeInfo->setKeyValue("init", getExpression(tree, s->init.get(), xsink), xsink);
            nodeInfo->setKeyValue("condition", getExpression(tree, s->condition.get(), xsink), xsink);
            nodeInfo->setKeyValue("iteration", getExpression(tree, s->iteration.get(), xsink), xsink);
            nodeInfo->setKeyValue("statement", getStatement(tree, s->statement.get(), xsink), xsink);
            break;
        }
        case ASTStatementKind::ASK_Foreach: {
            ASTForeachStatement* s = static_cast<ASTForeachStatement*>(stmt);
            nodeInfo->setKeyValue("value", getExpression(tree, s->value.get(), xsink), xsink);
            nodeInfo->setKeyValue("source", getExpression(tree, s->source.get(), xsink), xsink);
            nodeInfo->setKeyValue("statement", getStatement(tree, s->statement.get(), xsink), xsink);
            break;
        }
        case ASTStatementKind::ASK_If: {
            ASTIfStatement* s = static_cast<ASTIfStatement*>(stmt);
            nodeInfo->setKeyValue("condition", getExpression(tree, s->condition.get(), xsink), xsink);
            nodeInfo->setKeyValue("stmtThen", getStatement(tree, s->stmtThen.get(), xsink), xsink);
            nodeInfo->setKeyValue("stmtElse", getStatement(tree, s->stmtElse.get(), xsink), xsink);
            break;
        }
        case ASTStatementKind::ASK_OnBlockExit: {
            ASTOnBlockExitStatement* s = static_cast<ASTOnBlockExitStatement*>(stmt);
            if (s->condition == ASTOnBlockExitStatement::Condition::AOBEC_Error)
                nodeInfo->setKeyValue("condition", new QoreStringNode("error"), xsink);
            else if (s->condition == ASTOnBlockExitStatement::Condition::AOBEC_Exit)
                nodeInfo->setKeyValue("condition", new QoreStringNode("exit"), xsink);
            else if (s->condition == ASTOnBlockExitStatement::Condition::AOBEC_Success)
                nodeInfo->setKeyValue("condition", new QoreStringNode("success"), xsink);
            nodeInfo->setKeyValue("statement", getStatement(tree, s->statement.get(), xsink), xsink);
            break;
        }
        case ASTStatementKind::ASK_Rethrow:
            break;
        case ASTStatementKind::ASK_Return: {
            ASTReturnStatement* s = static_cast<ASTReturnStatement*>(stmt);
            nodeInfo->setKeyValue("retval", getExpression(tree, s->retval.get(), xsink), xsink);
            break;
        }
        case ASTStatementKind::ASK_Summarize: {
            ASTSummarizeStatement* s = static_cast<ASTSummarizeStatement*>(stmt);
            nodeInfo->setKeyValue("name", getExpression(tree, s->name.get(), xsink), xsink);
            nodeInfo->setKeyValue("data", getExpression(tree, s->data.get(), xsink), xsink);
            nodeInfo->setKeyValue("by", getExpression(tree, s->by.get(), xsink), xsink);
            ReferenceHolder<QoreListNode> contextMods(new QoreListNode, xsink);
            if (*xsink)
                return nullptr;
            for (size_t i = 0, count = s->contextMods.size(); i < count; i++)
                contextMods->push(getExpression(tree, s->contextMods[i], xsink), xsink);
            nodeInfo->setKeyValue("contextMods", contextMods.release(), xsink);
            nodeInfo->setKeyValue("statements", getStatement(tree, s->statements.get(), xsink), xsink);
            break;
        }
        case ASTStatementKind::ASK_Switch: {
            ASTSwitchStatement* s = static_cast<ASTSwitchStatement*>(stmt);
            nodeInfo->setKeyValue("variable", getExpression(tree, s->variable.get(), xsink), xsink);
            nodeInfo->setKeyValue("body", getExpression(tree, s->body.get(), xsink), xsink);
            break;
        }
        case ASTStatementKind::ASK_ThreadExit:
            break;
        case ASTStatementKind::ASK_Throw: {
            ASTThrowStatement* s = static_cast<ASTThrowStatement*>(stmt);
            nodeInfo->setKeyValue("expression", getExpression(tree, s->expression.get(), xsink), xsink);
            break;
        }
        case ASTStatementKind::ASK_Try: {
            ASTTryStatement* s = static_cast<ASTTryStatement*>(stmt);
            nodeInfo->setKeyValue("tryStmt", getStatement(tree, s->tryStmt.get(), xsink), xsink);
            nodeInfo->setKeyValue("catchVar", getExpression(tree, s->catchVar.get(), xsink), xsink);
            nodeInfo->setKeyValue("catchStmt", getStatement(tree, s->catchStmt.get(), xsink), xsink);
            break;
        }
        case ASTStatementKind::ASK_While: {
            ASTWhileStatement* s = static_cast<ASTWhileStatement*>(stmt);
            nodeInfo->setKeyValue("condition", getExpression(tree, s->condition.get(), xsink), xsink);
            nodeInfo->setKeyValue("statement", getStatement(tree, s->statement.get(), xsink), xsink);
            break;
        }
        default:
            break;
    }
    if (*xsink)
        return nullptr;
    return nodeInfo.release();
}

QoreListNode* GetNodesInfoQuery::get(ASTTree* tree) {
    if (!tree)
        return nullptr;

    ExceptionSink xsink;
    ReferenceHolder<QoreListNode> lst(new QoreListNode, &xsink);
    if (xsink) {
        lst = nullptr;
        xsink.clear();
        return nullptr;
    }

    for (size_t i = 0, count = tree->nodes.size(); i < count; i++) {
        ReferenceHolder<QoreHashNode> nodeInfo(&xsink);
        ASTNode* node = tree->nodes[i];
        switch (node->getNodeType()) {
            case ANT_Declaration: {
                ASTDeclaration* decl = static_cast<ASTDeclaration*>(node);
                nodeInfo = getDeclaration(tree, decl, &xsink);
                break;
            }
            case ANT_Expression: {
                ASTExpression* expr = static_cast<ASTExpression*>(node);
                nodeInfo = getExpression(tree, expr, &xsink);
                break;
            }
            case ANT_Name: {
                ASTName* name = static_cast<ASTName*>(node);
                nodeInfo = getName(tree, name, &xsink);
                break;
            }
            case ANT_ParseOption: {
                ASTParseOption* po = static_cast<ASTParseOption*>(node);
                nodeInfo = getParseOption(tree, po, &xsink);
                break;
            }
            case ANT_Statement: {
                ASTStatement* stmt = static_cast<ASTStatement*>(node);
                nodeInfo = getStatement(tree, stmt, &xsink);
                break;
            }
            case ANT_None:
            default:
                break;
        }
        if (xsink) {
            lst = nullptr;
            xsink.clear();
            return nullptr;
        }
        if (nodeInfo) {
            lst->push(nodeInfo.release(), &xsink);
            if (xsink) {
                lst = nullptr;
                xsink.clear();
                return nullptr;
            }
        }
    }

    return lst.release();
}
