/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  AstPrinter.cpp

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

#include "AstPrinter.h"

#include <sstream>
#include <string>

#include "AstTreePrinter.h"
#include "ast/AST.h"

static std::string escapeString(const std::string& str) {
    std::ostringstream newstr;
    for (size_t i = 0, count = str.size(); i < count; i++) {
        char c = str[i];
        if (c == '\b')
            newstr << "\\b";
        else if (c == '\f')
            newstr << "\\f";
        else if (c == '\n')
            newstr << "\\n";
        else if (c == '\r')
            newstr << "\\r";
        else if (c == '\t')
            newstr << "\\t";
        else
            newstr << c;
    }
    return newstr.str();
}

void AstPrinter::printAssignmentExpression(std::ostream& os, ASTAssignmentExpression* ae) {
    if (!ae)
        return;
    if (!ae->left || !ae->right)
        return;
    if (ae->left->getKind() == ASTExpressionKind::AEK_Decl) {
        printDeclExpression(os, static_cast<ASTDeclExpression*>(ae->left.get()));
    }
    if (ae->weak)
        os << " := ";
    else
        os << " = ";
    if (ae->right->getKind() == ASTExpressionKind::AEK_Literal) {
        ASTLiteralExpression* le = static_cast<ASTLiteralExpression*>(ae->right.get());
        printLiteralExpression(os, le);
    }
    else if (ae->right->getKind() == ASTExpressionKind::AEK_Name) {
        ASTNameExpression* ne = static_cast<ASTNameExpression*>(ae->right.get());
        os << ne->name.name;
    }
    // TODO
}

void AstPrinter::printDeclExpression(std::ostream& os, ASTDeclExpression* de) {
    if (!de)
        return;
    ASTDeclaration* decl = de->declaration.get();
    if (!decl)
        return;
    if (decl->getKind() == ASTDeclarationKind::ADK_Variable)
        printVariableSignature(os, static_cast<ASTVariableDeclaration*>(decl));
    else if (decl->getKind() == ASTDeclarationKind::ADK_Constant)
        printConstantSignature(os, static_cast<ASTConstantDeclaration*>(decl));
    else if (decl->getKind() == ASTDeclarationKind::ADK_Typedef)
        printTypedefSignature(os, static_cast<ASTTypedefDeclaration*>(decl));
}

void AstPrinter::printListExpression(std::ostream& os, ASTListExpression* le) {
    if (!le)
        return;
    for (size_t i = 0, count = le->elements.size(); i < count; i++) {
        if (i > 0)
            os << ", ";
        ASTExpression* expr = le->elements[i];
        if (!expr)
            continue;
        if (expr->getKind() == ASTExpressionKind::AEK_Decl)
            printDeclExpression(os, static_cast<ASTDeclExpression*>(expr));
        else if (expr->getKind() == ASTExpressionKind::AEK_List)
            printListExpression(os, static_cast<ASTListExpression*>(expr));
        else if (expr->getKind() == ASTExpressionKind::AEK_Assignment)
            printAssignmentExpression(os, static_cast<ASTAssignmentExpression*>(expr));
    }
}

void AstPrinter::printLiteralExpression(std::ostream& os, ASTLiteralExpression* le) {
    if (!le)
        return;
    switch (le->kind) {
        case ALEK_Binary:
            os << "<" << le->value.str << ">";
            break;
        case ALEK_Date:
            os << le->value.str;
            break;
        case ALEK_Float:
            os << le->value.d;
            break;
        case ALEK_Int:
            os << le->value.i;
            break;
        case ALEK_Number:
            os << le->value.str;
            break;
        case ALEK_String: {
            std::string str(escapeString(*le->value.stdstr));
            os << "\"" << str << "\"";
            break;
        }
        default:
            break;
    }
}

void AstPrinter::printClassSignature(std::ostream& os, ASTClassDeclaration* d) {
    if (!d)
        return;
    if (!d->modifiers.empty()) {
        AstTreePrinter::printModifiers(os, d->modifiers, 0, true);
        os << " ";
    }
    os << "class " << d->name.name;
    size_t i = 0, count = d->inherits.size();
    if (count)
        os << " inherits ";
    for (; i < count; i++) {
        ASTSuperclassDeclaration* sd = d->inherits[i];
        if (sd) {
            if (i != 0)
                os << ", ";
            if (!sd->modifiers.empty()) {
                AstTreePrinter::printModifiers(os, sd->modifiers, 0, true);
                os << " ";
            }
            os << sd->name.name;
        }
    }
}

void AstPrinter::printConstantSignature(std::ostream& os, ASTConstantDeclaration* d) {
    if (!d)
        return;
    if (!d->modifiers.empty()) {
        AstTreePrinter::printModifiers(os, d->modifiers, 0, true);
        os << " ";
    }
    os << "const " << d->name.name;
    if (d->value && d->value->getKind() == ASTExpressionKind::AEK_Literal) {
        os << " = ";
        ASTLiteralExpression* le = static_cast<ASTLiteralExpression*>(d->value.get());
        printLiteralExpression(os, le);
    }
}

void AstPrinter::printFunctionSignature(std::ostream& os, ASTFunctionDeclaration* d) {
    if (!d)
        return;
    if (!d->modifiers.empty()) {
        AstTreePrinter::printModifiers(os, d->modifiers, 0, true);
        os << " ";
    }
    if (d->returnType) {
        ASTNameExpression* ret;
        if (d->returnType->getKind() == ASTExpressionKind::AEK_Returns)
            ret = static_cast<ASTReturnsExpression*>(d->returnType.get())->typeName.get();
        else
            ret = static_cast<ASTNameExpression*>(d->returnType.get());
        if (ret)
            os << ret->name.name << " ";
    }
    os << d->name.name << "(";
    if (d->params) {
        if (d->params->getKind() == ASTExpressionKind::AEK_Decl)
            printDeclExpression(os, static_cast<ASTDeclExpression*>(d->params.get()));
        else if (d->params->getKind() == ASTExpressionKind::AEK_List)
            printListExpression(os, static_cast<ASTListExpression*>(d->params.get()));
    }
    os << ")";
}

void AstPrinter::printHashDeclSignature(std::ostream& os, ASTHashDeclaration* d) {
    if (!d)
        return;
    if (!d->modifiers.empty()) {
        AstTreePrinter::printModifiers(os, d->modifiers, 0, true);
        os << " ";
    }
    os << "hashdecl " << d->name.name;
}

void AstPrinter::printHashMemberSignature(std::ostream& os, ASTHashMemberDeclaration* d) {
    if (!d)
        return;
    if (!d->typeName.name.empty())
        os << d->typeName.name << " ";
    os << d->name.name;
}

void AstPrinter::printTypedefSignature(std::ostream& os, ASTTypedefDeclaration* d) {
    if (!d)
        return;
    if (!d->modifiers.empty()) {
        AstTreePrinter::printModifiers(os, d->modifiers, 0, true);
        os << " ";
    }
    os << "typedef " << d->name.name << " = " << d->typeName.name;
}

void AstPrinter::printVariableSignature(std::ostream& os, ASTVariableDeclaration* d) {
    if (!d)
        return;
    if (!d->modifiers.empty()) {
        AstTreePrinter::printModifiers(os, d->modifiers, 0, true);
        os << " ";
    }
    if (!d->typeName.name.empty())
        os << d->typeName.name;
    os << " " << d->name.name;
}
