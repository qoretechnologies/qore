/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    ql_mongodb.qpp

    Qore mongodb module - namespace-level functions

    Copyright (C) 2025 Qore Technologies, s.r.o.

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
*/

#include "qore/Qore.h"

#include <mongoc/mongoc.h>
# 33 "ql_mongodb.qpp"
# 38 "ql_mongodb.qpp"
// string mongodb_get_driver_version() {}
static QoreValue f_mongodb_get_driver_version(const QoreListNode* args, q_rt_flags_t rtflags, ExceptionSink* xsink) {
# 47 "ql_mongodb.qpp"
    return new QoreStringNode(mongoc_get_version());
}


DLLLOCAL void init_mongodb_functions(QoreNamespace& ns) {
    // string mongodb_get_driver_version() {}
    ns.addBuiltinVariant("mongodb_get_driver_version", (q_func_n_t)f_mongodb_get_driver_version, QCF_CONSTANT, QDOM_DEFAULT, stringTypeInfo);

}
