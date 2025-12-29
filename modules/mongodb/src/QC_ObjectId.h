/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_ObjectId.h

    Qore mongodb module - ObjectId class

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

#ifndef _QORE_MODULE_MONGODB_QC_OBJECTID_H
#define _QORE_MODULE_MONGODB_QC_OBJECTID_H

#include <qore/Qore.h>

#include <mongoc/mongoc.h>
#include <bson/bson.h>

DLLEXPORT extern qore_classid_t CID_OBJECTID;
DLLLOCAL extern QoreClass* QC_OBJECTID;

DLLLOCAL void preinitObjectIdClass();
DLLLOCAL QoreClass* initObjectIdClass(QoreNamespace& ns);

//! MongoDB ObjectId wrapper class
class QoreObjectId : public AbstractPrivateData {
public:
    //! Create a new ObjectId
    DLLLOCAL QoreObjectId() {
        bson_oid_init(&oid, nullptr);
    }

    //! Create from an existing bson_oid_t
    DLLLOCAL QoreObjectId(const bson_oid_t& id) : oid(id) {
    }

    //! Create from a hex string
    DLLLOCAL QoreObjectId(const char* hex_string, ExceptionSink* xsink) {
        if (!bson_oid_is_valid(hex_string, strlen(hex_string))) {
            xsink->raiseException("MONGODB-OBJECTID-ERROR", "invalid ObjectId hex string: '%s'", hex_string);
            return;
        }
        bson_oid_init_from_string(&oid, hex_string);
    }

    //! Get the ObjectId as a hex string
    DLLLOCAL QoreStringNode* toString() const {
        char str[25];
        bson_oid_to_string(&oid, str);
        return new QoreStringNode(str);
    }

    //! Get the timestamp from the ObjectId
    DLLLOCAL int64_t getTimestamp() const {
        return bson_oid_get_time_t(&oid);
    }

    //! Get a pointer to the raw bson_oid_t
    DLLLOCAL const bson_oid_t* getOid() const {
        return &oid;
    }

    //! Compare two ObjectIds
    DLLLOCAL int compare(const QoreObjectId* other) const {
        return bson_oid_compare(&oid, other->getOid());
    }

    //! Check equality
    DLLLOCAL bool equals(const QoreObjectId* other) const {
        return bson_oid_equal(&oid, other->getOid());
    }

    //! Get the hash code for this ObjectId
    DLLLOCAL int64_t hashCode() const {
        return bson_oid_hash(&oid);
    }

    //! Reference self
    DLLLOCAL QoreObjectId* refSelf() const {
        ref();
        return const_cast<QoreObjectId*>(this);
    }

private:
    bson_oid_t oid;
};

#endif // _QORE_MODULE_MONGODB_QC_OBJECTID_H
