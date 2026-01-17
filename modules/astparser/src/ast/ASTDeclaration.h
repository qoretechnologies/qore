/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  ASTDeclaration.h

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

#ifndef _QLS_AST_ASTDECLARATION_H
#define _QLS_AST_ASTDECLARATION_H

#include <memory>
#include <string>

#include "ASTNode.h"
#include "ASTDeclarationKind.h"

class ASTDeclaration : public ASTNode {
public:
    //! Pointer type.
    using Ptr = std::unique_ptr<ASTDeclaration>;

    //! Documentation comment attached to this declaration (if any).
    //! This holds the text of Qore doc comments (#! line comments or
    //! Doxygen-style / ** ... * / block comments) that immediately precede the declaration.
    std::string* docComment = nullptr;

public:
    ASTDeclaration() : ASTNode() {}
    ASTDeclaration(const ASTParseLocation& l) : ASTNode(l) {}

    virtual ~ASTDeclaration() {
        delete docComment;
    }

    //! Sets the documentation comment for this declaration.
    void setDocComment(std::string* comment) {
        delete docComment;
        docComment = comment;
    }

    //! Returns the documentation comment (may be nullptr).
    std::string* getDocComment() const {
        return docComment;
    }

    //! Returns true if this declaration has a doc comment.
    bool hasDocComment() const {
        return docComment != nullptr;
    }

    virtual ASTDeclarationKind getKind() const = 0;

    virtual ASTNodeType getNodeType() {
        return ANT_Declaration;
    }
};

#endif // _QLS_AST_ASTDECLARATION_H
