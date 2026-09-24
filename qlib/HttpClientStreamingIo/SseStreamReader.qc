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

        #! Received SSE data (UTF-8) awaiting event boundary parsing
        /** Kept as bytes, so that a boundary is found with byte offsets and a character split across chunks is only
            decoded once the event is complete
        */
        binary sse_buffer = binary();

        #! The size of @ref sse_buffer that has been searched for an event boundary
        /** Only the data received since is searched, so that an event received in many chunks is not searched from
            the start for each chunk
        */
        int scanned = 0;

        #! Data chunks consumed by a caller before constructing this reader
        list<auto> pending_data = ();

        #! Current offset in @ref pending_data
        int pending_data_offset = 0;

        #! True when the stream has ended (end_stream received or error)
        bool done = False;

        #! The maximum size of an event in bytes; <= 0 means no limit
        int max_event_size;
    }

    #! Creates an SSE reader for the given stream handle
    /** The stream handle must have already called \c startStreaming() before
        constructing the reader.

        @param stream an HttpClientStreamHandle in streaming mode
        @param content_encoding optional Content-Encoding for decompression
        (e.g., \c "gzip", \c "br"); pass \c NOTHING for uncompressed streams
        @param initial_data optional stream data already read by the caller
        while validating response metadata
        @param max_event_size the maximum size in bytes of an event and of the data decompressed from one received
        chunk; an event that grows beyond it before it ends raises \c SSE-EVENT-TOO-LARGE and cancels the stream, so
        that a stream that never ends an event, or a small compressed stream that expands to a huge one, cannot
        exhaust memory; \c 0 (the default) means @ref HttpClientStreamingIo::DefaultMaxEventSize
        "DefaultMaxEventSize"; a negative value means no limit

        @throw ENCODING-ERROR if the content encoding is not supported

        @since %HttpClientStreamingIo 1.0 added the \a max_event_size parameter
    */
    constructor(HttpClientIo::HttpClientStreamHandle stream, *string content_encoding,
            *list<auto> initial_data, int max_event_size = 0) {
        self.stream = stream;
        self.max_event_size = max_event_size ?: DefaultMaxEventSize;
        decompressor = new StreamDecompressor(content_encoding, self.max_event_size);
        if (initial_data) {
            pending_data = initial_data;
        }
    }

    #! Reads the next SSE event from the stream
    /** Blocks until an SSE event is available, the stream ends, or the timeout
        expires.  Comment-only SSE frames are consumed internally as
        keep-alives and do not get returned to callers; the timeout applies
        to idle reads, so keep-alive frames reset the timeout while this
        method continues waiting for a dispatchable event.  Returns
        \c NOTHING on timeout or when the stream is complete.

        @param timeout_ms timeout for this event wait; comments and incomplete frames
            do not restart it; blocks indefinitely if not specified

        @return the parsed SSE event, or \c NOTHING on timeout or stream end

        @throw SSE-STREAM-ERROR if the stream reported an error
        @throw SSE-EVENT-TOO-LARGE an event, or the data decompressed from one received chunk, exceeds the maximum
        event size; the stream is canceled

        @par SSE Event Format
        Each event is delimited by \c "\\n\\n" and parsed into a
        \c hash<SseMessageInfo> with fields:
        - \c event: event type string (from \c "event:" line)
        - \c data: event data (from \c "data:" line(s))
        - \c id: event ID (from \c "id:" line)
        - \c retry: reconnection interval in ms (from \c "retry:" line)
    */
    *hash<SseMessageInfo> readEvent(*timeout timeout_ms) {
        *date read_deadline = timeout_ms > 0
            ? now_us() + milliseconds(timeout_ms) : NOTHING;
        while (!done) {
            # Check buffer for a complete SSE event.  Events are delimited by a
            # blank line, which is "\n\n" (LF framing) or "\r\n\r\n" (CRLF
            # framing).  Detecting only "\n\n" silently merges every event of a
            # CRLF-framed stream (e.g. Google Gemini's streamGenerateContent)
            # because "\r\n\r\n" does not contain "\n\n": the buffer is only
            # flushed at end-of-stream, concatenating multiple JSON payloads
            # into one data field that then fails to parse.  Detect both forms
            # and consume up to whichever blank-line boundary appears first.
            # A line may also end with a CR alone, so "\r\r" is a blank line too.  A boundary can end in the data
            # received last, so the search starts before its first byte.
            int start = scanned > 3 ? scanned - 3 : 0;
            int sep = -1;
            int sep_len = 0;
            foreach binary sep_bin in ((<0d0a0d0a>, <0a0a>, <0d0d>)) {
                int pos = sse_buffer.find(sep_bin, start);
                if (pos >= 0 && (sep < 0 || pos < sep)) {
                    sep = pos;
                    sep_len = sep_bin.size();
                }
            }
            if (sep >= 0) {
                # an event is limited even when it is complete
                if (max_event_size > 0 && sep > max_event_size) {
                    eventTooLarge();
                }
                string event_text = sse_buffer.substr(0, sep + sep_len).toString("UTF-8");
                sse_buffer = sse_buffer.substr(sep + sep_len);
                scanned = 0;
                *hash<SseMessageInfo> evt =
                    Socket::parseServerSentEvent(event_text);
                if (isCommentOnlyEvent(evt)) {
                    continue;
                }
                return evt;
            }
            scanned = sse_buffer.size();
            # an event that has not ended is limited
            if (max_event_size > 0 && sse_buffer.size() > max_event_size) {
                eventTooLarge();
            }

            # Read more data from the stream.  RestClientIo may have
            # consumed the first body/end-stream item while validating the
            # response status, so replay those items before reading the
            # underlying stream handle again.
            auto data;
            if (pending_data_offset < pending_data.size()) {
                data = pending_data[pending_data_offset++];
                if (pending_data_offset >= pending_data.size()) {
                    pending_data = ();
                    pending_data_offset = 0;
                }
            } else {
                try {
                    *timeout remaining = timeout_ms;
                    if (read_deadline) {
                        remaining = (read_deadline - now_us()).durationMilliseconds();
                        if (remaining <= 0) { return NOTHING; }
                    }
                    data = stream.readData(remaining);
                } catch (hash<ExceptionInfo> ex) {
                    if (ex.err == "HTTPCLIENT-STREAM-CLOSED") {
                        return finishStream();
                    }
                    rethrow;
                }
            }
            if (!exists data) {
                # Timeout — return NOTHING without marking done
                return NOTHING;
            }

            if (data.typeCode() == NT_HASH) {
                if (data.end_stream) {
                    return finishStream();
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
                try {
                    sse_buffer += decompressor.decompressToBinary(data);
                } catch (hash<ExceptionInfo> ex) {
                    if (ex.err == "DECOMPRESSION-LIMIT-EXCEEDED") {
                        eventTooLarge();
                    }
                    rethrow;
                }
            } else {
                sse_buffer += binary(string(data));
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

    #! Cancels the stream and raises SSE-EVENT-TOO-LARGE
    private eventTooLarge() {
        sse_buffer = binary();
        cancel();
        throw "SSE-EVENT-TOO-LARGE", sprintf("the server-sent event exceeds the maximum event size of %d bytes",
            max_event_size);
    }

    #! Marks the stream complete and returns any final buffered SSE event
    private *hash<SseMessageInfo> finishStream() {
        done = True;
        if (sse_buffer.size()) {
            *hash<SseMessageInfo> last_evt = Socket::parseServerSentEvent(sse_buffer.toString("UTF-8"));
            sse_buffer = binary();
            if (last_evt && last_evt.data.val()) {
                return last_evt;
            }
        }
        return NOTHING;
    }

    #! Returns True if the parsed frame is an SSE comment-only keep-alive
    private static bool isCommentOnlyEvent(*hash<SseMessageInfo> evt) {
        return evt && evt.comment.val() && !evt.data.val()
            && !evt.event.val() && !evt.id.val() && !evt.retry.val();
    }
}

} # namespace HttpClientStreamingIo
