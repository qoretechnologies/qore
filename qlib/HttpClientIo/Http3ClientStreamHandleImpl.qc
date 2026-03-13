# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file Http3ClientStreamHandleImpl.qc HTTP/3 client stream handle implementation

/** Http3ClientStreamHandleImpl.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Main namespace
public namespace HttpClientIo {

#! HTTP/3 client stream handle implementation
/** This class extends HttpClientStreamHandle for HTTP/3 streams.
    HTTP/3 does not support the streaming mode available in HTTP/2.

    @par Synchronous Usage
    @code{.py}
Http3ClientStreamHandle stream = cast<Http3ClientStreamHandle>(manager.acquireStream("https://example.com"));
hash<auto> response = stream.request("GET", "/api/users");
printf("Status: %d\n", response.status_code);
    @endcode
*/
public class Http3ClientStreamHandle inherits HttpClientStreamHandle {
    private {
        #! Typed reference to the H3 connection (for datagram methods)
        Http3ClientConnection h3conn;
    }

    #! Creates a new stream handle
    constructor(HttpClientConnection conn, timeout request_timeout = DefaultRequestTimeout,
            *Logger logger) : HttpClientStreamHandle(conn, request_timeout, logger) {
        h3conn = cast<Http3ClientConnection>(conn);
    }

    #! Copying is not supported
    private copy() {
        throw "COPY-ERROR", "Http3ClientStreamHandle objects cannot be copied";
    }

    #! Protocol-specific async request submission
    int submitAsyncImpl(string method, string path, *hash<auto> headers,
            *data body, code callback) {
        return conn.submitRequest(method, path, headers, body,
            sub (hash<auto> resp, *hash<auto> err) {
                try {
                    handleResponse(resp, err);
                } catch (hash<ExceptionInfo> ex) {
                    log(LoggerLevel::ERROR, "handleResponse error: %s: %s", ex.err, ex.desc);
                    if (!err) {
                        err = {"err": ex.err, "desc": ex.desc};
                    }
                }
                callback(resp, err);
            });
    }

    #! Protocol-specific async request + datagram submission for HTTP/3
    /** Queues both the HTTP/3 request (with suppress_wake) and a QUIC datagram
        before issuing a single wake, ensuring both are written in the same
        QUIC packet batch.

        @param method HTTP method
        @param path request path
        @param headers request headers (optional)
        @param body request body (optional)
        @param callback user callback to invoke after handleResponse()
        @param datagram the datagram payload to send

        @return the stream ID assigned to this request

        @see HttpClientStreamHandle::submitAsyncWithDatagram() for the public
        wrapper with state management
    */
    private int submitAsyncWithDatagramImpl(string method, string path, *hash<auto> headers,
            *data body, code callback, binary datagram) {
        # Submit the HTTP/3 request without waking the I/O controller
        int sid = conn.submitRequest(method, path, headers, body,
            sub (hash<auto> resp, *hash<auto> err) {
                try {
                    handleResponse(resp, err);
                } catch (hash<ExceptionInfo> ex) {
                    log(LoggerLevel::ERROR, "handleResponse error: %s: %s", ex.err, ex.desc);
                    if (!err) {
                        err = {"err": ex.err, "desc": ex.desc};
                    }
                }
                callback(resp, err);
            }, NOTHING, True);

        # Queue the datagram (both request and datagram are now pending)
        int qsid = h3conn.getSessionId();
        if (!qsid) {
            # Cancel the request we just submitted
            try { conn.cancelRequest(sid); } catch () {}
            throw "HTTPCLIENT-STATE-ERROR", "QUIC session not yet established";
        }
        try {
            h3conn.getSocket().submitQuicDatagram(qsid, sid, datagram);
        } catch (hash<ExceptionInfo> ex) {
            # Cancel the request we just submitted
            try { conn.cancelRequest(sid); } catch () {}
            rethrow;
        }

        # Single wake for both request and datagram
        conn.notifyPendingData();

        return sid;
    }

    #! Send a QUIC DATAGRAM frame on this stream's datagram channel (RFC 9221/9297)
    sendDatagram(binary data) {
        if (!exists stream_id) {
            throw "HTTPCLIENT-STATE-ERROR", "stream not yet established";
        }
        int sid = h3conn.getSessionId();
        if (!sid) {
            throw "HTTPCLIENT-STATE-ERROR", "QUIC session not yet established";
        }
        h3conn.getSocket().submitQuicDatagram(sid, stream_id, data);
        # Wake the I/O controller so the datagram is written promptly
        conn.notifyPendingData();
    }

    #! Read the next incoming QUIC datagram on this stream's channel (RFC 9221/9297)
    *binary readDatagram(timeout timeout_ms = 30s) {
        if (!exists stream_id) {
            throw "HTTPCLIENT-STATE-ERROR", "stream not yet established";
        }
        int sid = h3conn.getSessionId();
        if (!sid) {
            throw "HTTPCLIENT-STATE-ERROR", "QUIC session not yet established";
        }
        return h3conn.getSocket().readQuicDatagram(sid, stream_id, timeout_ms);
    }

    #! Check if the remote peer supports QUIC datagrams on this connection
    bool isDatagramSupported() {
        int sid = h3conn.getSessionId();
        if (!sid) {
            return False;
        }
        return h3conn.getSocket().isQuicDatagramSupported(sid);
    }

    #! Get the maximum datagram payload size for this connection
    int getMaxDatagramSize() {
        if (!exists stream_id) {
            return 0;
        }
        int sid = h3conn.getSessionId();
        if (!sid) {
            return 0;
        }
        return h3conn.getSocket().getQuicMaxDatagramSize(sid, stream_id);
    }
}

} # namespace HttpClientIo
