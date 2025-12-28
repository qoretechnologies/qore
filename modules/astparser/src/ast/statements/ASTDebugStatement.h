/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  ASTDebugStatement.h

  Qore AST Parser

  Copyright (C) 2023 - 2025 Qore Technologies, s.r.o.

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

#ifndef _QLS_AST_STATEMENTS_ASTDEBUGSTATEMENT_H
#define _QLS_AST_STATEMENTS_ASTDEBUGSTATEMENT_H

#include <memory>

#include "ast/ASTStatement.h"
#include "ast/ASTExpression.h"

class ASTStatementBlock;

class ASTDebugStatement : public ASTStatement {
public:
    //! Pointer type.
    using Ptr = std::unique_ptr<ASTDebugStatement>;

public:
    //! Debug expression (for expression form).
    ASTExpression::Ptr expression;

    //! Debug statement block (for block form).
    std::unique_ptr<ASTStatementBlock> stmtBlock;

public:
    //! Constructor for expression form: @debug(expr);
    ASTDebugStatement(ASTExpression* expr) :
        ASTStatement(),
        expression(expr),
        stmtBlock(nullptr) {}

    //! Constructor for block form: @debug { ... }
    ASTDebugStatement(ASTStatementBlock* block) :
        ASTStatement(),
        expression(nullptr),
        stmtBlock(block) {}

    virtual ~ASTDebugStatement() {}

    virtual ASTStatementKind getKind() const override {
        return ASTStatementKind::ASK_Debug;
    }
};

#endif // _QLS_AST_STATEMENTS_ASTDEBUGSTATEMENT_H
