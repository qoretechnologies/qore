// Copyright 2026 Qore Technologies, s.r.o.
// Minimal RFC 8441 WebSocket-over-HTTP/2 client using nghttp2.

#include <nghttp2/nghttp2.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/ssl.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {

const char kMagicGuid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
const int kTimeoutSeconds = 12;

struct ClientState {
    SSL_CTX* ctx = nullptr;
    SSL* ssl = nullptr;
    int fd = -1;
    nghttp2_session* session = nullptr;
    int32_t stream_id = -1;
    std::map<std::string, std::string> headers;
    bool headers_complete = false;
    bool sent_text = false;
    std::string ws_buffer;
    std::vector<std::string> messages;
};

struct DataProvider {
    std::string data;
    size_t offset = 0;
    bool no_end_stream = false;
};

struct HeaderBlock {
    std::vector<std::pair<std::string, std::string>> fields;
    std::vector<nghttp2_nv> nva;
};

static void die(const std::string& msg) {
    fprintf(stderr, "%s\n", msg.c_str());
    std::exit(1);
}

static bool debugEnabled() {
    static int enabled = -1;
    if (enabled < 0) {
        enabled = getenv("QORE_NGHTTP2_WS_DEBUG") ? 1 : 0;
    }
    return enabled == 1;
}

static void logDebug(const std::string& msg) {
    if (debugEnabled()) {
        fprintf(stderr, "%s\n", msg.c_str());
    }
}

static std::string toLower(const std::string& in) {
    std::string out = in;
    for (char& ch : out) {
        ch = static_cast<char>(::tolower(static_cast<unsigned char>(ch)));
    }
    return out;
}

static std::string computeAccept(const std::string& key) {
    std::string input = key + kMagicGuid;
    unsigned char sha[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(input.data()), input.size(), sha);
    static const char* b64 =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(28);
    for (int i = 0; i < SHA_DIGEST_LENGTH; i += 3) {
        uint32_t val = (sha[i] << 16);
        if (i + 1 < SHA_DIGEST_LENGTH) {
            val |= (sha[i + 1] << 8);
        }
        if (i + 2 < SHA_DIGEST_LENGTH) {
            val |= sha[i + 2];
        }
        out.push_back(b64[(val >> 18) & 0x3f]);
        out.push_back(b64[(val >> 12) & 0x3f]);
        out.push_back(i + 1 < SHA_DIGEST_LENGTH ? b64[(val >> 6) & 0x3f] : '=');
        out.push_back(i + 2 < SHA_DIGEST_LENGTH ? b64[val & 0x3f] : '=');
    }
    return out;
}

static std::string base64RandomKey() {
    unsigned char buf[16];
    if (RAND_bytes(buf, sizeof(buf)) != 1) {
        die("RAND_bytes failed");
    }
    unsigned char out[EVP_ENCODE_LENGTH(sizeof(buf))];
    int out_len = EVP_EncodeBlock(out, buf, sizeof(buf));
    if (out_len <= 0) {
        die("EVP_EncodeBlock failed");
    }
    return std::string(reinterpret_cast<char*>(out), out_len);
}

static ssize_t onDataRead(nghttp2_session*, int32_t, uint8_t* buf, size_t length,
        uint32_t* data_flags, nghttp2_data_source* source, void*) {
    DataProvider* dp = static_cast<DataProvider*>(source->ptr);
    size_t remaining = dp->data.size() - dp->offset;
    size_t to_copy = remaining < length ? remaining : length;
    if (to_copy > 0) {
        memcpy(buf, dp->data.data() + dp->offset, to_copy);
        dp->offset += to_copy;
    }
    if (dp->offset >= dp->data.size()) {
        *data_flags |= NGHTTP2_DATA_FLAG_EOF;
        if (dp->no_end_stream) {
            *data_flags |= NGHTTP2_DATA_FLAG_NO_END_STREAM;
        }
        delete dp;
    }
    return static_cast<ssize_t>(to_copy);
}

static std::string encodeWsFrame(uint8_t opcode, const std::string& payload) {
    std::string out;
    out.push_back(static_cast<char>(0x80 | (opcode & 0x0f)));
    size_t len = payload.size();
    if (len < 126) {
        out.push_back(static_cast<char>(len));
    } else if (len < 65536) {
        out.push_back(static_cast<char>(126));
        out.push_back(static_cast<char>((len >> 8) & 0xff));
        out.push_back(static_cast<char>(len & 0xff));
    } else {
        out.push_back(static_cast<char>(127));
        for (int i = 7; i >= 0; --i) {
            out.push_back(static_cast<char>((len >> (i * 8)) & 0xff));
        }
    }
    out.append(payload);
    return out;
}

static void tryParseWsMessages(ClientState* st) {
    while (st->ws_buffer.size() >= 2) {
        const unsigned char* buf = reinterpret_cast<const unsigned char*>(st->ws_buffer.data());
        uint8_t b0 = buf[0];
        uint8_t b1 = buf[1];
        uint8_t opcode = b0 & 0x0f;
        bool masked = (b1 & 0x80) != 0;
        size_t len = b1 & 0x7f;
        size_t offset = 2;
        if (len == 126) {
            if (st->ws_buffer.size() < 4) {
                return;
            }
            len = (buf[2] << 8) | buf[3];
            offset = 4;
        } else if (len == 127) {
            if (st->ws_buffer.size() < 10) {
                return;
            }
            uint64_t v = 0;
            for (int i = 0; i < 8; ++i) {
                v = (v << 8) | buf[2 + i];
            }
            len = static_cast<size_t>(v);
            offset = 10;
        }
        if (st->ws_buffer.size() < offset + len) {
            return;
        }
        if (masked) {
            die("masked server frame");
        }
        std::string payload = st->ws_buffer.substr(offset, len);
        st->ws_buffer.erase(0, offset + len);
        if (opcode == 0x1) {
            st->messages.push_back(payload);
        }
    }
}

static int connectTcp(const std::string& host, const std::string& port) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    int rv = getaddrinfo(host.c_str(), port.c_str(), &hints, &res);
    if (rv != 0) {
        die(std::string("getaddrinfo failed: ") + gai_strerror(rv));
    }
    int fd = -1;
    for (addrinfo* p = res; p; p = p->ai_next) {
        fd = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd == -1) {
            continue;
        }
        if (::connect(fd, p->ai_addr, p->ai_addrlen) == 0) {
            break;
        }
        ::close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    if (fd == -1) {
        die("failed to connect to host");
    }
    return fd;
}

static SSL_CTX* createContext() {
    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) {
        die("SSL_CTX_new failed");
    }
    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, nullptr);
    const unsigned char alpn[] = {0x02, 'h', '2'};
    SSL_CTX_set_alpn_protos(ctx, alpn, sizeof(alpn));
    return ctx;
}

static SSL* createSsl(SSL_CTX* ctx, int fd, const std::string& host) {
    SSL* ssl = SSL_new(ctx);
    if (!ssl) {
        die("SSL_new failed");
    }
    SSL_set_fd(ssl, fd);
    SSL_set_tlsext_host_name(ssl, host.c_str());
    if (SSL_connect(ssl) != 1) {
        die("SSL_connect failed");
    }
    const unsigned char* alpn = nullptr;
    unsigned int alpn_len = 0;
    SSL_get0_alpn_selected(ssl, &alpn, &alpn_len);
    if (!alpn || alpn_len != 2 || std::string(reinterpret_cast<const char*>(alpn), alpn_len) != "h2") {
        die("ALPN h2 not negotiated");
    }
    return ssl;
}

static int sendAll(SSL* ssl, const uint8_t* data, size_t len) {
    size_t offset = 0;
    while (offset < len) {
        int rv = SSL_write(ssl, data + offset, static_cast<int>(len - offset));
        if (rv <= 0) {
            int err = SSL_get_error(ssl, rv);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
                continue;
            }
            return -1;
        }
        offset += static_cast<size_t>(rv);
    }
    return 0;
}

static int flushPending(ClientState* st) {
    while (true) {
        const uint8_t* data = nullptr;
        ssize_t len = nghttp2_session_mem_send(st->session, &data);
        if (len < 0) {
            return -1;
        }
        if (len == 0) {
            break;
        }
        if (debugEnabled() && len >= NGHTTP2_CLIENT_MAGIC_LEN
            && memcmp(data, NGHTTP2_CLIENT_MAGIC, NGHTTP2_CLIENT_MAGIC_LEN) == 0) {
            fprintf(stderr, "nghttp2-ws-client: send client magic from nghttp2\n");
        }
        if (sendAll(st->ssl, data, static_cast<size_t>(len)) != 0) {
            return -1;
        }
    }
    return 0;
}

static int onHeader(nghttp2_session*, const nghttp2_frame* frame, const uint8_t* name,
        size_t namelen, const uint8_t* value, size_t valuelen, uint8_t, void* user_data) {
    ClientState* st = static_cast<ClientState*>(user_data);
    if (frame->hd.stream_id != st->stream_id) {
        return 0;
    }
    std::string key(reinterpret_cast<const char*>(name), namelen);
    std::string val(reinterpret_cast<const char*>(value), valuelen);
    st->headers[toLower(key)] = val;
    return 0;
}

static int onFrameRecv(nghttp2_session*, const nghttp2_frame* frame, void* user_data) {
    ClientState* st = static_cast<ClientState*>(user_data);
    if (frame->hd.stream_id != st->stream_id) {
        return 0;
    }
    if ((frame->hd.type == NGHTTP2_HEADERS || frame->hd.type == NGHTTP2_CONTINUATION)
        && (frame->hd.flags & NGHTTP2_FLAG_END_HEADERS)) {
        st->headers_complete = true;
        logDebug("nghttp2-ws-client: response headers complete");
    }
    if (frame->hd.type == NGHTTP2_GOAWAY) {
        logDebug("nghttp2-ws-client: received GOAWAY");
    } else if (frame->hd.type == NGHTTP2_RST_STREAM) {
        logDebug("nghttp2-ws-client: received RST_STREAM");
    }
    return 0;
}

static int onDataChunkRecv(nghttp2_session*, uint8_t, int32_t stream_id, const uint8_t* data,
        size_t len, void* user_data) {
    ClientState* st = static_cast<ClientState*>(user_data);
    if (stream_id != st->stream_id) {
        return 0;
    }
    st->ws_buffer.append(reinterpret_cast<const char*>(data), len);
    tryParseWsMessages(st);
    return 0;
}

static int submitWsData(ClientState* st, const std::string& data, bool no_end_stream) {
    auto* dp = new DataProvider();
    dp->data = data;
    dp->no_end_stream = no_end_stream;
    nghttp2_data_provider provider{};
    provider.source.ptr = dp;
    provider.read_callback = onDataRead;
    int rv = nghttp2_submit_data(st->session, NGHTTP2_FLAG_NONE, st->stream_id, &provider);
    if (rv != 0) {
        delete dp;
        return -1;
    }
    return 0;
}

static int onFrameSend(nghttp2_session*, const nghttp2_frame* frame, void*) {
    if (!debugEnabled()) {
        return 0;
    }
    if (frame->hd.type == NGHTTP2_SETTINGS) {
        fprintf(stderr, "nghttp2-ws-client: send SETTINGS (niv=%zu)\n", frame->settings.niv);
        for (size_t i = 0; i < frame->settings.niv; ++i) {
            fprintf(stderr, "  setting id=0x%x val=%u\n", frame->settings.iv[i].settings_id,
                frame->settings.iv[i].value);
        }
    } else if (frame->hd.type == NGHTTP2_HEADERS) {
        fprintf(stderr, "nghttp2-ws-client: send HEADERS stream=%d\n", frame->hd.stream_id);
    } else if (frame->hd.type == NGHTTP2_DATA) {
        fprintf(stderr, "nghttp2-ws-client: send DATA stream=%d\n", frame->hd.stream_id);
    }
    return 0;
}

static void initNghttp2(ClientState* st) {
    nghttp2_session_callbacks* callbacks = nullptr;
    if (nghttp2_session_callbacks_new(&callbacks) != 0) {
        die("nghttp2_session_callbacks_new failed");
    }
    nghttp2_session_callbacks_set_on_header_callback(callbacks, onHeader);
    nghttp2_session_callbacks_set_on_frame_recv_callback(callbacks, onFrameRecv);
    nghttp2_session_callbacks_set_on_frame_send_callback(callbacks, onFrameSend);
    nghttp2_session_callbacks_set_on_data_chunk_recv_callback(callbacks, onDataChunkRecv);
    nghttp2_session_callbacks_set_on_stream_close_callback(callbacks, nullptr);
    nghttp2_option* opts = nullptr;
    if (nghttp2_option_new(&opts) != 0) {
        nghttp2_session_callbacks_del(callbacks);
        die("nghttp2_option_new failed");
    }
    nghttp2_option_set_no_http_messaging(opts, 1);
    if (nghttp2_session_client_new2(&st->session, callbacks, st, opts) != 0) {
        nghttp2_option_del(opts);
        nghttp2_session_callbacks_del(callbacks);
        die("nghttp2_session_client_new2 failed");
    }
    nghttp2_option_del(opts);
    nghttp2_session_callbacks_del(callbacks);
}

static HeaderBlock makeRequestHeaders(const std::string& authority,
        const std::string& path, const std::string& ws_key) {
    HeaderBlock hb;
    hb.fields.reserve(7);
    hb.nva.reserve(7);
    hb.fields.emplace_back(":method", "CONNECT");
    hb.fields.emplace_back(":path", path);
    hb.fields.emplace_back(":scheme", "https");
    hb.fields.emplace_back(":authority", authority);
    hb.fields.emplace_back(":protocol", "websocket");
    hb.fields.emplace_back("sec-websocket-key", ws_key);
    hb.fields.emplace_back("sec-websocket-version", "13");
    for (const auto& f : hb.fields) {
        nghttp2_nv nv;
        nv.name = reinterpret_cast<uint8_t*>(const_cast<char*>(f.first.c_str()));
        nv.value = reinterpret_cast<uint8_t*>(const_cast<char*>(f.second.c_str()));
        nv.namelen = f.first.size();
        nv.valuelen = f.second.size();
        nv.flags = NGHTTP2_NV_FLAG_NONE;
        hb.nva.push_back(nv);
    }
    return hb;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 7) {
        fprintf(stderr, "Usage: nghttp2-ws-client -host <host> -port <port> -path <path>\n");
        return 2;
    }
    std::string host;
    std::string port;
    std::string path;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-host") && i + 1 < argc) {
            host = argv[++i];
        } else if (!strcmp(argv[i], "-port") && i + 1 < argc) {
            port = argv[++i];
        } else if (!strcmp(argv[i], "-path") && i + 1 < argc) {
            path = argv[++i];
        }
    }
    if (host.empty() || port.empty() || path.empty()) {
        fprintf(stderr, "Usage: nghttp2-ws-client -host <host> -port <port> -path <path>\n");
        return 2;
    }

    SSL_library_init();
    SSL_load_error_strings();

    ClientState st;
    st.ctx = createContext();
    st.fd = connectTcp(host, port);
    st.ssl = createSsl(st.ctx, st.fd, host);
    initNghttp2(&st);

    nghttp2_settings_entry iv[1] = {
        {NGHTTP2_SETTINGS_ENABLE_CONNECT_PROTOCOL, 1},
    };
    if (nghttp2_submit_settings(st.session, NGHTTP2_FLAG_NONE, iv, 1) != 0) {
        die("failed to submit settings");
    }

    std::string ws_key = base64RandomKey();
    std::string expected_accept = computeAccept(ws_key);
    std::string authority = host + ":" + port;
    HeaderBlock headers = makeRequestHeaders(authority, path, ws_key);
    nghttp2_priority_spec pri_spec;
    nghttp2_priority_spec_init(&pri_spec, 0, NGHTTP2_DEFAULT_WEIGHT, 0);
    st.stream_id = nghttp2_submit_headers(st.session, NGHTTP2_FLAG_END_HEADERS, -1,
        &pri_spec, headers.nva.data(), headers.nva.size(), nullptr);
    if (st.stream_id < 0) {
        die("failed to submit CONNECT request");
    }
    if (flushPending(&st) != 0) {
        die("failed to send initial HTTP/2 frames");
    }

    bool handshake_ok = false;
    bool exchange_ok = false;
    const std::string message_text = "nghttp2-test";
    time_t deadline = time(nullptr) + kTimeoutSeconds;
    while (time(nullptr) < deadline) {
        if (nghttp2_session_want_write(st.session)) {
            if (flushPending(&st) != 0) {
                die("failed to write pending data");
            }
        }
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(st.fd, &rfds);
        timeval tv{};
        tv.tv_sec = 0;
        tv.tv_usec = 200000;
        int rv = select(st.fd + 1, &rfds, nullptr, nullptr, &tv);
        if (rv > 0 && FD_ISSET(st.fd, &rfds)) {
            uint8_t buf[8192];
            int n = SSL_read(st.ssl, buf, sizeof(buf));
            if (n > 0) {
                ssize_t rcv = nghttp2_session_mem_recv(st.session, buf, n);
                if (rcv < 0) {
                    die("nghttp2_session_mem_recv failed");
                }
            } else if (n == 0) {
                break;
            } else {
                int err = SSL_get_error(st.ssl, n);
                if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
                    die("SSL_read failed");
                }
            }
        }

        if (st.headers_complete && !handshake_ok) {
            auto it = st.headers.find(":status");
            if (it == st.headers.end() || it->second != "200") {
                die("CONNECT failed");
            }
            auto ait = st.headers.find("sec-websocket-accept");
            if (ait == st.headers.end() || ait->second != expected_accept) {
                die("Sec-WebSocket-Accept mismatch");
            }
            logDebug("nghttp2-ws-client: handshake ok");
            handshake_ok = true;
        }

        if (handshake_ok && !st.sent_text) {
            if (submitWsData(&st, encodeWsFrame(0x1, message_text), true) != 0) {
                die("failed to send websocket data");
            }
            st.sent_text = true;
            if (flushPending(&st) != 0) {
                die("failed to flush websocket data");
            }
        }

        if (st.messages.size() >= 2) {
            if (st.messages[0] != "welcome" || st.messages[1] != message_text) {
                die("unexpected websocket message exchange");
            }
            exchange_ok = true;
            logDebug("nghttp2-ws-client: message exchange ok");
            break;
        }
    }

    if (!handshake_ok) {
        std::string status = st.headers.count(":status") ? st.headers[":status"] : "missing";
        std::string accept = st.headers.count("sec-websocket-accept") ? "present" : "missing";
        die("timeout waiting for websocket handshake (status=" + status + " accept=" + accept + ")");
    }
    if (!exchange_ok) {
        die("timeout waiting for websocket messages");
    }

    nghttp2_session_del(st.session);
    SSL_shutdown(st.ssl);
    SSL_free(st.ssl);
    SSL_CTX_free(st.ctx);
    close(st.fd);
    return 0;
}
