/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    Http2Session.h

    HTTP/2 Session wrapper using nghttp2

    Qore Programming Language

    Copyright (C) 2025 - 2026 Qore Technologies, s.r.o.

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

#ifndef _QORE_HTTP2_SESSION_H
#define _QORE_HTTP2_SESSION_H

#include <nghttp2/nghttp2.h>

#include <qore/Qore.h>
#include <qore/InputStream.h>

#include <cctype>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

// Forward declarations
struct qore_socket_private;

//! HTTP/2 stream state
enum class Http2StreamState {
    Idle,
    Open,
    HalfClosedLocal,
    HalfClosedRemote,
    Closed
};

//! HTTP/2 stream type for protocol upgrades (RFC 8441, SSE)
enum class Http2StreamType {
    Normal,         //!< Regular HTTP/2 request/response
    WebSocket,      //!< WebSocket over HTTP/2 (RFC 8441)
    Sse             //!< Server-Sent Events stream
};

//! HTTP/2 stream information
struct Http2StreamInfo {
    int32_t stream_id = 0;
    Http2StreamState state = Http2StreamState::Idle;
    Http2StreamType stream_type = Http2StreamType::Normal;

    // Request data (for server) / Response data (for client)
    std::string method;
    std::string path;
    std::string authority;
    std::string scheme;
    int status_code = 0;

    // RFC 8441: Extended CONNECT protocol (for WebSocket over HTTP/2)
    std::string connect_protocol;  //!< Value of :protocol pseudo-header (e.g., "websocket")
    bool is_connect = false;       //!< True if this is a CONNECT request

    // Headers (values stored as vectors to support duplicate header names per RFC 7540 Section 8.1.2.5)
    std::map<std::string, std::vector<std::string>> headers;

    //! Trailer headers (received after body in a trailing HEADERS frame)
    std::map<std::string, std::vector<std::string>> trailers;

    //! True when receiving trailer headers (after initial headers + body)
    bool receiving_trailers = false;

    // Body data
    std::vector<char> body;

    //! Maximum body size in bytes (0 = unlimited)
    int64 max_body_size = 0;

    // Stream priority
    int32_t weight = 16;
    int32_t dependency = 0;
    bool exclusive = false;

    // Completion flags
    bool headers_complete = false;
    bool body_complete = false;
    bool reset = false;
    bool marked_complete = false;  //!< True if already added to completed_streams (prevents duplicates)
    bool dispatched = false;       //!< True after headers-only dispatch (stream stays in map for DATA accumulation)
    bool streaming = false;        //!< True for streaming requests (bidi/client-streaming) on client side
    uint32_t error_code = 0;

    //! Returns true if this is a WebSocket over HTTP/2 stream (RFC 8441)
    DLLLOCAL bool isWebSocket() const {
        return stream_type == Http2StreamType::WebSocket ||
               (is_connect && connect_protocol == "websocket");
    }

    //! Returns true if this is an SSE stream
    DLLLOCAL bool isSse() const { return stream_type == Http2StreamType::Sse; }

    //! Returns true if this stream uses a streaming protocol (WebSocket or SSE)
    DLLLOCAL bool isStreaming() const { return isWebSocket() || isSse(); }

    DLLLOCAL Http2StreamInfo() = default;
    DLLLOCAL Http2StreamInfo(int32_t id) : stream_id(id), state(Http2StreamState::Open) {}
};

//! HTTP/2 Settings
struct Http2Settings {
    uint32_t header_table_size = 4096;
    uint32_t enable_push = 1;
    uint32_t max_concurrent_streams = 100;
    uint32_t initial_window_size = 65535;
    uint32_t max_frame_size = 16384;
    uint32_t max_header_list_size = UINT32_MAX;
    //! RFC 8441: Extended CONNECT for WebSocket over HTTP/2.
    /** Default is 0 per the HTTP/2 spec.  Servers that want to advertise support must
        call Http2Session::setEnableConnectProtocol(true) before the connection preface
        is sent; this is handled automatically by SocketHttp2ServerPollOperation::initSession()
        based on qore_socket_private::h2_enable_connect_protocol.
    */
    uint32_t enable_connect_protocol = 0;
};

//! HTTP/2 Session wrapper using nghttp2
class Http2Session {
public:
    //! Compaction threshold for the send buffer (bytes consumed before erasing)
    /** When send_offset exceeds this value, the consumed prefix is erased.
        O(remaining_data) but amortized: only fires after this many bytes consumed.
    */
    static constexpr size_t SEND_BUFFER_COMPACTION_THRESHOLD = 65536;

    //! Create a client-side HTTP/2 session
    /** @param sock Socket for the connection
        @param xsink Exception sink for error reporting
        @param scheme URL scheme - "https" for h2 (default), "http" for h2c
        @return New Http2Session or nullptr on error
    */
    DLLLOCAL static std::shared_ptr<Http2Session> createClient(qore_socket_private* sock, ExceptionSink* xsink,
        const char* scheme = "https");

    //! Create a server-side HTTP/2 session
    /** @param sock Socket for the connection
        @param xsink Exception sink for error reporting
        @param scheme URL scheme - "https" for h2 (default), "http" for h2c
        @return New Http2Session or nullptr on error
    */
    DLLLOCAL static std::shared_ptr<Http2Session> createServer(qore_socket_private* sock, ExceptionSink* xsink,
        const char* scheme = "https");

    DLLLOCAL ~Http2Session();

    //! Returns true if this is a server-side session
    DLLLOCAL bool isServer() const { return is_server; }

    //! Sets the maximum request body size for new streams (0 = unlimited)
    DLLLOCAL void setMaxRequestBodySize(int64 size) { max_request_body_size = size; }

    //! Returns the maximum request body size
    DLLLOCAL int64 getMaxRequestBodySize() const { return max_request_body_size; }

    //! Send the connection preface (client) or SETTINGS (server)
    DLLLOCAL int sendConnectionPreface(ExceptionSink* xsink);

    //! Submit a SETTINGS frame
    DLLLOCAL int submitSettings(const Http2Settings& settings, ExceptionSink* xsink);

    //! Submit a request (client-side)
    /** @param method HTTP method
        @param path Request path
        @param headers Request headers
        @param body Request body (can be nullptr)
        @param body_len Length of request body
        @param xsink Exception sink for error reporting
        @return stream ID on success, -1 on error
    */
    DLLLOCAL int32_t submitRequest(const char* method, const char* path,
        const strcase_str_map_t& headers,
        const void* body, size_t body_len, ExceptionSink* xsink,
        bool streaming = false);

    //! Submit a request with support for duplicate header names (client-side)
    /** Uses a vector of pairs to allow multiple entries with the same header name,
        which is required for HTTP/2 cookie headers per RFC 7540 Section 8.1.2.5.
        @param method HTTP method
        @param path Request path
        @param headers Request headers as name/value pairs (may contain duplicate names)
        @param body Request body (can be nullptr)
        @param body_len Length of request body
        @param xsink Exception sink for error reporting
        @param streaming If true, keep the stream open for subsequent sendStreamData() calls
        @return stream ID on success, -1 on error
    */
    DLLLOCAL int32_t submitRequest(const char* method, const char* path,
        const std::vector<std::pair<std::string, std::string>>& headers,
        const void* body, size_t body_len, ExceptionSink* xsink,
        bool streaming = false);

    //! Submit a response (server-side)
    /** @param stream_id Stream ID for the response
        @param status_code HTTP status code
        @param headers Response headers
        @param body Response body (can be nullptr)
        @param body_len Length of response body
        @param xsink Exception sink for error reporting
        @return 0 on success, -1 on error
    */
    DLLLOCAL int submitResponse(int32_t stream_id, int status_code,
        const strcase_str_map_t& headers,
        const void* body, size_t body_len, ExceptionSink* xsink);

    //! Submit a response with support for duplicate header names (server-side)
    /** Uses a vector of pairs to allow multiple entries with the same header name,
        which is needed for headers like Set-Cookie that must not be combined.
        @param stream_id Stream ID for the response
        @param status_code HTTP status code
        @param headers Response headers as name/value pairs (may contain duplicate names)
        @param body Response body (can be nullptr)
        @param body_len Length of response body
        @param xsink Exception sink for error reporting
        @return 0 on success, -1 on error
    */
    DLLLOCAL int submitResponse(int32_t stream_id, int status_code,
        const std::vector<std::pair<std::string, std::string>>& headers,
        const void* body, size_t body_len, ExceptionSink* xsink);

    //! Submit a streaming response (server-side, headers only, deferred body)
    /** Submits response HEADERS without END_STREAM. Body data is sent incrementally
        via sendStreamData() calls.
        @param stream_id Stream ID for the response
        @param status_code HTTP status code
        @param headers Response headers
        @param xsink Exception sink for error reporting
        @return 0 on success, -1 on error

        @since Qore 2.2
    */
    DLLLOCAL int submitResponseStreaming(int32_t stream_id, int status_code,
        const strcase_str_map_t& headers, ExceptionSink* xsink);

    //! Submit a PUSH_PROMISE (server-side)
    /** @param stream_id Stream ID of the associated request
        @param path Path of the pushed resource
        @param headers Headers for the pushed resource
        @param xsink Exception sink for error reporting
        @return promised stream ID on success, -1 on error
    */
    DLLLOCAL int32_t submitPushPromise(int32_t stream_id, const char* path,
        const strcase_str_map_t& headers, ExceptionSink* xsink);

    //! Submit a RST_STREAM frame to cancel a stream
    DLLLOCAL int submitRstStream(int32_t stream_id, uint32_t error_code, ExceptionSink* xsink);

    //! Submit a GOAWAY frame for graceful shutdown
    DLLLOCAL int submitGoaway(uint32_t error_code, const char* opaque_data = nullptr,
        size_t len = 0, ExceptionSink* xsink = nullptr);

    //! Submit a PING frame
    DLLLOCAL int submitPing(const uint8_t* opaque_data = nullptr, ExceptionSink* xsink = nullptr);

    //! Submit a WINDOW_UPDATE frame
    DLLLOCAL int submitWindowUpdate(int32_t stream_id, int32_t increment, ExceptionSink* xsink);

    //! Submit a PRIORITY frame to set stream priority
    /** @param stream_id Stream ID to prioritize
        @param dependency Stream ID this depends on (0 for root)
        @param weight Priority weight (1-256, default 16)
        @param exclusive If true, becomes exclusive dependency
        @param xsink Exception sink for error reporting
        @return 0 on success, -1 on error

        @since Qore 2.2
    */
    DLLLOCAL int submitPriority(int32_t stream_id, int32_t dependency, int32_t weight,
        bool exclusive, ExceptionSink* xsink);

    //! Send data on a stream without closing it (for SSE/WebSocket streaming)
    /** @param stream_id Stream ID to send data on
        @param data Data to send
        @param len Length of data
        @param end_stream If true, sends END_STREAM flag (closes the stream for sending)
        @param xsink Exception sink for error reporting
        @return 0 on success, -1 on error (exception set), 1 = buffer full (non-fatal, data not appended)
    */
    DLLLOCAL int sendStreamData(int32_t stream_id, const void* data, size_t len,
        bool end_stream, ExceptionSink* xsink);

    //! Submit HTTP/2 trailer headers on a stream (server-side)
    /** Sends a HEADERS frame with END_STREAM flag containing trailer fields.
        The data provider for this stream is configured to signal EOF without
        END_STREAM on the last DATA frame, so that the trailer HEADERS carries
        the END_STREAM flag.

        @param stream_id Stream ID to send trailers on
        @param trailers Trailer header name/value pairs
        @param xsink Exception sink for error reporting
        @return 0 on success, -1 on error
    */
    DLLLOCAL int submitTrailers(int32_t stream_id,
        const strcase_str_map_t& trailers, ExceptionSink* xsink);

    //! Set the stream type (for protocol upgrades like WebSocket/SSE)
    DLLLOCAL void setStreamType(int32_t stream_id, Http2StreamType type);

    //! Submit a response for an extended CONNECT request (RFC 8441)
    /** Used to accept WebSocket over HTTP/2 connections
        @param stream_id Stream ID for the CONNECT request
        @param status_code HTTP status code (200 to accept, 4xx to reject)
        @param headers Response headers
        @param xsink Exception sink for error reporting
        @return 0 on success, -1 on error
    */
    DLLLOCAL int submitConnectResponse(int32_t stream_id, int status_code,
        const strcase_str_map_t& headers, ExceptionSink* xsink);

    //! Returns true if RFC 8441 extended CONNECT protocol is enabled locally
    DLLLOCAL bool isConnectProtocolEnabled() const {
        return local_settings.enable_connect_protocol != 0;
    }

    //! Returns true if the remote peer has advertised ENABLE_CONNECT_PROTOCOL
    DLLLOCAL bool isRemoteConnectProtocolEnabled() const {
        return remote_settings_received && remote_settings.enable_connect_protocol != 0;
    }

    //! Returns true if remote SETTINGS have been received and ENABLE_CONNECT_PROTOCOL was not set
    /** Used by the client to detect servers that don't support extended CONNECT (RFC 8441)
        before sending a CONNECT request with :protocol.  On some nghttp2 versions, the
        server silently drops :protocol when ENABLE_CONNECT_PROTOCOL is not advertised.
    */
    DLLLOCAL bool isExtendedConnectRejected() const {
        return remote_settings_received && !remote_settings.enable_connect_protocol;
    }

    //! Sets whether to advertise ENABLE_CONNECT_PROTOCOL in SETTINGS
    /** Must be called before sendConnectionPreface() to take effect.
    */
    DLLLOCAL void setEnableConnectProtocol(bool enable) {
        local_settings.enable_connect_protocol = enable ? 1 : 0;
    }

    //! Send all pending data (async, non-blocking)
    /** @param timeout_ms Timeout in milliseconds (ignored, always non-blocking)
        @param xsink Exception sink for error reporting
        @return 0 on success, -1 if need to poll for POLLOUT
    */
    DLLLOCAL int sendPendingData(int timeout_ms, ExceptionSink* xsink);

    //! Send all pending data (blocking with timeout to ensure flush)
    /** @param timeout_ms Timeout in milliseconds
        @param xsink Exception sink for error reporting
        @return 0 on success, -1 if still have data to send
    */
    DLLLOCAL int sendPendingDataBlocking(int timeout_ms, ExceptionSink* xsink);

    //! Receive and process data
    /** @param timeout_ms Timeout in milliseconds (-1 for infinite)
        @param xsink Exception sink for error reporting
        @return 0 on success/timeout (data may have been received), 1 if connection was closed by peer, -1 on error
    */
    DLLLOCAL int receiveData(int timeout_ms, ExceptionSink* xsink);

    //! Returns true if there is pending request/response body data for stream_id
    DLLLOCAL bool hasPendingBodyData(int32_t stream_id) const;

    //! Returns true if there is already-buffered data in the socket read buffer or SSL layer
    /** This is a lightweight check (no I/O, no SSL operations) that checks internal counters.
        Used by isHttp2DataAvailable() to detect buffered data without holding the socket lock.
        @return true if there is buffered data available for reading
        @since Qore 2.1
    */
    DLLLOCAL bool hasSocketBufferedData() const;

    //! Get a stream by ID
    DLLLOCAL Http2StreamInfo* getStream(int32_t stream_id);

    //! Take available stream data without blocking
    /** @param stream_id stream ID
        @param max_bytes maximum bytes to return; 0 means all available
        @param xsink Exception sink for error reporting
    */
    DLLLOCAL BinaryNode* takeStreamData(int32_t stream_id, size_t max_bytes, ExceptionSink* xsink);

    //! Take a completed stream (removes it from the session)
    DLLLOCAL std::unique_ptr<Http2StreamInfo> takeCompletedStream();

    //! Returns true if there are completed streams waiting
    DLLLOCAL bool hasCompletedStreams() const { return !completed_streams.empty(); }

    //! Enable/disable headers-only mode
    /** When enabled, markStreamComplete() keeps streams with headers_complete in the
        map instead of moving them to completed_streams. This allows
        takeHeadersReadyStreamCopy() to find them even when END_STREAM arrives in the
        same batch as HEADERS.

        @param v True to enable headers-only mode
        @since %Qore 2.3
    */
    DLLLOCAL void setHeadersOnlyMode(bool v);

    //! Atomically find a headers-ready stream, copy it, and mark dispatched
    /** Finds the first stream with headers_complete && !dispatched, copies it, clears the
        body on the copy, and marks the original as dispatched — all under a single lock to
        prevent TOCTOU races.

        The body is cleared on the copy (not the original) so that DATA frames that arrived
        with HEADERS remain in the original for readHttp2StreamDataBlock() to return.
        getOutput() skips the body field for H2S_HEADERS_READY mode to prevent duplication.

        @return a copy of the stream info (with empty body), or nullptr if no headers-ready
        stream exists
        @since %Qore 2.3
    */
    DLLLOCAL std::unique_ptr<Http2StreamInfo> takeHeadersReadyStreamCopy();

    //! Check if stream's body is complete (END_STREAM received)
    /** @param stream_id the stream to check
        @return True if END_STREAM has been received (body_complete), or if the
        stream is not found (a cleaned-up stream is effectively complete)
        @since %Qore 2.3
    */
    DLLLOCAL bool isStreamComplete(int32_t stream_id) const;

    //! Remove stream from map (cleanup after handler finishes)
    /** @param stream_id the stream to remove
        @since %Qore 2.3
    */
    DLLLOCAL void cleanupStream(int32_t stream_id);

    //! Check if any streaming client streams have body data available
    /** Used by client multiplex to deliver intermediate body data for streaming streams.
        @param stream_id [out] set to the stream ID with data, if found
        @return true if a streaming stream with body data was found
        @since %Qore 2.3
    */
    DLLLOCAL bool hasStreamingData(int32_t& stream_id);

    //! Callback type for stream completion notification (HTTP/2 client multiplexing)
    /** Callback arguments:
        - \c stream_id: the completed stream ID
        - \c stream: the stream info (may be nullptr if stream was reset before completion)
        - \c xsink: exception sink for error reporting
    */
    using StreamCompleteCallback = std::function<void(int32_t stream_id, Http2StreamInfo* stream,
        ExceptionSink* xsink)>;

    //! Sets the stream completion callback for HTTP/2 client multiplexing
    /** When set, this callback is invoked each time a stream completes (response received,
        stream reset, or error). This enables multiplexed response routing in client scenarios.

        @param callback the callback to invoke on stream completion
    */
    DLLLOCAL void setStreamCompleteCallback(StreamCompleteCallback callback) {
        std::lock_guard<std::recursive_mutex> lg(m);
        stream_complete_callback = std::move(callback);
    }

    //! Clears the stream completion callback
    DLLLOCAL void clearStreamCompleteCallback() {
        printd(5, "clearStreamCompleteCallback() session=%p about to lock\n", this);
        if (getenv("QORE_HTTP2_DEBUG")) {
            fprintf(stderr, "HTTP2 DEBUG: clearStreamCompleteCallback() session=%p isServer=%d tid=%d about to lock\n",
                this, is_server ? 1 : 0, q_gettid());
            fflush(stderr);
        }
        std::lock_guard<std::recursive_mutex> lg(m);
        printd(5, "clearStreamCompleteCallback() locked, clearing callback\n");
        if (getenv("QORE_HTTP2_DEBUG")) {
            fprintf(stderr, "HTTP2 DEBUG: clearStreamCompleteCallback() session=%p tid=%d locked, clearing callback\n",
                this, q_gettid());
            fflush(stderr);
        }
        stream_complete_callback = nullptr;
        printd(5, "clearStreamCompleteCallback() done\n");
        if (getenv("QORE_HTTP2_DEBUG")) {
            fprintf(stderr, "HTTP2 DEBUG: clearStreamCompleteCallback() session=%p tid=%d done\n", this, q_gettid());
            fflush(stderr);
        }
    }

    //! Returns true if a stream completion callback is set
    DLLLOCAL bool hasStreamCompleteCallback() const {
        std::lock_guard<std::recursive_mutex> lg(m);
        return static_cast<bool>(stream_complete_callback);
    }

    //! Returns the number of active streams
    DLLLOCAL size_t getActiveStreamCount() const { return streams.size(); }

    //! Returns true if session is in GOAWAY state
    DLLLOCAL bool isGoawayReceived() const { return goaway_received; }

    //! Returns the last stream ID from GOAWAY
    DLLLOCAL int32_t getLastStreamId() const { return last_stream_id; }

    //! Returns true if the session wants to read
    DLLLOCAL bool wantRead() const;

    //! Returns true if the session wants to write
    DLLLOCAL bool wantWrite() const;

    //! Returns true if there is data in the send buffer waiting to be sent
    /** This is separate from wantWrite() because nghttp2_session_want_write()
        only accounts for data inside nghttp2's internal buffers, not our send_buffer.
    */
    DLLLOCAL bool hasPendingData() const { return send_offset < send_buffer.size(); }

    //! Returns the remote flow control window size for a given stream
    /** Returns the number of bytes the server can send on this stream before
        a WINDOW_UPDATE is needed from the client.
        @param stream_id the stream ID to check (0 for connection-level window)
        @return the remote window size in bytes, or -1 on error
    */
    DLLLOCAL int32_t getStreamRemoteWindowSize(int32_t stream_id) const {
        std::lock_guard<std::recursive_mutex> lg(m);
        if (stream_id == 0) {
            return nghttp2_session_get_remote_window_size(session);
        }
        return nghttp2_session_get_stream_remote_window_size(session, stream_id);
    }

    //! Get current settings
    DLLLOCAL Http2Settings getSettings() const { return local_settings; }

    //! Get remote peer settings
    DLLLOCAL Http2Settings getRemoteSettings() const { return remote_settings; }

    //! Get the underlying nghttp2 session handle
    DLLLOCAL nghttp2_session* getSession() const { return session; }

private:
    // NOTE: recursive mutex is required because nghttp2 callbacks can re-enter
    // Http2Session methods that also lock the session state.
    mutable std::recursive_mutex m;
    //! When true, markStreamComplete() keeps undispatched headers-complete streams in the map
    bool headers_only_mode = false;
    DLLLOCAL Http2Session(qore_socket_private* sock, bool is_server, const char* scheme);
    DLLLOCAL int init(ExceptionSink* xsink);

    // nghttp2 callbacks
    static ssize_t sendCallback(nghttp2_session* session, const uint8_t* data,
        size_t length, int flags, void* user_data);
    static int onFrameRecvCallback(nghttp2_session* session,
        const nghttp2_frame* frame, void* user_data);
    static int onFrameSendCallback(nghttp2_session* session,
        const nghttp2_frame* frame, void* user_data);
    static int onDataChunkRecvCallback(nghttp2_session* session, uint8_t flags,
        int32_t stream_id, const uint8_t* data, size_t len, void* user_data);
    static int onStreamCloseCallback(nghttp2_session* session, int32_t stream_id,
        uint32_t error_code, void* user_data);
    static int onHeaderCallback(nghttp2_session* session, const nghttp2_frame* frame,
        const uint8_t* name, size_t namelen, const uint8_t* value, size_t valuelen,
        uint8_t flags, void* user_data);
    static int onBeginHeadersCallback(nghttp2_session* session,
        const nghttp2_frame* frame, void* user_data);
    static int onInvalidFrameRecvCallback(nghttp2_session* session,
        const nghttp2_frame* frame, int lib_error_code, void* user_data);
    static int onInvalidHeaderCallback(nghttp2_session* session,
        const nghttp2_frame* frame, const uint8_t* name, size_t namelen,
        const uint8_t* value, size_t valuelen, uint8_t flags, void* user_data);

    // Helper to convert headers to nghttp2 format
    DLLLOCAL std::vector<nghttp2_nv> makeNv(const strcase_str_map_t& headers);

    // Template implementations for submit methods to avoid map→vector<pair> copy
    template<typename HeaderRange>
    DLLLOCAL int32_t submitRequestImpl(const char* method, const char* path,
        const HeaderRange& headers, const void* body, size_t body_len,
        ExceptionSink* xsink, bool streaming);
    template<typename HeaderRange>
    DLLLOCAL int submitResponseImpl(int32_t stream_id, int status_code,
        const HeaderRange& headers, const void* body, size_t body_len,
        ExceptionSink* xsink);

    // Internal stream management
    DLLLOCAL Http2StreamInfo* getOrCreateStream(int32_t stream_id);
    DLLLOCAL void markStreamComplete(int32_t stream_id);

    qore_socket_private* sock;
    nghttp2_session* session = nullptr;
    bool is_server;
    std::string scheme;  //!< "https" for h2, "http" for h2c

    // Stream management (unordered_map: O(1) lookup by stream_id, no ordered iteration needed)
    std::unordered_map<int32_t, std::unique_ptr<Http2StreamInfo>> streams;
    std::queue<std::unique_ptr<Http2StreamInfo>> completed_streams;

    // Stream completion callback for client multiplexing
    StreamCompleteCallback stream_complete_callback;

    // Send buffer
    std::vector<char> send_buffer;
    size_t send_offset = 0;

    // Settings
    Http2Settings local_settings;
    Http2Settings remote_settings;
    bool remote_settings_received = false;  //!< True after first SETTINGS frame from peer

    //! Maximum request body size in bytes (0 = unlimited), propagated to new streams
    int64 max_request_body_size = 0;

    // GOAWAY state
    bool goaway_received = false;
    bool goaway_sent = false;
    int32_t last_stream_id = 0;

    // Request body data for callbacks - owns the data
    struct BodyData {
        std::vector<uint8_t> data;
        size_t offset = 0;
        bool end_stream = false;  //!< if true, signal EOF after this data is consumed

        BodyData() = default;
        BodyData(const void* src, size_t len, bool end_stream = false)
            : data(reinterpret_cast<const uint8_t*>(src),
                   reinterpret_cast<const uint8_t*>(src) + len),
              end_stream(end_stream) {}
    };
    std::unordered_map<int32_t, BodyData> pending_body_data;

    struct DataProviderContext {
        nghttp2_data_provider provider{};
        Http2Session* h2 = nullptr;
        bool defer_on_empty = false;
        bool no_end_stream = false;
        bool remove_on_empty = true;
        //! Trailers to submit after body EOF (set by submitTrailers())
        std::vector<std::pair<std::string, std::string>> pending_trailers;
        //! Error from nghttp2_submit_trailer() failure in data provider callback
        int trailer_submit_error = 0;
    };
    std::unordered_map<int32_t, std::unique_ptr<DataProviderContext>> pending_data_providers;

    DLLLOCAL static ssize_t dataProviderReadCallback(nghttp2_session* session, int32_t stream_id,
        uint8_t* buf, size_t length, uint32_t* data_flags, nghttp2_data_source* source,
        void* user_data);

    //! Info about an InputStream being streamed on an HTTP/2 response
    /** @since %Qore 2.3
    */
    struct StreamInputStreamInfo {
        SimpleRefHolder<InputStream> input_stream;
        int stream_fd = -1;
        bool is_pollable = false;
        bool need_reassign = true;
        bool eof = false;

        StreamInputStreamInfo() = default;
        StreamInputStreamInfo(InputStream* is)
            : input_stream(is), stream_fd(is->getPollableDescriptor()),
              is_pollable(stream_fd >= 0) {}
    };
    std::unordered_map<int32_t, StreamInputStreamInfo> stream_input_streams_;

    //! Atomic flag for lock-free hasActiveStreamInputStreams() checks in the I/O hot path
    std::atomic<bool> has_active_input_streams_{false};

public:
    //! Store an InputStream for a stream (I/O thread will read from it)
    /** @since %Qore 2.3
    */
    DLLLOCAL void setStreamInputStream(int32_t stream_id, InputStream* is, ExceptionSink* xsink);

    //! Returns true if there are active InputStreams being processed
    /** @since %Qore 2.3
    */
    DLLLOCAL bool hasActiveStreamInputStreams() const {
        return has_active_input_streams_.load(std::memory_order_acquire);
    }

    //! Process one chunk from each active InputStream (called by I/O thread)
    /** @since %Qore 2.3
    */
    DLLLOCAL void processStreamInputStreams(ExceptionSink* xsink);

    //! Clean up all InputStreams (called on session destruction)
    /** @since %Qore 2.3
    */
    DLLLOCAL void cleanupStreamInputStreams(ExceptionSink* xsink);

    //! Get extra fds for all active pollable InputStreams
    /** @since %Qore 2.3
    */
    DLLLOCAL void getExtraFds(std::vector<std::pair<int, int>>& extra_fds) const;
};

//! Shared pointer type for Http2Session with thread-safe atomic reference counting
using Http2SessionPtr = std::shared_ptr<Http2Session>;

//! Convert HTTP/2 or HTTP/3 multi-value headers map to a QoreHashNode
/** Template to accept maps with any comparator (e.g. std::less or ltstrcase).

    Handles duplicate header names per RFC 7540 Section 8.1.2.5:
    - cookie: concatenated with "; " into a single string
    - Single-value headers: stored as QoreStringNode
    - Multi-value headers: stored as QoreListNode (matches HTTP/1.1 behavior)

    @param h2_headers HTTP/2 or HTTP/3 headers map (name -> vector of values)
    @param lowercase if true, convert header names to lowercase
    @return new QoreHashNode with the converted headers
*/
template<typename Comparator>
DLLLOCAL inline QoreHashNode* httpMultiHeadersToQoreHash(
    const std::map<std::string, std::vector<std::string>, Comparator>& h2_headers,
    bool lowercase = false) {
    QoreHashNode* headers = new QoreHashNode(autoTypeInfo);
    for (const auto& h : h2_headers) {
        std::string name;
        if (lowercase) {
            name.reserve(h.first.size());
            for (unsigned char ch : h.first) {
                name.push_back(std::tolower(ch));
            }
        } else {
            name = h.first;
        }

        const std::vector<std::string>& vals = h.second;
        if (vals.empty()) {
            continue;
        }
        if (vals.size() == 1) {
            // Single value: store as plain string
            headers->setKeyValue(name.c_str(), new QoreStringNode(vals[0]), nullptr);
        } else if (strcasecmp(name.c_str(), "cookie") == 0) {
            // RFC 7540 Section 8.1.2.5: concatenate cookie values with "; "
            QoreStringNode* joined = new QoreStringNode;
            for (size_t i = 0; i < vals.size(); ++i) {
                if (i > 0) {
                    joined->concat("; ");
                }
                joined->concat(vals[i].c_str());
            }
            headers->setKeyValue(name.c_str(), joined, nullptr);
        } else {
            // Multiple values: store as list (matches HTTP/1.1 duplicate header behavior)
            QoreListNode* l = new QoreListNode(autoTypeInfo);
            for (const auto& v : vals) {
                l->push(new QoreStringNode(v), nullptr);
            }
            headers->setKeyValue(name.c_str(), l, nullptr);
        }
    }
    return headers;
}

#endif // _QORE_HTTP2_SESSION_H
