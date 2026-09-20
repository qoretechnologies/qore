/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    HttpClientEventSink.h

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

#ifndef _QORE_INTERN_HTTPCLIENTEVENTSINK_H

#define _QORE_INTERN_HTTPCLIENTEVENTSINK_H

#include <qore/Qore.h>
#include <qore/QoreEvents.h>
#include <qore/QoreQueue.h>

#include <cerrno>
#include <cstdlib>

//! Delivers %HTTP client protocol events to the event queue configured on an HTTPClient object
/** The connection manager performs its I/O on pooled connections whose sockets are shared between
    requests and between client objects, so the socket-level event queue of the client object cannot
    be used to report what happens on the wire for one request: a shared connection would mix the
    events of concurrent requests, and the caller's queue would outlive the request that configured it.

    An instance of this class is owned by an HTTPClient object and carries the queue configuration that
    @ref QoreHttpClientObject::setEventQueue() installs.  A request passes a reference to its sink down
    the submit path (see @c HttpClientConnectionManagerBase::request()), where it is attached to the
    per-request completion action, and the protocol layers (H1, H2, H3) report events through the sink
    of the request that they belong to.

    The queue configuration is read under @ref m at each event, so clearing the event queue stops
    delivery for requests that are already in flight, exactly as it does for socket-level events.

    Events are reported from the I/O thread, so the queue must have no maximum size (the
    @c HTTPClient::setEventQueue() method enforces this) — @ref QoreQueue::pushAndTakeRef() never
    blocks and never runs Qore code.

    The event hashes have the same layout as the socket-level events, including the @c "id" key, which
    identifies the client object's own socket, so that events reported here and events reported by the
    client object itself can be correlated by the receiver.

    @since %Qore 3.0
*/
class HttpClientEventSink : public QoreReferenceCounter {
public:
    //! Creates the sink with the identity reported in the @c "id" key of each event
    DLLLOCAL explicit HttpClientEventSink(int64 id) : id(id) {
    }

    DLLLOCAL void ref() {
        ROreference();
    }

    DLLLOCAL void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            clear(xsink);
            delete this;
        }
    }

    //! Sets the event queue configuration; the queue and argument references are taken
    DLLLOCAL void set(ExceptionSink* xsink, Queue* q, QoreValue new_arg, bool new_with_data) {
        Queue* old_queue;
        QoreValue old_arg;
        {
            AutoLocker al(m);
            old_queue = queue;
            old_arg = arg;
            queue = q;
            arg = new_arg;
            with_data = new_with_data;
        }
        // release the old references outside the lock; a Queue deref can run a destructor
        if (old_arg) {
            old_arg.discard(xsink);
        }
        if (old_queue) {
            old_queue->deref(xsink);
        }
    }

    //! Clears the event queue configuration
    DLLLOCAL void clear(ExceptionSink* xsink) {
        set(xsink, nullptr, QoreValue(), false);
    }

    //! Sets the identity reported in the @c "id" key of each event
    /** Called when the client object's socket is replaced, so that events keep reporting the identity
        of the socket that the client object currently owns.
    */
    DLLLOCAL void setId(int64 new_id) {
        AutoLocker al(m);
        id = new_id;
    }

    //! Returns @ref True if an event queue is configured
    DLLLOCAL bool active() const {
        AutoLocker al(m);
        return (bool)queue;
    }

    //! Returns @ref True if an event queue is configured and data events are enabled
    DLLLOCAL bool dataEnabled() const {
        AutoLocker al(m);
        return queue && with_data;
    }

    //! Reports @ref QORE_EVENT_HTTP_SEND_MESSAGE before a request message is sent
    /** @param msg the request line of the message (ex: @c "GET / HTTP/1.1")
        @param headers the headers of the request
    */
    DLLLOCAL void sendMessage(const QoreString& msg, const QoreHashNode* headers) {
        AutoLocker al(m);
        if (!queue) {
            return;
        }
        ReferenceHolder<QoreHashNode> h(getEventIntern(QORE_EVENT_HTTP_SEND_MESSAGE), nullptr);
        h->setKeyValue("message", new QoreStringNode(msg), nullptr);
        if (headers) {
            h->setKeyValue("headers", headers->copy(), nullptr);
        }
        queue->pushAndTakeRef(h.release());
    }

    //! Reports the response header events: @ref QORE_EVENT_HTTP_MESSAGE_RECEIVED and, if the response
    //! declares one, @ref QORE_EVENT_HTTP_CONTENT_LENGTH
    /** @param hdr the response header hash as delivered to the caller: header names in lower case,
        with the status line reported in the @c "status_code", @c "status_message", and
        @c "http_version" keys
    */
    DLLLOCAL void responseHeaders(const QoreHashNode& hdr) {
        AutoLocker al(m);
        if (!queue) {
            return;
        }
        {
            ReferenceHolder<QoreHashNode> h(getEventIntern(QORE_EVENT_HTTP_MESSAGE_RECEIVED), nullptr);
            h->setKeyValue("headers", hdr.hashRefSelf(), nullptr);
            queue->pushAndTakeRef(h.release());
        }
        int64 len;
        if (!getContentLength(hdr, len)) {
            ReferenceHolder<QoreHashNode> h(getEventIntern(QORE_EVENT_HTTP_CONTENT_LENGTH), nullptr);
            h->setKeyValue("len", len, nullptr);
            queue->pushAndTakeRef(h.release());
        }
    }

    //! Reports @ref QORE_EVENT_HTTP_CHUNK_SIZE when the size of the next chunk of a chunked response is known
    /** @param size the size of the chunk in bytes
        @param total_read the number of bytes read for the chunk size line
    */
    DLLLOCAL void chunkSize(size_t size, size_t total_read) {
        AutoLocker al(m);
        if (!queue) {
            return;
        }
        ReferenceHolder<QoreHashNode> h(getEventIntern(QORE_EVENT_HTTP_CHUNK_SIZE), nullptr);
        h->setKeyValue("size", (int64)size, nullptr);
        h->setKeyValue("total_read", (int64)total_read, nullptr);
        queue->pushAndTakeRef(h.release());
    }

    //! Reports @ref QORE_EVENT_HTTP_CHUNKED_DATA_RECEIVED after a chunk of a chunked response is read
    /** @param bytes the size of the chunk in bytes
        @param total_read the number of bytes read for the chunk including its terminating CRLF
    */
    DLLLOCAL void chunkedDataReceived(size_t bytes, size_t total_read) {
        AutoLocker al(m);
        if (!queue) {
            return;
        }
        ReferenceHolder<QoreHashNode> h(getEventIntern(QORE_EVENT_HTTP_CHUNKED_DATA_RECEIVED), nullptr);
        h->setKeyValue("read", (int64)bytes, nullptr);
        h->setKeyValue("total_read", (int64)total_read, nullptr);
        queue->pushAndTakeRef(h.release());
    }

    //! Reports @ref QORE_EVENT_HTTP_CHUNKED_DATA_READ with the chunk data if data events are enabled
    DLLLOCAL void chunkedDataRead(const void* data, size_t size) {
        AutoLocker al(m);
        if (!queue || !with_data || !size) {
            return;
        }
        ReferenceHolder<QoreHashNode> h(getEventIntern(QORE_EVENT_HTTP_CHUNKED_DATA_READ), nullptr);
        SimpleRefHolder<BinaryNode> b(new BinaryNode);
        b->append(data, size);
        h->setKeyValue("data", b.release(), nullptr);
        queue->pushAndTakeRef(h.release());
    }

    //! Returns the content length declared by a response header hash
    /** @param hdr the response header hash with header names in lower case
        @param len the content length, if the response declares one

        @return 0 if the response declares a content length, -1 if not
    */
    DLLLOCAL static int getContentLength(const QoreHashNode& hdr, int64& len) {
        QoreValue v = hdr.getKeyValue("content-length");
        // a response that repeats the header is delivered as a list; the first value is the one that the
        // response body was read with
        if (v.getType() == NT_LIST) {
            const QoreListNode* l = v.get<const QoreListNode>();
            if (l->empty()) {
                return -1;
            }
            v = l->retrieveEntry(0);
        }
        if (v.getType() == NT_STRING) {
            const QoreStringNode* str = v.get<const QoreStringNode>();
            char* end;
            errno = 0;
            long long rv = strtoll(str->c_str(), &end, 10);
            if (errno || end == str->c_str()) {
                return -1;
            }
            len = rv;
            return 0;
        }
        if (v.isNullOrNothing()) {
            return -1;
        }
        len = v.getAsBigInt();
        return 0;
    }

private:
    mutable QoreThreadLock m;

    //! The event queue (ref'd) or nullptr if no queue is configured
    Queue* queue = nullptr;

    //! The argument reported in the @c "arg" key of each event
    QoreValue arg;

    //! True if data events are enabled
    bool with_data = false;

    //! The identity reported in the @c "id" key of each event
    int64 id;

    //! Returns a new event hash with the common keys set; @ref m must be held
    DLLLOCAL QoreHashNode* getEventIntern(int event) const {
        QoreHashNode* h = new QoreHashNode(autoTypeInfo);
        if (arg) {
            h->setKeyValue("arg", arg.refSelf(), nullptr);
        }
        h->setKeyValue("event", event, nullptr);
        h->setKeyValue("source", QORE_SOURCE_HTTPCLIENT, nullptr);
        h->setKeyValue("id", id, nullptr);
        return h;
    }
};

#endif // _QORE_INTERN_HTTPCLIENTEVENTSINK_H
