# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file Http1ClientStreamHandleImpl.qc HTTP/1.1 client stream handle implementation

/** Http1ClientStreamHandleImpl.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! HTTP/1.1 client stream handle
/** This class extends HttpClientStreamHandle for HTTP/1.1 connections.

    HTTP/1.1 is a serial protocol — each stream handle represents a single
    request/response exchange on the connection. Unlike HTTP/2, there is no
    multiplexed streaming support.

    @par Usage
    @code{.py}
Http1ClientStreamHandle stream = cast<Http1ClientStreamHandle>(manager.acquireStream("http://example.com"));
hash<auto> response = stream.request("GET", "/api/data");
printf("Status: %d\n", response.status_code);
    @endcode
*/
public class Http1ClientStreamHandle inherits HttpClientStreamHandle {
    #! Creates a new stream handle
    constructor(Http1ClientConnection conn, timeout request_timeout = DefaultRequestTimeout,
            *LoggerInterface logger) : HttpClientStreamHandle(conn, request_timeout, logger) {
    }

    #! Copying is not supported
    private copy() {
        throw "COPY-ERROR", "Http1ClientStreamHandle objects cannot be copied";
    }

    #! Protocol-specific async request submission
    /** @note The implementation calls handleResponse() to resolve the
        Promise used by the synchronous request() API.
    */
    int submitAsyncImpl(string method, string path, *hash<auto> headers,
            *data body, HttpResponseCallback callback) {
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
}

} # namespace HttpClientIo
