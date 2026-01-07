/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  AstPrinter.h

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

#ifndef _QLS_ASTPRINTER_H
#define _QLS_ASTPRINTER_H

#include <ostream>

class ASTClassDeclaration;
class ASTConstantDeclaration;
class ASTFunctionDeclaration;
class ASTHashDeclaration;
class ASTHashMemberDeclaration;
class ASTTypedefDeclaration;
class ASTVariableDeclaration;

class ASTAssignmentExpression;
class ASTDeclExpression;
class ASTListExpression;
class ASTLiteralExpression;

class AstPrinter {
public:
    static void printAssignmentExpression(std::ostream& os, ASTAssignmentExpression* ae);
    static void printDeclExpression(std::ostream& os, ASTDeclExpression* de);
    static void printListExpression(std::ostream& os, ASTListExpression* le);
    static void printLiteralExpression(std::ostream& os, ASTLiteralExpression* le);

    static void printClassSignature(std::ostream& os, ASTClassDeclaration* d);
    static void printConstantSignature(std::ostream& os, ASTConstantDeclaration* d);
    static void printFunctionSignature(std::ostream& os, ASTFunctionDeclaration* d);
    static void printHashDeclSignature(std::ostream& os, ASTHashDeclaration* d);
    static void printHashMemberSignature(std::ostream& os, ASTHashMemberDeclaration* d);
    static void printTypedefSignature(std::ostream& os, ASTTypedefDeclaration* d);
    static void printVariableSignature(std::ostream& os, ASTVariableDeclaration* d);
};

#endif // _QLS_ASTPRINTER_H
