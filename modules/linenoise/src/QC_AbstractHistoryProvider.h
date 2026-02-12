/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_AbstractHistoryProvider.h

    Qore Programming Language

    Copyright 2012 - 2026 Qore Technologies, s.r.o.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef QC_ABSTRACTHISTORYPROVIDER_H
#define QC_ABSTRACTHISTORYPROVIDER_H

#include <qore/Qore.h>

DLLLOCAL extern qore_classid_t CID_ABSTRACTHISTORYPROVIDER;
DLLLOCAL extern QoreClass* QC_ABSTRACTHISTORYPROVIDER;

class HistoryProviderPriv : public AbstractPrivateData {
public:
    QoreObject* self;
    const QoreMethod* prevMethod;
    const QoreMethod* nextMethod;
    const QoreMethod* resetMethod;
    const QoreMethod* searchMethod;

    HistoryProviderPriv(QoreObject* obj) : self(obj) {
        const QoreClass* cls = obj->getClass();
        prevMethod = cls->findMethod("prev");
        nextMethod = cls->findMethod("next");
        resetMethod = cls->findMethod("reset");
        searchMethod = cls->findMethod("search");
        assert(prevMethod && nextMethod && resetMethod && searchMethod);
    }

    //! Returns true if the subclass overrides the default search() method
    bool hasSearchOverride() const {
        return searchMethod->getClass() != QC_ABSTRACTHISTORYPROVIDER;
    }

    //! Invoke prev(string prefix) -> *string; returns strdup'd string or NULL
    char* callPrev(const char* prefix, ExceptionSink* xsink) {
        ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), xsink);
        args->push(new QoreStringNode(prefix, QCS_UTF8), nullptr);
        ValueHolder rv(prevMethod->execManaged(self, *args, xsink), xsink);
        if (*xsink || rv->getType() != NT_STRING) {
            return NULL;
        }
        return strdup(rv->get<QoreStringNode>()->c_str());
    }

    //! Invoke next(string prefix) -> *string; returns strdup'd string or NULL
    char* callNext(const char* prefix, ExceptionSink* xsink) {
        ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), xsink);
        args->push(new QoreStringNode(prefix, QCS_UTF8), nullptr);
        ValueHolder rv(nextMethod->execManaged(self, *args, xsink), xsink);
        if (*xsink || rv->getType() != NT_STRING) {
            return NULL;
        }
        return strdup(rv->get<QoreStringNode>()->c_str());
    }

    //! Invoke reset()
    void callReset(ExceptionSink* xsink) {
        resetMethod->execManaged(self, nullptr, xsink).discard(xsink);
    }

    //! Invoke search(string pattern, int direction) -> *string; returns strdup'd string or NULL
    char* callSearch(const char* pattern, int direction, ExceptionSink* xsink) {
        ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), xsink);
        args->push(new QoreStringNode(pattern, QCS_UTF8), nullptr);
        args->push(direction, nullptr);
        ValueHolder rv(searchMethod->execManaged(self, *args, xsink), xsink);
        if (*xsink || rv->getType() != NT_STRING) {
            return NULL;
        }
        return strdup(rv->get<QoreStringNode>()->c_str());
    }
};

#endif // QC_ABSTRACTHISTORYPROVIDER_H
