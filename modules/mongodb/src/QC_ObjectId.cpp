/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_ObjectId.qpp

    Qore mongodb module - ObjectId class definition

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
#include "QC_ObjectId.h"
/* Qore class mongodb::ObjectId */

qore_classid_t CID_OBJECTID;
QoreClass* QC_OBJECTID;

// int ObjectId::compare(ObjectId other){}
static QoreValue ObjectId_compare_C8ObjectId(QoreObject* self, QoreObjectId* oid, const QoreListNode* args, q_rt_flags_t rtflags, ExceptionSink* xsink) {
    HARD_QORE_VALUE_OBJ_DATA(other, QoreObjectId, args, 0, CID_OBJECTID, "ObjectId::compare()", "ObjectId", xsink);
    if (*xsink)
        return 0;
# 127 "QC_ObjectId.qpp"
    ReferenceHolder<QoreObjectId> holder(other, xsink);
    return oid->compare(other);
}

// ObjectId::constructor() {}
static void ObjectId_constructor(QoreObject* self, const QoreListNode* args, q_rt_flags_t rtflags, ExceptionSink* xsink) {
# 62 "QC_ObjectId.qpp"
self->setPrivate(CID_OBJECTID, new QoreObjectId());
}

// ObjectId::constructor(string hex_string) {}
static void ObjectId_constructor_Vs(QoreObject* self, const QoreListNode* args, q_rt_flags_t rtflags, ExceptionSink* xsink) {
    const QoreStringNode* hex_string = HARD_QORE_VALUE_STRING(args, 0);
# 76 "QC_ObjectId.qpp"
SimpleRefHolder<QoreObjectId> holder(new QoreObjectId(hex_string->c_str(), xsink));
    if (*xsink) {
        return;
    }
    self->setPrivate(CID_OBJECTID, holder.release());
}

// ObjectId::copy() {}
static void ObjectId_copy(QoreObject* self, QoreObject* old, QoreObjectId* oid, ExceptionSink* xsink) {
# 90 "QC_ObjectId.qpp"
self->setPrivate(CID_OBJECTID, new QoreObjectId(*oid->getOid()));
}

// bool ObjectId::equals(ObjectId other){}
static QoreValue ObjectId_equals_C8ObjectId(QoreObject* self, QoreObjectId* oid, const QoreListNode* args, q_rt_flags_t rtflags, ExceptionSink* xsink) {
    HARD_QORE_VALUE_OBJ_DATA(other, QoreObjectId, args, 0, CID_OBJECTID, "ObjectId::equals()", "ObjectId", xsink);
    if (*xsink)
        return 0;
# 143 "QC_ObjectId.qpp"
    ReferenceHolder<QoreObjectId> holder(other, xsink);
    return oid->equals(other);
}

// date ObjectId::getTimestamp(){}
static QoreValue ObjectId_getTimestamp(QoreObject* self, QoreObjectId* oid, const QoreListNode* args, q_rt_flags_t rtflags, ExceptionSink* xsink) {
# 114 "QC_ObjectId.qpp"
    return DateTimeNode::makeAbsolute(currentTZ(), oid->getTimestamp(), 0);
}

// int ObjectId::hashCode(){}
static QoreValue ObjectId_hashCode(QoreObject* self, QoreObjectId* oid, const QoreListNode* args, q_rt_flags_t rtflags, ExceptionSink* xsink) {
# 156 "QC_ObjectId.qpp"
    return oid->hashCode();
}

// string ObjectId::toString(){}
static QoreValue ObjectId_toString(QoreObject* self, QoreObjectId* oid, const QoreListNode* args, q_rt_flags_t rtflags, ExceptionSink* xsink) {
# 102 "QC_ObjectId.qpp"
    return oid->toString();
}

DLLLOCAL void preinitObjectIdClass() {
    QC_OBJECTID = new QoreBuiltinClass("ObjectId", "::mongodb::ObjectId", QDOM_DEFAULT);
    CID_OBJECTID = QC_OBJECTID->getID();
    QC_OBJECTID->setSystem();
}

DLLLOCAL QoreClass* initObjectIdClass(QoreNamespace& ns) {
    if (!QC_OBJECTID)
        preinitObjectIdClass();

    QC_OBJECTID->setFinal();

    // int ObjectId::compare(ObjectId other){}
    QC_OBJECTID->addMethod("compare", (q_method_n_t)ObjectId_compare_C8ObjectId, Public, QCF_CONSTANT, QDOM_DEFAULT, bigIntTypeInfo, 1, QC_OBJECTID->getTypeInfo(), QORE_PARAM_NO_ARG, "other");

    // ObjectId::constructor() {}
    QC_OBJECTID->addConstructor(ObjectId_constructor, Public, QCF_NO_FLAGS, QDOM_DEFAULT);

    // ObjectId::constructor(string hex_string) {}
    QC_OBJECTID->addConstructor(ObjectId_constructor_Vs, Public, QCF_NO_FLAGS, QDOM_DEFAULT, 1, stringTypeInfo, QORE_PARAM_NO_ARG, "hex_string");

    // ObjectId::copy() {}
    QC_OBJECTID->setCopy((q_copy_t)ObjectId_copy);

    // bool ObjectId::equals(ObjectId other){}
    QC_OBJECTID->addMethod("equals", (q_method_n_t)ObjectId_equals_C8ObjectId, Public, QCF_CONSTANT, QDOM_DEFAULT, boolTypeInfo, 1, QC_OBJECTID->getTypeInfo(), QORE_PARAM_NO_ARG, "other");

    // date ObjectId::getTimestamp(){}
    QC_OBJECTID->addMethod("getTimestamp", (q_method_n_t)ObjectId_getTimestamp, Public, QCF_CONSTANT, QDOM_DEFAULT, dateTypeInfo);

    // int ObjectId::hashCode(){}
    QC_OBJECTID->addMethod("hashCode", (q_method_n_t)ObjectId_hashCode, Public, QCF_CONSTANT, QDOM_DEFAULT, bigIntTypeInfo);

    // string ObjectId::toString(){}
    QC_OBJECTID->addMethod("toString", (q_method_n_t)ObjectId_toString, Public, QCF_CONSTANT, QDOM_DEFAULT, stringTypeInfo);

    return QC_OBJECTID;
}
