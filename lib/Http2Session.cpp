/* -*- indent-tabs-mode: nil -*- */
/*
    Http2Session.cpp

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

#include <qore/Qore.h>

#include "qore/intern/Http2Session.h"
#include "qore/intern/qore_socket_private.h"

#include <algorithm>
#include <cctype>
#include <cinttypes>
#include <cstdlib>
#include <cstring>
#include <mutex>

static bool http2DebugEnabled() {
    static std::once_flag flag;
    static int enabled = 0;
    std::call_once(flag, []() {
        enabled = getenv("QORE_HTTP2_DEBUG") ? 1 : 0;
    });
    return enabled == 1;
}

// nghttp2 1.60.0 introduced nghttp2_session_mem_send2 with nghttp2_ssize
// Use the new API when available for better cross-platform compatibility
#include <nghttp2/nghttp2ver.h>
#if NGHTTP2_VERSION_NUM >= 0x013c00  // 1.60.0
#define QORE_USE_NGHTTP2_MEM_SEND2 1
#endif

Http2Session::Http2Session(qore_socket_private* sock, bool is_server, const char* scheme)
    : sock(sock), is_server(is_server), scheme(scheme ? scheme : "https") {
}

Http2Session::~Http2Session() {
    if (session) {
        nghttp2_session_del(session);
    }
}

Http2Session* Http2Session::createClient(qore_socket_private* sock, ExceptionSink* xsink,
        const char* scheme) {
    std::unique_ptr<Http2Session> h2(new Http2Session(sock, false, scheme));
    if (h2->init(xsink)) {
        return nullptr;
    }
    return h2.release();
}

Http2Session* Http2Session::createServer(qore_socket_private* sock, ExceptionSink* xsink,
        const char* scheme) {
    std::unique_ptr<Http2Session> h2(new Http2Session(sock, true, scheme));
    if (h2->init(xsink)) {
        return nullptr;
    }
    return h2.release();
}

int Http2Session::init(ExceptionSink* xsink) {
    std::lock_guard<std::recursive_mutex> lg(m);
    local_settings.initial_window_size = 1024 * 1024;
    nghttp2_session_callbacks* callbacks;
    if (nghttp2_session_callbacks_new(&callbacks) != 0) {
        xsink->raiseException("HTTP2-ERROR", "failed to create nghttp2 session callbacks");
        return -1;
    }

    // Set callbacks
    nghttp2_session_callbacks_set_send_callback(callbacks, sendCallback);
    nghttp2_session_callbacks_set_on_frame_recv_callback(callbacks, onFrameRecvCallback);
    nghttp2_session_callbacks_set_on_data_chunk_recv_callback(callbacks, onDataChunkRecvCallback);
    nghttp2_session_callbacks_set_on_stream_close_callback(callbacks, onStreamCloseCallback);
    nghttp2_session_callbacks_set_on_header_callback(callbacks, onHeaderCallback);
    nghttp2_session_callbacks_set_on_begin_headers_callback(callbacks, onBeginHeadersCallback);
    nghttp2_session_callbacks_set_on_frame_send_callback(callbacks, onFrameSendCallback);
    nghttp2_session_callbacks_set_on_invalid_frame_recv_callback(callbacks, onInvalidFrameRecvCallback);
    nghttp2_session_callbacks_set_on_invalid_header_callback(callbacks, onInvalidHeaderCallback);

    nghttp2_option* opts = nullptr;
    int rv = nghttp2_option_new(&opts);
    if (rv != 0) {
        nghttp2_session_callbacks_del(callbacks);
        xsink->raiseException("HTTP2-ERROR", "failed to create nghttp2 session options: %s",
            nghttp2_strerror(rv));
        return -1;
    }
    // We buffer DATA frames; manage WINDOW_UPDATE manually.
    nghttp2_option_set_no_auto_window_update(opts, 1);
    // RFC 8441: enable extended CONNECT support in nghttp2.

    if (is_server) {
        rv = nghttp2_session_server_new2(&session, callbacks, this, opts);
    } else {
        rv = nghttp2_session_client_new2(&session, callbacks, this, opts);
    }

    nghttp2_option_del(opts);
    nghttp2_session_callbacks_del(callbacks);

    if (rv != 0) {
        xsink->raiseException("HTTP2-ERROR", "failed to create nghttp2 session: %s",
            nghttp2_strerror(rv));
        return -1;
    }

    return 0;
}

int Http2Session::sendConnectionPreface(ExceptionSink* xsink) {
    std::lock_guard<std::recursive_mutex> lg(m);
    // Both client and server must send SETTINGS frame as part of the connection preface
    // For client: magic string + SETTINGS
    // For server: SETTINGS (sent in response to client preface)
    //
    // Note: SETTINGS_ENABLE_PUSH MUST NOT be sent by servers with value 1
    // (RFC 7540 Section 6.5.2: "An endpoint MUST NOT send a SETTINGS frame with
    // SETTINGS_ENABLE_PUSH set to 1 when it is not a client.")
    // So we only include it for client sessions.

    std::vector<nghttp2_settings_entry> iv;
    iv.push_back({NGHTTP2_SETTINGS_HEADER_TABLE_SIZE, local_settings.header_table_size});
    if (!is_server) {
        // Only clients can send ENABLE_PUSH setting
        iv.push_back({NGHTTP2_SETTINGS_ENABLE_PUSH, local_settings.enable_push});
    }
    iv.push_back({NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, local_settings.max_concurrent_streams});
    iv.push_back({NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE, local_settings.initial_window_size});
    iv.push_back({NGHTTP2_SETTINGS_MAX_FRAME_SIZE, local_settings.max_frame_size});
    iv.push_back({NGHTTP2_SETTINGS_MAX_HEADER_LIST_SIZE, local_settings.max_header_list_size});
    // RFC 8441: Enable extended CONNECT protocol for WebSocket over HTTP/2
    if (local_settings.enable_connect_protocol) {
        iv.push_back({NGHTTP2_SETTINGS_ENABLE_CONNECT_PROTOCOL, local_settings.enable_connect_protocol});
    }

    int rv = nghttp2_submit_settings(session, NGHTTP2_FLAG_NONE, iv.data(), iv.size());
    if (rv != 0) {
        xsink->raiseException("HTTP2-ERROR", "failed to submit SETTINGS: %s",
            nghttp2_strerror(rv));
        return -1;
    }
    // Use blocking version to ensure preface is sent completely
    return sendPendingDataBlocking(-1, xsink);
}

int Http2Session::submitSettings(const Http2Settings& settings, ExceptionSink* xsink) {
    std::lock_guard<std::recursive_mutex> lg(m);
    local_settings = settings;
    if (http2DebugEnabled()) {
        fprintf(stderr, "HTTP2 DEBUG: send SETTINGS enable_connect_protocol=%u\n",
            settings.enable_connect_protocol);
        fflush(stderr);
    }

    // Note: SETTINGS_ENABLE_PUSH MUST NOT be sent by servers with value 1
    std::vector<nghttp2_settings_entry> iv;
    iv.push_back({NGHTTP2_SETTINGS_HEADER_TABLE_SIZE, settings.header_table_size});
    if (!is_server) {
        iv.push_back({NGHTTP2_SETTINGS_ENABLE_PUSH, settings.enable_push});
    }
    iv.push_back({NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, settings.max_concurrent_streams});
    iv.push_back({NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE, settings.initial_window_size});
    iv.push_back({NGHTTP2_SETTINGS_MAX_FRAME_SIZE, settings.max_frame_size});
    iv.push_back({NGHTTP2_SETTINGS_MAX_HEADER_LIST_SIZE, settings.max_header_list_size});
    // RFC 8441: Enable extended CONNECT protocol for WebSocket over HTTP/2
    if (settings.enable_connect_protocol) {
        iv.push_back({NGHTTP2_SETTINGS_ENABLE_CONNECT_PROTOCOL, settings.enable_connect_protocol});
    }

    int rv = nghttp2_submit_settings(session, NGHTTP2_FLAG_NONE, iv.data(), iv.size());
    if (rv != 0) {
        xsink->raiseException("HTTP2-ERROR", "failed to submit SETTINGS: %s",
            nghttp2_strerror(rv));
        return -1;
    }
    return 0;
}

std::vector<nghttp2_nv> Http2Session::makeNv(const std::map<std::string, std::string>& headers) {
    std::vector<nghttp2_nv> nva;
    nva.reserve(headers.size());

    for (const auto& h : headers) {
        nghttp2_nv nv;
        nv.name = reinterpret_cast<uint8_t*>(const_cast<char*>(h.first.c_str()));
        nv.value = reinterpret_cast<uint8_t*>(const_cast<char*>(h.second.c_str()));
        nv.namelen = h.first.size();
        nv.valuelen = h.second.size();
        nv.flags = NGHTTP2_NV_FLAG_NONE;
        nva.push_back(nv);
    }

    return nva;
}

static std::string toLowerHeaderName(const std::string& name) {
    std::string out(name);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

int32_t Http2Session::submitRequest(const char* method, const char* path,
        const std::map<std::string, std::string>& headers,
        const void* body, size_t body_len, ExceptionSink* xsink) {
    std::lock_guard<std::recursive_mutex> lg(m);
    if (is_server) {
        xsink->raiseException("HTTP2-ERROR", "cannot submit request on server session");
        return -1;
    }

    // Build pseudo-headers
    std::vector<nghttp2_nv> nva;
    nva.reserve(headers.size() + 5);  // +5 for :method, :path, :scheme, :authority, :protocol

    // Add pseudo-headers first
    std::string method_str(method);
    std::string path_str(path);
    std::string scheme_str(scheme);  // Use stored scheme for h2c support
    std::string authority;
    std::string protocol;  // RFC 8441: :protocol pseudo-header for extended CONNECT

    // Get authority from headers if present
    auto it = headers.find("host");
    if (it != headers.end()) {
        authority = it->second;
    } else {
        it = headers.find("Host");
        if (it != headers.end()) {
            authority = it->second;
        }
    }

    // RFC 8441: Get :protocol for extended CONNECT (e.g., "websocket")
    it = headers.find(":protocol");
    if (it != headers.end()) {
        protocol = it->second;
    }

    nghttp2_nv nv_method = {
        reinterpret_cast<uint8_t*>(const_cast<char*>(":method")),
        reinterpret_cast<uint8_t*>(const_cast<char*>(method_str.c_str())),
        7, method_str.size(), NGHTTP2_NV_FLAG_NONE
    };
    nva.push_back(nv_method);

    // RFC 8441: For extended CONNECT, :path and :scheme are still required (unlike regular CONNECT)
    nghttp2_nv nv_path = {
        reinterpret_cast<uint8_t*>(const_cast<char*>(":path")),
        reinterpret_cast<uint8_t*>(const_cast<char*>(path_str.c_str())),
        5, path_str.size(), NGHTTP2_NV_FLAG_NONE
    };
    nva.push_back(nv_path);

    nghttp2_nv nv_scheme = {
        reinterpret_cast<uint8_t*>(const_cast<char*>(":scheme")),
        reinterpret_cast<uint8_t*>(const_cast<char*>(scheme_str.c_str())),
        7, scheme_str.size(), NGHTTP2_NV_FLAG_NONE
    };
    nva.push_back(nv_scheme);

    if (!authority.empty()) {
        nghttp2_nv nv_authority = {
            reinterpret_cast<uint8_t*>(const_cast<char*>(":authority")),
            reinterpret_cast<uint8_t*>(const_cast<char*>(authority.c_str())),
            10, authority.size(), NGHTTP2_NV_FLAG_NONE
        };
        nva.push_back(nv_authority);
    }

    // RFC 8441: Add :protocol pseudo-header for extended CONNECT (e.g., WebSocket over HTTP/2)
    if (!protocol.empty()) {
        nghttp2_nv nv_protocol = {
            reinterpret_cast<uint8_t*>(const_cast<char*>(":protocol")),
            reinterpret_cast<uint8_t*>(const_cast<char*>(protocol.c_str())),
            9, protocol.size(), NGHTTP2_NV_FLAG_NONE
        };
        nva.push_back(nv_protocol);
    }

    // Add regular headers (excluding pseudo-headers, host, and connection-specific headers)
    // RFC 7540 Section 8.1.2.2: HTTP/2 MUST NOT use connection-specific header fields
    std::vector<std::string> lowered_names;
    lowered_names.reserve(headers.size());
    for (const auto& h : headers) {
        std::string lname = toLowerHeaderName(h.first);
        // Skip pseudo-headers and host (handled separately)
        if (lname[0] == ':' || lname == "host") {
            continue;
        }
        // Skip connection-specific headers (forbidden in HTTP/2)
        if (lname == "connection" || lname == "keep-alive" || lname == "proxy-connection" ||
            lname == "transfer-encoding" || lname == "upgrade") {
            continue;
        }
        lowered_names.push_back(lname);
        nghttp2_nv nv = {
            reinterpret_cast<uint8_t*>(const_cast<char*>(lowered_names.back().c_str())),
            reinterpret_cast<uint8_t*>(const_cast<char*>(h.second.c_str())),
            lowered_names.back().size(), h.second.size(), NGHTTP2_NV_FLAG_NONE
        };
        nva.push_back(nv);
    }

    nghttp2_data_provider* data_prd = nullptr;
    std::unique_ptr<DataProviderContext> provider_ctx;

    // RFC 8441: For extended CONNECT (WebSocket over HTTP/2), we must NOT send END_STREAM
    // because we need to send data after the handshake completes.
    // Check if this is a CONNECT request with :protocol (extended CONNECT)
    bool is_extended_connect = (method_str == "CONNECT" && !protocol.empty());

    // RFC 8441: Client SHOULD NOT send extended CONNECT if server hasn't advertised
    // SETTINGS_ENABLE_CONNECT_PROTOCOL.  Only check if we've already received remote settings
    // (remote_settings_received is set when we process the peer's SETTINGS frame).
    if (is_extended_connect && remote_settings_received
            && !remote_settings.enable_connect_protocol) {
        xsink->raiseException("HTTP2-CONNECT-ERROR",
            "server does not support extended CONNECT protocol (RFC 8441); "
            "SETTINGS_ENABLE_CONNECT_PROTOCOL was not advertised");
        return -1;
    }

    bool has_body = false;
    BodyData pending_data;
    if (body && body_len > 0) {
        // Store body data for callback (copies the data) after stream_id is known
        pending_data = BodyData(body, body_len);
        has_body = true;
    }
    if (has_body || is_extended_connect) {
        provider_ctx = std::make_unique<DataProviderContext>();
        provider_ctx->h2 = this;
        provider_ctx->provider.source.ptr = provider_ctx.get();
        provider_ctx->provider.read_callback = dataProviderReadCallback;
        provider_ctx->defer_on_empty = is_extended_connect;
        provider_ctx->no_end_stream = is_extended_connect;
        provider_ctx->remove_on_empty = !is_extended_connect;
        data_prd = &provider_ctx->provider;
    }

    int32_t stream_id = nghttp2_submit_request(session, nullptr, nva.data(), nva.size(),
        data_prd, this);

    if (stream_id < 0) {
        xsink->raiseException("HTTP2-ERROR", "failed to submit request: %s",
            nghttp2_strerror(stream_id));
        return -1;
    }

    if (has_body) {
        pending_body_data.emplace(stream_id, std::move(pending_data));
    }
    if (provider_ctx) {
        pending_data_providers.emplace(stream_id, std::move(provider_ctx));
    }

    // Create stream info and mark as streaming for extended CONNECT
    Http2StreamInfo* stream = getOrCreateStream(stream_id);
    if (stream) {
        stream->is_connect = (method_str == "CONNECT");
        if (!protocol.empty()) {
            stream->connect_protocol = protocol;
        }
    }
    if (is_extended_connect && stream) {
        stream->stream_type = Http2StreamType::WebSocket;  // Mark as bidirectional streaming
    }

    return stream_id;
}

int Http2Session::submitResponse(int32_t stream_id, int status_code,
        const std::map<std::string, std::string>& headers,
        const void* body, size_t body_len, ExceptionSink* xsink) {
    std::lock_guard<std::recursive_mutex> lg(m);
    if (!is_server) {
        xsink->raiseException("HTTP2-ERROR", "cannot submit response on client session");
        return -1;
    }

    // Build response headers
    std::vector<nghttp2_nv> nva;
    nva.reserve(headers.size() + 1);

    // Add :status pseudo-header
    std::string status_str = std::to_string(status_code);
    nghttp2_nv nv_status = {
        reinterpret_cast<uint8_t*>(const_cast<char*>(":status")),
        reinterpret_cast<uint8_t*>(const_cast<char*>(status_str.c_str())),
        7, status_str.size(), NGHTTP2_NV_FLAG_NONE
    };
    nva.push_back(nv_status);

    // Add regular headers, filtering out connection-specific headers
    std::vector<std::string> lowered_names;
    lowered_names.reserve(headers.size());
    for (const auto& h : headers) {
        std::string lname = toLowerHeaderName(h.first);
        if (lname[0] == ':') {
            continue;
        }
        // Skip connection-specific headers (forbidden in HTTP/2)
        if (lname == "connection" || lname == "keep-alive" || lname == "proxy-connection" ||
            lname == "transfer-encoding" || lname == "upgrade") {
            continue;
        }
        lowered_names.push_back(lname);
        nghttp2_nv nv = {
            reinterpret_cast<uint8_t*>(const_cast<char*>(lowered_names.back().c_str())),
            reinterpret_cast<uint8_t*>(const_cast<char*>(h.second.c_str())),
            lowered_names.back().size(), h.second.size(), NGHTTP2_NV_FLAG_NONE
        };
        nva.push_back(nv);
    }

    nghttp2_data_provider* data_prd = nullptr;
    std::unique_ptr<DataProviderContext> provider_ctx;

    if (body && body_len > 0) {
        // Store body data for callback (copies the data)
        pending_body_data[stream_id] = BodyData(body, body_len);

        provider_ctx = std::make_unique<DataProviderContext>();
        provider_ctx->h2 = this;
        provider_ctx->provider.source.ptr = provider_ctx.get();
        provider_ctx->provider.read_callback = dataProviderReadCallback;
        data_prd = &provider_ctx->provider;
    }

    printd(5, "submitResponse() stream_id=%d status=%d body_len=%zu nva.size=%zu\n",
        stream_id, status_code, body_len, nva.size());

    int rv = nghttp2_submit_response(session, stream_id, nva.data(), nva.size(), data_prd);
    if (rv != 0) {
        xsink->raiseException("HTTP2-ERROR", "failed to submit response: %s",
            nghttp2_strerror(rv));
        printd(5, "submitResponse() FAILED: %s\n", nghttp2_strerror(rv));
        return -1;
    }

    printd(5, "submitResponse() SUCCESS: stream_id=%d want_write=%d want_read=%d\n",
        stream_id, nghttp2_session_want_write(session), nghttp2_session_want_read(session));
    if (provider_ctx) {
        pending_data_providers.emplace(stream_id, std::move(provider_ctx));
    }
    return 0;
}

int Http2Session::submitResponseStreaming(int32_t stream_id, int status_code,
        const std::map<std::string, std::string>& headers, ExceptionSink* xsink) {
    std::lock_guard<std::recursive_mutex> lg(m);
    if (!is_server) {
        xsink->raiseException("HTTP2-ERROR", "cannot submit response on client session");
        return -1;
    }

    // Build response headers
    std::vector<nghttp2_nv> nva;
    nva.reserve(headers.size() + 1);

    // Add :status pseudo-header
    std::string status_str = std::to_string(status_code);
    nghttp2_nv nv_status = {
        reinterpret_cast<uint8_t*>(const_cast<char*>(":status")),
        reinterpret_cast<uint8_t*>(const_cast<char*>(status_str.c_str())),
        7, status_str.size(), NGHTTP2_NV_FLAG_NONE
    };
    nva.push_back(nv_status);

    // Add regular headers, filtering out connection-specific headers
    std::vector<std::string> lowered_names;
    lowered_names.reserve(headers.size());
    for (const auto& h : headers) {
        std::string lname = toLowerHeaderName(h.first);
        if (lname[0] == ':') {
            continue;
        }
        if (lname == "connection" || lname == "keep-alive" || lname == "proxy-connection" ||
            lname == "transfer-encoding" || lname == "upgrade") {
            continue;
        }
        lowered_names.push_back(lname);
        nghttp2_nv nv = {
            reinterpret_cast<uint8_t*>(const_cast<char*>(lowered_names.back().c_str())),
            reinterpret_cast<uint8_t*>(const_cast<char*>(h.second.c_str())),
            lowered_names.back().size(), h.second.size(), NGHTTP2_NV_FLAG_NONE
        };
        nva.push_back(nv);
    }

    // Create a deferred data provider - body will be fed via sendStreamData()
    auto provider_ctx = std::make_unique<DataProviderContext>();
    provider_ctx->h2 = this;
    provider_ctx->provider.source.ptr = provider_ctx.get();
    provider_ctx->provider.read_callback = dataProviderReadCallback;
    provider_ctx->defer_on_empty = true;
    provider_ctx->no_end_stream = false;
    provider_ctx->remove_on_empty = true;

    printd(5, "submitResponseStreaming() stream_id=%d status=%d nva.size=%zu\n",
        stream_id, status_code, nva.size());

    int rv = nghttp2_submit_response(session, stream_id, nva.data(), nva.size(),
        &provider_ctx->provider);
    if (rv != 0) {
        xsink->raiseException("HTTP2-ERROR", "failed to submit streaming response: %s",
            nghttp2_strerror(rv));
        return -1;
    }

    pending_data_providers.emplace(stream_id, std::move(provider_ctx));
    printd(5, "submitResponseStreaming() SUCCESS stream_id=%d\n", stream_id);
    return 0;
}

int32_t Http2Session::submitPushPromise(int32_t stream_id, const char* path,
        const std::map<std::string, std::string>& headers, ExceptionSink* xsink) {
    std::lock_guard<std::recursive_mutex> lg(m);
    if (!is_server) {
        xsink->raiseException("HTTP2-ERROR", "cannot submit push promise on client session");
        return -1;
    }

    // Build push promise headers
    std::vector<nghttp2_nv> nva;
    nva.reserve(headers.size() + 3);

    // Add pseudo-headers
    std::string method_str("GET");
    std::string path_str(path);
    std::string scheme_str(scheme);  // Use stored scheme for h2c support

    nghttp2_nv nv_method = {
        reinterpret_cast<uint8_t*>(const_cast<char*>(":method")),
        reinterpret_cast<uint8_t*>(const_cast<char*>(method_str.c_str())),
        7, method_str.size(), NGHTTP2_NV_FLAG_NONE
    };
    nva.push_back(nv_method);

    nghttp2_nv nv_path = {
        reinterpret_cast<uint8_t*>(const_cast<char*>(":path")),
        reinterpret_cast<uint8_t*>(const_cast<char*>(path_str.c_str())),
        5, path_str.size(), NGHTTP2_NV_FLAG_NONE
    };
    nva.push_back(nv_path);

    nghttp2_nv nv_scheme = {
        reinterpret_cast<uint8_t*>(const_cast<char*>(":scheme")),
        reinterpret_cast<uint8_t*>(const_cast<char*>(scheme_str.c_str())),
        7, scheme_str.size(), NGHTTP2_NV_FLAG_NONE
    };
    nva.push_back(nv_scheme);

    // Add regular headers
    for (const auto& h : headers) {
        if (h.first[0] == ':') {
            continue;
        }
        nghttp2_nv nv = {
            reinterpret_cast<uint8_t*>(const_cast<char*>(h.first.c_str())),
            reinterpret_cast<uint8_t*>(const_cast<char*>(h.second.c_str())),
            h.first.size(), h.second.size(), NGHTTP2_NV_FLAG_NONE
        };
        nva.push_back(nv);
    }

    int32_t promised_stream_id = nghttp2_submit_push_promise(session, NGHTTP2_FLAG_NONE,
        stream_id, nva.data(), nva.size(), nullptr);

    if (promised_stream_id < 0) {
        xsink->raiseException("HTTP2-ERROR", "failed to submit push promise: %s",
            nghttp2_strerror(promised_stream_id));
        return -1;
    }

    return promised_stream_id;
}

int Http2Session::submitRstStream(int32_t stream_id, uint32_t error_code, ExceptionSink* xsink) {
    std::lock_guard<std::recursive_mutex> lg(m);
    int rv = nghttp2_submit_rst_stream(session, NGHTTP2_FLAG_NONE, stream_id, error_code);
    if (rv != 0) {
        if (xsink) {
            xsink->raiseException("HTTP2-ERROR", "failed to submit RST_STREAM: %s",
                nghttp2_strerror(rv));
        }
        return -1;
    }
    return 0;
}

int Http2Session::submitGoaway(uint32_t error_code, const char* opaque_data,
        size_t len, ExceptionSink* xsink) {
    std::lock_guard<std::recursive_mutex> lg(m);
    int32_t last_id = nghttp2_session_get_last_proc_stream_id(session);
    int rv = nghttp2_submit_goaway(session, NGHTTP2_FLAG_NONE, last_id, error_code,
        reinterpret_cast<const uint8_t*>(opaque_data), len);
    if (rv != 0) {
        if (xsink) {
            xsink->raiseException("HTTP2-ERROR", "failed to submit GOAWAY: %s",
                nghttp2_strerror(rv));
        }
        return -1;
    }
    goaway_sent = true;
    return 0;
}

int Http2Session::submitPing(const uint8_t* opaque_data, ExceptionSink* xsink) {
    std::lock_guard<std::recursive_mutex> lg(m);
    int rv = nghttp2_submit_ping(session, NGHTTP2_FLAG_NONE, opaque_data);
    if (rv != 0) {
        if (xsink) {
            xsink->raiseException("HTTP2-ERROR", "failed to submit PING: %s",
                nghttp2_strerror(rv));
        }
        return -1;
    }
    return 0;
}

int Http2Session::submitWindowUpdate(int32_t stream_id, int32_t increment, ExceptionSink* xsink) {
    std::lock_guard<std::recursive_mutex> lg(m);
    int rv = nghttp2_submit_window_update(session, NGHTTP2_FLAG_NONE, stream_id, increment);
    if (rv != 0) {
        if (xsink) {
            xsink->raiseException("HTTP2-ERROR", "failed to submit WINDOW_UPDATE: %s",
                nghttp2_strerror(rv));
        }
        return -1;
    }
    return 0;
}

int Http2Session::submitPriority(int32_t stream_id, int32_t dependency, int32_t weight,
        bool exclusive, ExceptionSink* xsink) {
    std::lock_guard<std::recursive_mutex> lg(m);
    nghttp2_priority_spec pri_spec;
    nghttp2_priority_spec_init(&pri_spec, dependency, weight, exclusive ? 1 : 0);

    int rv = nghttp2_submit_priority(session, NGHTTP2_FLAG_NONE, stream_id, &pri_spec);
    if (rv != 0) {
        if (xsink) {
            xsink->raiseException("HTTP2-ERROR", "failed to submit PRIORITY: %s",
                nghttp2_strerror(rv));
        }
        return -1;
    }

    // Update stream info
    Http2StreamInfo* stream = getStream(stream_id);
    if (stream) {
        stream->weight = weight;
        stream->dependency = dependency;
        stream->exclusive = exclusive;
    }

    return 0;
}

int Http2Session::sendPendingData(int timeout_ms, ExceptionSink* xsink) {
    std::lock_guard<std::recursive_mutex> lg(m);
    printd(5, "sendPendingData() want_write=%d isServer=%d timeout_ms=%d send_buffer.size=%zu\n",
        nghttp2_session_want_write(session), is_server, timeout_ms, send_buffer.size());
    // First, collect data from nghttp2; drain until it reports no more data.
    // We intentionally loop until nghttp2_session_mem_send* returns 0 to avoid
    // relying on nghttp2_session_want_write() as a loop condition.
    size_t total_collected = 0;
    while (true) {
        const uint8_t* data;
#ifdef QORE_USE_NGHTTP2_MEM_SEND2
        nghttp2_ssize len = nghttp2_session_mem_send2(session, &data);
#else
        ssize_t len = nghttp2_session_mem_send(session, &data);
#endif
        printd(5, "sendPendingData() nghttp2_session_mem_send len=%zd\n", (ssize_t)len);
        if (len < 0) {
#ifdef QORE_USE_NGHTTP2_MEM_SEND2
            xsink->raiseException("HTTP2-ERROR", "nghttp2_session_mem_send2 failed: %s",
#else
            xsink->raiseException("HTTP2-ERROR", "nghttp2_session_mem_send failed: %s",
#endif
                nghttp2_strerror(static_cast<int>(len)));
            return -1;
        }
        if (len == 0) {
            break;
        }
        send_buffer.insert(send_buffer.end(), data, data + len);
        total_collected += len;
    }
    printd(5, "sendPendingData() collected %zu bytes from nghttp2, send_buffer.size=%zu\n",
        total_collected, send_buffer.size());

    // Send buffered data using proper async I/O pattern (like SocketSendPollState::continuePoll)
    if (!send_buffer.empty()) {
        printd(5, "sendPendingData() sending %zu bytes to socket (ssl=%p)\n", send_buffer.size(), sock->ssl);
        {
            size_t off = 0;
            int count = 0;
            while (off + 9 <= send_buffer.size() && count < 32) {
                const uint8_t* hdr = reinterpret_cast<const uint8_t*>(&send_buffer[off]);
                uint32_t length = (uint32_t(hdr[0]) << 16) | (uint32_t(hdr[1]) << 8) | uint32_t(hdr[2]);
                uint8_t type = hdr[3];
                uint8_t flags = hdr[4];
                uint32_t stream_id = ((uint32_t(hdr[5]) & 0x7f) << 24) | (uint32_t(hdr[6]) << 16)
                    | (uint32_t(hdr[7]) << 8) | uint32_t(hdr[8]);
                printd(5, "sendPendingData() frame[%d] len=%u type=%u flags=0x%x stream_id=%u\n",
                    count, length, type, flags, stream_id);
                size_t frame_size = 9 + length;
                if (off + frame_size > send_buffer.size()) {
                    printd(5, "sendPendingData() frame[%d] truncated: off=%zu frame_size=%zu buf=%zu\n",
                        count, off, frame_size, send_buffer.size());
                    break;
                }
                off += frame_size;
                ++count;
            }
        }

        // Always use non-blocking mode for async I/O
        OptionalNonBlockingHelper nbh(*sock, true, xsink);
        if (*xsink) {
            return -1;
        }

        // Loop until buffer is empty or we need to poll
        while (!send_buffer.empty()) {
            ssize_t rc;
            if (sock->ssl) {
                // Use doNonBlockingIo for proper async SSL I/O
                // Returns: SOCK_POLLIN/SOCK_POLLOUT if need to poll, 0 if done, < 0 on error
                size_t real_io = 0;
                printd(5, "sendPendingData() calling doNonBlockingIo to_send=%zu\n", send_buffer.size());
                rc = sock->ssl->doNonBlockingIo(xsink, "sendPendingData",
                    const_cast<char*>(reinterpret_cast<const char*>(send_buffer.data())),
                    send_buffer.size(), SslAction::WRITE, real_io);
                printd(5, "sendPendingData() doNonBlockingIo rc=%zd real_io=%zu xsink=%d\n", rc, real_io, (int)*xsink);
                if (*xsink) {
                    return -1;
                }

                // Erase sent data immediately (even if we need to poll after)
                if (real_io > 0) {
                    send_buffer.erase(send_buffer.begin(), send_buffer.begin() + real_io);
                    printd(5, "sendPendingData() erased %zu bytes, remaining=%zu\n", real_io, send_buffer.size());
                }

                if (!rc) {
                    // Data was written successfully and SSL doesn't need to poll - continue loop
                    continue;
                }
                if (rc == SOCK_POLLOUT || rc == SOCK_POLLIN) {
                    // SSL needs to wait for socket - return the actual poll direction
                    // Note: SOCK_POLLIN can happen during TLS renegotiation even when writing
                    printd(5, "sendPendingData() SSL needs poll for %s, remaining=%zu\n",
                        rc == SOCK_POLLIN ? "POLLIN" : "POLLOUT", send_buffer.size());
                    return rc;  // Return actual poll direction needed
                }
                // rc < 0 but no exception - shouldn't happen
                printd(5, "sendPendingData() SSL unexpected: rc=%zd (breaking)\n", rc);
                break;
            } else {
                // Non-SSL: use regular send
                rc = ::send(sock->sock, send_buffer.data(), send_buffer.size(), 0);
                printd(5, "sendPendingData() send rc=%zd errno=%d\n", rc, errno);
                if (rc >= 0) {
                    // Erase sent data immediately
                    if (rc > 0) {
                        send_buffer.erase(send_buffer.begin(), send_buffer.begin() + rc);
                    }
                    continue;
                }
                sock_get_error();
                if (errno == EINTR) {
                    continue;
                }
                if (errno == EAGAIN
#ifdef EWOULDBLOCK
                    || errno == EWOULDBLOCK
#endif
                ) {
                    // Would block - signal poll needed
                    printd(5, "sendPendingData() socket would block, remaining=%zu\n", send_buffer.size());
                    return SOCK_POLLOUT;  // Need to poll for POLLOUT and retry
                }
                xsink->raiseErrnoException("SOCKET-SEND-ERROR", errno, "error while sending HTTP/2 data");
                return -1;
            }
        }

        printd(5, "sendPendingData() all data sent, buffer empty\n");
    } else {
        printd(5, "sendPendingData() no data to send\n");
    }

    printd(5, "sendPendingData() complete, want_write=%d send_buffer.size=%zu\n",
        nghttp2_session_want_write(session), send_buffer.size());
    return 0;
}

int Http2Session::sendPendingDataBlocking(int timeout_ms, ExceptionSink* xsink) {
    std::lock_guard<std::recursive_mutex> lg(m);
    printd(5, "sendPendingDataBlocking() want_write=%d send_buffer.size=%zu timeout_ms=%d\n",
        nghttp2_session_want_write(session), send_buffer.size(), timeout_ms);

    // First, collect any additional data from nghttp2
    while (true) {
        const uint8_t* data;
#ifdef QORE_USE_NGHTTP2_MEM_SEND2
        nghttp2_ssize len = nghttp2_session_mem_send2(session, &data);
#else
        ssize_t len = nghttp2_session_mem_send(session, &data);
#endif
        printd(5, "sendPendingDataBlocking() nghttp2_session_mem_send len=%zd\n", (ssize_t)len);
        if (len < 0) {
#ifdef QORE_USE_NGHTTP2_MEM_SEND2
            xsink->raiseException("HTTP2-ERROR", "nghttp2_session_mem_send2 failed: %s",
#else
            xsink->raiseException("HTTP2-ERROR", "nghttp2_session_mem_send failed: %s",
#endif
                nghttp2_strerror(static_cast<int>(len)));
            return -1;
        }
        if (len == 0) {
            break;
        }
        send_buffer.insert(send_buffer.end(), data, data + len);
    }

    // Send buffered data using blocking I/O with timeout
    if (!send_buffer.empty()) {
        printd(5, "sendPendingDataBlocking() sending %zu bytes with blocking timeout=%d\n",
            send_buffer.size(), timeout_ms);
        {
            size_t off = 0;
            int count = 0;
            while (off + 9 <= send_buffer.size() && count < 32) {
                const uint8_t* hdr = reinterpret_cast<const uint8_t*>(&send_buffer[off]);
                uint32_t length = (uint32_t(hdr[0]) << 16) | (uint32_t(hdr[1]) << 8) | uint32_t(hdr[2]);
                uint8_t type = hdr[3];
                uint8_t flags = hdr[4];
                uint32_t stream_id = ((uint32_t(hdr[5]) & 0x7f) << 24) | (uint32_t(hdr[6]) << 16)
                    | (uint32_t(hdr[7]) << 8) | uint32_t(hdr[8]);
                printd(5, "sendPendingDataBlocking() frame[%d] len=%u type=%u flags=0x%x stream_id=%u\n",
                    count, length, type, flags, stream_id);
                size_t frame_size = 9 + length;
                if (off + frame_size > send_buffer.size()) {
                    printd(5, "sendPendingDataBlocking() frame[%d] truncated: off=%zu frame_size=%zu buf=%zu\n",
                        count, off, frame_size, send_buffer.size());
                    break;
                }
                off += frame_size;
                ++count;
            }
        }

        int64 total_sent = 0;
        ssize_t rc = sock->sendIntern(xsink, "Http2Session", "sendPendingDataBlocking",
            reinterpret_cast<const char*>(send_buffer.data()), send_buffer.size(),
            timeout_ms, total_sent);

        printd(5, "sendPendingDataBlocking() sendIntern returned rc=%zd total_sent=%" PRId64 "\n", rc, total_sent);

        if (total_sent > 0) {
            send_buffer.erase(send_buffer.begin(), send_buffer.begin() + total_sent);
        }
        printd(5, "sendPendingDataBlocking() remaining send_buffer.size=%zu\n", send_buffer.size());

        if (!send_buffer.empty()) {
            printd(5, "sendPendingDataBlocking() buffer not empty, remaining=%zu\n", send_buffer.size());
            return -1;  // Still have data to send
        }

        if (*xsink) {
            return -1;
        }
    }

    printd(5, "sendPendingDataBlocking() complete, want_write=%d send_buffer.size=%zu\n",
        nghttp2_session_want_write(session), send_buffer.size());
    return 0;
}

int Http2Session::receiveData(int timeout_ms, ExceptionSink* xsink) {
    std::lock_guard<std::recursive_mutex> lg(m);
    char* buf;
    printd(5, "receiveData() ENTRY fd=%d isServer=%d timeout_ms=%d\n", sock->sock, is_server, timeout_ms);
    // Use suppress_exception=true to avoid exception on timeout
    ssize_t len = sock->brecv(xsink, "receiveData", buf, 16384, 0, timeout_ms, false, true);
    printd(5, "receiveData() brecv len=%zd fd=%d isServer=%d xsink=%d\n", len, sock->sock, is_server, (int)*xsink);
    if (len < 0) {
        // Check for timeout (QSE_TIMEOUT = -3)
        if (len == QSE_TIMEOUT) {
            printd(5, "receiveData() timeout (no data)\n");
            return 0;  // Not an error, just no data available
        }
        printd(5, "receiveData() error len=%zd\n", len);
        return -1;
    }
    if (len == 0) {
        // EOF received - connection was closed by peer
        printd(5, "receiveData() len=0 (connection closed by peer)\n");
        return 1;  // Connection closed - distinct from timeout (0)
    }
    if (len >= 9) {
        uint8_t* hdr = reinterpret_cast<uint8_t*>(buf);
        uint32_t flen = (uint32_t(hdr[0]) << 16) | (uint32_t(hdr[1]) << 8) | uint32_t(hdr[2]);
        uint8_t ftype = hdr[3];
        uint8_t fflags = hdr[4];
        uint32_t fstream = ((uint32_t(hdr[5]) & 0x7f) << 24) | (uint32_t(hdr[6]) << 16)
            | (uint32_t(hdr[7]) << 8) | uint32_t(hdr[8]);
        if (local_settings.max_frame_size && flen > local_settings.max_frame_size) {
            printd(5, "receiveData() first frame header len=%u type=%u flags=0x%x stream_id=%u local_max=%u\n",
                flen, ftype, fflags, fstream, local_settings.max_frame_size);
        }
    }

    ssize_t rv = nghttp2_session_mem_recv(session, reinterpret_cast<uint8_t*>(buf), len);
    printd(5, "receiveData() nghttp2_session_mem_recv rv=%zd (input len=%zd)\n", rv, len);
    if (rv < 0) {
        printd(1, "receiveData() nghttp2_session_mem_recv error: %s (rv=%zd)\n",
            nghttp2_strerror(static_cast<int>(rv)), rv);
        xsink->raiseException("HTTP2-ERROR", "nghttp2_session_mem_recv failed: %s",
            nghttp2_strerror(static_cast<int>(rv)));
        return -1;
    }

    return 0;
}

bool Http2Session::hasPendingBodyData(int32_t stream_id) const {
    std::lock_guard<std::recursive_mutex> lg(m);
    return pending_body_data.find(stream_id) != pending_body_data.end();
}

Http2StreamInfo* Http2Session::getStream(int32_t stream_id) {
    std::lock_guard<std::recursive_mutex> lg(m);
    auto it = streams.find(stream_id);
    if (it != streams.end()) {
        return it->second.get();
    }
    return nullptr;
}

BinaryNode* Http2Session::takeStreamData(int32_t stream_id, size_t max_bytes, ExceptionSink* xsink) {
    std::lock_guard<std::recursive_mutex> lg(m);
    (void)xsink;
    auto it = streams.find(stream_id);
    if (it == streams.end() || it->second->body.empty()) {
        return nullptr;
    }

    size_t avail = it->second->body.size();
    size_t take = (!max_bytes || max_bytes > avail) ? avail : max_bytes;
    if (!take) {
        return nullptr;
    }

    BinaryNode* rv = new BinaryNode();
    rv->append(it->second->body.data(), take);
    it->second->body.erase(it->second->body.begin(), it->second->body.begin() + take);
    return rv;
}

Http2StreamInfo* Http2Session::getOrCreateStream(int32_t stream_id) {
    std::lock_guard<std::recursive_mutex> lg(m);
    auto it = streams.find(stream_id);
    if (it != streams.end()) {
        return it->second.get();
    }
    auto info = std::make_unique<Http2StreamInfo>(stream_id);
    Http2StreamInfo* ptr = info.get();
    streams[stream_id] = std::move(info);
    return ptr;
}

void Http2Session::markStreamComplete(int32_t stream_id) {
    std::lock_guard<std::recursive_mutex> lg(m);
    auto it = streams.find(stream_id);
    if (it != streams.end()) {
        // Prevent duplicate entries in completed_streams
        if (it->second->marked_complete) {
            printd(5, "markStreamComplete(%d) already marked complete, skipping\n", stream_id);
            return;
        }
        it->second->marked_complete = true;

        bool is_connect = it->second->is_connect;
        // For CONNECT streams on the server, we need to keep the stream in the map so we can respond
        // For client-side sessions, we also keep the stream in the map so the caller can access it
        // Create a copy for completed_streams instead of moving
        if (is_connect || !is_server) {
            auto copy = std::make_unique<Http2StreamInfo>(*it->second);
            completed_streams.push(std::move(copy));
            // Keep the original in streams for the caller to find
        } else {
            completed_streams.push(std::move(it->second));
            streams.erase(it);
        }
        if (http2DebugEnabled()) {
            fprintf(stderr, "HTTP2 DEBUG: markStreamComplete stream=%d is_connect=%d completed=%zu\n",
                stream_id, is_connect ? 1 : 0, completed_streams.size());
            fflush(stderr);
        }
    }
}

std::unique_ptr<Http2StreamInfo> Http2Session::takeCompletedStream() {
    std::lock_guard<std::recursive_mutex> lg(m);
    if (completed_streams.empty()) {
        return nullptr;
    }
    auto stream = std::move(completed_streams.front());
    completed_streams.pop();
    if (http2DebugEnabled()) {
        fprintf(stderr, "HTTP2 DEBUG: takeCompletedStream stream=%d remaining=%zu\n",
            stream ? stream->stream_id : -1, completed_streams.size());
        fflush(stderr);
    }
    return stream;
}

bool Http2Session::wantRead() const {
    std::lock_guard<std::recursive_mutex> lg(m);
    return nghttp2_session_want_read(session) != 0;
}

bool Http2Session::wantWrite() const {
    std::lock_guard<std::recursive_mutex> lg(m);
    return nghttp2_session_want_write(session) != 0 || send_offset < send_buffer.size();
}

// nghttp2 callbacks

int Http2Session::onFrameSendCallback(nghttp2_session* session,
        const nghttp2_frame* frame, void* user_data) {
    Http2Session* h2 = static_cast<Http2Session*>(user_data);
    uint32_t remote_max = h2 ? h2->remote_settings.max_frame_size : 0;
    uint32_t local_max = h2 ? h2->local_settings.max_frame_size : 0;
    int is_server = h2 ? h2->is_server : 0;
    int fd = h2 ? h2->sock->sock : -1;
    printd(5, "onFrameSendCallback type=%d stream_id=%d flags=%d len=%d remote_max_frame=%u local_max_frame=%u "
        "isServer=%d fd=%d\n",
        frame->hd.type, frame->hd.stream_id, frame->hd.flags, (int)frame->hd.length, remote_max, local_max,
        is_server, fd);
    if (remote_max && frame->hd.length > remote_max) {
        printd(1, "HTTP2 WARN: outgoing frame length %d exceeds peer max_frame_size %u\n",
            (int)frame->hd.length, remote_max);
    }
    if (frame->hd.type == NGHTTP2_GOAWAY) {
        printd(5, "onFrameSendCallback GOAWAY: last_stream_id=%d error_code=%u\n",
            frame->goaway.last_stream_id, frame->goaway.error_code);
        if (http2DebugEnabled()) {
            fprintf(stderr, "HTTP2 DEBUG: send GOAWAY last_stream_id=%d error_code=%u\n",
                frame->goaway.last_stream_id, frame->goaway.error_code);
            fflush(stderr);
        }
    } else if (frame->hd.type == NGHTTP2_RST_STREAM) {
        printd(5, "onFrameSendCallback RST_STREAM: stream_id=%d error_code=%u\n",
            frame->hd.stream_id, frame->rst_stream.error_code);
        if (http2DebugEnabled()) {
            fprintf(stderr, "HTTP2 DEBUG: send RST_STREAM stream=%d error_code=%u\n",
                frame->hd.stream_id, frame->rst_stream.error_code);
            fflush(stderr);
        }
    } else if (frame->hd.type == NGHTTP2_HEADERS) {
        bool end_stream = (frame->hd.flags & NGHTTP2_FLAG_END_STREAM) != 0;
        bool end_headers = (frame->hd.flags & NGHTTP2_FLAG_END_HEADERS) != 0;
        if (http2DebugEnabled()) {
            fprintf(stderr, "HTTP2 DEBUG: send HEADERS stream=%d END_STREAM=%d END_HEADERS=%d\n",
                frame->hd.stream_id, end_stream ? 1 : 0, end_headers ? 1 : 0);
            fflush(stderr);
        }
    } else if (frame->hd.type == NGHTTP2_DATA) {
        bool end_stream = (frame->hd.flags & NGHTTP2_FLAG_END_STREAM) != 0;
        if (http2DebugEnabled()) {
            fprintf(stderr, "HTTP2 DEBUG: send DATA stream=%d len=%d END_STREAM=%d\n",
                frame->hd.stream_id, (int)frame->hd.length, end_stream ? 1 : 0);
            fflush(stderr);
        }
    }
    return 0;
}

ssize_t Http2Session::sendCallback(nghttp2_session* session, const uint8_t* data,
        size_t length, int flags, void* user_data) {
    // We use mem_send instead, so this callback just returns success
    return static_cast<ssize_t>(length);
}

int Http2Session::onFrameRecvCallback(nghttp2_session* session,
        const nghttp2_frame* frame, void* user_data) {
    Http2Session* h2 = static_cast<Http2Session*>(user_data);
    if (h2 && h2->local_settings.max_frame_size
        && frame->hd.length > h2->local_settings.max_frame_size) {
        printd(1, "HTTP2 WARN: incoming frame length %d exceeds local max_frame_size %u\n",
            (int)frame->hd.length, h2->local_settings.max_frame_size);
    }
    printd(5, "onFrameRecvCallback type=%d stream_id=%d flags=%d len=%d isServer=%d fd=%d\n",
        frame->hd.type, frame->hd.stream_id, frame->hd.flags, (int)frame->hd.length, h2->is_server, h2->sock->sock);
    if (frame->hd.type == NGHTTP2_GOAWAY) {
        printd(5, "onFrameRecvCallback GOAWAY: last_stream_id=%d error_code=%u\n",
            frame->goaway.last_stream_id, frame->goaway.error_code);
        if (http2DebugEnabled()) {
            fprintf(stderr, "HTTP2 DEBUG: recv GOAWAY last_stream_id=%d error_code=%u\n",
                frame->goaway.last_stream_id, frame->goaway.error_code);
            fflush(stderr);
        }
    } else if (frame->hd.type == NGHTTP2_RST_STREAM) {
        printd(5, "onFrameRecvCallback RST_STREAM: stream_id=%d error_code=%u\n",
            frame->hd.stream_id, frame->rst_stream.error_code);
        if (http2DebugEnabled()) {
            fprintf(stderr, "HTTP2 DEBUG: recv RST_STREAM stream=%d error_code=%u\n",
                frame->hd.stream_id, frame->rst_stream.error_code);
            fflush(stderr);
        }
    }

    switch (frame->hd.type) {
        case NGHTTP2_HEADERS: {
            if (frame->hd.flags & NGHTTP2_FLAG_END_HEADERS) {
                Http2StreamInfo* stream = h2->getStream(frame->hd.stream_id);
                if (stream) {
                    stream->headers_complete = true;
                    // For requests without a body (like GET), END_STREAM is on the HEADERS frame
                    if (frame->hd.flags & NGHTTP2_FLAG_END_STREAM) {
                        stream->body_complete = true;
                        // Mark as complete so the handler is called
                        // For CONNECT streams, markStreamComplete() keeps the stream in the map
                        if (http2DebugEnabled()) {
                            fprintf(stderr,
                                "HTTP2 DEBUG: headers complete stream=%d END_STREAM=1 is_connect=%d\n",
                                frame->hd.stream_id, stream->is_connect ? 1 : 0);
                            fflush(stderr);
                        }
                        h2->markStreamComplete(frame->hd.stream_id);
                    } else if (h2->is_server && stream->is_connect) {
                        // RFC 8441: Extended CONNECT requests have no body, so they're complete
                        // once headers are received. The client doesn't send END_STREAM because
                        // it needs to send data on the stream after the handshake completes.
                        printd(5, "onFrameRecvCallback: CONNECT request complete (no END_STREAM expected)\n");
                        if (http2DebugEnabled()) {
                            fprintf(stderr,
                                "HTTP2 DEBUG: headers complete stream=%d END_STREAM=0 is_connect=1\n",
                                frame->hd.stream_id);
                            fflush(stderr);
                        }
                        stream->body_complete = true;
                        h2->markStreamComplete(frame->hd.stream_id);
                    }
                }
            }
            break;
        }

        case NGHTTP2_DATA:
            printd(5, "onFrameRecvCallback DATA: stream_id=%d flags=%d END_STREAM=%d isServer=%d\n",
                frame->hd.stream_id, frame->hd.flags,
                (frame->hd.flags & NGHTTP2_FLAG_END_STREAM) != 0, h2->is_server);
            if (frame->hd.flags & NGHTTP2_FLAG_END_STREAM) {
                Http2StreamInfo* stream = h2->getStream(frame->hd.stream_id);
                printd(5, "onFrameRecvCallback DATA END_STREAM: stream=%p headers_complete=%d\n",
                    stream, stream ? stream->headers_complete : -1);
                if (stream) {
                    stream->body_complete = true;
                    if (stream->headers_complete) {
                        printd(5, "onFrameRecvCallback: calling markStreamComplete(%d)\n", frame->hd.stream_id);
                        h2->markStreamComplete(frame->hd.stream_id);
                    }
                }
            }
            break;

        case NGHTTP2_SETTINGS:
            if (frame->hd.flags & NGHTTP2_FLAG_ACK) {
                // Our settings have been acknowledged
            } else {
                // Received remote settings
                h2->remote_settings_received = true;
                if (http2DebugEnabled()) {
                    fprintf(stderr, "HTTP2 DEBUG: received SETTINGS niv=%zu\n", frame->settings.niv);
                    fflush(stderr);
                }
                for (size_t i = 0; i < frame->settings.niv; ++i) {
                    const nghttp2_settings_entry& e = frame->settings.iv[i];
                    switch (e.settings_id) {
                        case NGHTTP2_SETTINGS_HEADER_TABLE_SIZE:
                            h2->remote_settings.header_table_size = e.value;
                            break;
                        case NGHTTP2_SETTINGS_ENABLE_PUSH:
                            h2->remote_settings.enable_push = e.value;
                            break;
                        case NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS:
                            h2->remote_settings.max_concurrent_streams = e.value;
                            break;
                        case NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE:
                            h2->remote_settings.initial_window_size = e.value;
                            break;
                        case NGHTTP2_SETTINGS_MAX_FRAME_SIZE:
                            h2->remote_settings.max_frame_size = e.value;
                            break;
                        case NGHTTP2_SETTINGS_MAX_HEADER_LIST_SIZE:
                            h2->remote_settings.max_header_list_size = e.value;
                            break;
                        case NGHTTP2_SETTINGS_ENABLE_CONNECT_PROTOCOL:
                            // RFC 8441: Server advertises support for extended CONNECT
                            h2->remote_settings.enable_connect_protocol = e.value;
                            if (http2DebugEnabled()) {
                                fprintf(stderr,
                                    "HTTP2 DEBUG: received SETTINGS_ENABLE_CONNECT_PROTOCOL=%u\n", e.value);
                            }
                            break;
                    }
                }
                printd(5, "onFrameRecvCallback SETTINGS: remote max_frame_size=%u initial_window=%u "
                    "max_header_list=%u enable_connect_protocol=%u\n",
                    h2->remote_settings.max_frame_size, h2->remote_settings.initial_window_size,
                    h2->remote_settings.max_header_list_size, h2->remote_settings.enable_connect_protocol);
            }
            break;

        case NGHTTP2_GOAWAY:
            h2->goaway_received = true;
            h2->last_stream_id = frame->goaway.last_stream_id;
            break;

        case NGHTTP2_PRIORITY: {
            // Update stream priority info
            Http2StreamInfo* stream = h2->getStream(frame->hd.stream_id);
            if (stream) {
                stream->weight = frame->priority.pri_spec.weight;
                stream->dependency = frame->priority.pri_spec.stream_id;
                stream->exclusive = frame->priority.pri_spec.exclusive != 0;
            }
            break;
        }

        default:
            break;
    }

    return 0;
}

int Http2Session::onDataChunkRecvCallback(nghttp2_session* session, uint8_t flags,
        int32_t stream_id, const uint8_t* data, size_t len, void* user_data) {
    Http2Session* h2 = static_cast<Http2Session*>(user_data);
    printd(5, "onDataChunkRecvCallback stream_id=%d len=%zu isServer=%d\n", stream_id, len, h2->is_server);
    Http2StreamInfo* stream = h2->getOrCreateStream(stream_id);
    if (stream) {
        stream->body.insert(stream->body.end(), data, data + len);
        printd(5, "onDataChunkRecvCallback stream body_size now=%zu\n", stream->body.size());
    }
    if (len) {
        int rv = h2->submitWindowUpdate(stream_id, len, nullptr);
        if (rv) {
            printd(5, "onDataChunkRecvCallback: submitWindowUpdate stream_id=%d failed\n", stream_id);
        }
        rv = h2->submitWindowUpdate(0, len, nullptr);
        if (rv) {
            printd(5, "onDataChunkRecvCallback: submitWindowUpdate connection failed\n");
        }
    }
    return 0;
}

int Http2Session::onStreamCloseCallback(nghttp2_session* session, int32_t stream_id,
        uint32_t error_code, void* user_data) {
    Http2Session* h2 = static_cast<Http2Session*>(user_data);
    if (h2 && http2DebugEnabled()) {
        fprintf(stderr, "HTTP2 DEBUG: stream close stream=%d error_code=%u\n",
            stream_id, error_code);
        fflush(stderr);
    }
    Http2StreamInfo* stream = h2->getStream(stream_id);
    if (stream) {
        stream->state = Http2StreamState::Closed;
        stream->error_code = error_code;
        if (error_code != 0) {
            stream->reset = true;
        }
        // Mark as complete even if there was an error (including RST_STREAM without headers)
        stream->body_complete = true;
        h2->markStreamComplete(stream_id);
    }
    h2->pending_body_data.erase(stream_id);
    h2->pending_data_providers.erase(stream_id);
    return 0;
}

ssize_t Http2Session::dataProviderReadCallback(nghttp2_session* session, int32_t stream_id,
        uint8_t* buf, size_t length, uint32_t* data_flags, nghttp2_data_source* source,
        void* user_data) {
    (void)session;
    DataProviderContext* ctx = static_cast<DataProviderContext*>(source->ptr);
    Http2Session* h2 = ctx ? ctx->h2 : static_cast<Http2Session*>(user_data);
    if (!h2 || !ctx) {
        *data_flags |= NGHTTP2_DATA_FLAG_EOF;
        return 0;
    }

    auto it = h2->pending_body_data.find(stream_id);
    if (it == h2->pending_body_data.end()) {
        if (ctx->defer_on_empty) {
            printd(5, "dataProviderReadCallback() stream_id=%d requested=%zu deferred\n", stream_id, length);
            return NGHTTP2_ERR_DEFERRED;
        }
        *data_flags |= NGHTTP2_DATA_FLAG_EOF;
        if (ctx->remove_on_empty) {
            h2->pending_data_providers.erase(stream_id);
        }
        printd(5, "dataProviderReadCallback() stream_id=%d requested=%zu EOF(no data)\n", stream_id, length);
        return 0;
    }

    BodyData& bd = it->second;
    size_t remaining = bd.data.size() - bd.offset;
    size_t to_copy = std::min(remaining, length);

    if (to_copy > 0) {
        memcpy(buf, bd.data.data() + bd.offset, to_copy);
        bd.offset += to_copy;
    }

    if (bd.offset >= bd.data.size()) {
        bool is_end = bd.end_stream;
        h2->pending_body_data.erase(it);
        if (ctx->no_end_stream) {
            *data_flags |= NGHTTP2_DATA_FLAG_NO_END_STREAM;
        } else if (is_end) {
            // Explicitly signaled end of stream
            *data_flags |= NGHTTP2_DATA_FLAG_EOF;
            if (ctx->remove_on_empty) {
                h2->pending_data_providers.erase(stream_id);
            }
        } else if (ctx->defer_on_empty) {
            // Streaming mode: buffer consumed but more data may arrive;
            // defer until the next sendStreamData() call resumes us.
            // Don't set EOF — the caller will signal end_stream explicitly.
        } else {
            *data_flags |= NGHTTP2_DATA_FLAG_EOF;
            if (ctx->remove_on_empty) {
                h2->pending_data_providers.erase(stream_id);
            }
        }
    }

    printd(5, "dataProviderReadCallback() stream_id=%d requested=%zu sent=%zu remaining=%zu flags=%s%s\n",
        stream_id, length, to_copy, remaining > to_copy ? (remaining - to_copy) : 0,
        (*data_flags & NGHTTP2_DATA_FLAG_EOF) ? "EOF" : "",
        (*data_flags & NGHTTP2_DATA_FLAG_NO_END_STREAM) ? "+NO_END_STREAM" : "");
    return static_cast<ssize_t>(to_copy);
}

int Http2Session::onHeaderCallback(nghttp2_session* session, const nghttp2_frame* frame,
        const uint8_t* name, size_t namelen, const uint8_t* value, size_t valuelen,
        uint8_t flags, void* user_data) {
    Http2Session* h2 = static_cast<Http2Session*>(user_data);
    Http2StreamInfo* stream = h2->getOrCreateStream(frame->hd.stream_id);
    if (!stream) {
        return 0;
    }

    std::string header_name(reinterpret_cast<const char*>(name), namelen);
    std::string header_value(reinterpret_cast<const char*>(value), valuelen);

    // Handle pseudo-headers
    if (header_name == ":status") {
        stream->status_code = std::stoi(header_value);
    } else if (header_name == ":method") {
        stream->method = header_value;
        // RFC 8441: Detect CONNECT requests for WebSocket over HTTP/2
        if (header_value == "CONNECT") {
            stream->is_connect = true;
        }
        if (http2DebugEnabled()) {
            fprintf(stderr, "HTTP2 DEBUG: received :method=%s (is_connect=%d)\n",
                header_value.c_str(), stream->is_connect ? 1 : 0);
            fflush(stderr);
        }
    } else if (header_name == ":path") {
        stream->path = header_value;
    } else if (header_name == ":authority") {
        stream->authority = header_value;
    } else if (header_name == ":scheme") {
        stream->scheme = header_value;
    } else if (header_name == ":protocol") {
        // RFC 8441: Extended CONNECT protocol pseudo-header
        stream->connect_protocol = header_value;
        if (header_value == "websocket") {
            stream->stream_type = Http2StreamType::WebSocket;
        }
        if (http2DebugEnabled()) {
            fprintf(stderr,
                "HTTP2 DEBUG: received :protocol=%s (enable_connect_protocol=%u)\n",
                header_value.c_str(), h2->remote_settings.enable_connect_protocol);
            fflush(stderr);
        }
    } else {
        stream->headers[header_name] = header_value;
    }

    return 0;
}

int Http2Session::onBeginHeadersCallback(nghttp2_session* session,
        const nghttp2_frame* frame, void* user_data) {
    Http2Session* h2 = static_cast<Http2Session*>(user_data);

    // For server: Create stream for incoming request
    // For client: Stream already exists for our request
    if (h2->is_server && frame->hd.type == NGHTTP2_HEADERS) {
        h2->getOrCreateStream(frame->hd.stream_id);
    }
    if (http2DebugEnabled()) {
        fprintf(stderr, "HTTP2 DEBUG: begin headers stream=%d type=%d flags=0x%x\n",
            frame->hd.stream_id, frame->hd.type, frame->hd.flags);
        fflush(stderr);
    }

    return 0;
}

int Http2Session::onInvalidFrameRecvCallback(nghttp2_session* session,
        const nghttp2_frame* frame, int lib_error_code, void* user_data) {
    // Log invalid frames for debugging but don't fail
    Http2Session* h2 = static_cast<Http2Session*>(user_data);
    printd(1, "Http2Session::onInvalidFrameRecvCallback() type: %d stream_id: %d flags: 0x%x len=%d err=%s "
        "remote_max_frame=%u local_max_frame=%u\n",
        frame->hd.type, frame->hd.stream_id, frame->hd.flags, (int)frame->hd.length,
        nghttp2_strerror(lib_error_code), h2->remote_settings.max_frame_size, h2->local_settings.max_frame_size);
    return 0;
}

int Http2Session::onInvalidHeaderCallback(nghttp2_session* session,
        const nghttp2_frame* frame, const uint8_t* name, size_t namelen,
        const uint8_t* value, size_t valuelen, uint8_t flags, void* user_data) {
    // Log invalid headers for debugging but don't fail
    std::string hdr_name(reinterpret_cast<const char*>(name), namelen);
    std::string hdr_val(reinterpret_cast<const char*>(value), valuelen);
    if (http2DebugEnabled()) {
        fprintf(stderr, "HTTP2 DEBUG: invalid header stream=%d %s=%s\n",
            frame->hd.stream_id, hdr_name.c_str(), hdr_val.c_str());
        fflush(stderr);
    }
    printd(3, "Http2Session::onInvalidHeaderCallback() stream_id: %d header: %s=%s\n",
        frame->hd.stream_id, hdr_name.c_str(), hdr_val.c_str());
    return 0;
}

int Http2Session::sendStreamData(int32_t stream_id, const void* data, size_t len,
        bool end_stream, ExceptionSink* xsink) {
    std::lock_guard<std::recursive_mutex> lg(m);
    if (http2DebugEnabled()) {
        fprintf(stderr, "HTTP2 DEBUG: sendStreamData stream=%d len=%zu end_stream=%d\n",
            stream_id, len, end_stream);
        fflush(stderr);
    }
    printd(5, "sendStreamData() stream_id=%d len=%zu end_stream=%d isServer=%d\n", stream_id, len, end_stream, is_server);
    // For streaming responses, the stream may have been moved to completed_streams
    // when the request was fully received (markStreamComplete), but the response
    // is still being sent via the deferred data provider. Check both streams and
    // pending_data_providers.
    Http2StreamInfo* stream = getStream(stream_id);
    if (!stream && pending_data_providers.find(stream_id) == pending_data_providers.end()) {
        xsink->raiseException("HTTP2-ERROR", "stream %d not found", stream_id);
        return -1;
    }

    // Check if there's already too much buffered data (flow control backpressure)
    // Limit to 1MB per stream to prevent unbounded memory growth
    constexpr size_t MAX_STREAM_BUFFER = 1024 * 1024;
    auto it = pending_body_data.find(stream_id);
    if (it != pending_body_data.end()) {
        size_t pending = it->second.data.size() - it->second.offset;
        if (pending > MAX_STREAM_BUFFER) {
            xsink->raiseException("HTTP2-FLOW-CONTROL", "stream %d buffer full: %zu bytes pending, "
                "waiting for peer to consume data", stream_id, pending);
            return -1;
        }
    }

    // Append data to the pending buffer for the data provider callback
    if (it == pending_body_data.end()) {
        // No existing data - create new entry
        pending_body_data[stream_id] = BodyData(data, len, end_stream);
    } else {
        // Append to existing data (preserving unread data)
        BodyData& bd = it->second;
        if (len > 0) {
            const uint8_t* src = reinterpret_cast<const uint8_t*>(data);
            bd.data.insert(bd.data.end(), src, src + len);
        }
        if (end_stream) {
            bd.end_stream = true;
        }
    }
    {
        auto it2 = pending_body_data.find(stream_id);
        size_t pending = it2 == pending_body_data.end() ? 0 : (it2->second.data.size() - it2->second.offset);
        printd(5, "sendStreamData() stream_id=%d pending_buffer=%zu\n", stream_id, pending);
    }

    // Resume the stream's data provider to trigger sending the data
    // The data provider was set up when the request/response was submitted
    // and returns NGHTTP2_ERR_DEFERRED when no data is available
    int rv = nghttp2_session_resume_data(session, stream_id);
    printd(5, "sendStreamData() nghttp2_session_resume_data rv=%d\n", rv);

    // NGHTTP2_ERR_INVALID_ARGUMENT means the stream is not deferred or doesn't exist
    // This can happen if the data provider hasn't been called yet
    if (rv == NGHTTP2_ERR_INVALID_ARGUMENT) {
        // The stream is not in deferred state - the data will be picked up
        // on the next send cycle when nghttp2 calls the data provider
        printd(5, "sendStreamData() stream not deferred, data queued for next send cycle\n");
    } else if (rv != 0) {
        xsink->raiseException("HTTP2-ERROR", "failed to resume stream data: %s",
            nghttp2_strerror(rv));
        return -1;
    }

    return 0;
}

void Http2Session::setStreamType(int32_t stream_id, Http2StreamType type) {
    std::lock_guard<std::recursive_mutex> lg(m);
    Http2StreamInfo* stream = getStream(stream_id);
    if (stream) {
        stream->stream_type = type;
    }
}

int Http2Session::submitConnectResponse(int32_t stream_id, int status_code,
        const std::map<std::string, std::string>& headers, ExceptionSink* xsink) {
    std::lock_guard<std::recursive_mutex> lg(m);
    if (!is_server) {
        xsink->raiseException("HTTP2-ERROR", "cannot submit CONNECT response on client session");
        return -1;
    }
    if (http2DebugEnabled()) {
        fprintf(stderr, "HTTP2 DEBUG: submitConnectResponse stream=%d status=%d\n",
            stream_id, status_code);
        fflush(stderr);
    }

    Http2StreamInfo* stream = getStream(stream_id);
    if (!stream) {
        xsink->raiseException("HTTP2-ERROR", "stream %d not found", stream_id);
        return -1;
    }

    if (!stream->is_connect) {
        xsink->raiseException("HTTP2-ERROR", "stream %d is not a CONNECT request", stream_id);
        return -1;
    }

    // Build response headers
    std::vector<nghttp2_nv> nva;
    nva.reserve(headers.size() + 1);

    // Add :status pseudo-header
    std::string status_str = std::to_string(status_code);
    nghttp2_nv nv_status = {
        reinterpret_cast<uint8_t*>(const_cast<char*>(":status")),
        reinterpret_cast<uint8_t*>(const_cast<char*>(status_str.c_str())),
        7, status_str.size(), NGHTTP2_NV_FLAG_NONE
    };
    nva.push_back(nv_status);

    // Add regular headers - filter out HTTP/2 forbidden headers and convert to lowercase
    // Store lowercase names to keep them alive during the nghttp2 call
    std::vector<std::string> lowered_names;
    lowered_names.reserve(headers.size());

    for (const auto& h : headers) {
        // Skip pseudo-headers (already handled above)
        if (h.first[0] == ':') {
            continue;
        }
        // RFC 7540 Section 8.1.2.2: Skip connection-specific headers (forbidden in HTTP/2)
        std::string lname = h.first;
        std::transform(lname.begin(), lname.end(), lname.begin(), ::tolower);
        if (lname == "connection" || lname == "keep-alive" || lname == "proxy-connection" ||
            lname == "transfer-encoding" || lname == "upgrade") {
            continue;
        }
        // Store lowercase name
        lowered_names.push_back(lname);
        nghttp2_nv nv = {
            reinterpret_cast<uint8_t*>(const_cast<char*>(lowered_names.back().c_str())),
            reinterpret_cast<uint8_t*>(const_cast<char*>(h.second.c_str())),
            lowered_names.back().size(), h.second.size(), NGHTTP2_NV_FLAG_NONE
        };
        nva.push_back(nv);
    }

    std::unique_ptr<DataProviderContext> provider_ctx = std::make_unique<DataProviderContext>();
    provider_ctx->h2 = this;
    provider_ctx->provider.source.ptr = provider_ctx.get();
    provider_ctx->provider.read_callback = dataProviderReadCallback;
    provider_ctx->defer_on_empty = true;
    provider_ctx->no_end_stream = true;
    provider_ctx->remove_on_empty = false;

    int rv = nghttp2_submit_response(session, stream_id, nva.data(), nva.size(),
        &provider_ctx->provider);
    if (rv != 0) {
        xsink->raiseException("HTTP2-ERROR", "failed to submit CONNECT response: %s",
            nghttp2_strerror(rv));
        return -1;
    }
    if (http2DebugEnabled()) {
        fprintf(stderr, "HTTP2 DEBUG: submitConnectResponse ok stream=%d\n", stream_id);
        fflush(stderr);
    }
    pending_data_providers.emplace(stream_id, std::move(provider_ctx));

    // If successful (2xx), mark stream as WebSocket type if protocol is websocket
    if (status_code >= 200 && status_code < 300 && stream->connect_protocol == "websocket") {
        stream->stream_type = Http2StreamType::WebSocket;
    }

    return 0;
}
