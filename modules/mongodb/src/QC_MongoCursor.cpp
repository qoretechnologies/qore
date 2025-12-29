/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_MongoCursor.qpp

    Qore mongodb module - MongoCursor class definition

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
#include "QC_MongoCursor.h"
/* Qore class mongodb::MongoCursor */

qore_classid_t CID_MONGOCURSOR;
QoreClass* QC_MONGOCURSOR;

// nothing MongoCursor::close(){}
static QoreValue MongoCursor_close(QoreObject* self, QoreMongoCursor* cursor, const QoreListNode* args, q_rt_flags_t rtflags, ExceptionSink* xsink) {
# 117 "QC_MongoCursor.qpp"
    cursor->close();
    return QoreValue();
}

// bool MongoCursor::more(){}
static QoreValue MongoCursor_more(QoreObject* self, QoreMongoCursor* cursor, const QoreListNode* args, q_rt_flags_t rtflags, ExceptionSink* xsink) {
# 83 "QC_MongoCursor.qpp"
    return cursor->more(xsink);
}

// *hash<auto> MongoCursor::next(){}
static QoreValue MongoCursor_next(QoreObject* self, QoreMongoCursor* cursor, const QoreListNode* args, q_rt_flags_t rtflags, ExceptionSink* xsink) {
# 65 "QC_MongoCursor.qpp"
    return cursor->next(xsink);
}

// list<hash<auto>> MongoCursor::toList(){}
static QoreValue MongoCursor_toList(QoreObject* self, QoreMongoCursor* cursor, const QoreListNode* args, q_rt_flags_t rtflags, ExceptionSink* xsink) {
# 103 "QC_MongoCursor.qpp"
    return cursor->toList(xsink);
}

DLLLOCAL void preinitMongoCursorClass() {
    QC_MONGOCURSOR = new QoreBuiltinClass("MongoCursor", "::mongodb::MongoCursor", QDOM_DEFAULT);
    CID_MONGOCURSOR = QC_MONGOCURSOR->getID();
    QC_MONGOCURSOR->setSystem();
}

DLLLOCAL QoreClass* initMongoCursorClass(QoreNamespace& ns) {
    if (!QC_MONGOCURSOR)
        preinitMongoCursorClass();

    QC_MONGOCURSOR->setFinal();

    // nothing MongoCursor::close(){}
    QC_MONGOCURSOR->addMethod("close", (q_method_n_t)MongoCursor_close, Public, QCF_NO_FLAGS, QDOM_DEFAULT, nothingTypeInfo);

    // bool MongoCursor::more(){}
    QC_MONGOCURSOR->addMethod("more", (q_method_n_t)MongoCursor_more, Public, QCF_NO_FLAGS, QDOM_DEFAULT, boolTypeInfo);

    // *hash<auto> MongoCursor::next(){}
    QC_MONGOCURSOR->addMethod("next", (q_method_n_t)MongoCursor_next, Public, QCF_NO_FLAGS, QDOM_DEFAULT, autoHashOrNothingTypeInfo);

    // list<hash<auto>> MongoCursor::toList(){}
    QC_MONGOCURSOR->addMethod("toList", (q_method_n_t)MongoCursor_toList, Public, QCF_NO_FLAGS, QDOM_DEFAULT, qore_get_complex_list_type(autoHashTypeInfo));

    return QC_MONGOCURSOR;
}
