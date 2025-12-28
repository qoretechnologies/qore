/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  ASTStatementKind.h

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

#ifndef _QLS_AST_ASTSTATEMENTKIND_H
#define _QLS_AST_ASTSTATEMENTKIND_H

//! Describes the kind of statement.
enum ASTStatementKind {
    ASK__reserved = 0,          // don't use
    ASK_Block = 1,              //!< Identifies an instance of \ref ASTStatementBlock.
    ASK_Break = 2,              //!< Identifies an instance of \ref ASTBreakStatement.
    ASK_Call = 3,               //!< Identifies an instance of \ref ASTCallStatement.
    ASK_Context = 4,            //!< Identifies an instance of \ref ASTContextStatement.
    ASK_Continue = 5,           //!< Identifies an instance of \ref ASTContinueStatement.
    ASK_DoWhile = 6,            //!< Identifies an instance of \ref ASTDoWhileStatement.
    ASK_Expression = 7,         //!< Identifies an instance of \ref ASTExpressionStatement.
    ASK_For = 8,                //!< Identifies an instance of \ref ASTForStatement.
    ASK_Foreach = 9,            //!< Identifies an instance of \ref ASTForeachStatement.
    ASK_If = 10,                //!< Identifies an instance of \ref ASTIfStatement.
    ASK_OnBlockExit = 11,       //!< Identifies an instance of \ref ASTOnBlockExitStatement.
    ASK_Rethrow = 12,           //!< Identifies an instance of \ref ASTRethrowStatement.
    ASK_Return = 13,            //!< Identifies an instance of \ref ASTReturnStatement.
    ASK_Summarize = 14,         //!< Identifies an instance of \ref ASTSummarizeStatement.
    ASK_Switch = 15,            //!< Identifies an instance of \ref ASTSwitchStatement.
    ASK_ThreadExit = 16,        //!< Identifies an instance of \ref ASTThreadExitStatement.
    ASK_Throw = 17,             //!< Identifies an instance of \ref ASTThrowStatement.
    ASK_Try = 18,               //!< Identifies an instance of \ref ASTTryStatement.
    ASK_While = 19,             //!< Identifies an instance of \ref ASTWhileStatement.
    ASK_Assert = 20,            //!< Identifies an instance of \ref ASTAssertStatement.
    ASK_Debug = 21,             //!< Identifies an instance of \ref ASTDebugStatement.
};

#endif // _QLS_AST_ASTSTATEMENTKIND_H
