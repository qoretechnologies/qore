/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ASTLiteralExpression.h

    Qore AST Parser

    Copyright (C) 2017 - 2024 Qore Technologies, s.r.o.

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

#ifndef _QLS_AST_EXPRESSIONS_ASTLITERALEXPRESSION_H
#define _QLS_AST_EXPRESSIONS_ASTLITERALEXPRESSION_H

#include <cstdlib>
#include <string>

#include "ast/ASTExpression.h"

#include <qore/common.h>

enum ALEKind {
    ALEK_Binary,
    ALEK_Date,
    ALEK_Float,
    ALEK_Int,
    ALEK_Number,
    ALEK_String,
    ALEK_Nothing,
    ALEK_Null,
    ALEK_Bool,
    ALEK_Ellipses,
};

class ASTLiteralExpression : public ASTExpression {
public:
    union ALEData {
        bool b;
        int64_t i;
        double d;
        char* str;
        std::string* stdstr;
    };

public:
    //! Kind of value.
    ALEKind kind;

    //! Literal value.
    ALEData value;

public:
    DLLLOCAL ASTLiteralExpression(ALEKind k, int64_t val) :
        ASTExpression(),
        kind(k)
    {
        value.i = val;
    }

    DLLLOCAL ASTLiteralExpression(ALEKind k, double val) :
        ASTExpression(),
        kind(k)
    {
        value.d = val;
    }

    DLLLOCAL ASTLiteralExpression(ALEKind k, char* val) :
        ASTExpression(),
        kind(k)
    {
        value.str = val;
    }

    DLLLOCAL ASTLiteralExpression(ALEKind k, std::string* val) :
        ASTExpression(),
        kind(k)
    {
        value.stdstr = val;
    }

    DLLLOCAL ASTLiteralExpression(bool b) :
        ASTExpression(),
        kind(ALEK_Bool)
    {
        value.b = b;
    }

    DLLLOCAL ASTLiteralExpression(ALEKind k) : kind(k) {
    }

    DLLLOCAL virtual ~ASTLiteralExpression() {
        if (kind == ALEK_Binary || kind == ALEK_Date || kind == ALEK_Number)
            free(value.str);
        if (kind == ALEK_String)
            delete value.stdstr;
    }

    DLLLOCAL virtual ASTExpressionKind getKind() const override {
        return ASTExpressionKind::AEK_Literal;
    }
};

class ASTNothing : public ASTLiteralExpression {
public:
    DLLLOCAL ASTNothing() : ASTLiteralExpression(ALEK_Nothing) {
    }
};

class ASTNull : public ASTLiteralExpression {
public:
    DLLLOCAL ASTNull() : ASTLiteralExpression(ALEK_Null) {
    }
};

#endif // _QLS_AST_EXPRESSIONS_ASTLITERALEXPRESSION_H
