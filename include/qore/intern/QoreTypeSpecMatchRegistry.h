/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreTypeSpecMatchRegistry.h

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

#ifndef _QORE_QORETYESPEMATCHREGISTRY_H
#define _QORE_QORETYESPEMATCHREGISTRY_H

#include "qore/intern/QoreTypeInfo.h"

// Context passed to each handler function
struct QoreTypeSpecMatchCtx {
    const QoreTypeSpec& t;
    bool& may_not_match;
    bool& may_need_filter;
    qore_type_result_e& max_result;
    bool known_initial_assignment;
};

// Handler function type
typedef qore_type_result_e (*QoreTypeSpecMatchFn)(const QoreTypeSpec&, QoreTypeSpecMatchCtx&);

// Registry entry structure
struct QoreTypeSpecMatchHandlerInfo {
    const char* name;
    q_typespec_t typespec;
    QoreTypeSpecMatchFn handler;
    const char* description;
};

// Registry table and lookup function
extern const QoreTypeSpecMatchHandlerInfo QORE_TYPE_SPEC_MATCH_REGISTRY[];

const QoreTypeSpecMatchHandlerInfo* getQoreTypeSpecMatchHandler(q_typespec_t typespec);

#endif
