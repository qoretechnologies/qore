/* -*- indent-tabs-mode: nil -*- */
/*
    Http2Session.cpp

    HTTP/2 Session wrapper using nghttp2

    Qore Programming Language

    Copyright (C) 2025 Qore Technologies, s.r.o.

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

#ifdef HAVE_HTTP2

#include "qore/intern/Http2Session.h"
#include "qore/intern/qore_socket_private.h"

#include <cstring>

Http2Session::Http2Session(qore_socket_private* sock, bool is_server)
    : sock(sock), is_server(is_server) {
}

Http2Session::~Http2Session() {
    if (session) {
        nghttp2_session_del(session);
    }
}

Http2Session* Http2Session::createClient(qore_socket_private* sock, ExceptionSink* xsink) {
    std::unique_ptr<Http2Session> h2(new Http2Session(sock, false));
    if (h2->init(xsink)) {
        return nullptr;
    }
    return h2.release();
}

Http2Session* Http2Session::createServer(qore_socket_private* sock, ExceptionSink* xsink) {
    std::unique_ptr<Http2Session> h2(new Http2Session(sock, true));
    if (h2->init(xsink)) {
        return nullptr;
    }
    return h2.release();
}

int Http2Session::init(ExceptionSink* xsink) {
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

    int rv;
    if (is_server) {
        rv = nghttp2_session_server_new(&session, callbacks, this);
    } else {
        rv = nghttp2_session_client_new(&session, callbacks, this);
    }

    nghttp2_session_callbacks_del(callbacks);

    if (rv != 0) {
        xsink->raiseException("HTTP2-ERROR", "failed to create nghttp2 session: %s",
            nghttp2_strerror(rv));
        return -1;
    }

    return 0;
}

int Http2Session::sendConnectionPreface(ExceptionSink* xsink) {
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
    if (is_server && local_settings.enable_connect_protocol) {
        iv.push_back({NGHTTP2_SETTINGS_ENABLE_CONNECT_PROTOCOL, local_settings.enable_connect_protocol});
    }

    int rv = nghttp2_submit_settings(session, NGHTTP2_FLAG_NONE, iv.data(), iv.size());
    if (rv != 0) {
        xsink->raiseException("HTTP2-ERROR", "failed to submit SETTINGS: %s",
            nghttp2_strerror(rv));
        return -1;
    }
    return sendPendingData(-1, xsink);
}

int Http2Session::submitSettings(const Http2Settings& settings, ExceptionSink* xsink) {
    local_settings = settings;

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
    if (is_server && settings.enable_connect_protocol) {
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

int32_t Http2Session::submitRequest(const char* method, const char* path,
        const std::map<std::string, std::string>& headers,
        const void* body, size_t body_len, ExceptionSink* xsink) {
    if (is_server) {
        xsink->raiseException("HTTP2-ERROR", "cannot submit request on server session");
        return -1;
    }

    // Build pseudo-headers
    std::vector<nghttp2_nv> nva;
    nva.reserve(headers.size() + 4);

    // Add pseudo-headers first
    std::string method_str(method);
    std::string path_str(path);
    std::string scheme_str("https");
    std::string authority;

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

    // Add regular headers (excluding pseudo-headers, host, and connection-specific headers)
    // RFC 7540 Section 8.1.2.2: HTTP/2 MUST NOT use connection-specific header fields
    for (const auto& h : headers) {
        // Skip pseudo-headers and host (handled separately)
        if (h.first[0] == ':' || h.first == "host" || h.first == "Host") {
            continue;
        }
        // Skip connection-specific headers (forbidden in HTTP/2)
        if (h.first == "Connection" || h.first == "connection" ||
            h.first == "Keep-Alive" || h.first == "keep-alive" ||
            h.first == "Proxy-Connection" || h.first == "proxy-connection" ||
            h.first == "Transfer-Encoding" || h.first == "transfer-encoding" ||
            h.first == "Upgrade" || h.first == "upgrade") {
            continue;
        }
        nghttp2_nv nv = {
            reinterpret_cast<uint8_t*>(const_cast<char*>(h.first.c_str())),
            reinterpret_cast<uint8_t*>(const_cast<char*>(h.second.c_str())),
            h.first.size(), h.second.size(), NGHTTP2_NV_FLAG_NONE
        };
        nva.push_back(nv);
    }

    nghttp2_data_provider* data_prd = nullptr;
    nghttp2_data_provider data_provider;

    if (body && body_len > 0) {
        // Store body data for callback (copies the data)
        int32_t stream_id = nghttp2_session_get_next_stream_id(session);
        pending_body_data[stream_id] = BodyData(body, body_len);

        data_provider.source.ptr = this;
        data_provider.read_callback = [](nghttp2_session* session, int32_t stream_id,
                uint8_t* buf, size_t length, uint32_t* data_flags,
                nghttp2_data_source* source, void* user_data) -> ssize_t {
            Http2Session* h2 = static_cast<Http2Session*>(user_data);
            auto it = h2->pending_body_data.find(stream_id);
            if (it == h2->pending_body_data.end()) {
                *data_flags |= NGHTTP2_DATA_FLAG_EOF;
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
                *data_flags |= NGHTTP2_DATA_FLAG_EOF;
                h2->pending_body_data.erase(it);
            }

            return static_cast<ssize_t>(to_copy);
        };
        data_prd = &data_provider;
    }

    int32_t stream_id = nghttp2_submit_request(session, nullptr, nva.data(), nva.size(),
        data_prd, this);

    if (stream_id < 0) {
        xsink->raiseException("HTTP2-ERROR", "failed to submit request: %s",
            nghttp2_strerror(stream_id));
        return -1;
    }

    // Create stream info
    getOrCreateStream(stream_id);

    return stream_id;
}

int Http2Session::submitResponse(int32_t stream_id, int status_code,
        const std::map<std::string, std::string>& headers,
        const void* body, size_t body_len, ExceptionSink* xsink) {
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

    nghttp2_data_provider* data_prd = nullptr;
    nghttp2_data_provider data_provider;

    if (body && body_len > 0) {
        // Store body data for callback (copies the data)
        pending_body_data[stream_id] = BodyData(body, body_len);

        data_provider.source.ptr = this;
        data_provider.read_callback = [](nghttp2_session* session, int32_t stream_id,
                uint8_t* buf, size_t length, uint32_t* data_flags,
                nghttp2_data_source* source, void* user_data) -> ssize_t {
            Http2Session* h2 = static_cast<Http2Session*>(user_data);
            auto it = h2->pending_body_data.find(stream_id);
            if (it == h2->pending_body_data.end()) {
                *data_flags |= NGHTTP2_DATA_FLAG_EOF;
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
                *data_flags |= NGHTTP2_DATA_FLAG_EOF;
                h2->pending_body_data.erase(it);
            }

            return static_cast<ssize_t>(to_copy);
        };
        data_prd = &data_provider;
    }

    int rv = nghttp2_submit_response(session, stream_id, nva.data(), nva.size(), data_prd);
    if (rv != 0) {
        xsink->raiseException("HTTP2-ERROR", "failed to submit response: %s",
            nghttp2_strerror(rv));
        return -1;
    }

    return 0;
}

int32_t Http2Session::submitPushPromise(int32_t stream_id, const char* path,
        const std::map<std::string, std::string>& headers, ExceptionSink* xsink) {
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
    std::string scheme_str("https");

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

int Http2Session::sendPendingData(int timeout_ms, ExceptionSink* xsink) {
    // First, collect data from nghttp2
    while (nghttp2_session_want_write(session)) {
        const uint8_t* data;
        ssize_t len = nghttp2_session_mem_send(session, &data);
        if (len < 0) {
            xsink->raiseException("HTTP2-ERROR", "nghttp2_session_mem_send failed: %s",
                nghttp2_strerror(static_cast<int>(len)));
            return -1;
        }
        if (len == 0) {
            break;
        }
        send_buffer.insert(send_buffer.end(), data, data + len);
    }

    // Send buffered data
    if (!send_buffer.empty()) {
        int rc = sock->send(xsink, "Http2Session", "sendPendingData",
            send_buffer.data(), send_buffer.size(), timeout_ms);
        if (rc < 0) {
            return -1;
        }
        // Clear buffer after sending
        send_buffer.clear();
    }

    return 0;
}

int Http2Session::receiveData(int timeout_ms, ExceptionSink* xsink) {
    char* buf;
    ssize_t len = sock->brecv(xsink, "receiveData", buf, 16384, 0, timeout_ms, false);
    if (len < 0) {
        return -1;
    }
    if (len == 0) {
        // Connection closed or timeout
        return -1;
    }

    ssize_t rv = nghttp2_session_mem_recv(session, reinterpret_cast<uint8_t*>(buf), len);
    if (rv < 0) {
        xsink->raiseException("HTTP2-ERROR", "nghttp2_session_mem_recv failed: %s",
            nghttp2_strerror(static_cast<int>(rv)));
        return -1;
    }

    return 0;
}

Http2StreamInfo* Http2Session::getStream(int32_t stream_id) {
    auto it = streams.find(stream_id);
    if (it != streams.end()) {
        return it->second.get();
    }
    return nullptr;
}

Http2StreamInfo* Http2Session::getOrCreateStream(int32_t stream_id) {
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
    auto it = streams.find(stream_id);
    if (it != streams.end()) {
        completed_streams.push(std::move(it->second));
        streams.erase(it);
    }
}

std::unique_ptr<Http2StreamInfo> Http2Session::takeCompletedStream() {
    if (completed_streams.empty()) {
        return nullptr;
    }
    auto stream = std::move(completed_streams.front());
    completed_streams.pop();
    return stream;
}

bool Http2Session::wantRead() const {
    return nghttp2_session_want_read(session) != 0;
}

bool Http2Session::wantWrite() const {
    return nghttp2_session_want_write(session) != 0 || send_offset < send_buffer.size();
}

// nghttp2 callbacks

ssize_t Http2Session::sendCallback(nghttp2_session* session, const uint8_t* data,
        size_t length, int flags, void* user_data) {
    // We use mem_send instead, so this callback just returns success
    return static_cast<ssize_t>(length);
}

int Http2Session::onFrameRecvCallback(nghttp2_session* session,
        const nghttp2_frame* frame, void* user_data) {
    Http2Session* h2 = static_cast<Http2Session*>(user_data);

    switch (frame->hd.type) {
        case NGHTTP2_HEADERS: {
            if (frame->hd.flags & NGHTTP2_FLAG_END_HEADERS) {
                Http2StreamInfo* stream = h2->getStream(frame->hd.stream_id);
                if (stream) {
                    stream->headers_complete = true;
                    // For requests without a body (like GET), END_STREAM is on the HEADERS frame
                    if (frame->hd.flags & NGHTTP2_FLAG_END_STREAM) {
                        stream->body_complete = true;
                        h2->markStreamComplete(frame->hd.stream_id);
                    }
                }
            }
            break;
        }

        case NGHTTP2_DATA:
            if (frame->hd.flags & NGHTTP2_FLAG_END_STREAM) {
                Http2StreamInfo* stream = h2->getStream(frame->hd.stream_id);
                if (stream) {
                    stream->body_complete = true;
                    if (stream->headers_complete) {
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
                            break;
                    }
                }
            }
            break;

        case NGHTTP2_GOAWAY:
            h2->goaway_received = true;
            h2->last_stream_id = frame->goaway.last_stream_id;
            break;

        default:
            break;
    }

    return 0;
}

int Http2Session::onDataChunkRecvCallback(nghttp2_session* session, uint8_t flags,
        int32_t stream_id, const uint8_t* data, size_t len, void* user_data) {
    Http2Session* h2 = static_cast<Http2Session*>(user_data);
    Http2StreamInfo* stream = h2->getOrCreateStream(stream_id);
    if (stream) {
        stream->body.insert(stream->body.end(), data, data + len);
    }
    return 0;
}

int Http2Session::onStreamCloseCallback(nghttp2_session* session, int32_t stream_id,
        uint32_t error_code, void* user_data) {
    Http2Session* h2 = static_cast<Http2Session*>(user_data);
    Http2StreamInfo* stream = h2->getStream(stream_id);
    if (stream) {
        stream->state = Http2StreamState::Closed;
        stream->error_code = error_code;
        if (error_code != 0) {
            stream->reset = true;
        }
        // Mark as complete even if there was an error
        stream->body_complete = true;
        if (stream->headers_complete) {
            h2->markStreamComplete(stream_id);
        }
    }
    return 0;
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

    return 0;
}

int Http2Session::sendStreamData(int32_t stream_id, const void* data, size_t len,
        bool end_stream, ExceptionSink* xsink) {
    Http2StreamInfo* stream = getStream(stream_id);
    if (!stream) {
        xsink->raiseException("HTTP2-ERROR", "stream %d not found", stream_id);
        return -1;
    }

    // Store data for the data provider callback (copies the data)
    pending_body_data[stream_id] = BodyData(data, len);

    // Create a data provider for this stream
    nghttp2_data_provider data_provider;
    data_provider.source.ptr = this;
    data_provider.read_callback = [](nghttp2_session* session, int32_t stream_id,
            uint8_t* buf, size_t length, uint32_t* data_flags,
            nghttp2_data_source* source, void* user_data) -> ssize_t {
        Http2Session* h2 = static_cast<Http2Session*>(user_data);
        auto it = h2->pending_body_data.find(stream_id);
        if (it == h2->pending_body_data.end()) {
            // No more data - check if we should send END_STREAM
            // Note: For streaming, we don't set EOF here
            return NGHTTP2_ERR_DEFERRED;
        }

        BodyData& bd = it->second;
        size_t remaining = bd.data.size() - bd.offset;
        size_t to_copy = std::min(remaining, length);

        if (to_copy > 0) {
            memcpy(buf, bd.data.data() + bd.offset, to_copy);
            bd.offset += to_copy;
        }

        if (bd.offset >= bd.data.size()) {
            h2->pending_body_data.erase(it);
            // Check stream type to determine if we should send EOF
            Http2StreamInfo* stream = h2->getStream(stream_id);
            if (stream && !stream->isStreaming()) {
                *data_flags |= NGHTTP2_DATA_FLAG_EOF;
            }
        }

        return static_cast<ssize_t>(to_copy);
    };

    // Resume the stream with more data
    int rv = nghttp2_session_resume_data(session, stream_id);
    if (rv != 0 && rv != NGHTTP2_ERR_INVALID_ARGUMENT) {
        // INVALID_ARGUMENT means stream is not deferred, which is OK for first send
        xsink->raiseException("HTTP2-ERROR", "failed to resume stream data: %s",
            nghttp2_strerror(rv));
        return -1;
    }

    return 0;
}

void Http2Session::setStreamType(int32_t stream_id, Http2StreamType type) {
    Http2StreamInfo* stream = getStream(stream_id);
    if (stream) {
        stream->stream_type = type;
    }
}

int Http2Session::submitConnectResponse(int32_t stream_id, int status_code,
        const std::map<std::string, std::string>& headers, ExceptionSink* xsink) {
    if (!is_server) {
        xsink->raiseException("HTTP2-ERROR", "cannot submit CONNECT response on client session");
        return -1;
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

    // For CONNECT response, we don't send a body - the stream becomes a tunnel
    // Note: We do NOT set END_STREAM flag - the stream remains open for bidirectional data
    int rv = nghttp2_submit_response(session, stream_id, nva.data(), nva.size(), nullptr);
    if (rv != 0) {
        xsink->raiseException("HTTP2-ERROR", "failed to submit CONNECT response: %s",
            nghttp2_strerror(rv));
        return -1;
    }

    // If successful (2xx), mark stream as WebSocket type if protocol is websocket
    if (status_code >= 200 && status_code < 300 && stream->connect_protocol == "websocket") {
        stream->stream_type = Http2StreamType::WebSocket;
    }

    return 0;
}

#endif // HAVE_HTTP2
