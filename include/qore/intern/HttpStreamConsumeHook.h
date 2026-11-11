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
    DLLLOCAL void consumed(size_t bytes) override {
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

private:
    std::weak_ptr<Session> session;
    StreamId stream_id;
    std::string sock_hash;
};

//! Reports the response data that the events or messages taken from a Queue were parsed from (issue #5820)
/** The SSE parser (SseAction) and the WebSocket decoder (WebSocketStreamFrameState) turn a stream's data into events
    or messages that the application takes from a Queue: the parser records the data that each item was made from
    (record()) before it pushes the item, and the Queue reports the item's data when the application takes it, or
    when it is dropped with the Queue; data that makes no item (comments, control frames) is credited at once
    (creditNow()).  The item sizes are kept under a leaf mutex, which no other lock is taken under.
*/
class QueueItemConsumeHook : public QoreConsumeHook {
public:
    //! @param target the hook of the stream that the data was received on
    DLLLOCAL explicit QueueItemConsumeHook(std::shared_ptr<QoreConsumeHook> target);

    //! Records the data that an item was made from; called before the item is pushed
    DLLLOCAL void record(const void* item, size_t bytes);

    //! Reports data that makes no item at once
    DLLLOCAL void creditNow(size_t bytes) {
        if (bytes) {
            target->consumed(bytes);
        }
    }

    DLLLOCAL size_t measure(const QoreValue& v) const override;

    DLLLOCAL void consumed(size_t bytes) override {
        target->consumed(bytes);
    }

private:
    std::shared_ptr<QoreConsumeHook> target;
    mutable std::mutex m;
    mutable std::unordered_map<const void*, size_t> sizes;
};

#endif
