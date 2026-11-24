/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    qore_socket_object_private.h

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.

    Note that the Qore library is released under a choice of three open-source
    licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
    information.
*/

#ifndef _QORE_INTERN_QORESOCKETOBJECTPRIVATE_H
#define _QORE_INTERN_QORESOCKETOBJECTPRIVATE_H

#include <qore/QoreSocketObject.h>

#include <utility>
#include "qore/intern/QC_Socket.h"

// an in-memory set of trusted CA certificates (include/qore/intern/QC_SSLTrustStore.h)
class QoreSSLTrustStore;

//! The library-internal operations of QoreSocketObject (qore#4380)
/** These functions were non-inline DLLLOCAL members of the public QoreSocketObject class: they are not part of the
    module API (they were never exported), so they live here, in the implementation of the library.  Construct the
    object from the socket object and call the operation:

    @code
    qore_socket_object_private(sock).isHttp2(xsink);
    @endcode

    The object holds only the pointer to the socket object and a reference to its private data, and it is a friend of
    QoreSocketObject.
*/
class qore_socket_object_private {
public:
    //! the socket object
    QoreSocketObject* so;
    //! its private data
    my_socket_priv*& priv;

    DLLLOCAL explicit qore_socket_object_private(QoreSocketObject* n_so) : so(n_so), priv(n_so->priv) {
    }

    //! for a holder of the socket object (ReferenceHolder, SimpleRefHolder, ...)
    template <typename H, typename = decltype(std::declval<H&>().operator->())>
    DLLLOCAL explicit qore_socket_object_private(H& holder) : qore_socket_object_private(holder.operator->()) {
    }

    //! for the operations that are const members of the socket object
    DLLLOCAL explicit qore_socket_object_private(const QoreSocketObject* n_so)
            : so(const_cast<QoreSocketObject*>(n_so)), priv(const_cast<QoreSocketObject*>(n_so)->priv) {
    }

    //! Returns the bytes of the socket's send buffer that the peer has not acknowledged, or 0 if unknown
    DLLLOCAL int64 getUnacknowledgedSendBytes() const;

    //! Shuts down the sending side of the connection only (a half-close); see Socket::shutdownWrite()
    /** @return 0 for success, -1 for error (exception raised)
    */
    DLLLOCAL int shutdownWrite(ExceptionSink* xsink);

    DLLLOCAL QoreStringNode* getSSLCipherNameString(ExceptionSink* xsink);

    DLLLOCAL QoreStringNode* getSSLCipherVersionString(ExceptionSink* xsink);

    DLLLOCAL bool isSecure(ExceptionSink* xsink);

    DLLLOCAL bool isSecureForAsyncPoll() const;

    //! Internal async-poll helper for idle-data probes already running on the I/O thread.
    /** This is the nonblocking implementation used by async poll operations.
        This method acquires the socket lock before probing.
        Public synchronous callers must use @ref checkIdleData(), which delegates
        through the async I/O controller.

        @since %Qore 3.0
    */
    DLLLOCAL int checkIdleDataForAsyncPoll(ExceptionSink* xsink, bool buffered_is_data = false);

    //! Internal async-poll helper for idle-data probes when the socket lock is already held.
    /** This is the lock-held variant of @ref checkIdleDataForAsyncPoll().

        @param xsink exception sink; set on SSL error
        @param buffered_is_data if true, bytes that the socket has already read from the descriptor count as data
        (a server whose client sent its next request before the response to the previous one, pipelining, or an HTTP
        client that has bytes after the end of a response, which keep the connection from being reused)

        @return > 0  application-layer data is available
        @return   0  no application data; any TLS post-handshake record was drained
        @return  -1  connection closed or error (socket already closed or xsink set)

        @since %Qore 3.0
    */
    DLLLOCAL int checkIdleDataForAsyncPollLocked(ExceptionSink* xsink, bool buffered_is_data = false);

    //! Returns the number of bytes that the socket has read from its descriptor and not yet returned to a reader
    /** Such bytes are returned before any other by the next read, also after a TLS connection is started on the
        socket, which never reads them: an HTTP client checks that none follow a proxy's response to \c CONNECT
        before it starts the TLS connection to the target server through the tunnel (qore#7486).  Acquires the
        socket lock.

        @since %Qore 3.0
    */
    DLLLOCAL size_t getBufferedReadBytes() const;

    //! Error-reporting variant of @ref setNoDelay() for poll operation code.
    /** @ref setNoDelay() is already safe on async I/O execution paths -- libqore selects the
        inline (non-controller) route for socket config actions itself -- so this helper differs
        only in error reporting: it raises failures on @a xsink instead of flattening them to -1.

        @param nodelay the TCP_NODELAY value to set
        @param xsink exception sink
        @return 0 on success, -1 on error

        @since %Qore 3.0
    */
    DLLLOCAL int setNoDelayForAsyncPoll(int nodelay, ExceptionSink* xsink);

    //! Error-reporting variant of @ref setUserTimeout() for poll operation code.
    /** @ref setUserTimeout() is already safe on async I/O execution paths -- libqore selects the
        inline (non-controller) route for socket config actions itself -- so this helper differs
        only in error reporting: it raises failures on @a xsink instead of flattening them to -1.

        @param ms the TCP_USER_TIMEOUT value in milliseconds
        @param xsink exception sink
        @return 0 on success, -1 on error

        @since %Qore 3.0
    */
    DLLLOCAL int setUserTimeoutForAsyncPoll(int ms, ExceptionSink* xsink);

    DLLLOCAL void setAlpnProtocolsForAsyncPoll(const QoreListNode* protocols, ExceptionSink* xsink);

    //! Sets the CA locations from the async I/O thread; see setSslCaLocations()
    DLLLOCAL void setSslCaLocationsForAsyncPoll(const char* ca_file, const char* ca_path, ExceptionSink* xsink);

    //! Sets or clears the in-memory trust store that peer certificates are verified against (qoretechnologies/qore#7248)
    /** On the async I/O thread it is set directly, otherwise with a TLS state poll operation, as
        QoreSocketObject::setSslCaLocations() sets the CA locations
    */
    DLLLOCAL void setSslTrustStore(QoreSSLTrustStore* ts, ExceptionSink* xsink);

    //! Sets the in-memory trust store from the async I/O thread; see setSslTrustStore()
    DLLLOCAL void setSslTrustStoreForAsyncPoll(QoreSSLTrustStore* ts, ExceptionSink* xsink);

    DLLLOCAL QoreStringNode* getAlpnProtocol(ExceptionSink* xsink) const;

    DLLLOCAL QoreStringNode* getAlpnProtocolForAsyncPoll() const;

    DLLLOCAL bool isHttp2(ExceptionSink* xsink) const;

    //! Internal HTTP/2 client helper: enqueue request without waking the controller.
    /** The caller must wake the owning poll operation after registering the
        stream action that will receive the response.

        @since %Qore 3.0
    */
    DLLLOCAL int32_t submitHttp2RequestNoWake(const QoreHashNode* headers, const void* body,
        size_t body_len, ExceptionSink* xsink, bool streaming = false);

    //! Mark an HTTP/2 stream for incremental response body delivery
    /** Internal direct variant for callers that already serialized request submission on the
        async I/O thread; mutates only lock-protected Http2Session bookkeeping (no nghttp2 calls),
        so it is safe to call from any thread.
    */
    DLLLOCAL void setHttp2StreamStreamingDirect(int32_t stream_id);

    //! Records the response headers of the HTTP/2 client session for event reporting
    /** @see Http2Session::setRecordResponseHeaderEvents()

        @since %Qore 3.0
    */
    DLLLOCAL void setHttp2RecordResponseHeaderEvents(bool v);

    //! Records the response headers of the HTTP/3 client session for event reporting
    /** @see QuicSession::setRecordResponseHeaderEvents()

        @since %Qore 3.0
    */
    DLLLOCAL void setQuicRecordResponseHeaderEvents(bool v);

    //! Sends the application's WebSocket close frame on an HTTP/2 stream, unless one was sent on it before
    /** See Http2Session::sendStreamCloseFrame(); otherwise as sendHttp2StreamData()

        @return 0 if the frame was sent, 1 if a close frame was sent on the stream before (only the end of the
        stream is sent, if requested), -1 on error (exception raised)

        @since %Qore 3.0
    */
    DLLLOCAL int sendHttp2StreamWebSocketClose(int32_t stream_id, const BinaryNode* data, bool end_stream,
            ExceptionSink* xsink);

    //! Internal async-poll helper for HTTP/2 stream DATA reads.
    /** Public synchronous callers must use @ref readHttp2StreamData(), which
        delegates through the async I/O controller. Poll operations use this
        helper so I/O-thread stream dispatch can drain the already-owned HTTP/2
        session buffer directly.

        @since %Qore 3.0
    */
    DLLLOCAL BinaryNode* readHttp2StreamDataForAsyncPoll(int32_t stream_id, size_t max_bytes,
            ExceptionSink* xsink);

    //! Internal async-poll helper for HTTP/2 pending data flushes.
    /** Public synchronous callers must use @ref flushHttp2PendingData(), which
        delegates through the async I/O controller.  Poll operations use this
        helper so I/O-thread execution can flush the session directly.

        @param xsink exception sink
        @return 0 on success, @ref SOCK_POLLIN or @ref SOCK_POLLOUT if another
        readiness event is needed, -1 on error

        @since %Qore 3.0
    */
    DLLLOCAL int flushHttp2PendingDataForAsyncPoll(ExceptionSink* xsink);

    //! Internal async-poll helper for HTTP/2 stream DATA enqueue.
    /** Public synchronous callers must use @ref sendHttp2StreamData(), which
        delegates through the async I/O controller.  Poll operations use this
        helper so protocol callbacks can enqueue DATA directly on the locked HTTP/2 session, also when an
        application thread installs a WebSocket decoder and feeds buffered frames. It never waits for an I/O
        command; off the I/O thread it wakes the controller to flush the queued bytes.

        @param stream_id the HTTP/2 stream ID
        @param data bytes to enqueue, or nullptr for no body data
        @param end_stream whether this ends the stream
        @param xsink receives session, flow-control or controller errors
        @return 0 on success, -1 on error

        @since %Qore 3.0
    */
    DLLLOCAL int sendHttp2StreamDataForAsyncPoll(int32_t stream_id, const BinaryNode* data,
            bool end_stream, ExceptionSink* xsink);

    //! Sends a WebSocket control frame (a pong or a close echo) of a stream's frame state on an HTTP/2 stream
    /** A close frame and a pong are not refused for a full send buffer; see Http2Session::sendStreamControlFrame()
        (issue #5900).  Safe on the I/O thread, as sendHttp2StreamDataForAsyncPoll().

        @return 0 if the frame was buffered or kept, 1 if the frame was refused for a full buffer (a frame that is
        not a pong or a close, or a stream's second close frame), -1 on error (exception raised)

        @since %Qore 3.0
    */
    DLLLOCAL int sendHttp2StreamControlFrameForAsyncPoll(int32_t stream_id, const BinaryNode* data,
            ExceptionSink* xsink);

    //! Internal variant of resetHttp2StreamAsync() with an explicit HTTP/2 error code
    DLLLOCAL int resetHttp2StreamAsync(int32_t stream_id, uint32_t error_code, ExceptionSink* xsink);

    //! Internal async-poll helper for HTTP/2 stream closed checks.
    DLLLOCAL bool isHttp2StreamClosedForAsyncPoll(int32_t stream_id) const;

    //! Internal async-poll helper for HTTP/2 remote stream closed checks.
    DLLLOCAL bool isHttp2StreamRemoteClosedForAsyncPoll(int32_t stream_id) const;

    //! Internal async-poll helper: True if the request body of an HTTP/2 server stream exceeded the maximum size
    DLLLOCAL bool isHttp2StreamBodyTooLargeForAsyncPoll(int32_t stream_id) const;

    //! Internal async-poll helper: True if the peer ended an HTTP/2 stream with END_STREAM
    DLLLOCAL bool isHttp2StreamEndStreamReceivedForAsyncPoll(int32_t stream_id) const;

    //! True if this endpoint has not ended its side of an HTTP/2 stream (see Http2Session::isStreamLocalOpen())
    DLLLOCAL bool isHttp2StreamLocalOpen(int32_t stream_id) const;

    //! Returns true if the HTTP/2 stream is known and was reset (issue #5859)
    DLLLOCAL bool isHttp2StreamResetForAsyncPoll(int32_t stream_id) const;

    //! Sets the host name of the next TLS connection made on the socket (async-poll helper)
    /** The name is sent with SNI and the server certificate is verified against it; by default it is the host
        that the socket connected to.  A TLS connection to a server through a proxy tunnel is made for the server,
        not for the proxy that the socket connected to.

        @since %Qore 3.0
    */
    DLLLOCAL void setSslTargetHostForAsyncPoll(const char* host);

    //! Returns where the socket's current connection leads, or a null pointer if no asynchronous connect recorded it
    /** Can be called from any thread; see QoreConnectionDestinations

        @since %Qore 3.0
    */
    DLLLOCAL std::shared_ptr<const QoreConnectionDestinations> getConnectedDestinations() const;

    //! Returns the trailers received for an HTTP/2 stream, or nullptr if none were received
    /** Can be called from any thread

        @since %Qore 3.0
    */
    DLLLOCAL QoreHashNode* getHttp2StreamTrailers(int32_t stream_id) const;

    //! Drains stream IDs reset by the peer since the last call (I/O thread only)
    /** Used by the HTTP/2 server poll operation to surface peer RST_STREAMs for
        persistent-session teardown.  Returns an empty vector if there is no
        HTTP/2 session.
    */
    DLLLOCAL std::vector<int32_t> takeHttp2PeerResetReportsForAsyncPoll();

    //! Releases the records that the HTTP/2 session of the socket keeps for a stream that the caller is done with
    /** See Http2Session::releaseStream(); a no-op if the socket has no HTTP/2 session.  Not for an I/O thread that
        holds the lock of the socket.
    */
    DLLLOCAL void releaseHttp2Stream(int32_t stream_id);

    //! Marks an HTTP/2 server stream whose request body is delivered to a Queue that the handler registers
    /** See Http2Session::setStreamBodyConsumer(); a no-op if the socket has no HTTP/2 session.  Not for an I/O thread
        that holds the lock of the socket.
    */
    DLLLOCAL void setHttp2StreamBodyConsumer(int32_t stream_id);

    //! Releases the body consumer of an HTTP/2 server stream once its Queue has received the end of the body
    /** See Http2Session::releaseStreamBodyConsumer(); a no-op if the socket has no HTTP/2 session.  Not for an I/O
        thread that holds the lock of the socket.
    */
    DLLLOCAL void releaseHttp2StreamBodyConsumer(int32_t stream_id);

    //! Releases the records that a QUIC session of the socket keeps for a stream that the caller is done with
    /** See QuicSession::releaseStream(); a no-op if the socket has no such session.  Not for an I/O thread that
        holds the lock of the socket.
    */
    DLLLOCAL void releaseQuicStream(int64_t session_id, int64_t stream_id);

    //! True if the QUIC session still has the bidirectional stream open (see QuicSession::isBidiStreamOpen())
    DLLLOCAL bool isQuicBidiStreamOpen(int64_t session_id, int64_t stream_id) const;

    //! Watches the response on an HTTP/2 server stream so that the result of its transmission is reported
    /** Must be called before the response is submitted; see Http2Session::watchResponseSend().  The result is
        taken with takeHttp2ResponseSendResultsForAsyncPoll().

        @return 0 on success, -1 if an exception was raised
    */
    DLLLOCAL int watchHttp2ResponseSend(int32_t stream_id, ExceptionSink* xsink);

    //! Sends a GOAWAY on the socket's server HTTP/2 session before the server closes the connection (qore#5899)
    /** Does nothing if the socket has no server HTTP/2 session, or if a GOAWAY was sent already; the frame is written
        without blocking on the I/O thread of the socket, and a caller on another thread waits for that; a caller on
        an I/O thread or in a continuePoll worker only submits the frame, which the connection's poll operation writes

        @return 0 for success, -1 if an exception was raised
    */
    DLLLOCAL int sendHttp2ShutdownGoaway(ExceptionSink* xsink);

    //! Starts a graceful drain of the socket's server HTTP/2 session (qore#5899)
    /** Submits a GOAWAY like sendHttp2ShutdownGoaway(); the streams open then are served to their end, and the
        connection's poll operation closes the connection when none is left.  Does nothing if the socket has no
        server HTTP/2 session.  Run on the I/O thread of the socket like sendHttp2ShutdownGoaway()

        @return 0 for success, -1 if an exception was raised
    */
    DLLLOCAL int startHttp2Drain(ExceptionSink* xsink);

    //! Returns true if a drain of the socket's HTTP/2 session was started and it has no open stream and nothing to write
    DLLLOCAL bool isHttp2DrainComplete() const;

    //! Returns true if the server drains the socket's connection (see Socket::drainHttpConnection())
    DLLLOCAL bool isHttpDraining() const;

    //! Drains the transmission results of HTTP/2 responses watched with watchHttp2ResponseSend() (I/O thread only)
    /** @return (stream ID, sent) pairs; empty if there is no HTTP/2 session
    */
    DLLLOCAL std::vector<std::pair<int32_t, bool>> takeHttp2ResponseSendResultsForAsyncPoll();

    //! Watches the response on an HTTP/3 server stream so that the result of its transmission is reported
    /** Must be called before the response is submitted; see QuicSession::watchResponseSend().

        @return 0 on success, -1 if an exception was raised (for example when the QUIC session is gone)
    */
    DLLLOCAL int watchQuicResponseSend(int64_t session_id, int64_t stream_id, ExceptionSink* xsink);

    //! Returns true if the given QUIC session of the socket is closed or no longer exists
    /** Takes the socket's lock and its QUIC session lock; it does not wait for the I/O thread.

        @param session_id the QUIC session
    */
    DLLLOCAL bool isQuicSessionClosedOrGone(int64_t session_id) const;

    DLLLOCAL long verifyPeerCertificate(ExceptionSink* xsink);

    DLLLOCAL QoreObject* getRemoteCertificate(ExceptionSink* xsink) const;

    //! Sets the maximum size in bytes of an HTTP/2 or HTTP/3 response body received into memory (0 = unlimited)
    /** Can be called from any thread; client sessions on this socket read the value when response data arrives.
    */
    DLLLOCAL void setMaxResponseBodySize(int64 size);

    //! Returns the time in microseconds since data was last received on the socket, or -1 if no data was received
    /** Can be called from any thread; see qore_socket_private::last_data_recv_us

        @since %Qore 3.0
    */
    DLLLOCAL int64 getDataIdleUs() const;

    //! Sets the encoding assumed for text HTTP/2 response bodies whose encoding is not determined otherwise
    /** Can be called from any thread; client sessions on this socket read the value when a response is complete.

        @param enc the assumed encoding; nullptr means ISO-8859-1
    */
    DLLLOCAL void setHttpAssumedEncoding(const QoreEncoding* enc);

    //! Sets whether whole text HTTP/2 client response bodies are decoded to strings
    /** Can be called from any thread; client sessions on this socket read the value when a response is complete.

        @param decode if false, bodies are returned as received
    */
    DLLLOCAL void setHttpDecodeBody(bool decode);

    //! Internal no-wake variant used when callers must register stream state before polling resumes
    DLLLOCAL int64_t submitQuicRequestNoWake(const char* method, const char* path,
        const QoreHashNode* headers, const void* body, size_t body_len, ExceptionSink* xsink);

    //! Internal no-wake variant used when callers must register stream state before polling resumes
    DLLLOCAL int64_t submitQuicRequestStreamingNoWake(const char* method, const char* path,
        const QoreHashNode* headers, ExceptionSink* xsink);

    //! Internal async-poll helper for client-side HTTP/3 stream DATA enqueue.
    /** Public synchronous callers must use @ref sendQuicClientStreamData(), which
        delegates through the async I/O controller. Poll operations use this
        helper so protocol callbacks can enqueue DATA directly on the locked QUIC session, also when an
        application thread installs a WebSocket decoder and feeds buffered frames. It never waits for an I/O
        command; off the I/O thread it wakes the controller to flush the queued bytes.

        @param stream_id the stream ID returned by submitQuicRequestStreaming()
        @param data body data (may be nullptr with len 0 to send empty DATA + END_STREAM)
        @param len length of data
        @param end_stream true to signal the end of the request body
        @param xsink exception sink

        @return 0 on success, 1 if buffer full (backpressure), -1 on error

        @since %Qore 3.0
    */
    DLLLOCAL int sendQuicClientStreamDataForAsyncPoll(int64_t stream_id, const void* data, size_t len,
        bool end_stream, ExceptionSink* xsink);

    //! Sends a WebSocket control frame (a pong or a close echo) of a stream's frame state on an HTTP/3 client stream
    /** See QuicSession::sendStreamControlFrame() (issue #5900); safe on the I/O thread, as
        sendQuicClientStreamDataForAsyncPoll().

        @return 0 if the frame was staged or kept, 1 if the frame was refused (a frame that is not a pong or a close,
        or a stream's second close frame), -1 on error (exception raised)

        @since %Qore 3.0
    */
    DLLLOCAL int sendQuicClientStreamControlFrameForAsyncPoll(int64_t stream_id, const void* data, size_t len,
        ExceptionSink* xsink);

    //! Mark a client-side HTTP/3 stream for incremental response body delivery
    /** Internal direct variant for callers that already serialized request submission on the
        async I/O thread; mutates only lock-protected QuicSession bookkeeping (no ngtcp2 calls),
        so it is safe to call from any thread.

        @since %Qore 3.0
    */
    DLLLOCAL void setQuicClientStreamStreamingDirect(int64_t stream_id);

    //! Submit trailers on a client-side HTTP/3 streaming request
    /** Internal helper for HTTP/3 client connection code; delegates the
        QuicSession mutation to the async I/O controller.

        @since %Qore 3.0
    */
    DLLLOCAL int submitQuicClientTrailers(int64_t stream_id, const QoreHashNode* trailers,
        ExceptionSink* xsink);

    //! Internal async-aware HTTP/3 stream cancellation helper
    /** Non-I/O-thread callers delegate cancellation to the async I/O
        controller.  I/O-thread callers cancel directly on the already-owned
        session.  Missing sessions are ignored to keep cleanup paths
        idempotent.

        @since %Qore 3.0
    */
    DLLLOCAL int cancelQuicStreamAsync(int64_t session_id, int64_t stream_id, ExceptionSink* xsink);

    //! Sets the limit of the data of an HTTP/2 stream that is buffered for sending; see Http2Session::setStreamSendLimit()
    /** @return 0 on success, -1 on error (exception raised)

        @since %Qore 3.0
    */
    DLLLOCAL int setHttp2StreamSendLimit(int32_t stream_id, int64_t limit, ExceptionSink* xsink);

    //! Sets the limit of the data of an HTTP/3 streaming response that is staged for sending; see QuicSession::setStreamSendLimit()
    /** @return 0 on success, -1 on error (exception raised)

        @since %Qore 3.0
    */
    DLLLOCAL int setQuicStreamSendLimit(int64_t session_id, int64_t stream_id, int64_t limit, ExceptionSink* xsink);

    //! Has the stream credit of an HTTP/2 client stream's response data granted as the action's consumer takes it
    /** Installs a consume hook (see HttpStreamConsumeHook) on @p action if it is a streaming action that delivers
        the data to a consumer (ChannelAction) or discards it (StreamingHeadersPromiseNotifierAction,
        FrameStateAction), and then defers the stream's credit (issue #5820); with any other action, the stream's
        credit is granted as the data is received.  Called before the action is registered for the stream.

        @since %Qore 3.0
    */
    DLLLOCAL void installHttp2StreamConsumeHook(int32_t stream_id, AbstractAsyncAction* action);

    //! Defers the stream credit of an HTTP/2 client stream's data until it is taken from the stream (issue #5859)
    /** For a stream whose data is drained to a registered stream queue: the data that the queue has no room for
        stays in the stream without credit.

        @since %Qore 3.0
    */
    DLLLOCAL void setHttp2StreamCreditOnConsume(int32_t stream_id);

    //! Fails an HTTP/2 client stream with the error given: its response is the error (issue #5859)
    /** @return true if the stream was failed, false if it has ended or there is no HTTP/2 session

        @since %Qore 3.0
    */
    DLLLOCAL bool failHttp2Stream(int32_t stream_id, const char* err, const char* desc);

    //! Has the stream credit of an HTTP/3 client stream's response data granted as the action's consumer takes it
    /** See installHttp2StreamConsumeHook().

        @since %Qore 3.0
    */
    DLLLOCAL void installQuicStreamConsumeHook(int64_t session_id, int64_t stream_id, AbstractAsyncAction* action);

    //! Sends the application's WebSocket close frame on an HTTP/3 stream, unless one was sent on it before
    /** See QuicSession::sendStreamCloseFrame(); otherwise as submitQuicStreamData()

        @return 0 if the frame was staged, 1 if a close frame was staged on the stream before (only the end of the
        stream is sent, if requested), -1 on error (exception raised)

        @since %Qore 3.0
    */
    DLLLOCAL int submitQuicStreamWebSocketClose(int64_t session_id, int64_t stream_id, const void* data, size_t len,
        bool end_stream, ExceptionSink* xsink);

    //! Returns the trailers received for an HTTP/3 stream, or nullptr if none were received
    /** @since %Qore 3.0
    */
    DLLLOCAL QoreHashNode* getQuicStreamTrailers(int64_t session_id, int64_t stream_id, ExceptionSink* xsink) const;

    //! Internal async-poll helper for submitting and flushing an HTTP/2 PING.
    /** Public synchronous callers must use @ref submitHttp2Ping(), which
        delegates through the async I/O controller.  Poll operations use this
        helper so I/O-thread execution can submit and flush directly.

        @param xsink exception sink
        @return 0 on success, @ref SOCK_POLLIN or @ref SOCK_POLLOUT if another
        readiness event is needed, -1 on error

        @since %Qore 3.0
    */
    DLLLOCAL int submitHttp2PingForAsyncPoll(ExceptionSink* xsink);

#ifdef DEBUG
    //! Debug-only: arm a one-shot fd-swap simulation on the next controller wait.
    /** Called via the @c dbg_force_fd_swap_next_wait() debug builtin in
        @ref lib/ql_debug.cpp.  DLLLOCAL so the function never appears
        in the public ABI — debug-only test hook with no release-build
        surface.

        @since %Qore 3.0
    */
    DLLLOCAL void dbgForceFdSwapNextWait();

    //! Debug-only: closes the socket from another thread at the start of the next SSL I/O call on it
    /** Called via the @c dbg_close_socket_at_next_ssl_io() debug builtin in @ref lib/ql_debug.cpp: the close is
        made where an I/O call is about to use the TLS connection, as an abort or a close made by another thread
        can be.

        @since %Qore 3.0
    */
    DLLLOCAL void dbgCloseAtNextSslIo();

    //! Debug-only: makes the next close of the socket reset the connection at once
    /** Called via the @c dbg_reset_socket() debug builtin in @ref lib/ql_debug.cpp, so a test peer can reset a
        connection deterministically on every platform; see qore_socket_private::dbg_abortive_close.

        @since %Qore 3.0
    */
    DLLLOCAL void dbgResetOnClose();

    //! Debug-only: the next SSL write fails with SSL_ERROR_SSL while errno holds EPIPE
    DLLLOCAL void dbgForceSslErrorSslAtNextWrite();

    //! Debug-only: the next TLS write stages its ciphertext, as with a full socket buffer, and the drain of it fails as
    //! when the peer reset the connection
    DLLLOCAL void dbgForceStagedWriteFailure();

    //! Debug-only: returns the number of records that a session of the socket keeps for its streams, by kind
    /** Called via the @c dbg_socket_stream_records() debug builtin in @ref lib/ql_debug.cpp, so tests can show that
        a long-lived session does not keep records for the streams it has finished.

        @param session_id the ID of a QUIC session of the socket; 0 for the HTTP/2 session of the socket
        @param xsink raises \c DBG-ARGUMENT-ERROR if the socket has no such session

        @since %Qore 3.0
    */
    DLLLOCAL QoreHashNode* dbgGetStreamRecordCounts(int64_t session_id, ExceptionSink* xsink);
#endif
};

#endif // _QORE_INTERN_QORESOCKETOBJECTPRIVATE_H
