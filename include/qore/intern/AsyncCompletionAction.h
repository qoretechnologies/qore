/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    AsyncCompletionAction.h

    Qore Programming Language

    Copyright (C) 2026 Qore Technologies, s.r.o.

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

#include <qore/Qore.h>
#include <qore/QoreFuture.h>
#include <qore/QoreCounter.h>

#include "qore/intern/QoreChannel.h"
#include "qore/intern/QoreEventNotifier.h"

#include <vector>

//! Abstract base for I/O completion actions — pure C++, no Qore interpreter
/** Completion actions describe WHAT to do with the result of an async I/O
    operation. They are executed on the I/O thread by the poll operation's
    continuePoll() — but since they are pure C++ (no execValue()), they
    cannot block the I/O thread or run untrusted user code.

    Actions are ref-counted. The poll operation holds a ref; deref when the
    stream completes or the operation is cancelled.

    @since %Qore 2.3
*/
class AbstractAsyncAction : public QoreReferenceCounter {
public:
    DLLLOCAL virtual ~AbstractAsyncAction() = default;

    //! Called on the I/O thread when the operation produces a successful result
    /** @param output the result value (ref'd — action must deref or transfer ownership)
        @param xsink exception sink
    */
    DLLLOCAL virtual void execute(QoreValue output, ExceptionSink* xsink) = 0;

    //! Called on the I/O thread when the operation fails
    /** @param err error string
        @param desc error description
        @param xsink exception sink
    */
    DLLLOCAL virtual void executeError(const char* err, const char* desc,
        ExceptionSink* xsink) = 0;

    //! Release all held references
    DLLLOCAL virtual void cleanup(ExceptionSink* xsink) = 0;

    //! Returns true if this is a streaming action (e.g., ChannelAction)
    /** Used by H2/H1 dispatch to decide whether to split body+end_stream
        into separate items for the consumer.
    */
    DLLLOCAL virtual bool isStreaming() const { return false; }

    //! Called when the stream is complete (end_stream received)
    /** Streaming actions override this to close the channel.
        Default is a no-op for non-streaming actions (e.g., PromiseAction).
    */
    DLLLOCAL virtual void complete(ExceptionSink* xsink) {}

    DLLLOCAL void ref() { ROreference(); }
    DLLLOCAL void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            cleanup(xsink);
            delete this;
        }
    }
};

//! Resolves a QorePromise with the result
/** Used for synchronous request() calls — the user thread blocks on
    Future::get() and receives the result when the I/O thread resolves
    the Promise.

    @since %Qore 2.3
*/
class PromiseAction : public AbstractAsyncAction {
public:
    //! Creates a PromiseAction from a QorePromise and optional QoreObject
    /** @param promise the QorePromise private data (will be ref'd)
        @param promise_obj the Promise QoreObject — if provided, a strong ref is
            held to prevent the QoreObject from being garbage collected (which would
            trigger Promise::destructor() → promiseDestroyed() and invalidate the
            promise before the I/O thread resolves it)
    */
    DLLLOCAL PromiseAction(QorePromise* promise, QoreObject* promise_obj = nullptr)
        : promise(promise), promise_obj(promise_obj) {
        assert(promise);
        promise->ref();
        if (promise_obj) {
            promise_obj->ref();
        }
    }

    DLLLOCAL void execute(QoreValue output, ExceptionSink* xsink) override {
        promise->set(output, xsink);
    }

    DLLLOCAL void executeError(const char* err, const char* desc,
            ExceptionSink* xsink) override {
        promise->setError(err, desc, QoreValue(), xsink);
    }

    DLLLOCAL void cleanup(ExceptionSink* xsink) override {
        if (promise) {
            promise->deref(xsink);
            promise = nullptr;
        }
        if (promise_obj) {
            promise_obj->deref(xsink);
            promise_obj = nullptr;
        }
    }

private:
    QorePromise* promise;
    QoreObject* promise_obj = nullptr;
};

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
            printd(0, "ChannelAction::execute(): channel full, discarding value\n");
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
            printd(0, "ChannelAction::executeError(): channel full, discarding error\n");
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

//! Decrements a QoreCounter
/** Used for synchronization — e.g., wait for N operations to complete.

    @since %Qore 2.3
*/
class CounterAction : public AbstractAsyncAction {
public:
    DLLLOCAL CounterAction(QoreCounter* counter) : counter(counter) {
        assert(counter);
        // QoreCounter doesn't have ref/deref — it's typically stack-allocated
        // or member-owned. The caller must ensure it outlives this action.
    }

    DLLLOCAL void execute(QoreValue output, ExceptionSink* xsink) override {
        output.discard(xsink);
        counter->dec(xsink);
    }

    DLLLOCAL void executeError(const char* err, const char* desc,
            ExceptionSink* xsink) override {
        counter->dec(xsink);
    }

    DLLLOCAL void cleanup(ExceptionSink* xsink) override {
        counter = nullptr;
    }

private:
    QoreCounter* counter;
};

//! Executes multiple actions in sequence
/** Used to combine actions — e.g., PromiseAction + CounterAction for
    synchronous request with completion signaling.

    @since %Qore 2.3
*/
class CompositeAction : public AbstractAsyncAction {
public:
    DLLLOCAL CompositeAction() = default;

    //! Add an action (takes ownership — ref'd by caller)
    DLLLOCAL void add(AbstractAsyncAction* action) {
        assert(action);
        action->ref();
        actions.push_back(action);
    }

    DLLLOCAL void execute(QoreValue output, ExceptionSink* xsink) override {
        for (size_t i = 0; i < actions.size(); ++i) {
            if (i < actions.size() - 1) {
                // All but last get a ref'd copy
                actions[i]->execute(output.refSelf(), xsink);
            } else {
                // Last action takes ownership
                actions[i]->execute(output, xsink);
            }
        }
    }

    DLLLOCAL void executeError(const char* err, const char* desc,
            ExceptionSink* xsink) override {
        for (auto* action : actions) {
            action->executeError(err, desc, xsink);
        }
    }

    DLLLOCAL void cleanup(ExceptionSink* xsink) override {
        for (auto* action : actions) {
            action->deref(xsink);
        }
        actions.clear();
    }

private:
    std::vector<AbstractAsyncAction*> actions;
};

#endif // _QORE_INTERN_ASYNCCOMPLETIONACTION_H
