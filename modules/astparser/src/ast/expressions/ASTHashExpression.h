/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  ASTHashExpression.h

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

  Note that the Qore library is released under a choice of three open-source
  licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
  information.
*/

#ifndef _QLS_AST_EXPRESSIONS_ASTHASHEXPRESSION_H
#define _QLS_AST_EXPRESSIONS_ASTHASHEXPRESSION_H

#include <vector>

#include "ast/ASTExpression.h"
#include "ast/expressions/ASTHashElementExpression.h"

class ASTHashExpression : public ASTExpression {
public:
    //! Pointer type.
    using Ptr = std::unique_ptr<ASTHashExpression>;

    //! Kind of hash delimiter used.
    enum class HashKind {
        AHE_CurlyBraces = 0,   //!< Hash with curly braces: {"key": val}
        AHE_Parentheses = 1,   //!< Hash with parentheses: ("key": val)
    };

public:
    //! Hash elements.
    std::vector<ASTHashElementExpression*> elements;

    //! Delimiter kind (curly braces vs parentheses).
    HashKind hashKind = HashKind::AHE_CurlyBraces;

public:
    ASTHashExpression(std::vector<ASTHashElementExpression*>* elems = nullptr) : ASTExpression() {
        if (elems)
            elements.swap(*elems);
    }

    virtual ~ASTHashExpression() {
        for (size_t i = 0, count = elements.size(); i < count; i++)
            delete elements[i];
        elements.clear();
    }

    virtual ASTExpressionKind getKind() const override {
        return ASTExpressionKind::AEK_Hash;
    }
};

#endif // _QLS_AST_EXPRESSIONS_ASTHASHEXPRESSION_H
