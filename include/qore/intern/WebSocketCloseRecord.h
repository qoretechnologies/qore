/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    WebSocketCloseRecord.h

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

#ifndef _QORE_INTERN_WEBSOCKETCLOSERECORD_H
#define _QORE_INTERN_WEBSOCKETCLOSERECORD_H

#include <qore/AbstractPrivateData.h>

#include <atomic>
#include <utility>

//! The record that a close frame was sent on the stream of a WebSocket connection over HTTP/2 or HTTP/3 (#6077)
/** A WebSocket connection has one close frame, after which no data frame is sent (RFC 6455 section 5.5.1).  The
    session records the close frame of a stream in this object when it buffers it (the frame state's own, or the
    application's); the stream context of the connection holds the object as a read-only
    @ref Qore::WebSocketCloseRecord "WebSocketCloseRecord" and reads it before it calls the transport, so the record
    decides after the stream or the session has ended, when the session's own records are gone.

    The flag is a single atomic: it is set with release and read with acquire semantics, without a lock and without
    re-entering the interpreter.
*/
class WebSocketCloseRecord : public AbstractPrivateData {
public:
    //! Records that a close frame was sent on the stream
    DLLLOCAL void setCloseSent() {
        close_sent.store(true, std::memory_order_release);
    }

    //! Returns true if a close frame was sent on the stream
    DLLLOCAL bool isCloseSent() const {
        return close_sent.load(std::memory_order_acquire);
    }

private:
    std::atomic<bool> close_sent{false};
};

//! A counted reference to a WebSocketCloseRecord
class WebSocketCloseRecordRef {
public:
    DLLLOCAL WebSocketCloseRecordRef() = default;

    //! Takes over the reference of a new record
    DLLLOCAL explicit WebSocketCloseRecordRef(WebSocketCloseRecord* r) : r(r) {
    }

    DLLLOCAL WebSocketCloseRecordRef(const WebSocketCloseRecordRef& old) : r(old.r) {
        if (r) {
            r->ref();
        }
    }

    DLLLOCAL WebSocketCloseRecordRef(WebSocketCloseRecordRef&& old) noexcept : r(old.r) {
        old.r = nullptr;
    }

    DLLLOCAL WebSocketCloseRecordRef& operator=(WebSocketCloseRecordRef old) noexcept {
        std::swap(r, old.r);
        return *this;
    }

    DLLLOCAL ~WebSocketCloseRecordRef() {
        if (r) {
            r->deref();
        }
    }

    //! Creates a new record
    DLLLOCAL static WebSocketCloseRecordRef create() {
        return WebSocketCloseRecordRef(new WebSocketCloseRecord);
    }

    DLLLOCAL WebSocketCloseRecord* get() const {
        return r;
    }

    DLLLOCAL WebSocketCloseRecord* operator->() const {
        return r;
    }

    DLLLOCAL explicit operator bool() const {
        return r != nullptr;
    }

    //! Returns a new reference to the record for a new owner, or nullptr
    DLLLOCAL WebSocketCloseRecord* refSelf() const {
        if (r) {
            r->ref();
        }
        return r;
    }

private:
    WebSocketCloseRecord* r = nullptr;
};

class QoreSocketObject;
class QoreClass;

//! the Qore class of WebSocketCloseRecord objects
DLLLOCAL extern QoreClass* QC_WEBSOCKETCLOSERECORD;

//! Returns the record of the close frame of a WebSocket connection on a server-side HTTP/2 stream of the socket
/** @param create true to create the record (when the stream's frame state is registered)

    @return the stream's record, from the socket's HTTP/2 session or, once the session has ended, from the records
    that the socket kept; an empty reference if there is none
*/
DLLLOCAL WebSocketCloseRecordRef qore_socket_object_get_h2_ws_close_record(QoreSocketObject* s, int32_t stream_id,
        bool create = false);

//! Returns the record of the close frame of a WebSocket connection on an HTTP/3 stream of a QUIC session
/** @return the stream's record, or an empty reference if the socket has no such QUIC session or the session has no
    streaming response on the stream
*/
DLLLOCAL WebSocketCloseRecordRef qore_socket_object_get_quic_ws_close_record(QoreSocketObject* s,
        int64_t session_id, int64_t stream_id);

#endif
