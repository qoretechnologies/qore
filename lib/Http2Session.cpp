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
#include "qore/intern/QoreIoUring.h"
#include "qore/intern/QuicCommon.h"
#include "qore/intern/qore_socket_private.h"

#include <algorithm>
#include <cctype>
#include <cinttypes>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <unordered_set>

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
    // No io_uring cleanup needed here — PendingRead holds a weak_ptr<Http2Session>,
    // which expires naturally when this destructor runs. processCompletions() will
    // skip delivery for expired sessions.
    // InputStream cleanup (unassignThread) is handled by the I/O thread via
    // processStreamInputStreams() during normal operation.
    if (session) {
        nghttp2_session_del(session);
    }
}

std::shared_ptr<Http2Session> Http2Session::createClient(qore_socket_private* sock, ExceptionSink* xsink,
        const char* scheme) {
    // Use shared_ptr with raw pointer since constructor is private
    std::shared_ptr<Http2Session> h2(new Http2Session(sock, false, scheme));
    if (h2->init(xsink)) {
        return nullptr;
    }
    return h2;
}

std::shared_ptr<Http2Session> Http2Session::createServer(qore_socket_private* sock, ExceptionSink* xsink,
        const char* scheme) {
    // Use shared_ptr with raw pointer since constructor is private
    std::shared_ptr<Http2Session> h2(new Http2Session(sock, true, scheme));
    if (h2->init(xsink)) {
        return nullptr;
    }
    return h2;
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

std::vector<nghttp2_nv> Http2Session::makeNv(const strcase_str_map_t& headers) {
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

// HTTP/2 forbids hop-by-hop headers from HTTP/1.x (RFC 9113 Section 8.2.2)
static const auto& forbidden_headers = getForbiddenHopByHopHeaders();

//! Build regular (non-pseudo) headers for nghttp2, lowercasing names in-place
/** Works with any range of pair-like elements (std::map, std::vector<std::pair>, etc.)
    @param nva output vector of nghttp2_nv to append to
    @param lowered_names storage for lowered name strings (must outlive nghttp2 call)
    @param headers input headers to process
    @param skip_host if true, skip "host" header (used for requests where :authority replaces host)
*/
template<typename HeaderRange>
static void buildRegularHeaders(std::vector<nghttp2_nv>& nva,
        std::vector<std::string>& lowered_names, const HeaderRange& headers,
        bool skip_host) {
    for (const auto& h : headers) {
        // Lowercase in-place
        std::string lname(h.first);
        if (lname.empty()) {
            continue;
        }
        std::transform(lname.begin(), lname.end(), lname.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        if (lname[0] == ':') {
            continue;
        }
        if (skip_host && lname == "host") {
            continue;
        }
        if (forbidden_headers.count(lname)) {
            continue;
        }
        lowered_names.push_back(std::move(lname));
        nghttp2_nv nv = {
            reinterpret_cast<uint8_t*>(const_cast<char*>(lowered_names.back().c_str())),
            reinterpret_cast<uint8_t*>(const_cast<char*>(h.second.c_str())),
            lowered_names.back().size(), h.second.size(), NGHTTP2_NV_FLAG_NONE
        };
        nva.push_back(nv);
    }
}

int32_t Http2Session::submitRequest(const char* method, const char* path,
        const strcase_str_map_t& headers,
        const void* body, size_t body_len, ExceptionSink* xsink,
        bool streaming) {
    return submitRequestImpl(method, path, headers, body, body_len, xsink, streaming);
}

int32_t Http2Session::submitRequest(const char* method, const char* path,
        const std::vector<std::pair<std::string, std::string>>& headers,
        const void* body, size_t body_len, ExceptionSink* xsink,
        bool streaming) {
    return submitRequestImpl(method, path, headers, body, body_len, xsink, streaming);
}

template<typename HeaderRange>
int32_t Http2Session::submitRequestImpl(const char* method, const char* path,
        const HeaderRange& headers,
        const void* body, size_t body_len, ExceptionSink* xsink,
        bool streaming) {
    // Build headers outside the lock - no session state access needed here
    std::vector<nghttp2_nv> nva;
    nva.reserve(headers.size() + 5);

    // Extract pseudo-headers and special headers from the input
    std::string method_str(method);
    std::string path_str(path);
    std::string scheme_str;
    std::string authority;
    std::string protocol;

    for (const auto& h : headers) {
        if (h.first == ":scheme") {
            scheme_str = h.second;
        } else if (h.first == ":authority") {
            authority = h.second;
        } else if (h.first == ":protocol") {
            protocol = h.second;
        } else if (!strcasecmp(h.first.c_str(), "host") && authority.empty()) {
            authority = h.second;
        }
    }

    // is_server and scheme are set once at construction and never change — safe to read without lock
    if (is_server) {
        xsink->raiseException("HTTP2-ERROR", "cannot submit request on server session");
        return -1;
    }

    if (scheme_str.empty()) {
        scheme_str = scheme;
    }

    // Add :method pseudo-header
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

    if (!authority.empty()) {
        nghttp2_nv nv_authority = {
            reinterpret_cast<uint8_t*>(const_cast<char*>(":authority")),
            reinterpret_cast<uint8_t*>(const_cast<char*>(authority.c_str())),
            10, authority.size(), NGHTTP2_NV_FLAG_NONE
        };
        nva.push_back(nv_authority);
    }

    if (!protocol.empty()) {
        nghttp2_nv nv_protocol = {
            reinterpret_cast<uint8_t*>(const_cast<char*>(":protocol")),
            reinterpret_cast<uint8_t*>(const_cast<char*>(protocol.c_str())),
            9, protocol.size(), NGHTTP2_NV_FLAG_NONE
        };
        nva.push_back(nv_protocol);
    }

    // Add regular headers (excluding pseudo-headers, host, and forbidden headers)
    std::vector<std::string> lowered_names;
    lowered_names.reserve(headers.size());
    buildRegularHeaders(nva, lowered_names, headers, true /* skip_host */);

    bool is_extended_connect = (method_str == "CONNECT" && !protocol.empty());

    nghttp2_data_provider* data_prd = nullptr;
    std::unique_ptr<DataProviderContext> provider_ctx;

    bool has_body = false;
    BodyData pending_data;
    if (body && body_len > 0) {
        pending_data = BodyData(body, body_len);
        has_body = true;
    }
    if (has_body || is_extended_connect || streaming) {
        provider_ctx = std::make_unique<DataProviderContext>();
        provider_ctx->h2 = this;
        provider_ctx->provider.source.ptr = provider_ctx.get();
        provider_ctx->provider.read_callback = dataProviderReadCallback;
        provider_ctx->defer_on_empty = is_extended_connect || streaming;
        provider_ctx->no_end_stream = is_extended_connect || streaming;
        provider_ctx->remove_on_empty = !is_extended_connect && !streaming;
        data_prd = &provider_ctx->provider;
    }

    if (http2DebugEnabled()) {
        fprintf(stderr, "HTTP2 DEBUG: submitRequest sending %zu headers:\n", nva.size());
        for (const auto& nv : nva) {
            fprintf(stderr, "  %.*s: %.*s\n", (int)nv.namelen, nv.name, (int)nv.valuelen, nv.value);
        }
        fflush(stderr);
    }

    // Lock only for nghttp2 API calls and session state access
    std::lock_guard<std::recursive_mutex> lg(m);

    if (is_extended_connect && remote_settings_received
            && !remote_settings.enable_connect_protocol) {
        xsink->raiseException("HTTP2-CONNECT-ERROR",
            "server does not support extended CONNECT protocol (RFC 8441); "
            "SETTINGS_ENABLE_CONNECT_PROTOCOL was not advertised");
        return -1;
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

    Http2StreamInfo* stream = getOrCreateStream(stream_id);
    if (stream) {
        stream->is_connect = (method_str == "CONNECT");
        stream->streaming = streaming;
        if (!protocol.empty()) {
            stream->connect_protocol = protocol;
        }
    }
    if (is_extended_connect && stream) {
        stream->stream_type = Http2StreamType::WebSocket;
    }

    return stream_id;
}

int Http2Session::submitResponse(int32_t stream_id, int status_code,
        const strcase_str_map_t& headers,
        const void* body, size_t body_len, ExceptionSink* xsink) {
    return submitResponseImpl(stream_id, status_code, headers, body, body_len, xsink);
}

int Http2Session::submitResponse(int32_t stream_id, int status_code,
        const std::vector<std::pair<std::string, std::string>>& headers,
        const void* body, size_t body_len, ExceptionSink* xsink) {
    return submitResponseImpl(stream_id, status_code, headers, body, body_len, xsink);
}

template<typename HeaderRange>
int Http2Session::submitResponseImpl(int32_t stream_id, int status_code,
        const HeaderRange& headers,
        const void* body, size_t body_len, ExceptionSink* xsink) {
    // is_server is set once at construction and never changes — safe to read without lock
    if (!is_server) {
        xsink->raiseException("HTTP2-ERROR", "cannot submit response on client session");
        return -1;
    }

    // Build response headers outside the lock
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

    // Add regular headers, filtering out pseudo and forbidden headers
    std::vector<std::string> lowered_names;
    lowered_names.reserve(headers.size());
    buildRegularHeaders(nva, lowered_names, headers, false /* skip_host */);

    nghttp2_data_provider* data_prd = nullptr;
    std::unique_ptr<DataProviderContext> provider_ctx;

    if (body && body_len > 0) {
        provider_ctx = std::make_unique<DataProviderContext>();
        provider_ctx->h2 = this;
        provider_ctx->provider.source.ptr = provider_ctx.get();
        provider_ctx->provider.read_callback = dataProviderReadCallback;
        data_prd = &provider_ctx->provider;
    }

    printd(5, "submitResponse() stream_id=%d status=%d body_len=%zu nva.size=%zu\n",
        stream_id, status_code, body_len, nva.size());

    // Lock only for nghttp2 API calls and session state access
    std::lock_guard<std::recursive_mutex> lg(m);

    if (body && body_len > 0) {
        pending_body_data[stream_id] = BodyData(body, body_len);
    }

    int rv = nghttp2_submit_response(session, stream_id, nva.data(), nva.size(), data_prd);
    if (rv != 0) {
        xsink->raiseException("HTTP2-ERROR", "failed to submit response: %s",
            nghttp2_strerror(rv));
        return -1;
    }

    if (provider_ctx) {
        pending_data_providers.emplace(stream_id, std::move(provider_ctx));
    }
    return 0;
}

int Http2Session::submitResponseStreaming(int32_t stream_id, int status_code,
        const strcase_str_map_t& headers, ExceptionSink* xsink) {
    // is_server is set once at construction and never changes — safe to read without lock
    if (!is_server) {
        xsink->raiseException("HTTP2-ERROR", "cannot submit response on client session");
        return -1;
    }

    // Build response headers outside the lock
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

    // Add regular headers, filtering out pseudo and forbidden headers
    std::vector<std::string> lowered_names;
    lowered_names.reserve(headers.size());
    buildRegularHeaders(nva, lowered_names, headers, false /* skip_host */);

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

    // Lock only for nghttp2 API calls and session state access
    std::lock_guard<std::recursive_mutex> lg(m);

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
        const strcase_str_map_t& headers, ExceptionSink* xsink) {
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
    size_t pending = send_buffer.size() - send_offset;
    printd(5, "sendPendingData() want_write=%d isServer=%d timeout_ms=%d pending=%zu\n",
        nghttp2_session_want_write(session), is_server, timeout_ms, pending);
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

    // Compact send buffer when consumed prefix exceeds threshold
    if (send_offset > SEND_BUFFER_COMPACTION_THRESHOLD) {
        send_buffer.erase(send_buffer.begin(), send_buffer.begin() + send_offset);
        send_offset = 0;
    }

    pending = send_buffer.size() - send_offset;
    printd(5, "sendPendingData() collected %zu bytes from nghttp2, pending=%zu\n",
        total_collected, pending);

    // Send buffered data using proper async I/O pattern (like SocketSendPollState::continuePoll)
    if (send_offset < send_buffer.size()) {
        printd(5, "sendPendingData() sending %zu bytes to socket (ssl=%p)\n", pending, sock->ssl);
        {
            size_t off = send_offset;
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

        // Loop until buffer consumed or we need to poll
        while (send_offset < send_buffer.size()) {
            ssize_t rc;
            size_t to_send = send_buffer.size() - send_offset;
            if (sock->ssl) {
                // Use doNonBlockingIo for proper async SSL I/O
                // Returns: SOCK_POLLIN/SOCK_POLLOUT if need to poll, 0 if done, < 0 on error
                size_t real_io = 0;
                printd(5, "sendPendingData() calling doNonBlockingIo to_send=%zu\n", to_send);
                rc = sock->ssl->doNonBlockingIo(xsink, "sendPendingData",
                    const_cast<char*>(reinterpret_cast<const char*>(send_buffer.data() + send_offset)),
                    to_send, SslAction::WRITE, real_io);
                printd(5, "sendPendingData() doNonBlockingIo rc=%zd real_io=%zu xsink=%d\n", rc, real_io, (int)*xsink);
                if (*xsink) {
                    return -1;
                }

                // Advance offset past sent data
                if (real_io > 0) {
                    send_offset += real_io;
                    printd(5, "sendPendingData() sent %zu bytes, remaining=%zu\n",
                        real_io, send_buffer.size() - send_offset);
                }

                if (!rc) {
                    // Data was written successfully and SSL doesn't need to poll - continue loop
                    continue;
                }
                if (rc == SOCK_POLLOUT || rc == SOCK_POLLIN) {
                    // SSL needs to wait for socket - return the actual poll direction
                    // Note: SOCK_POLLIN can happen during TLS renegotiation even when writing
                    printd(5, "sendPendingData() SSL needs poll for %s, remaining=%zu\n",
                        rc == SOCK_POLLIN ? "POLLIN" : "POLLOUT", send_buffer.size() - send_offset);
                    return rc;  // Return actual poll direction needed
                }
                // rc < 0 but no exception - shouldn't happen
                printd(5, "sendPendingData() SSL unexpected: rc=%zd (breaking)\n", rc);
                break;
            } else {
                // Non-SSL: use regular send
                rc = ::send(sock->sock, send_buffer.data() + send_offset, to_send, 0);
                printd(5, "sendPendingData() send rc=%zd errno=%d\n", rc, errno);
                if (rc >= 0) {
                    // Advance offset past sent data
                    if (rc > 0) {
                        send_offset += rc;
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
                    printd(5, "sendPendingData() socket would block, remaining=%zu\n",
                        send_buffer.size() - send_offset);
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

    // Release memory when fully drained to prevent long-lived idle connections
    // from holding onto large send buffer allocations
    if (send_offset == send_buffer.size()) {
        send_buffer.clear();
        send_offset = 0;
    }

    printd(5, "sendPendingData() complete, want_write=%d pending=%zu\n",
        nghttp2_session_want_write(session), send_buffer.size() - send_offset);
    return 0;
}

int Http2Session::sendPendingDataBlocking(int timeout_ms, ExceptionSink* xsink) {
    std::lock_guard<std::recursive_mutex> lg(m);
    size_t pending = send_buffer.size() - send_offset;
    printd(5, "sendPendingDataBlocking() want_write=%d pending=%zu timeout_ms=%d\n",
        nghttp2_session_want_write(session), pending, timeout_ms);

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

    // Compact send buffer when consumed prefix exceeds threshold
    if (send_offset > SEND_BUFFER_COMPACTION_THRESHOLD) {
        send_buffer.erase(send_buffer.begin(), send_buffer.begin() + send_offset);
        send_offset = 0;
    }

    // Send buffered data using blocking I/O with timeout
    pending = send_buffer.size() - send_offset;
    if (send_offset < send_buffer.size()) {
        printd(5, "sendPendingDataBlocking() sending %zu bytes with blocking timeout=%d\n",
            pending, timeout_ms);
        {
            size_t off = send_offset;
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
            reinterpret_cast<const char*>(send_buffer.data() + send_offset), pending,
            timeout_ms, total_sent);

        printd(5, "sendPendingDataBlocking() sendIntern returned rc=%zd total_sent=%" PRId64 "\n", rc, total_sent);

        if (total_sent > 0) {
            send_offset += total_sent;
        }
        pending = send_buffer.size() - send_offset;
        printd(5, "sendPendingDataBlocking() remaining pending=%zu\n", pending);

        if (send_offset < send_buffer.size()) {
            printd(5, "sendPendingDataBlocking() buffer not empty, remaining=%zu\n", pending);
            return -1;  // Still have data to send
        }

        if (*xsink) {
            return -1;
        }
    }

    // Release memory when fully drained to prevent long-lived idle connections
    // from holding onto large send buffer allocations
    if (send_offset == send_buffer.size()) {
        send_buffer.clear();
        send_offset = 0;
    }

    printd(5, "sendPendingDataBlocking() complete, want_write=%d pending=%zu\n",
        nghttp2_session_want_write(session), send_buffer.size() - send_offset);
    return 0;
}

int Http2Session::receiveData(int timeout_ms, ExceptionSink* xsink) {
    if (http2DebugEnabled()) {
        fprintf(stderr, "HTTP2 DEBUG: receiveData() session=%p isServer=%d tid=%d about to lock\n",
            this, is_server, q_gettid());
        fflush(stderr);
    }
    std::lock_guard<std::recursive_mutex> lg(m);
    if (http2DebugEnabled()) {
        fprintf(stderr, "HTTP2 DEBUG: receiveData() session=%p isServer=%d tid=%d locked\n",
            this, is_server, q_gettid());
        fflush(stderr);
    }
    char* buf;
    printd(5, "receiveData() ENTRY fd=%d isServer=%d timeout_ms=%d\n", sock->sock, is_server, timeout_ms);
    // Use suppress_exception=true to avoid exception on timeout
    ssize_t len = sock->brecv(xsink, "receiveData", buf, 16384, 0, timeout_ms, false, true);
    printd(5, "receiveData() brecv len=%zd fd=%d isServer=%d xsink=%d\n", len, sock->sock, is_server, (int)*xsink);
    if (len < 0) {
        // Check for timeout (QSE_TIMEOUT = -3)
        if (len == QSE_TIMEOUT) {
            printd(5, "receiveData() timeout (no data)\n");
            if (http2DebugEnabled()) {
                fprintf(stderr, "HTTP2 DEBUG: receiveData() session=%p tid=%d releasing lock (timeout)\n",
                    this, q_gettid());
                fflush(stderr);
            }
            return 0;  // Not an error, just no data available
        }
        printd(5, "receiveData() error len=%zd\n", len);
        if (http2DebugEnabled()) {
            fprintf(stderr, "HTTP2 DEBUG: receiveData() session=%p tid=%d releasing lock (recv error)\n",
                this, q_gettid());
            fflush(stderr);
        }
        return -1;
    }
    if (len == 0) {
        // EOF received - connection was closed by peer
        printd(5, "receiveData() len=0 (connection closed by peer)\n");
        if (http2DebugEnabled()) {
            fprintf(stderr, "HTTP2 DEBUG: receiveData() session=%p tid=%d releasing lock (EOF)\n",
                this, q_gettid());
            fflush(stderr);
        }
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
        if (http2DebugEnabled()) {
            fprintf(stderr, "HTTP2 DEBUG: receiveData() session=%p tid=%d releasing lock (error)\n",
                this, q_gettid());
            fflush(stderr);
        }
        return -1;
    }

    if (http2DebugEnabled()) {
        fprintf(stderr, "HTTP2 DEBUG: receiveData() session=%p tid=%d releasing lock (success)\n",
            this, q_gettid());
        fflush(stderr);
    }
    return 0;
}

bool Http2Session::hasPendingBodyData(int32_t stream_id) const {
    std::lock_guard<std::recursive_mutex> lg(m);
    return pending_body_data.find(stream_id) != pending_body_data.end();
}

bool Http2Session::hasSocketBufferedData() const {
    return sock->buflen > 0 || (sock->ssl && sock->ssl->pending() > 0);
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
    info->max_body_size = max_request_body_size;
    Http2StreamInfo* ptr = info.get();
    streams[stream_id] = std::move(info);
    return ptr;
}

void Http2Session::markStreamComplete(int32_t stream_id) {
    Http2StreamInfo* stream_ptr = nullptr;
    StreamCompleteCallback callback_copy;
    bool should_erase_after_callback = false;

    {
        std::lock_guard<std::recursive_mutex> lg(m);
        auto it = streams.find(stream_id);
        if (it == streams.end()) {
            return;
        }

        // Prevent duplicate entries in completed_streams
        if (it->second->marked_complete) {
            printd(5, "markStreamComplete(%d) already marked complete, skipping\n", stream_id);
            return;
        }
        it->second->marked_complete = true;
        stream_ptr = it->second.get();

        // If stream was dispatched via headers-only mode, the handler is reading DATA
        // incrementally. Keep stream in map for continued DATA accumulation; don't push
        // to completed_streams or erase.
        if (it->second->dispatched) {
            printd(5, "markStreamComplete(%d) dispatched stream, keeping in map\n", stream_id);
            if (http2DebugEnabled()) {
                fprintf(stderr, "HTTP2 DEBUG: markStreamComplete stream=%d dispatched, keeping in map\n",
                    stream_id);
                fflush(stderr);
            }
            return;
        }

        // In headers-only mode, if headers are complete but the stream hasn't been
        // dispatched yet (e.g., HEADERS + DATA + END_STREAM arrived in one batch),
        // keep the stream in the map so takeHeadersReadyStreamCopy() can find it.
        if (headers_only_mode && it->second->headers_complete) {
            printd(5, "markStreamComplete(%d) headers-only mode, keeping in map for dispatch\n",
                stream_id);
            return;
        }

        bool is_connect = it->second->is_connect;

        // Copy callback to invoke outside the lock
        callback_copy = stream_complete_callback;

        if (callback_copy) {
            // When using callback mechanism, don't push to completed_streams.
            // Erase stream after callback completion:
            // - Server mode: erase non-CONNECT streams (CONNECT needed to detect closes)
            // - Client mode: always erase after callback (managed by Qore layer)
            should_erase_after_callback = is_server ? !is_connect : true;
            if (http2DebugEnabled()) {
                fprintf(stderr, "HTTP2 DEBUG: markStreamComplete stream=%d using callback "
                    "(erase_after=%d)\n", stream_id, should_erase_after_callback ? 1 : 0);
                fflush(stderr);
            }
        } else {
            // No callback - use completed_streams queue
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

    // Invoke stream completion callback outside the lock to avoid deadlocks
    if (callback_copy) {
        ExceptionSink xsink;
        callback_copy(stream_id, stream_ptr, &xsink);
        // Ignore any exceptions from the callback

        // Now erase server-side non-CONNECT streams to prevent memory leak
        if (should_erase_after_callback) {
            std::lock_guard<std::recursive_mutex> lg(m);
            streams.erase(stream_id);
            if (http2DebugEnabled()) {
                fprintf(stderr, "HTTP2 DEBUG: markStreamComplete stream=%d erased after callback\n",
                    stream_id);
                fflush(stderr);
            }
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

void Http2Session::setHeadersOnlyMode(bool v) {
    std::lock_guard<std::recursive_mutex> lg(m);
    headers_only_mode = v;
}


std::unique_ptr<Http2StreamInfo> Http2Session::takeHeadersReadyStreamCopy() {
    std::lock_guard<std::recursive_mutex> lg(m);
    for (auto& [id, info] : streams) {
        if (info->headers_complete && !info->dispatched) {
            // Copy stream info for the caller (headers, method, path, etc.)
            auto copy = std::make_unique<Http2StreamInfo>(*info);
            // Clear body on the COPY: any DATA that arrived with HEADERS stays
            // in the original for readHttp2StreamDataBlock() to return.
            // getOutput() skips body for H2S_HEADERS_READY mode to prevent
            // duplicate data delivery.
            copy->body.clear();
            info->dispatched = true;
            printd(5, "takeHeadersReadyStreamCopy(%d)\n", id);
            return copy;
        }
    }
    return nullptr;
}

bool Http2Session::isStreamComplete(int32_t stream_id) const {
    std::lock_guard<std::recursive_mutex> lg(m);
    auto it = streams.find(stream_id);
    if (it == streams.end()) {
        return true;  // Stream not found, treat as complete
    }
    return it->second->body_complete;
}

bool Http2Session::isStreamClosed(int32_t stream_id) const {
    std::lock_guard<std::recursive_mutex> lg(m);
    auto it = streams.find(stream_id);
    if (it == streams.end()) {
        return true;  // Stream not found, treat as closed
    }
    return it->second->state == Http2StreamState::Closed;
}

bool Http2Session::isStreamRemoteClosed(int32_t stream_id) const {
    std::lock_guard<std::recursive_mutex> lg(m);
    if (!session) {
        return true;
    }
    return nghttp2_session_get_stream_remote_close(session, stream_id) == 1;
}

void Http2Session::cleanupStream(int32_t stream_id) {
    std::lock_guard<std::recursive_mutex> lg(m);
    auto it = streams.find(stream_id);
    if (it != streams.end()) {
        printd(5, "cleanupStream(%d) removing dispatched stream\n", stream_id);
        if (http2DebugEnabled()) {
            fprintf(stderr, "HTTP2 DEBUG: cleanupStream stream=%d removing dispatched stream\n",
                stream_id);
            fflush(stderr);
        }
        streams.erase(it);
    }
}

bool Http2Session::hasStreamingData(int32_t& out_stream_id) {
    std::lock_guard<std::recursive_mutex> lg(m);
    for (auto& it : streams) {
        if (it.second->streaming && it.second->headers_complete && !it.second->body.empty()) {
            out_stream_id = it.first;
            return true;
        }
    }
    return false;
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
    if (http2DebugEnabled()) {
        fprintf(stderr, "HTTP2 DEBUG: onFrameRecvCallback session=%p isServer=%d type=%d stream=%d\n",
            h2, h2 ? h2->is_server : -1, frame->hd.type, frame->hd.stream_id);
        fflush(stderr);
    }
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
                    if (frame->headers.cat == NGHTTP2_HCAT_HEADERS) {
                        // Trailer HEADERS frame (after initial headers + body data)
                        stream->receiving_trailers = false;
                        if (http2DebugEnabled()) {
                            fprintf(stderr,
                                "HTTP2 DEBUG: trailer headers complete stream=%d "
                                "END_STREAM=%d trailer_count=%zu\n",
                                frame->hd.stream_id,
                                (frame->hd.flags & NGHTTP2_FLAG_END_STREAM) ? 1 : 0,
                                stream->trailers.size());
                            fflush(stderr);
                        }
                        if (frame->hd.flags & NGHTTP2_FLAG_END_STREAM) {
                            stream->body_complete = true;
                            h2->markStreamComplete(frame->hd.stream_id);
                        }
                    } else {
                        // Initial HEADERS (request or response)
                        stream->headers_complete = true;
                        // For requests without a body (like GET), END_STREAM is on the HEADERS frame
                        if (frame->hd.flags & NGHTTP2_FLAG_END_STREAM) {
                            stream->headers_end_stream = true;
                            stream->body_complete = true;
                            // Mark as complete so the handler is called
                            // For CONNECT streams, markStreamComplete() keeps the stream in the map
                            if (http2DebugEnabled()) {
                                fprintf(stderr,
                                    "HTTP2 DEBUG: headers complete stream=%d END_STREAM=1 "
                                    "is_connect=%d\n",
                                    frame->hd.stream_id, stream->is_connect ? 1 : 0);
                                fflush(stderr);
                            }
                            h2->markStreamComplete(frame->hd.stream_id);
                        } else if (h2->is_server && stream->is_connect) {
                            // RFC 8441: Extended CONNECT with :protocol when
                            // ENABLE_CONNECT_PROTOCOL is not advertised must be rejected.
                            // Some nghttp2 versions don't enforce this automatically, so reject
                            // explicitly to ensure consistent behavior.
                            if (!stream->connect_protocol.empty()
                                    && !h2->local_settings.enable_connect_protocol) {
                                printd(5, "onFrameRecvCallback: rejecting extended CONNECT "
                                    "(ENABLE_CONNECT_PROTOCOL not set) stream=%d\n",
                                    frame->hd.stream_id);
                                if (http2DebugEnabled()) {
                                    fprintf(stderr,
                                        "HTTP2 DEBUG: rejecting extended CONNECT stream=%d "
                                        "(ENABLE_CONNECT_PROTOCOL not set)\n",
                                        frame->hd.stream_id);
                                    fflush(stderr);
                                }
                                // Submit RST_STREAM; onStreamCloseCallback is called
                                // synchronously, which sets reset=true and calls
                                // markStreamComplete()
                                nghttp2_submit_rst_stream(session, NGHTTP2_FLAG_NONE,
                                    frame->hd.stream_id, NGHTTP2_PROTOCOL_ERROR);
                            } else {
                                // Normal CONNECT: request is complete once headers are received.
                                // The client doesn't send END_STREAM because it needs to send
                                // data on the stream after the handshake completes.
                                printd(5, "onFrameRecvCallback: CONNECT request complete "
                                    "(no END_STREAM expected)\n");
                                if (http2DebugEnabled()) {
                                    fprintf(stderr,
                                        "HTTP2 DEBUG: headers complete stream=%d END_STREAM=0 "
                                        "is_connect=1\n",
                                        frame->hd.stream_id);
                                    fflush(stderr);
                                }
                                stream->body_complete = true;
                                h2->markStreamComplete(frame->hd.stream_id);
                            }
                        }
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
    Http2StreamInfo* stream;
    if (h2->is_server) {
        // Server mode: use getStream() — if the stream was already cleaned up
        // (e.g. handler finished or body streaming error), silently discard the
        // data.  Do NOT send RST_STREAM here — the handler may have just queued
        // an error response that hasn't been flushed yet, and RST_STREAM(CANCEL)
        // can cancel that pending response.  The response's END_STREAM flag will
        // close the server half; the client will stop sending after receiving it.
        stream = h2->getStream(stream_id);
        if (!stream) {
            printd(5, "onDataChunkRecvCallback: server stream %d already cleaned up, "
                "discarding %zu bytes\n", stream_id, len);
            // Still need to update flow control window for the consumed data
            if (len) {
                h2->submitWindowUpdate(0, len, nullptr);
            }
            return 0;
        }
    } else {
        stream = h2->getOrCreateStream(stream_id);
    }
    if (stream) {
        stream->body.insert(stream->body.end(), data, data + len);
        printd(5, "onDataChunkRecvCallback stream body_size now=%zu\n", stream->body.size());
        // Check body size against limit
        if (stream->max_body_size > 0
                && (int64)stream->body.size() > stream->max_body_size) {
            printd(1, "onDataChunkRecvCallback: body too large (%zu > " QLLD ") stream %d\n",
                stream->body.size(), stream->max_body_size, stream_id);
            // Send RST_STREAM with REFUSED_STREAM
            nghttp2_submit_rst_stream(session, NGHTTP2_FLAG_NONE, stream_id,
                NGHTTP2_REFUSED_STREAM);
            stream->body.clear();
            return 0;
        }
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
    // Wake any handler threads blocked in waitForStreamDrain() for this stream
    h2->notifyStreamDrain();
    // If an InputStream is active for this stream, mark it EOF so
    // processStreamInputStreams() will not attempt to call sendStreamData()
    // after the stream is closed.  Without this, the next poll iteration
    // would find the InputStream still registered, read a chunk from the
    // file, and call sendStreamData() which raises "stream N not found"
    // (because both getStream() and pending_data_providers return nothing),
    // terminating the entire HTTP/2 connection and causing ERR_CONNECTION_CLOSED
    // for all in-flight streams including unrelated ones (e.g. large file transfers).
    {
        auto iit = h2->stream_input_streams_.find(stream_id);
        if (iit != h2->stream_input_streams_.end()) {
            iit->second.eof = true;
        }
    }
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

    // Helper lambda: submit pending trailers after signaling EOF + NO_END_STREAM.
    // nghttp2_submit_trailer() must be called AFTER the data provider callback signals
    // NGHTTP2_DATA_FLAG_EOF | NGHTTP2_DATA_FLAG_NO_END_STREAM, not before — otherwise
    // nghttp2 may skip DATA frames and send trailer HEADERS immediately.
    auto submitPendingTrailers = [&]() {
        if (ctx->pending_trailers.empty()) {
            return;
        }
        std::vector<nghttp2_nv> nva;
        nva.reserve(ctx->pending_trailers.size());
        for (const auto& t : ctx->pending_trailers) {
            nghttp2_nv nv = {
                reinterpret_cast<uint8_t*>(const_cast<char*>(t.first.c_str())),
                reinterpret_cast<uint8_t*>(const_cast<char*>(t.second.c_str())),
                t.first.size(), t.second.size(), NGHTTP2_NV_FLAG_NONE
            };
            nva.push_back(nv);
        }
        int rv = nghttp2_submit_trailer(session, stream_id, nva.data(), nva.size());
        if (rv != 0) {
            printd(1, "dataProviderReadCallback: nghttp2_submit_trailer failed: %s\n",
                nghttp2_strerror(rv));
            ctx->trailer_submit_error = rv;
        }
        if (http2DebugEnabled()) {
            fprintf(stderr, "HTTP2 DEBUG: dataProviderReadCallback submitted %zu trailers "
                "for stream %d\n", ctx->pending_trailers.size(), stream_id);
            fflush(stderr);
        }
    };

    auto it = h2->pending_body_data.find(stream_id);
    if (it == h2->pending_body_data.end()) {
        if (ctx->defer_on_empty) {
            printd(5, "dataProviderReadCallback() stream_id=%d requested=%zu deferred\n", stream_id, length);
            return NGHTTP2_ERR_DEFERRED;
        }
        *data_flags |= NGHTTP2_DATA_FLAG_EOF;
        if (ctx->no_end_stream) {
            *data_flags |= NGHTTP2_DATA_FLAG_NO_END_STREAM;
            submitPendingTrailers();
            if (ctx->trailer_submit_error) {
                // Trailer submission failed; RST_STREAM this stream
                return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
            }
        }
        if (ctx->remove_on_empty) {
            h2->pending_data_providers.erase(stream_id);
        }
        printd(5, "dataProviderReadCallback() stream_id=%d requested=%zu EOF(no data) no_end_stream=%d\n",
            stream_id, length, ctx->no_end_stream ? 1 : 0);
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
            if (is_end && ctx->pending_trailers.empty()) {
                // End of stream with no trailers: let DATA carry END_STREAM directly
                // This happens for streaming client requests where sendData(data, True)
                // signals the end of the request body without sending trailers.
                *data_flags |= NGHTTP2_DATA_FLAG_EOF;
                if (ctx->remove_on_empty) {
                    h2->pending_data_providers.erase(stream_id);
                }
            } else {
                *data_flags |= NGHTTP2_DATA_FLAG_NO_END_STREAM;
                if (is_end) {
                    // Trailer mode: signal EOF without END_STREAM so trailers carry END_STREAM
                    *data_flags |= NGHTTP2_DATA_FLAG_EOF;
                    submitPendingTrailers();
                    if (ctx->trailer_submit_error) {
                        // Trailer submission failed; RST_STREAM this stream
                        return NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
                    }
                    // After trailers + EOF, the data provider is no longer needed;
                    // nghttp2 won't call it again, so clean it up proactively
                    h2->pending_data_providers.erase(stream_id);
                }
            }
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

    // Notify waitForStreamDrain() that buffer space has been freed
    if (to_copy > 0) {
        h2->notifyStreamDrain();
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

    if (http2DebugEnabled()) {
        fprintf(stderr, "HTTP2 DEBUG: onHeaderCallback stream=%d header=%s: %s trailer=%d\n",
            frame->hd.stream_id, header_name.c_str(), header_value.c_str(),
            stream->receiving_trailers ? 1 : 0);
        fflush(stderr);
    }

    // Trailer headers go to the trailers map
    if (stream->receiving_trailers) {
        stream->trailers[header_name].push_back(header_value);
        return 0;
    }

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
        stream->headers[header_name].push_back(header_value);
    }

    return 0;
}

int Http2Session::onBeginHeadersCallback(nghttp2_session* session,
        const nghttp2_frame* frame, void* user_data) {
    Http2Session* h2 = static_cast<Http2Session*>(user_data);

    if (http2DebugEnabled()) {
        fprintf(stderr, "HTTP2 DEBUG: begin headers stream=%d type=%d flags=0x%x cat=%d\n",
            frame->hd.stream_id, frame->hd.type, frame->hd.flags,
            frame->hd.type == NGHTTP2_HEADERS ? frame->headers.cat : -1);
        fflush(stderr);
    }

    if (frame->hd.type == NGHTTP2_HEADERS) {
        if (frame->headers.cat == NGHTTP2_HCAT_HEADERS) {
            // Trailer HEADERS frame (NGHTTP2_HCAT_HEADERS = non-initial HEADERS after body)
            Http2StreamInfo* stream = h2->getStream(frame->hd.stream_id);
            if (stream) {
                stream->receiving_trailers = true;
            }
        } else if (h2->is_server) {
            // For server: Create stream for incoming request
            // For client: Stream already exists for our request
            h2->getOrCreateStream(frame->hd.stream_id);
        }
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
    std::string hdr_name(reinterpret_cast<const char*>(name), namelen);
    std::string hdr_val(reinterpret_cast<const char*>(value), valuelen);
    if (http2DebugEnabled()) {
        fprintf(stderr, "HTTP2 DEBUG: invalid header stream=%d %s=%s\n",
            frame->hd.stream_id, hdr_name.c_str(), hdr_val.c_str());
        fflush(stderr);
    }
    printd(3, "Http2Session::onInvalidHeaderCallback() stream_id: %d header: %s=%s\n",
        frame->hd.stream_id, hdr_name.c_str(), hdr_val.c_str());

    // RFC 8441: When :protocol is rejected as invalid (e.g., ENABLE_CONNECT_PROTOCOL not set),
    // store the value so onFrameRecvCallback can reject the stream with RST_STREAM via the
    // Layer 2 extended CONNECT rejection check.  We cannot rely on
    // NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE here because some nghttp2 versions do not honor
    // this return value from on_invalid_header_callback (they treat it as 0, silently dropping
    // the header and causing the CONNECT to be processed without :protocol).
    if (hdr_name == ":protocol") {
        Http2Session* h2 = static_cast<Http2Session*>(user_data);
        Http2StreamInfo* stream = h2->getStream(frame->hd.stream_id);
        if (stream) {
            stream->connect_protocol = hdr_val;
        }
        return 0;
    }

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
    bool has_provider = pending_data_providers.find(stream_id) != pending_data_providers.end();
    if (!stream && !has_provider) {
        printd(2, "sendStreamData() stream %d NOT FOUND: stream=%p has_provider=%d end_stream=%d\n",
            stream_id, stream, has_provider ? 1 : 0, end_stream ? 1 : 0);
        xsink->raiseException("HTTP2-ERROR", "stream %d not found", stream_id);
        return -1;
    }

    // Check if the stream was closed by RST_STREAM from the peer
    if (stream && stream->state == Http2StreamState::Closed) {
        printd(2, "sendStreamData() stream %d CLOSED: reset=%d error_code=%u end_stream=%d\n",
            stream_id, stream->reset ? 1 : 0, stream->error_code, end_stream ? 1 : 0);
        xsink->raiseException("HTTP2-STREAM-RESET",
            "stream %d was reset by peer (error code %u)", stream_id, stream->error_code);
        return -1;
    }

    // Check if there's already too much buffered data (flow control backpressure)
    // Limit to 1MB per stream to prevent unbounded memory growth
    // Allow zero-length end_stream sends through — they just set the flag without
    // adding data, so they must not be rejected by backpressure
    constexpr size_t MAX_STREAM_BUFFER = 1024 * 1024;
    auto it = pending_body_data.find(stream_id);
    if (it != pending_body_data.end() && (len > 0 || !end_stream)) {
        size_t pending = it->second.data.size() - it->second.offset;
        if (pending > MAX_STREAM_BUFFER) {
            // Return 1 (buffer full, non-fatal) — caller decides how to handle
            printd(2, "sendStreamData() stream %d buffer full: %zu bytes pending, data dropped\n",
                stream_id, pending);
            return 1;
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

int Http2Session::submitTrailers(int32_t stream_id,
        const strcase_str_map_t& trailers, ExceptionSink* xsink) {
    std::lock_guard<std::recursive_mutex> lg(m);

    if (http2DebugEnabled()) {
        fprintf(stderr, "HTTP2 DEBUG: submitTrailers stream=%d trailer_count=%zu\n",
            stream_id, trailers.size());
        fflush(stderr);
    }
    printd(5, "submitTrailers() stream_id=%d trailer_count=%zu\n", stream_id, trailers.size());

    // Validate that the stream exists — the stream may have been moved to completed_streams
    // after markStreamComplete(), but the data provider should still be active for response
    // streaming.  Check both streams and pending_data_providers (same pattern as sendStreamData).
    Http2StreamInfo* stream = getStream(stream_id);
    if (!stream && pending_data_providers.find(stream_id) == pending_data_providers.end()) {
        xsink->raiseException("HTTP2-ERROR", "stream %d not found; cannot send trailers on a "
            "closed or non-existent stream", stream_id);
        return -1;
    }

    // Configure data provider to signal EOF without END_STREAM (so trailers carry END_STREAM)
    auto dp_it = pending_data_providers.find(stream_id);
    if (dp_it != pending_data_providers.end()) {
        dp_it->second->no_end_stream = true;
        dp_it->second->defer_on_empty = false;

        // Store trailers in the data provider context — nghttp2_submit_trailer() must be
        // called AFTER the data provider callback signals EOF + NO_END_STREAM, not before.
        // The dataProviderReadCallback will call nghttp2_submit_trailer() at the right time.
        for (const auto& t : trailers) {
            std::string lname(t.first);
            if (lname.empty()) {
                continue;
            }
            std::transform(lname.begin(), lname.end(), lname.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            if (lname[0] != ':') {
                dp_it->second->pending_trailers.emplace_back(std::move(lname), t.second);
            }
        }

        // Ensure body data signals end of stream
        auto bd_it = pending_body_data.find(stream_id);
        if (bd_it != pending_body_data.end()) {
            bd_it->second.end_stream = true;
        } else {
            // No pending body data - create an empty entry to trigger EOF
            pending_body_data[stream_id] = BodyData(nullptr, 0, true);
        }

        // Resume the data provider to flush pending body data and trigger EOF
        int rv = nghttp2_session_resume_data(session, stream_id);
        if (rv != 0 && rv != NGHTTP2_ERR_INVALID_ARGUMENT) {
            xsink->raiseException("HTTP2-ERROR", "failed to resume stream data for trailers: %s",
                nghttp2_strerror(rv));
            return -1;
        }
    } else {
        // No data provider — submit trailers directly (body already complete)
        // lowered_names keeps the lowercased header name strings alive while nghttp2_nv
        // pointers reference their .c_str() data; must outlive the nghttp2_submit_trailer call
        std::vector<std::string> lowered_names;
        std::vector<nghttp2_nv> nva;
        nva.reserve(trailers.size());
        lowered_names.reserve(trailers.size());

        for (const auto& t : trailers) {
            std::string lname(t.first);
            if (lname.empty()) {
                continue;
            }
            std::transform(lname.begin(), lname.end(), lname.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            if (lname[0] == ':') {
                continue;
            }
            lowered_names.push_back(std::move(lname));
            nghttp2_nv nv = {
                reinterpret_cast<uint8_t*>(const_cast<char*>(lowered_names.back().c_str())),
                reinterpret_cast<uint8_t*>(const_cast<char*>(t.second.c_str())),
                lowered_names.back().size(), t.second.size(), NGHTTP2_NV_FLAG_NONE
            };
            nva.push_back(nv);
        }

        int rv = nghttp2_submit_trailer(session, stream_id, nva.data(), nva.size());
        if (rv != 0) {
            xsink->raiseException("HTTP2-ERROR", "failed to submit trailers: %s",
                nghttp2_strerror(rv));
            return -1;
        }
    }

    printd(5, "submitTrailers() stream_id=%d SUCCESS\n", stream_id);
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
        const strcase_str_map_t& headers, ExceptionSink* xsink) {
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
        if (forbidden_headers.count(lname)) {
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

void Http2Session::setStreamInputStream(int32_t stream_id, InputStream* is, ExceptionSink* xsink) {
    std::lock_guard<std::recursive_mutex> lg(m);
    stream_input_streams_.emplace(stream_id, StreamInputStreamInfo(is));
    has_active_input_streams_.store(true, std::memory_order_release);
    printd(5, "Http2Session::setStreamInputStream() stream_id=%d pollable=%d fd=%d\n",
        stream_id, stream_input_streams_[stream_id].is_pollable,
        stream_input_streams_[stream_id].stream_fd);
}

int Http2Session::waitForStreamDrain(int32_t stream_id, int timeout_ms) {
    constexpr size_t MAX_STREAM_BUFFER = 1024 * 1024;

    // Phase 1: check predicate under m
    {
        std::lock_guard<std::recursive_mutex> lg(m);
        auto it = pending_body_data.find(stream_id);
        if (it == pending_body_data.end()) {
            return -1;  // stream not found
        }
        size_t pending = it->second.data.size() - it->second.offset;
        if (pending <= MAX_STREAM_BUFFER) {
            return 0;  // buffer already below threshold
        }
    }

    if (timeout_ms == 0) {
        return 1;  // no wait requested, buffer is full
    }

    // Phase 2: wait on drain_cv_ with generation-based wakeup
    unsigned gen = drain_gen_.load(std::memory_order_acquire);
    auto deadline = (timeout_ms > 0)
        ? std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms)
        : std::chrono::steady_clock::time_point::max();

    while (true) {
        {
            std::unique_lock<std::mutex> dlock(drain_mtx_);
            auto gen_changed = [this, gen]() {
                return drain_gen_.load(std::memory_order_acquire) != gen;
            };
            if (timeout_ms < 0) {
                drain_cv_.wait(dlock, gen_changed);
            } else {
                if (!drain_cv_.wait_until(dlock, deadline, gen_changed)) {
                    return 1;  // timed out
                }
            }
        }

        // Re-check predicate under m
        {
            std::lock_guard<std::recursive_mutex> lg(m);
            auto it = pending_body_data.find(stream_id);
            if (it == pending_body_data.end()) {
                return -1;  // stream gone
            }
            size_t pending = it->second.data.size() - it->second.offset;
            if (pending <= MAX_STREAM_BUFFER) {
                return 0;  // drained
            }
        }

        // Update generation for next wait iteration
        gen = drain_gen_.load(std::memory_order_acquire);
    }
}

void Http2Session::notifyStreamDrain() {
    {
        std::lock_guard<std::mutex> dlock(drain_mtx_);
        drain_gen_.fetch_add(1, std::memory_order_release);
    }
    drain_cv_.notify_all();
}

void Http2Session::processStreamInputStreams(ExceptionSink* xsink) {
    std::lock_guard<std::recursive_mutex> lg(m);
    constexpr size_t MAX_STREAM_BUFFER = 1024 * 1024;

    printd(5, "Http2Session::processStreamInputStreams() count=%zu\n", stream_input_streams_.size());
    for (auto& [stream_id, info] : stream_input_streams_) {
        if (info.eof) {
            continue;
        }

        // Backpressure: skip if buffer full
        auto it = pending_body_data.find(stream_id);
        if (it != pending_body_data.end()) {
            size_t pending = it->second.data.size() - it->second.offset;
            if (pending > MAX_STREAM_BUFFER) {
                continue;
            }
        }

        // Thread reassignment (first call only)
        if (info.need_reassign) {
            info.input_stream->reassignThread(xsink);
            if (*xsink) {
                return;
            }
            info.need_reassign = false;
        }

#if defined(__linux__) && defined(HAVE_IO_URING)
        // io_uring path for regular files — truly async kernel reads
        if (info.is_regular_file && io_uring) {
            // First, try to deliver any pending buffer retained due to backpressure
            if (info.pending_iouring_buf) {
                int rv = sendStreamData(stream_id, info.pending_iouring_buf.get(),
                    info.pending_iouring_len, false, xsink);
                if (*xsink) {
                    info.eof = true;
                    continue;
                }
                if (rv == 1) {
                    // Still full — skip this stream and try again next iteration
                    continue;
                }
                // Successfully delivered — advance offset and release buffer
                info.file_offset += info.pending_iouring_len;
                info.pending_iouring_buf.reset();
                info.pending_iouring_len = 0;
            }
            if (!info.iouring_read_pending) {
                int rv = io_uring->submitRead(info.stream_fd, info.file_offset,
                                               IOURING_READ_SIZE, stream_id,
                                               shared_from_this());
                if (rv == 0) {
                    info.iouring_read_pending = true;
                }
                // If SQ full (rv == -1), try again next iteration
            }
            // Data delivery happens in handleAsyncReadCompletion()
            continue;
        }
#endif

        // Read one chunk per stream per iteration
        if (info.is_pollable) {
            SimpleRefHolder<BinaryNode> chunk(new BinaryNode);
            chunk->preallocate(65536);
            int64 count = info.input_stream->readNonBlock(
                const_cast<void*>(chunk->getPtr()), 65536, xsink);
            if (*xsink) {
                info.eof = true;
                continue;
            }
            if (count < 0) {
                // EAGAIN — not ready yet, event loop will wake us
                continue;
            }
            if (count == 0) {
                // EOF
                info.eof = true;
                sendStreamData(stream_id, nullptr, 0, true, xsink);
            } else {
                chunk->setSize(count);
                sendStreamData(stream_id, chunk->getPtr(), count, false, xsink);
            }
        } else {
            // Non-pollable (memory streams) — readHelper never blocks
            SimpleRefHolder<BinaryNode> chunk(info.input_stream->readHelper(65536, xsink));
            if (*xsink) {
                info.eof = true;
                continue;
            }
            if (!chunk || !chunk->size()) {
                info.eof = true;
                sendStreamData(stream_id, nullptr, 0, true, xsink);
            } else {
                sendStreamData(stream_id, chunk->getPtr(), chunk->size(), false, xsink);
            }
        }
        if (*xsink) {
            return;
        }
    }

    // Clean up completed streams (unassignThread + erase)
    for (auto it = stream_input_streams_.begin(); it != stream_input_streams_.end(); ) {
        if (it->second.eof) {
#if defined(__linux__) && defined(HAVE_IO_URING)
            if (io_uring && it->second.iouring_read_pending) {
                io_uring->cancelStream(it->first, shared_from_this());
            }
#endif
            if (!it->second.need_reassign) {
                ExceptionSink tmp;
                it->second.input_stream->unassignThread(&tmp);
            }
            it = stream_input_streams_.erase(it);
        } else {
            ++it;
        }
    }
    // Update atomic flag after cleanup
    if (stream_input_streams_.empty()) {
        has_active_input_streams_.store(false, std::memory_order_release);
    }
}

void Http2Session::getExtraFds(std::vector<std::pair<int, int>>& extra_fds) const {
    std::lock_guard<std::recursive_mutex> lg(m);
    for (auto& [stream_id, info] : stream_input_streams_) {
        if (!info.eof && info.is_pollable && info.stream_fd >= 0
                && info.is_epoll_compatible) {
            extra_fds.push_back({info.stream_fd, SOCK_POLLIN});
        }
    }
}

#if defined(__linux__) && defined(HAVE_IO_URING)
void Http2Session::handleAsyncReadCompletion(int32_t stream_id, const char* data,
                                              size_t length, int error,
                                              std::unique_ptr<char[]> buffer,
                                              ExceptionSink* xsink) {
    std::lock_guard<std::recursive_mutex> lg(m);

    auto it = stream_input_streams_.find(stream_id);
    if (it == stream_input_streams_.end()) {
        // Stream already closed/cancelled — ignore stale completion
        return;
    }

    auto& info = it->second;
    info.iouring_read_pending = false;

    if (error) {
        printd(2, "Http2Session::handleAsyncReadCompletion() stream=%d error=%s\n",
            stream_id, strerror(error));
        xsink->raiseException("FILE-READ-ERROR",
            "async file read failed for stream %d: %s", stream_id, strerror(error));
        info.eof = true;
        return;
    }

    if (length == 0) {
        // EOF
        printd(5, "Http2Session::handleAsyncReadCompletion() stream=%d EOF\n", stream_id);
        info.eof = true;
        sendStreamData(stream_id, nullptr, 0, true, xsink);
    } else {
        // Try to send data; handle backpressure (buffer full)
        printd(5, "Http2Session::handleAsyncReadCompletion() stream=%d %zu bytes offset=%zu\n",
            stream_id, length, info.file_offset);
        int rv = sendStreamData(stream_id, data, length, false, xsink);
        if (*xsink) {
            // sendStreamData failed — stop the stream to prevent infinite retry
            info.eof = true;
        } else if (rv == 1) {
            // Buffer full — retain the buffer for retry in processStreamInputStreams()
            // Do NOT advance file_offset until data is successfully delivered
            printd(3, "Http2Session::handleAsyncReadCompletion() stream=%d backpressure, "
                "retaining %zu bytes for later delivery\n", stream_id, length);
            info.pending_iouring_buf = std::move(buffer);
            info.pending_iouring_len = length;
        } else {
            // Successfully enqueued — advance file offset
            info.file_offset += length;
        }
    }
}
#endif

// httpMultiHeadersToQoreHash is a template defined in Http2Session.h
