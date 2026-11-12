/* -*- mode: c++; indent-tabs-mode: nil -*- */
/** @file HttpStreamConsumeHook.h

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
*/

#ifndef _QORE_INTERN_HTTPSTREAMCONSUMEHOOK_H
#define _QORE_INTERN_HTTPSTREAMCONSUMEHOOK_H

#include "qore/intern/QoreConsumeHook.h"

#include <algorithm>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

//! Returns the size of the response body data that a value delivered for a client stream carries
/** The values are the hashes that the client's poll operations deliver (a partial response with a \c body key, or
    the final response); anything else carries no body data.  Takes no lock.
*/
DLLLOCAL size_t qore_http_stream_value_body_size(const QoreValue& v);

//! Wakes the async I/O controller thread that serves the socket with the given hash, if it still exists
DLLLOCAL void qore_http_stream_wake_socket(const std::string& sock_hash);

#ifdef DEBUG
//! Adds bytes reported as consumed after their session was gone to the debug accounting
DLLLOCAL void qore_http_stream_credit_orphaned(size_t bytes);
#endif

//! Grants the stream credit of the response data that the application of an HTTP/2 or HTTP/3 client consumes
/** Installed on the Channel or Queue that delivers a streaming response (issue #5820).  It holds a weak reference to
    the session, so a Channel that outlives its session reports nothing; it wakes the controller by the socket's hash
    instead of a reference to the socket.  See QoreConsumeHook for the lock order.

    @tparam Session Http2Session or QuicSession, with a creditConsumedStreamData(stream_id, bytes) method that returns
    the number of bytes credited
*/
template <class Session, typename StreamId>
class HttpStreamConsumeHook : public QoreConsumeHook {
public:
    DLLLOCAL HttpStreamConsumeHook(std::weak_ptr<Session> session, StreamId stream_id, std::string sock_hash)
            : session(std::move(session)), stream_id(stream_id), sock_hash(std::move(sock_hash)) {
    }

    DLLLOCAL size_t measure(const QoreValue& v) const override {
        return qore_http_stream_value_body_size(v);
    }

    //! Never throws: it is called from destructors and after locks are released
    /** The data consumed first repays the credit advanced for items that are complete (see progress()); when they are
        repaid, the data of the item in progress is credited in advance.
    */
    DLLLOCAL void consumed(size_t bytes) override {
        size_t credit;
        {
            std::lock_guard<std::mutex> lg(m);
            size_t repay = std::min(bytes, closed_advance);
            closed_advance -= repay;
            credit = bytes - repay;
            if (!closed_advance && open_pending) {
                credit += open_pending;
                open_advance += open_pending;
                open_pending = 0;
            }
        }
        grant(credit);
    }

    //! The data of the item in progress is credited at once while no credit advanced for a complete item is still
    //! to be repaid, and otherwise when it is
    /** The advance is repaid by the data consumed next (that of any item, or data that makes no item), not
        necessarily by the item it was granted for.  So the data that the client holds without granting its credit is
        bounded by the stream's window plus about one advanced item (which is bounded by the maximum event or message
        size), and an item larger than the window does not stall its stream.
    */
    DLLLOCAL void progress(size_t bytes) override {
        size_t credit = 0;
        {
            std::lock_guard<std::mutex> lg(m);
            if (closed_advance) {
                open_pending += bytes;
            } else {
                open_advance += bytes;
                credit = bytes;
            }
        }
        grant(credit);
    }

    DLLLOCAL void closeItem() override {
        std::lock_guard<std::mutex> lg(m);
        // the data of the item is reported again when it is consumed; what was not credited in advance is credited
        // then, and what was is taken back
        closed_advance += open_advance;
        open_advance = 0;
        open_pending = 0;
    }

private:
    //! credit advanced for complete items and not yet repaid by consumed data
    size_t closed_advance = 0;
    //! credit advanced for the item in progress
    size_t open_advance = 0;
    //! data of the item in progress waiting for the credit advanced before it to be repaid
    size_t open_pending = 0;
    //! a leaf mutex: no lock is taken under it
    std::mutex m;

    //! Never throws
    DLLLOCAL void grant(size_t bytes) {
        if (!bytes) {
            return;
        }
        try {
            std::shared_ptr<Session> s = session.lock();
            if (!s) {
#ifdef DEBUG
                qore_http_stream_credit_orphaned(bytes);
#endif
                return;
            }
            if (s->creditConsumedStreamData(stream_id, bytes)) {
                // the I/O thread sends the WINDOW_UPDATE / MAX_STREAM_DATA
                qore_http_stream_wake_socket(sock_hash);
            }
        } catch (...) {
            // the credit is lost; the stream's peer stalls at its window instead of the process ending
            assert(false);
        }
    }

    std::weak_ptr<Session> session;
    StreamId stream_id;
    std::string sock_hash;
};

//! Reports the response data that the events or messages taken from a Queue were parsed from (issue #5820)
/** The SSE parser (SseAction) and the WebSocket decoder (WebSocketStreamFrameState) turn a stream's data into events
    or messages that the application takes from a Queue: the parser records the data that each item was made from,
    with the hook of the item's stream (record()), before it pushes the item, and the Queue reports the item's data
    to that stream when the application takes the item, or when the item is dropped with the Queue.  One hook serves
    every stream whose items are pushed to the same Queue (see QoreQueue::getItemConsumeHook()).

    measure() moves the entries of the items taken into a list of the calling thread, and consumed(), which the
    Queue calls on the same thread after its lock is released, reports them to their streams.  The item sizes are
    kept under a leaf mutex, which no other lock is taken under.
*/
class QueueItemConsumeHook : public QoreConsumeHook {
public:
    //! Records the data that an item was made from, for the given stream; called before the item is pushed
    DLLLOCAL void record(const void* item, const std::shared_ptr<QoreConsumeHook>& stream_hook, size_t bytes);

    //! Forgets an item that was not pushed
    DLLLOCAL void forget(const void* item);

    DLLLOCAL size_t measure(const QoreValue& v) const override;

    //! Reports the data of the items that measure() took on this thread to their streams
    DLLLOCAL void consumed(size_t bytes) override;

private:
    mutable std::mutex m;
    mutable std::unordered_map<const void*, std::pair<std::shared_ptr<QoreConsumeHook>, size_t>> sizes;
};

#endif
