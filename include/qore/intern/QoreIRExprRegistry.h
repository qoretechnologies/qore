/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreIRExprRegistry.h

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

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

    Note that the Qore library is dual-licensed under LGPL and MIT licenses; see
    LICENSE.LGPL and LICENSE.MIT in the source code directory for details.
*/

#ifndef _QORE_QOREIREXPRREGISTRY_H
#define _QORE_QOREIREXPRREGISTRY_H

#include "qore/intern/QoreIR.h"

class QoreIRLowering;

// Context passed to each handler function
struct QoreIRExprCtx {
    QoreIRLowering& lowering;
    const QoreValue& expr;
    std::string& error;
};

// Handler function type
typedef QoreIRValue (*QoreIRExprHandlerFn)(QoreIRExprCtx&);

// Registry entry structure
struct QoreIRExprHandlerInfo {
    const char* name;
    QoreIRExprHandlerFn handler;
    const char* description;
};

// Registry table and access function
extern const QoreIRExprHandlerInfo QORE_IR_EXPR_REGISTRY[];
extern const size_t QORE_IR_EXPR_REGISTRY_SIZE;

#endif
