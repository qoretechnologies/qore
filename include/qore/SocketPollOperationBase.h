/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    SocketPollOperationBase.h

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

    Note that the Qore library is released under a choice of three open-source
    licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
    information.
*/

#ifndef _QORE_SOCKETPOLLOPERATIONBASE_H

#define _QORE_SOCKETPOLLOPERATIONBASE_H

#include "qore/TypedHashDecl.h"

#include <vector>
#include <utility>

//! Base class for C++ poll operations submitted to the async I/O controller
/** Subclass this to create custom poll operations that execute on the I/O
    thread. The AsyncIoController calls continuePoll() directly (no Qore
    interpreter overhead) when the class has C++ private data registered
    under CID_SOCKETPOLLOPERATIONBASE.

    @par Module Usage
    External modules can subclass this to implement protocol-specific I/O:
    @code{.cpp}
    #include <qore/SocketPollOperationBase.h>

    class MyProtocolPollOp : public SocketPollOperationBase {
    public:
        MyProtocolPollOp(QoreObject* self, ...) : SocketPollOperationBase(self) { }
        bool goalReached() const override { return done; }
        QoreHashNode* continuePoll(ExceptionSink* xsink) override { ... }
        void abort(ExceptionSink* xsink) override { ... }
    protected:
        const char* getStateImpl() const override { return "my-state"; }
    };
    @endcode

    @since %Qore 1.12 (public API since %Qore 2.3)
*/
class SocketPollOperationBase : public AbstractPrivateData {
public:
    //! Creates the poll operation with a QoreObject self reference
    /** @param self the QoreObject wrapping this private data (tRef'd)
    */
    DLLEXPORT SocketPollOperationBase(QoreObject* self) : self(self) {
        self->tRef();
    }

    //! Creates the poll operation without a self reference
    /** Used when self is set later via setSelf().
    */
    DLLEXPORT SocketPollOperationBase() {
    }

    //! Sets the QoreObject self reference
    /** @param self the QoreObject (tRef'd)
        @note Must be called before continuePoll() if the default constructor was used.
    */
    DLLEXPORT void setSelf(QoreObject* self) {
        assert(!this->self);
        assert(self);
        this->self.reset(self);
        self->tRef();
    }

    //! Returns the current state as a string
    DLLEXPORT QoreStringNode* getState() const {
        return new QoreStringNode(getStateImpl());
    }

    //! Returns the referenced socket QoreObject from the "sock" member
    DLLEXPORT QoreObject* getReferencedSocketObject(ExceptionSink* xsink) const {
        assert(self);
        ValueHolder rv((*self)->getReferencedMemberNoMethod("sock", xsink), xsink);
        if (rv->getType() != NT_OBJECT) {
            xsink->raiseException("POLL-ERROR", "'sock' member is no longer an object; got type '%s' instead",
                rv->getFullTypeName());
            return nullptr;
        }
        return rv.release().get<QoreObject>();
    }

    DLLEXPORT virtual ~SocketPollOperationBase() = default;

    //! Returns true when the operation has reached its goal
    DLLEXPORT virtual bool goalReached() const = 0;

    //! Aborts the operation
    DLLEXPORT virtual void abort(ExceptionSink* xsink) = 0;

    //! Continues the poll operation
    /** @return a SocketPollInfo hash if more I/O is needed, or nullptr if done
    */
    DLLEXPORT virtual QoreHashNode* continuePoll(ExceptionSink* xsink) = 0;

    //! Returns the output of the operation (default: nothing)
    DLLEXPORT virtual QoreValue getOutput() const {
        return QoreValue();
    }

protected:
    //! Weak reference to the QoreObject wrapping this private data
    QoreObjectWeakRefHolder self;

    //! Returns a SocketPollInfo hash for the given events
    DLLEXPORT QoreHashNode* getSocketPollInfoHash(ExceptionSink* xsink, int events) const {
        ReferenceHolder<QoreHashNode> info(new QoreHashNode(hashdeclSocketPollInfo, xsink), xsink);
        info->setKeyValue("events", events, xsink);
        info->setKeyValue("socket", getReferencedSocketObject(xsink), xsink);
        return info.release();
    }

    //! Returns a SocketPollInfo hash with extra file descriptors to monitor
    /** @since %Qore 2.3
    */
    DLLEXPORT QoreHashNode* getSocketPollInfoHash(ExceptionSink* xsink, int events,
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

    //! Returns the human-readable state string (subclasses must implement)
    DLLEXPORT virtual const char* getStateImpl() const = 0;
};

#endif // _QORE_SOCKETPOLLOPERATIONBASE_H
