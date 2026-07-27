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
#include "qore/intern/WebSocketStreamFrameState.h"
#include "qore/QoreQueue.h"
#include "qore/QoreString.h"

#include <memory>
#include <mutex>

//! Pushes the result to a QoreChannel (non-blocking trySend for I/O thread)
/** Used for streaming responses (SSE, gRPC, WebSocket, A2A). The I/O thread
    pushes each chunk/frame/event via trySend(); the user reads via recv()
    on their own thread.

    If trySend() returns false (channel full — backpressure), the value is
    discarded and a warning is logged. The I/O thread must never block.

    @since %Qore 3.0
*/
class DLLLOCAL ChannelAction : public AbstractAsyncAction {
public:
    DLLLOCAL ChannelAction(QoreChannel* channel, QoreEventNotifier* notifier = nullptr,
            QoreObject* notifier_obj = nullptr) : channel(channel), notifier(notifier), notifier_obj(notifier_obj) {
        assert(channel);
        channel->ref();
        if (notifier) {
            notifier->ref();
        }
        if (notifier_obj) {
            notifier_obj->ref();
        }
    }

    DLLLOCAL void execute(QoreValue output, ExceptionSink* xsink) override {
        std::lock_guard<std::mutex> lg(mtx);
        if (sse_state) {
            sse_state->execute(output, xsink);
            return;
        }
        if (frame_state) {
            // After installFrameState(): body bytes flow into the frame
            // decoder instead of the channel.  Serialized with the install
            // via mtx, so no pre-swap dispatch can leak into the channel
            // after the install returns, and no post-swap dispatch can be
            // ordered before the residual drain.
            feedHashBodyLocked(output, frame_state.get());
            output.discard(xsink);
            return;
        }
        if (!channel->trySend(output)) {
            // Backpressure: channel full — discard and log
            output.discard(xsink);
            printd(5, "ChannelAction::execute(): channel full, discarding value\n");
        } else {
            notify();
        }
    }

    DLLLOCAL bool isStreaming() const override { return true; }

    DLLLOCAL void complete(ExceptionSink* xsink) override {
        std::lock_guard<std::mutex> lg(mtx);
        if (completed) {
            return;
        }
        completed = true;
        if (sse_state) {
            sse_state->complete(xsink);
            return;
        }
        if (frame_state) {
            frame_state->pushCloseSentinel();
            return;
        }
        if (channel) {
            channel->close();
            notify();
        }
    }

    DLLLOCAL void executeError(const char* err, const char* desc,
            ExceptionSink* xsink) override {
        std::lock_guard<std::mutex> lg(mtx);
        if (completed) {
            return;
        }
        completed = true;
        if (sse_state) {
            sse_state->executeError(err, desc, xsink);
            return;
        }
        if (frame_state) {
            Queue* q = frame_state->getMsgQueue();
            if (q) {
                ReferenceHolder<QoreHashNode> err_hash(new QoreHashNode(autoTypeInfo), xsink);
                err_hash->setKeyValue("err", new QoreStringNode(err), xsink);
                err_hash->setKeyValue("desc", new QoreStringNode(desc), xsink);
                q->pushAndTakeRef(err_hash.release());
                q->pushAndTakeRef(QoreValue());  // NOTHING sentinel
            }
            return;
        }
        // Push error as a hash so the receiver can distinguish errors from data
        ReferenceHolder<QoreHashNode> err_hash(new QoreHashNode(autoTypeInfo), xsink);
        err_hash->setKeyValue("err", new QoreStringNode(err), xsink);
        err_hash->setKeyValue("desc", new QoreStringNode(desc), xsink);
        QoreHashNode* h = err_hash.release();
        if (!channel->trySend(h)) {
            h->deref(xsink);
            printd(5, "ChannelAction::executeError(): channel full, discarding error\n");
        } else {
            notify();
        }
        channel->close();
        notify();
    }

    DLLLOCAL void cleanup(ExceptionSink* xsink) override {
        std::lock_guard<std::mutex> lg(mtx);
        if (sse_state) {
            sse_state->deref(xsink);
            sse_state = nullptr;
        }
        frame_state.reset();  // drops any installed msg_queue ref
        if (notifier_obj) {
            notifier_obj->deref(xsink);
            notifier_obj = nullptr;
        }
        if (notifier) {
            notifier->deref(xsink);
            notifier = nullptr;
        }
        if (channel) {
            channel->deref(xsink);
            channel = nullptr;
        }
    }

    //! Installs a WebSocket frame-state decoder on this ChannelAction
    /** Feeds @a initial (body bytes the caller already took off the channel)
        into the frame state, then drains any items still buffered in the
        channel into it, then publishes the frame state.  All three steps are
        serialized against execute(), complete() and executeError() via a
        per-action mutex, so in-flight dispatches that were targeting the old
        channel path cannot leak bytes past the install.

        @a initial exists because the stream handle's readData() splits a
        coalesced headers+body event: it returns the response hash without the
        body so the extended-CONNECT handshake can validate the status code,
        and holds the body back for the next read.  Those bytes precede
        everything still buffered here, so they must be fed first.

        @param fs the frame state (ownership transferred); holds one
               msg_queue ref that is dropped in cleanup()/destructor
        @param initial optional already-consumed response item to feed first
        @param xsink exception sink
    */
    DLLLOCAL void installFrameState(std::unique_ptr<WebSocketStreamFrameState> fs,
            QoreValue initial, ExceptionSink* xsink) {
        std::lock_guard<std::mutex> lg(mtx);
        if (sse_state) {
            initial.discard(xsink);
            xsink->raiseException("HTTPCLIENT-STATE-ERROR",
                "cannot install frame state after SSE state has been installed");
            return;
        }
        if (initial.getType() != NT_NOTHING) {
            feedHashBodyLocked(initial, fs.get());
            initial.discard(xsink);
        }
        // Drain anything the I/O thread already pushed before this call.
        // feedHashBodyLocked() extracts body bytes and hands them to the
        // decoder; non-body hashes (headers-only, metadata) are discarded.
        if (channel) {
            while (true) {
                bool has_value = false;
                QoreValue item = channel->tryRecv(has_value);
                if (!has_value) {
                    break;
                }
                feedHashBodyLocked(item, fs.get());
                item.discard(xsink);
            }
        }
        frame_state = std::move(fs);
        // The stream can already have ended before the swap (complete() ran
        // while the channel path was still active, so it closed the channel
        // instead of pushing the decoder's sentinel).  Deliver the sentinel
        // now, or the consumer waits for a close that never arrives.
        if (completed) {
            frame_state->pushCloseSentinel();
        }
    }

    //! Installs an SSE parser on this ChannelAction
    /** Drains any response items already buffered in the channel and feeds
        their body bytes through the SSE parser before publishing the parser
        for subsequent I/O-thread dispatches.

        @param queue parsed SSE event queue
        @param notifier optional EventNotifier signaled after parsed events
        @param notifier_obj optional EventNotifier object to keep the fd valid
        @param initial optional already-consumed response item to feed first
        @param xsink exception sink
    */
    DLLLOCAL void installSseState(Queue* queue, QoreEventNotifier* notifier,
        QoreObject* notifier_obj, QoreValue initial, ExceptionSink* xsink);

    //! Marks this action as eligible for delayed installSseState() after
    //! end_stream — callers must opt in to retention because every retained
    //! ChannelAction stays alive for the lifetime of the connection until
    //! installSseState() / cancelStream() / cleanup() releases it.
    DLLLOCAL void setSseInstallEligible() {
        sse_install_eligible = true;
    }

    //! @return true if this action should be retained after end_stream so
    //! that a later installSseState() call can drain buffered body bytes
    DLLLOCAL bool isSseInstallEligible() const {
        return sse_install_eligible;
    }

private:
    std::mutex mtx;
    QoreChannel* channel;
    std::unique_ptr<WebSocketStreamFrameState> frame_state;
    AbstractAsyncAction* sse_state = nullptr;
    QoreEventNotifier* notifier = nullptr;
    QoreObject* notifier_obj = nullptr;
    bool completed = false;
    bool sse_install_eligible = false;

    DLLLOCAL void notify() {
        if (notifier) {
            notifier->notify();
        }
    }

    //! Extracts the \c body binary from a response hash and feeds it into
    //! the decoder; called with \c mtx held.
    DLLLOCAL static void feedHashBodyLocked(QoreValue v,
            WebSocketStreamFrameState* fs) {
        if (!fs || v.getType() != NT_HASH) {
            return;
        }
        QoreValue body_val = v.get<const QoreHashNode>()->getKeyValue("body");
        if (body_val.getType() == NT_BINARY) {
            const BinaryNode* data = body_val.get<const BinaryNode>();
            if (data && data->size() > 0) {
                fs->feedData(data->getPtr(), data->size());
            }
        }
    }
};

//! Notifies an EventNotifier (signal only, no data)
/** Used for ping operations and poll-based integrations where the caller
    just needs to know the operation completed, not the result data.

    @since %Qore 3.0
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

//! Resolves a Promise AND signals an EventNotifier on completion
/** Combines data delivery (via Promise/Future) with pollable fd signaling
    (via EventNotifier).  Used by poll-based APIs (startPollSendRecv) that
    delegate to the conn_mgr: the I/O thread resolves the promise with the
    response data and simultaneously signals the notifier so the caller's
    poll loop wakes up.

    @since %Qore 3.0
*/
class PromiseNotifierAction : public AbstractAsyncAction {
public:
    DLLLOCAL PromiseNotifierAction(QorePromise* promise, QoreEventNotifier* notifier)
        : promise(promise), notifier(notifier) {
        assert(promise);
        assert(notifier);
        promise->ref();
        notifier->ref();
    }

    DLLLOCAL void execute(QoreValue output, ExceptionSink* xsink) override {
        promise->set(output, xsink);
        notifier->notify();
    }

    DLLLOCAL void executeError(const char* err, const char* desc,
            ExceptionSink* xsink) override {
        promise->setError(err, desc, QoreValue(), xsink);
        notifier->notify();
    }

    DLLLOCAL void cleanup(ExceptionSink* xsink) override {
        if (promise) {
            promise->deref(xsink);
            promise = nullptr;
        }
        if (notifier) {
            notifier->deref(xsink);
            notifier = nullptr;
        }
    }

private:
    QorePromise* promise;
    QoreEventNotifier* notifier;
};

//! Resolves a Promise on streaming response headers and signals an EventNotifier
/** Used by header-only streaming poll APIs such as SSE connect checks.  The
    action advertises streaming mode so protocol poll operations dispatch
    response headers before reading the long-lived body, then resolves the
    promise exactly once from that header hash.
*/
class StreamingHeadersPromiseNotifierAction : public AbstractAsyncAction {
public:
    DLLLOCAL StreamingHeadersPromiseNotifierAction(QorePromise* promise, QoreEventNotifier* notifier)
        : promise(promise), notifier(notifier) {
        assert(promise);
        assert(notifier);
        promise->ref();
        notifier->ref();
    }

    DLLLOCAL bool isStreaming() const override { return true; }

    DLLLOCAL void execute(QoreValue output, ExceptionSink* xsink) override {
        if (!done && output.getType() == NT_HASH
                && output.get<QoreHashNode>()->getKeyValue("status_code").getType()) {
            done = true;
            promise->set(output, xsink);
            notifier->notify();
            return;
        }
        output.discard(xsink);
    }

    DLLLOCAL void executeError(const char* err, const char* desc,
            ExceptionSink* xsink) override {
        if (!done) {
            done = true;
            promise->setError(err, desc, QoreValue(), xsink);
            notifier->notify();
        }
    }

    DLLLOCAL void complete(ExceptionSink* xsink) override {
        if (!done) {
            done = true;
            promise->setError("HTTPCLIENT-STREAM-CLOSED",
                "stream closed before response headers were received", QoreValue(), xsink);
            notifier->notify();
        }
    }

    DLLLOCAL void cleanup(ExceptionSink* xsink) override {
        if (promise) {
            promise->deref(xsink);
            promise = nullptr;
        }
        if (notifier) {
            notifier->deref(xsink);
            notifier = nullptr;
        }
    }

private:
    QorePromise* promise;
    QoreEventNotifier* notifier;
    bool done = false;
};

//! Parses a single SSE message from a text buffer into an SseMessageInfo hash
/** Standalone version of qore_socket_private::parseServerSentEvent().
    @param xsink exception sink
    @param buf the SSE message text (one event, terminated before the double newline)
    @return a new QoreHashNode with SseMessageInfo fields (event, data, id, retry, comment)
*/
DLLLOCAL QoreHashNode* parseSseEvent(ExceptionSink* xsink, const QoreString& buf);

//! Pushes parsed SSE events to a Queue — runs on the I/O thread
/** Intercepts body data from HTTP streaming responses (H1 chunked, H2 DATA,
    H3 DATA), accumulates text, splits at double-newline boundaries, parses
    each event via parseSseEvent(), and pushes SseMessageInfo hashes to a Queue.

    Zero thread hops — parsing happens in the I/O thread's continuePoll() cycle.
    The Queue consumer (Qore SseObservable) runs on the I/O controller's worker pool.

    @since %Qore 3.0
*/
class SseAction : public AbstractAsyncAction {
public:
    //! Creates an SSE action that pushes parsed events to a Queue
    /** @param queue the target Queue (will be ref'd)
    */
    DLLLOCAL SseAction(Queue* queue) : SseAction(queue, nullptr, nullptr) {
    }

    //! Creates an SSE action that pushes parsed events to a Queue and signals a notifier
    /** @param queue the target Queue (will be ref'd)
        @param notifier optional EventNotifier to signal after queue updates (will be ref'd)
        @param notifier_obj optional EventNotifier object to keep the notifier fd valid (will be ref'd)
    */
    DLLLOCAL SseAction(Queue* queue, QoreEventNotifier* notifier, QoreObject* notifier_obj)
        : queue(queue), notifier(notifier), notifier_obj(notifier_obj) {
        assert(queue);
        queue->ref();
        if (notifier) {
            notifier->ref();
        }
        if (notifier_obj) {
            notifier_obj->ref();
        }
    }

    //! Receives a body data chunk, accumulates and parses SSE events
    DLLLOCAL void execute(QoreValue output, ExceptionSink* xsink) override;

    DLLLOCAL bool isStreaming() const override { return true; }

    //! Pushes NOTHING sentinel to the Queue (stream complete)
    DLLLOCAL void complete(ExceptionSink* xsink) override {
        std::lock_guard<std::mutex> lg(mtx);
        if (queue) {
            queue->pushAndTakeRef(QoreValue());
            notify();
        }
    }

    //! Pushes an error hash to the Queue
    DLLLOCAL void executeError(const char* err, const char* desc,
            ExceptionSink* xsink) override {
        std::lock_guard<std::mutex> lg(mtx);
        if (queue) {
            ReferenceHolder<QoreHashNode> err_hash(new QoreHashNode(autoTypeInfo), xsink);
            err_hash->setKeyValue("err", new QoreStringNode(err), xsink);
            err_hash->setKeyValue("desc", new QoreStringNode(desc), xsink);
            queue->pushAndTakeRef(err_hash.release());
            queue->pushAndTakeRef(QoreValue());  // sentinel
            notify();
        }
    }

    DLLLOCAL void cleanup(ExceptionSink* xsink) override {
        std::lock_guard<std::mutex> lg(mtx);
        if (notifier_obj) {
            notifier_obj->deref(xsink);
            notifier_obj = nullptr;
        }
        if (notifier) {
            notifier->deref(xsink);
            notifier = nullptr;
        }
        if (queue) {
            queue->deref(xsink);
            queue = nullptr;
        }
    }

private:
    DLLLOCAL void notify() {
        if (notifier) {
            notifier->notify();
        }
    }

    Queue* queue;           //!< ref'd Queue for event delivery
    QoreEventNotifier* notifier = nullptr; //!< optional notifier for event wakeups
    QoreObject* notifier_obj = nullptr; //!< ref'd EventNotifier object keeping fd valid
    std::mutex mtx;         //!< serializes buffered replay with I/O-thread delivery
    QoreString sse_buffer;  //!< accumulated SSE text (UTF-8)
};

inline void ChannelAction::installSseState(Queue* queue, QoreEventNotifier* notifier,
        QoreObject* notifier_obj, QoreValue initial, ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lg(mtx);
    if (frame_state) {
        initial.discard(xsink);
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "cannot install SSE state after frame state has been installed");
        return;
    }
    if (sse_state) {
        initial.discard(xsink);
        xsink->raiseException("HTTPCLIENT-STATE-ERROR",
            "SSE state has already been installed");
        return;
    }

    ReferenceHolder<SseAction> action(new SseAction(queue, notifier, notifier_obj), xsink);
    if (initial.getType() != NT_NOTHING) {
        action->execute(initial, xsink);
        if (*xsink) {
            return;
        }
    }
    if (channel) {
        while (true) {
            bool has_value = false;
            QoreValue item = channel->tryRecv(has_value);
            if (!has_value) {
                break;
            }
            action->execute(item, xsink);
            if (*xsink) {
                return;
            }
        }
    }
    sse_state = action.release();
    if (completed) {
        sse_state->complete(xsink);
    }
}

//! Feeds body bytes into a WebSocketStreamFrameState for H2/H3 client WS tunnels
/** Used to swap in after the extended-CONNECT handshake completes so that
    subsequent DATA chunks are decoded into complete WebSocket messages in
    pure C++ (no O(N²) Qore binary concatenation) and pushed as typed
    hashes to the caller-supplied msg_queue.

    The action pulls the `body` binary out of each output hash delivered by
    the H2/H3 client poll operation and hands it to
    WebSocketStreamFrameState::feedData().  Control-frame responses
    (auto-pong, close-echo on error) are emitted via the send callback the
    frame state was constructed with.

    @since %Qore 3.0
*/
class DLLLOCAL FrameStateAction : public AbstractAsyncAction {
public:
    //! Creates a frame-state action owning a freshly-built WebSocketStreamFrameState
    /** @param frame_state the WebSocketStreamFrameState (ownership transferred)
    */
    DLLLOCAL explicit FrameStateAction(std::unique_ptr<WebSocketStreamFrameState> frame_state)
        : frame_state(std::move(frame_state)) {
        assert(this->frame_state);
    }

    //! Receives a body data chunk and feeds it into the frame state
    DLLLOCAL void execute(QoreValue output, ExceptionSink* xsink) override {
        if (output.getType() == NT_HASH) {
            QoreValue body_val = output.get<QoreHashNode>()->getKeyValue("body");
            if (body_val.getType() == NT_BINARY) {
                const BinaryNode* data = body_val.get<const BinaryNode>();
                if (frame_state && data->size() > 0) {
                    frame_state->feedData(data->getPtr(), data->size());
                }
            }
        }
        output.discard(xsink);
    }

    DLLLOCAL bool isStreaming() const override { return true; }

    //! Pushes a NOTHING sentinel so the msg_queue consumer sees stream end
    DLLLOCAL void complete(ExceptionSink* xsink) override {
        if (frame_state) {
            frame_state->pushCloseSentinel();
        }
    }

    //! Pushes an error hash and a NOTHING sentinel to the msg_queue
    DLLLOCAL void executeError(const char* err, const char* desc,
            ExceptionSink* xsink) override {
        if (!frame_state) {
            return;
        }
        Queue* q = frame_state->getMsgQueue();
        if (q) {
            ReferenceHolder<QoreHashNode> err_hash(new QoreHashNode(autoTypeInfo), xsink);
            err_hash->setKeyValue("err", new QoreStringNode(err), xsink);
            err_hash->setKeyValue("desc", new QoreStringNode(desc), xsink);
            q->pushAndTakeRef(err_hash.release());
            q->pushAndTakeRef(QoreValue());  // sentinel
        }
    }

    DLLLOCAL void cleanup(ExceptionSink* xsink) override {
        // unique_ptr destructor derefs the msg_queue (frame state holds one ref)
        frame_state.reset();
    }

    //! Feeds residual body bytes (drained from the pre-swap Channel) into the frame state
    /** Used by the swap path after replacing a ChannelAction with a
        FrameStateAction: any bytes still buffered in the old Channel must be
        funneled through the frame decoder before new DATA chunks start
        arriving via execute().
    */
    DLLLOCAL void feedResidual(const void* data, size_t len) {
        if (frame_state && data && len) {
            frame_state->feedData(data, len);
        }
    }

private:
    std::unique_ptr<WebSocketStreamFrameState> frame_state;
};

#endif // _QORE_INTERN_ASYNCCOMPLETIONACTION_H
