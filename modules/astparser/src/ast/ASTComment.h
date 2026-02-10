/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  ASTComment.h

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

  Note that the Qore library is released under a choice of three open-source
  licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
  information.
*/

#ifndef _QLS_AST_ASTCOMMENT_H
#define _QLS_AST_ASTCOMMENT_H

#include <string>

#include "ASTParseLocation.h"

//! Comment kind enumeration.
enum ASTCommentKind {
    ACK_Line,       //!< Line comment: # ...
    ACK_Block,      //!< Block comment: /* ... */
    ACK_DocLine,    //!< Doc line comment: #! ...
    ACK_DocBlock,   //!< Doc block comment: /** ... */
};

//! Represents a comment in the source code.
class ASTComment {
public:
    //! Comment kind.
    ASTCommentKind kind;

    //! Full text of the comment including delimiters (# prefix, /* */ delimiters, etc.).
    std::string text;

    //! Source location of the comment.
    ASTParseLocation loc;

    ASTComment() : kind(ACK_Line) {}

    ASTComment(ASTCommentKind k, const std::string& t, const ASTParseLocation& l)
        : kind(k), text(t), loc(l) {}

    ~ASTComment() {}
};

#endif // _QLS_AST_ASTCOMMENT_H
