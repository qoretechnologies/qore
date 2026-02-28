/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_SocketPollOperationBase.h

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

#ifndef _QORE_CLASS_SOCKETPOLLOPERATIONBASE_H

#define _QORE_CLASS_SOCKETPOLLOPERATIONBASE_H

#include "qore/TypedHashDecl.h"

#include <vector>
#include <utility>

class SocketPollOperationBase : public AbstractPrivateData {
public:
    DLLLOCAL SocketPollOperationBase(QoreObject* self) : self(self) {
        self->tRef();
    }

    DLLLOCAL SocketPollOperationBase() {
    }

    DLLLOCAL void setSelf(QoreObject* self) {
        assert(!this->self);
        assert(self);
        this->self.reset(self);
        self->tRef();
    }

    DLLLOCAL QoreStringNode* getState() const {
        return new QoreStringNode(getStateImpl());
    }

    DLLLOCAL QoreObject* getReferencedSocketObject(ExceptionSink* xsink) const {
        assert(self);
        ValueHolder rv(self->getReferencedMemberNoMethod("sock", xsink), xsink);
        if (rv->getType() != NT_OBJECT) {
            xsink->raiseException("POLL-ERROR", "'sock' member is no longer an object; got type '%s' instead",
                rv->getFullTypeName());
            return nullptr;
        }
        return rv.release().get<QoreObject>();
    }

    DLLLOCAL virtual ~SocketPollOperationBase() = default;

    DLLLOCAL virtual bool goalReached() const = 0;

    DLLLOCAL virtual void abort(ExceptionSink* xsink) = 0;

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) = 0;

    DLLLOCAL virtual QoreValue getOutput() const {
        return QoreValue();
    }

protected:
    QoreObjectWeakRefHolder self;

    DLLLOCAL QoreHashNode* getSocketPollInfoHash(ExceptionSink* xsink, int events) const {
        ReferenceHolder<QoreHashNode> info(new QoreHashNode(hashdeclSocketPollInfo, xsink), xsink);
        info->setKeyValue("events", events, xsink);
        info->setKeyValue("socket", getReferencedSocketObject(xsink), xsink);
        return info.release();
    }

    //! Returns a SocketPollInfo hash with extra file descriptors to monitor
    /** @since %Qore 2.3
    */
    DLLLOCAL QoreHashNode* getSocketPollInfoHash(ExceptionSink* xsink, int events,
            const std::vector<std::pair<int, int>>& extra_fds) const {
        ReferenceHolder<QoreHashNode> info(new QoreHashNode(hashdeclSocketPollInfo, xsink), xsink);
        info->setKeyValue("events", events, xsink);
        info->setKeyValue("socket", getReferencedSocketObject(xsink), xsink);
        if (!extra_fds.empty()) {
            ReferenceHolder<QoreListNode> list(
                new QoreListNode(hashdeclExtraPollFdInfo->getTypeInfo()), xsink);
            for (auto& [fd, ev] : extra_fds) {
                ReferenceHolder<QoreHashNode> h(new QoreHashNode(hashdeclExtraPollFdInfo, xsink), xsink);
                h->setKeyValue("fd", fd, xsink);
                h->setKeyValue("events", ev, xsink);
                list->push(h.release(), xsink);
            }
            info->setKeyValue("extra_fds", list.release(), xsink);
        }
        return info.release();
    }

    DLLLOCAL virtual const char* getStateImpl() const = 0;
};

DLLLOCAL QoreClass* initSocketPollOperationBaseClass(QoreNamespace& qorens);

#endif // _QORE_CLASS_SOCKETPOLLOPERATIONBASE_H
