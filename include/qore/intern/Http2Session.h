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

#include <string>
#include <vector>
#include <map>
#include <queue>
#include <memory>
#include <mutex>

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

    // Headers
    std::map<std::string, std::string> headers;

    // Body data
    std::vector<char> body;

    // Stream priority
    int32_t weight = 16;
    int32_t dependency = 0;
    bool exclusive = false;

    // Completion flags
    bool headers_complete = false;
    bool body_complete = false;
    bool reset = false;
    bool marked_complete = false;  //!< True if already added to completed_streams (prevents duplicates)
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
    uint32_t enable_connect_protocol = 1;  //!< RFC 8441: Enable extended CONNECT for WebSocket
};

//! HTTP/2 Session wrapper using nghttp2
class Http2Session {
public:
    //! Create a client-side HTTP/2 session
    /** @param sock Socket for the connection
        @param xsink Exception sink for error reporting
        @param scheme URL scheme - "https" for h2 (default), "http" for h2c
        @return New Http2Session or nullptr on error
    */
    DLLLOCAL static Http2Session* createClient(qore_socket_private* sock, ExceptionSink* xsink,
        const char* scheme = "https");

    //! Create a server-side HTTP/2 session
    /** @param sock Socket for the connection
        @param xsink Exception sink for error reporting
        @param scheme URL scheme - "https" for h2 (default), "http" for h2c
        @return New Http2Session or nullptr on error
    */
    DLLLOCAL static Http2Session* createServer(qore_socket_private* sock, ExceptionSink* xsink,
        const char* scheme = "https");

    DLLLOCAL ~Http2Session();

    //! Returns true if this is a server-side session
    DLLLOCAL bool isServer() const { return is_server; }

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
        @return stream ID on success, -1 on error
    */
    DLLLOCAL int32_t submitRequest(const char* method, const char* path,
        const std::map<std::string, std::string>& headers,
        const void* body, size_t body_len, ExceptionSink* xsink);

    //! Submit a response (server-side)
    /** @param stream_id Stream ID for the response
        @param status_code HTTP status code
        @param headers Response headers
        @param body Response body (can be nullptr)
        @param body_len Length of response body
        @return 0 on success, -1 on error
    */
    DLLLOCAL int submitResponse(int32_t stream_id, int status_code,
        const std::map<std::string, std::string>& headers,
        const void* body, size_t body_len, ExceptionSink* xsink);

    //! Submit a PUSH_PROMISE (server-side)
    /** @param stream_id Stream ID of the associated request
        @param path Path of the pushed resource
        @param headers Headers for the pushed resource
        @return promised stream ID on success, -1 on error
    */
    DLLLOCAL int32_t submitPushPromise(int32_t stream_id, const char* path,
        const std::map<std::string, std::string>& headers, ExceptionSink* xsink);

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
        @return 0 on success, -1 on error
    */
    DLLLOCAL int sendStreamData(int32_t stream_id, const void* data, size_t len,
        bool end_stream, ExceptionSink* xsink);

    //! Set the stream type (for protocol upgrades like WebSocket/SSE)
    DLLLOCAL void setStreamType(int32_t stream_id, Http2StreamType type);

    //! Submit a response for an extended CONNECT request (RFC 8441)
    /** Used to accept WebSocket over HTTP/2 connections
        @param stream_id Stream ID for the CONNECT request
        @param status_code HTTP status code (200 to accept, 4xx to reject)
        @param headers Response headers
        @return 0 on success, -1 on error
    */
    DLLLOCAL int submitConnectResponse(int32_t stream_id, int status_code,
        const std::map<std::string, std::string>& headers, ExceptionSink* xsink);

    //! Returns true if RFC 8441 extended CONNECT protocol is enabled
    DLLLOCAL bool isConnectProtocolEnabled() const {
        return local_settings.enable_connect_protocol != 0;
    }

    //! Send all pending data (async, non-blocking)
    /** @param timeout_ms Timeout in milliseconds (ignored, always non-blocking)
        @return 0 on success, -1 if need to poll for POLLOUT
    */
    DLLLOCAL int sendPendingData(int timeout_ms, ExceptionSink* xsink);

    //! Send all pending data (blocking with timeout to ensure flush)
    /** @param timeout_ms Timeout in milliseconds
        @return 0 on success, -1 if still have data to send
    */
    DLLLOCAL int sendPendingDataBlocking(int timeout_ms, ExceptionSink* xsink);

    //! Receive and process data
    /** @param timeout_ms Timeout in milliseconds (-1 for infinite)
        @return 0 on success/timeout (data may have been received), 1 if connection was closed by peer, -1 on error
    */
    DLLLOCAL int receiveData(int timeout_ms, ExceptionSink* xsink);

    //! Returns true if there is pending request/response body data for stream_id
    DLLLOCAL bool hasPendingBodyData(int32_t stream_id) const;

    //! Get a stream by ID
    DLLLOCAL Http2StreamInfo* getStream(int32_t stream_id);

    //! Take available stream data without blocking
    /** @param stream_id stream ID
        @param max_bytes maximum bytes to return; 0 means all available
    */
    DLLLOCAL BinaryNode* takeStreamData(int32_t stream_id, size_t max_bytes, ExceptionSink* xsink);

    //! Take a completed stream (removes it from the session)
    DLLLOCAL std::unique_ptr<Http2StreamInfo> takeCompletedStream();

    //! Returns true if there are completed streams waiting
    DLLLOCAL bool hasCompletedStreams() const { return !completed_streams.empty(); }

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
    DLLLOCAL bool hasPendingData() const { return !send_buffer.empty(); }

    //! Get current settings
    DLLLOCAL Http2Settings getSettings() const { return local_settings; }

    //! Get remote peer settings
    DLLLOCAL Http2Settings getRemoteSettings() const { return remote_settings; }

    //! Get the underlying nghttp2 session handle
    DLLLOCAL nghttp2_session* getSession() const { return session; }

private:
    mutable std::recursive_mutex m;
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
    DLLLOCAL std::vector<nghttp2_nv> makeNv(const std::map<std::string, std::string>& headers);

    // Internal stream management
    DLLLOCAL Http2StreamInfo* getOrCreateStream(int32_t stream_id);
    DLLLOCAL void markStreamComplete(int32_t stream_id);

    qore_socket_private* sock;
    nghttp2_session* session = nullptr;
    bool is_server;
    std::string scheme;  //!< "https" for h2, "http" for h2c

    // Stream management
    std::map<int32_t, std::unique_ptr<Http2StreamInfo>> streams;
    std::queue<std::unique_ptr<Http2StreamInfo>> completed_streams;

    // Send buffer
    std::vector<char> send_buffer;
    size_t send_offset = 0;

    // Settings
    Http2Settings local_settings;
    Http2Settings remote_settings;

    // GOAWAY state
    bool goaway_received = false;
    bool goaway_sent = false;
    int32_t last_stream_id = 0;

    // Request body data for callbacks - owns the data
    struct BodyData {
        std::vector<uint8_t> data;
        size_t offset = 0;

        BodyData() = default;
        BodyData(const void* src, size_t len)
            : data(reinterpret_cast<const uint8_t*>(src),
                   reinterpret_cast<const uint8_t*>(src) + len) {}
    };
    std::map<int32_t, BodyData> pending_body_data;

    struct DataProviderContext {
        nghttp2_data_provider provider{};
        Http2Session* h2 = nullptr;
        bool defer_on_empty = false;
        bool no_end_stream = false;
        bool remove_on_empty = true;
    };
    std::map<int32_t, std::unique_ptr<DataProviderContext>> pending_data_providers;

    DLLLOCAL static ssize_t dataProviderReadCallback(nghttp2_session* session, int32_t stream_id,
        uint8_t* buf, size_t length, uint32_t* data_flags, nghttp2_data_source* source,
        void* user_data);
};

#endif // _QORE_HTTP2_SESSION_H
