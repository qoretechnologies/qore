/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    qore_reflection.h

    Qore Programming Language

    Copyright (C) 2003 - 2024 Qore Technologies, s.r.o.

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

#ifndef _QORE_MODULE_REFLECTION_QORE_REFLECTION_H

#define _QORE_MODULE_REFLECTION_QORE_REFLECTION_H

#include "qore/Qore.h"

//! ClassAccessInfo hashdecl
DLLLOCAL extern const TypedHashDecl* hashdeclClassAccessInfo;

//! MethodAccessInfo hashdecl
DLLLOCAL extern const TypedHashDecl* hashdeclMethodAccessInfo;

DLLLOCAL inline QoreProgram* qore_reflection_get_runtime_program(RuntimeConfig& runtime_cfg) {
    QoreProgram* pgm = runtime_cfg.getProgram();
    return pgm ? pgm : getProgram();
}

DLLLOCAL inline const QoreTypeInfo* qore_reflection_get_type_from_string(const char* typestr, QoreProgram* pgm,
        ExceptionSink* xsink) {
    return qore_get_type_from_string(typestr, pgm, *xsink);
}

DLLLOCAL inline const QoreTypeInfo* qore_reflection_get_type_from_string(const char* typestr,
        RuntimeConfig& runtime_cfg, ExceptionSink* xsink, QoreProgram** pgm = nullptr) {
    QoreProgram* runtime_pgm = qore_reflection_get_runtime_program(runtime_cfg);
    if (pgm) {
        *pgm = runtime_pgm;
    }
    return qore_reflection_get_type_from_string(typestr, runtime_pgm, xsink);
}

#endif
