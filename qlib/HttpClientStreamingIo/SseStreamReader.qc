# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SseStreamReader.qc SSE event reader over HttpClientStreamHandle

/** SseStreamReader.qc Copyright 2026 Qore Technologies, s.r.o.

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
public namespace HttpClientStreamingIo {

#! Reads Server-Sent Events (SSE) from an HttpClientStreamHandle streaming connection
/** Works transparently across HTTP/1.1, HTTP/2, and HTTP/3.  Accumulates binary data
    chunks from \c readData(), applies optional decompression, and parses SSE events
    at \c "\\n\\n" boundaries using \c Socket::parseServerSentEvent().

    @note This class is \b not thread-safe.  Use from a single reader thread.

    @par Example: Reading SSE events over any HTTP version
    @code{.py}
HttpClientConnectionManager mgr({"url": "https://api.example.com"});
HttpClientStreamHandle stream = mgr.acquireStream();
stream.startStreaming("POST", "/events",
    {"Accept": "text/event-stream", "Content-Type": "application/json"},
);

SseStreamReader reader(stream);
while (*hash<SseMessageInfo> evt = reader.readEvent(30s)) {
    printf("Event: %y  Data: %y\n", evt.event, evt.data);
}
    @endcode

    @par Example: With content encoding
    @code{.py}
HttpClientStreamHandle stream = mgr.acquireStream();
stream.startStreaming("GET", "/stream", {"Accept-Encoding": "gzip"});

# Pass content encoding from response headers
SseStreamReader reader(stream, "gzip");
while (*hash<SseMessageInfo> evt = reader.readEvent(30s)) {
    processEvent(evt);
}
    @endcode
*/
public class SseStreamReader {
    private {
        #! The underlying stream handle
        HttpClientIo::HttpClientStreamHandle stream;

        #! Decompressor for Content-Encoding support
        StreamDecompressor decompressor;

        #! Accumulated SSE text buffer awaiting event boundary parsing
        string sse_buffer = "";

        #! True when the stream has ended (end_stream received or error)
        bool done = False;
    }

    #! Creates an SSE reader for the given stream handle
    /** The stream handle must have already called \c startStreaming() before
        constructing the reader.

        @param stream an HttpClientStreamHandle in streaming mode
        @param content_encoding optional Content-Encoding for decompression
        (e.g., \c "gzip", \c "br"); pass \c NOTHING for uncompressed streams

        @throw ENCODING-ERROR if the content encoding is not supported
    */
    constructor(HttpClientIo::HttpClientStreamHandle stream, *string content_encoding) {
        self.stream = stream;
        decompressor = new StreamDecompressor(content_encoding);
    }

    #! Reads the next SSE event from the stream
    /** Blocks until an SSE event is available, the stream ends, or the timeout
        expires.  Returns \c NOTHING on timeout or when the stream is complete.

        @param timeout_ms timeout for waiting; blocks indefinitely if not specified

        @return the parsed SSE event, or \c NOTHING on timeout or stream end

        @throw SSE-STREAM-ERROR if the stream reported an error
        @throw HTTPCLIENT-STREAM-CLOSED if the stream was closed unexpectedly

        @par SSE Event Format
        Each event is delimited by \c "\\n\\n" and parsed into a
        \c hash<SseMessageInfo> with fields:
        - \c event: event type string (from \c "event:" line)
        - \c data: event data (from \c "data:" line(s))
        - \c id: event ID (from \c "id:" line)
        - \c retry: reconnection interval in ms (from \c "retry:" line)
    */
    *hash<SseMessageInfo> readEvent(*timeout timeout_ms) {
        while (!done) {
            # Check buffer for a complete SSE event (delimited by \n\n)
            int sep = sse_buffer.find("\n\n");
            if (sep >= 0) {
                string event_text = sse_buffer.substr(0, sep + 2);
                sse_buffer = sse_buffer.substr(sep + 2);
                return Socket::parseServerSentEvent(event_text);
            }

            # Read more data from the stream
            auto data = stream.readData(timeout_ms);
            if (!exists data) {
                # Timeout — return NOTHING without marking done
                return NOTHING;
            }

            if (data.typeCode() == NT_HASH) {
                if (data.end_stream) {
                    done = True;
                    # Parse any remaining buffered data
                    if (sse_buffer.val()) {
                        *hash<SseMessageInfo> last_evt = Socket::parseServerSentEvent(sse_buffer);
                        sse_buffer = "";
                        if (last_evt && last_evt.data.val()) {
                            return last_evt;
                        }
                    }
                    return NOTHING;
                }
                if (data.error) {
                    done = True;
                    throw "SSE-STREAM-ERROR", sprintf("stream error: %y", data.error);
                }
                # Other hash data (e.g., headers) — skip
                continue;
            }

            # Decompress and buffer the raw data
            if (data.typeCode() == NT_BINARY) {
                sse_buffer += decompressor.decompressToString(data);
            } else {
                sse_buffer += string(data);
            }
        }
        return NOTHING;
    }

    #! Returns True if the stream has ended
    bool isDone() {
        return done;
    }

    #! Cancels the underlying stream
    cancel() {
        stream.cancel();
        done = True;
    }
}

} # namespace HttpClientStreamingIo
