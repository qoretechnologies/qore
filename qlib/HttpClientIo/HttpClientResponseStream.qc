# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file HttpClientResponseStream.qc a response body delivered as it is received

/** HttpClientResponseStream.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Options for HttpClientConnectionManager::openStream()
public hashdecl HttpClientOpenStreamOptions {
    #! the HTTP method
    string method = "GET";

    #! the request body, if any
    *data body;

    #! @ref True "True" to decompress a body with a content coding (\c Content-Encoding), @ref False "False" to
    #! deliver it as received
    bool decode = True;

    #! the maximum time to wait for the response headers; the \c request_timeout option of the manager if not set
    *timeout timeout;

    #! credentials for HTTP authentication; if not set, any user information in the URL
    /** @since HttpClientIo 1.0
    */
    *hash<HttpClientCredentials> credentials;
}

#! The range of a partial response (206), as given by its \c Content-Range header (RFC 9110 section 14.4)
public hashdecl HttpContentRangeInfo {
    #! the offset of the first byte of the body in the representation
    int first;

    #! the offset of the last byte of the body in the representation
    int last;

    #! the size of the complete representation, if known
    *int total;
}

#! A response whose body is delivered as it is received
/** Created with @ref HttpClientConnectionManager::openStream(), after any redirects have been followed; the
    response status and headers are available immediately, and the body is read in chunks with @ref read(), so a
    body of any size can be processed, for example written to a file, without being held in memory.

    A body with a content coding (\c Content-Encoding) is decompressed as it is read unless disabled with the
    \c decode option; @ref getReceivedSize() returns the size received from the server, which is the size of the
    compressed body in that case.

    @par Example:
    @code{.py}
HttpClientResponseStream stream = mgr.openStream("https://example.com/file.iso");
if (stream.getStatusCode() != 200) {
    throw "DOWNLOAD-ERROR", sprintf("HTTP %d", stream.getStatusCode());
}
FileOutputStream out("file.iso");
while (*binary chunk = stream.read(30s)) {
    out.write(chunk);
}
    @endcode

    @since HttpClientIo 1.0
*/
public class HttpClientResponseStream {
    private {
        #! the stream handle of the final response
        HttpClientStreamHandle sh;

        #! the response metadata
        hash<auto> response;

        #! the URL of the request that produced the response
        string effective_url;

        #! the redirects followed
        list<hash<HttpRedirectInfo>> redirects;

        #! body data received with the response headers
        list<binary> pending = ();

        #! True once the end of the body has been received
        bool ended = False;

        #! True once read() has returned the end of the body
        bool done = False;

        #! True if the stream was cancelled before the end of the body
        bool cancelled = False;

        #! the decompressor, if the body is decoded
        *HttpBodyDecompressor decoder;

        #! the number of body bytes received from the server
        int received = 0;

        #! protects ended and done, which cancel() can change from another thread
        Mutex m();
    }

    #! Creates the object; use HttpClientConnectionManager::openStream() to open a response stream
    /** @param sh the stream handle of the response
        @param response the response metadata
        @param effective_url the URL of the request that produced the response
        @param redirects the redirects followed
        @param pending body data received with the response headers
        @param ended True if the end of the body has been received
        @param decode True to decompress a body with a content coding
    */
    constructor(HttpClientStreamHandle sh, hash<auto> response, string effective_url,
            list<hash<HttpRedirectInfo>> redirects, list<binary> pending, bool ended, bool decode) {
        self.sh = sh;
        self.response = response;
        self.effective_url = effective_url;
        self.redirects = redirects;
        self.pending = pending;
        self.ended = ended;
        if (decode) {
            *string ce = getHeaderValue("content-encoding");
            if (ce.val() && ce.lwr() != "identity") {
                decoder = new HttpBodyDecompressor(ce);
            }
        }
        foreach binary chunk in (pending) {
            received += chunk.size();
        }
    }

    #! Cancels a stream whose body has not been read completely
    destructor() {
        cancel();
    }

    #! Returns the HTTP status code of the response
    int getStatusCode() {
        return response.status_code;
    }

    #! Returns the status message of the response, if any
    *string getStatusMessage() {
        return response.status_message;
    }

    #! Returns the response headers, with lower-case names
    hash<auto> getHeaders() {
        return response.headers ?? {};
    }

    #! Returns the value of the given response header, if present
    /** @param name the header name, in any case

        @return the value of the header, or the first value if the header was received more than once
    */
    *string getHeaderValue(string name) {
        auto v = response.headers{name.lwr()};
        if (v.typeCode() == NT_LIST) {
            return v[0];
        }
        return v;
    }

    #! Returns the protocol of the connection that the response was received on: \c "h1", \c "h2", or \c "h3"
    *string getProtocol() {
        return response.protocol;
    }

    #! Returns the URL of the request that produced the response, after any redirects
    string getEffectiveUrl() {
        return effective_url;
    }

    #! Returns the redirects followed to get the response, in the order followed
    list<hash<HttpRedirectInfo>> getRedirects() {
        return redirects;
    }

    #! Returns the value of the \c Content-Length header, if present and valid
    /** @return the size of the body as sent by the server; for a body with a content coding, this is the size of
        the encoded body
    */
    *int getContentLength() {
        *string cl = getHeaderValue("content-length");
        if (cl =~ /^\s*[0-9]+\s*$/) {
            return cl.toInt();
        }
    }

    #! Returns the range of a partial response (status code 206)
    /** @return the range given by the \c Content-Range header, or @ref NOTHING if the header is missing or does
        not give a valid byte range
    */
    *hash<HttpContentRangeInfo> getContentRange() {
        *string cr = getHeaderValue("content-range");
        *list<*string> m = (cr =~ x/^\s*bytes\s+([0-9]+)-([0-9]+)\/([0-9]+|\*)\s*$/i);
        if (!m) {
            return;
        }
        hash<HttpContentRangeInfo> rv(<HttpContentRangeInfo>{"first": m[0].toInt(), "last": m[1].toInt()});
        if (m[2] != "*") {
            rv.total = m[2].toInt();
        }
        if (rv.last < rv.first || (exists rv.total && rv.last >= rv.total)) {
            return;
        }
        return rv;
    }

    #! Returns @ref True "True" if the body is decompressed as it is read
    bool isDecoded() {
        return exists decoder;
    }

    #! Returns the number of body bytes received from the server so far
    /** For a body that is decompressed, this is the size of the compressed data received
    */
    int getReceivedSize() {
        return received;
    }

    #! Returns the next chunk of the body
    /** @param timeout_ms the maximum time to wait for data; waits without a limit if not set

        @return the next chunk of the body, which is never empty, or @ref NOTHING at the end of the body

        @throw HTTPCLIENT-TIMEOUT no data was received within the timeout; the stream can still be read
        @throw HTTPCLIENT-RECEIVE-ERROR the body could not be received completely
        @throw HTTPCLIENT-CANCELLED the stream was cancelled with @ref cancel() before the end of the body
        @throw DECOMPRESSION-ERROR or a codec-specific error if the body cannot be decompressed or was truncated
    */
    *binary read(*timeout timeout_ms) {
        # a decompressor can need more data before it produces any, and metadata events carry no data
        while (True) {
            *binary chunk = readIntern(timeout_ms);
            if (!exists chunk || chunk.size()) {
                return chunk;
            }
        }
    }

    #! Stops receiving the body; the rest of the body is discarded
    /** Can be called from any thread, for example to interrupt a thread blocked in @ref read(); unless the end of
        the body was received already, @ref read() then raises \c HTTPCLIENT-CANCELLED, so a cancelled body is never
        taken for a complete one
    */
    cancel() {
        {
            AutoLock al(m);
            if (ended) {
                return;
            }
            ended = True;
            done = True;
            cancelled = True;
        }
        sh.cancel();
    }

    #! Returns the next piece of the body, which can be empty, or NOTHING at the end of the body
    private *binary readIntern(*timeout timeout_ms) {
        {
            AutoLock al(m);
            if (done) {
                checkCancelledIntern();
                return;
            }
            if (pending) {
                return decodeChunk(shift pending);
            }
            if (ended) {
                return finishIntern();
            }
        }
        auto d = sh.readData(timeout_ms);
        {
            AutoLock al(m);
            checkCancelledIntern();
        }
        if (!exists d) {
            throw "HTTPCLIENT-TIMEOUT", sprintf("no response data received from %y within %d ms", effective_url,
                timeout_ms);
        }
        if (d.typeCode() == NT_HASH) {
            if (d.error) {
                {
                    AutoLock al(m);
                    ended = True;
                    done = True;
                }
                throw "HTTPCLIENT-RECEIVE-ERROR", sprintf("error receiving the response body from %y: %s: %s",
                    effective_url, d.error.err ?? "ERROR", d.error.desc ?? "unknown error"), d.error;
            }
            if (d.end_stream) {
                AutoLock al(m);
                ended = True;
                return finishIntern();
            }
            # a metadata event carries no body data
            return binary();
        }
        binary chunk = d.typeCode() == NT_BINARY ? d : binary(d);
        received += chunk.size();
        return decodeChunk(chunk);
    }

    #! Throws an exception if the stream was cancelled
    /** @note called with the lock held
    */
    private checkCancelledIntern() {
        if (cancelled) {
            throw "HTTPCLIENT-CANCELLED", sprintf("the response body from %y was cancelled", effective_url);
        }
    }

    #! Returns a decoded chunk
    private binary decodeChunk(binary chunk) {
        return decoder ? decoder.decompressToBinary(chunk) : chunk;
    }

    #! Returns the rest of a decoded body at the end of the body, or NOTHING if there is none
    /** @note called with the lock held
    */
    private *binary finishIntern() {
        done = True;
        if (decoder) {
            binary rest = decoder.finish();
            if (rest) {
                return rest;
            }
        }
    }
}
}
