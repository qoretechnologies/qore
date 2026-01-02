/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  ASTDeclarationKind.h

  Qore AST Parser

  Copyright (C) 2023 - 2024 Qore Technologies, s.r.o.

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

  Note that the Qore library is released under a choice of three open-source
  licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
  information.
*/

#ifndef _QLS_AST_ASTDECLARATIONKIND_H
#define _QLS_AST_ASTDECLARATIONKIND_H

//! Describes the kind of declaration.
enum ASTDeclarationKind {
    ADK__reserved = 0,      // don't use
    ADK_Class = 1,          //!< Identifies instances of \ref ASTClassDeclaration.
    ADK_Closure = 2,        //!< Identifies instances of \ref ASTClosureDeclaration.
    ADK_Constant = 3,       //!< Identifies instances of \ref ASTConstantDeclaration.
    ADK_Function = 4,       //!< Identifies instances of \ref ASTFunctionDeclaration.
    ADK_Hash = 5,           //!< Identifies instances of \ref ASTHashDeclaration.
    ADK_HashMember = 6,     //!< Identifies instances of \ref ASTHashMemberDeclaration.
    ADK_MemberGroup = 7,    //!< Identifies instances of \ref ASTMemberGroupDeclaration.
    ADK_Namespace = 8,      //!< Identifies instances of \ref ASTNamespaceDeclaration.
    ADK_Superclass = 9,     //!< Identifies instances of \ref ASTSuperclassDeclaration.
    ADK_Variable = 10,      //!< Identifies instances of \ref ASTVariableDeclaration.
    ADK_VarList = 11,       //!< Identifies instances of \ref ASTVarListDeclaration.
    ADK_Typedef = 12,       //!< Identifies instances of \ref ASTTypedefDeclaration.
};

#endif // _QLS_AST_ASTDECLARATIONKIND_H
