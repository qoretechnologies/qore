/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    AsyncCompletionAction.h

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

#ifndef _QORE_INTERN_ASYNCCOMPLETIONACTION_H
#define _QORE_INTERN_ASYNCCOMPLETIONACTION_H

#include "qore/AsyncCompletionAction.h"

#include "qore/intern/QoreChannel.h"
#include "qore/intern/QoreEventNotifier.h"

//! Pushes the result to a QoreChannel (non-blocking trySend for I/O thread)
/** Used for streaming responses (SSE, gRPC, WebSocket, A2A). The I/O thread
    pushes each chunk/frame/event via trySend(); the user reads via recv()
    on their own thread.

    If trySend() returns false (channel full — backpressure), the value is
    discarded and a warning is logged. The I/O thread must never block.

    @since %Qore 2.3
*/
class ChannelAction : public AbstractAsyncAction {
public:
    DLLLOCAL ChannelAction(QoreChannel* channel) : channel(channel) {
        assert(channel);
        channel->ref();
    }

    DLLLOCAL void execute(QoreValue output, ExceptionSink* xsink) override {
        if (!channel->trySend(output)) {
            // Backpressure: channel full — discard and log
            output.discard(xsink);
            printd(5, "ChannelAction::execute(): channel full, discarding value\n");
        }
    }

    DLLLOCAL bool isStreaming() const override { return true; }

    DLLLOCAL void complete(ExceptionSink* xsink) override {
        if (channel) {
            channel->close();
        }
    }

    DLLLOCAL void executeError(const char* err, const char* desc,
            ExceptionSink* xsink) override {
        // Push error as a hash so the receiver can distinguish errors from data
        ReferenceHolder<QoreHashNode> err_hash(new QoreHashNode(autoTypeInfo), xsink);
        err_hash->setKeyValue("err", new QoreStringNode(err), xsink);
        err_hash->setKeyValue("desc", new QoreStringNode(desc), xsink);
        QoreHashNode* h = err_hash.release();
        if (!channel->trySend(h)) {
            h->deref(xsink);
            printd(5, "ChannelAction::executeError(): channel full, discarding error\n");
        }
        channel->close();
    }

    DLLLOCAL void cleanup(ExceptionSink* xsink) override {
        if (channel) {
            channel->deref(xsink);
            channel = nullptr;
        }
    }

private:
    QoreChannel* channel;
};

//! Notifies an EventNotifier (signal only, no data)
/** Used for ping operations and poll-based integrations where the caller
    just needs to know the operation completed, not the result data.

    @since %Qore 2.3
*/
class EventNotifierAction : public AbstractAsyncAction {
public:
    DLLLOCAL EventNotifierAction(QoreEventNotifier* notifier) : notifier(notifier) {
        assert(notifier);
        notifier->ref();
    }

    DLLLOCAL void execute(QoreValue output, ExceptionSink* xsink) override {
        output.discard(xsink);  // signal only — discard data
        notifier->notify();
    }

    DLLLOCAL void executeError(const char* err, const char* desc,
            ExceptionSink* xsink) override {
        notifier->notify();  // notify even on error
    }

    DLLLOCAL void cleanup(ExceptionSink* xsink) override {
        if (notifier) {
            notifier->deref(xsink);
            notifier = nullptr;
        }
    }

private:
    QoreEventNotifier* notifier;
};

#endif // _QORE_INTERN_ASYNCCOMPLETIONACTION_H
