/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    WebSocketFrameCodec.cpp

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

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
#include <qore/WebSocketFrameCodec.h>

#include <openssl/rand.h>

#include <cstring>
#include <cassert>

// ---------------------------------------------------------------------------
// WebSocketFrameDecoder
// ---------------------------------------------------------------------------

WebSocketFrameDecoder::WebSocketFrameDecoder() = default;
WebSocketFrameDecoder::~WebSocketFrameDecoder() = default;

void WebSocketFrameDecoder::appendData(const void* data, size_t len) {
    if (!data || len == 0) {
        return;
    }
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    buffer.insert(buffer.end(), bytes, bytes + len);
}

size_t WebSocketFrameDecoder::getFrameSize() const {
    size_t avail = buffer.size() - read_pos;
    if (avail < 2) {
        return 0;
    }

    const uint8_t* buf = buffer.data() + read_pos;
    bool masked = (buf[1] & 0x80) != 0;
    uint64_t payload_len = buf[1] & 0x7F;
    size_t header_size = 2;

    if (payload_len == 126) {
        if (avail < 4) {
            return 0;
        }
        payload_len = (static_cast<uint64_t>(buf[2]) << 8) | buf[3];
        header_size = 4;
    } else if (payload_len == 127) {
        if (avail < 10) {
            return 0;
        }
        payload_len = 0;
        for (int i = 0; i < 8; ++i) {
            payload_len = (payload_len << 8) | buf[2 + i];
        }
        header_size = 10;
    }

    if (masked) {
        header_size += 4;
    }

    size_t total = header_size + static_cast<size_t>(payload_len);
    if (avail < total) {
        return 0;
    }
    return total;
}

bool WebSocketFrameDecoder::hasCompleteFrame() const {
    return getFrameSize() > 0;
}

WsFrame WebSocketFrameDecoder::nextFrame(ExceptionSink* xsink) {
    WsFrame frame{};
    frame.opcode = WSOP_CONTINUATION;
    frame.fin = false;
    frame.masked = false;
    frame.rsv = 0;
    frame.payload = nullptr;
    frame.close_code = 0;

    size_t frame_size = getFrameSize();
    if (frame_size == 0) {
        xsink->raiseException("WEBSOCKET-FRAME-ERROR", "incomplete frame data in buffer");
        return frame;
    }

    const uint8_t* buf = buffer.data() + read_pos;
    uint8_t b0 = buf[0];
    uint8_t b1 = buf[1];

    frame.fin = (b0 & 0x80) != 0;
    frame.rsv = (b0 >> 4) & 0x07;
    frame.opcode = static_cast<WsOpcode>(b0 & 0x0F);
    frame.masked = (b1 & 0x80) != 0;

    uint64_t payload_len = b1 & 0x7F;
    size_t header_size = 2;

    if (payload_len == 126) {
        payload_len = (static_cast<uint64_t>(buf[2]) << 8) | buf[3];
        header_size = 4;
    } else if (payload_len == 127) {
        payload_len = 0;
        for (int i = 0; i < 8; ++i) {
            payload_len = (payload_len << 8) | buf[2 + i];
        }
        header_size = 10;
    }

    const uint8_t* mask_key = nullptr;
    if (frame.masked) {
        mask_key = buf + header_size;
        header_size += 4;
    }

    // Extract payload
    const uint8_t* payload = buf + header_size;
    size_t plen = static_cast<size_t>(payload_len);
    SimpleRefHolder<BinaryNode> bin(new BinaryNode());
    if (plen > 0) {
        bin->append(payload, plen);

        // Unmask if needed
        if (frame.masked && mask_key) {
            uint8_t* data = static_cast<uint8_t*>(const_cast<void*>(bin->getPtr()));
            for (size_t i = 0; i < plen; ++i) {
                data[i] ^= mask_key[i % 4];
            }
        }
    }

    // Parse CLOSE code if applicable
    if (frame.opcode == WSOP_CLOSE && plen >= 2) {
        const uint8_t* close_data = static_cast<const uint8_t*>(bin->getPtr());
        frame.close_code = (static_cast<uint16_t>(close_data[0]) << 8) | close_data[1];
    }

    frame.payload = bin.release();

    // Advance read position
    read_pos += frame_size;

    // Compact buffer periodically to avoid unbounded growth
    if (read_pos > 4096) {
        compact();
    }

    return frame;
}

size_t WebSocketFrameDecoder::bufferedBytes() const {
    return buffer.size() - read_pos;
}

void WebSocketFrameDecoder::clear() {
    buffer.clear();
    read_pos = 0;
}

void WebSocketFrameDecoder::compact() {
    if (read_pos > 0) {
        buffer.erase(buffer.begin(), buffer.begin() + read_pos);
        read_pos = 0;
    }
}

// ---------------------------------------------------------------------------
// WebSocketFrameEncoder
// ---------------------------------------------------------------------------

BinaryNode* WebSocketFrameEncoder::encode(WsOpcode opcode, const void* payload,
        size_t payload_len, bool fin, bool mask, uint8_t rsv) {
    // Calculate header size
    size_t header_size = 2;
    if (payload_len >= 126 && payload_len < 65536) {
        header_size += 2;
    } else if (payload_len >= 65536) {
        header_size += 8;
    }
    if (mask) {
        header_size += 4;
    }

    SimpleRefHolder<BinaryNode> result(new BinaryNode());
    result->preallocate(header_size + payload_len);

    // Byte 0: FIN + RSV + Opcode
    uint8_t b0 = static_cast<uint8_t>(opcode) | ((rsv & 0x07) << 4);
    if (fin) {
        b0 |= 0x80;
    }
    result->append(&b0, 1);

    // Byte 1: MASK + Payload length
    uint8_t b1 = mask ? 0x80 : 0;
    if (payload_len < 126) {
        b1 |= static_cast<uint8_t>(payload_len);
        result->append(&b1, 1);
    } else if (payload_len < 65536) {
        b1 |= 126;
        result->append(&b1, 1);
        uint8_t ext[2] = {
            static_cast<uint8_t>((payload_len >> 8) & 0xFF),
            static_cast<uint8_t>(payload_len & 0xFF)
        };
        result->append(ext, 2);
    } else {
        b1 |= 127;
        result->append(&b1, 1);
        uint8_t ext[8];
        for (int i = 7; i >= 0; --i) {
            ext[7 - i] = static_cast<uint8_t>((payload_len >> (i * 8)) & 0xFF);
        }
        result->append(ext, 8);
    }

    // Masking key + masked payload
    if (mask) {
        uint8_t mask_key[4];
        // Use OpenSSL RAND_bytes for cryptographically secure masking key
        if (RAND_bytes(mask_key, 4) != 1) {
            // Fallback: use random() if RAND_bytes fails (should not happen in practice)
            uint32_t r = static_cast<uint32_t>(random());
            memcpy(mask_key, &r, 4);
        }

        result->append(mask_key, 4);

        if (payload && payload_len > 0) {
            const uint8_t* src = static_cast<const uint8_t*>(payload);
            std::vector<uint8_t> masked(payload_len);
            for (size_t i = 0; i < payload_len; ++i) {
                masked[i] = src[i] ^ mask_key[i % 4];
            }
            result->append(masked.data(), payload_len);
        }
    } else {
        if (payload && payload_len > 0) {
            result->append(payload, payload_len);
        }
    }

    return result.release();
}

BinaryNode* WebSocketFrameEncoder::encodeClose(uint16_t code, const char* reason, bool mask) {
    // Close payload: 2-byte code (network byte order) + optional UTF-8 reason
    size_t reason_len = reason ? strlen(reason) : 0;
    std::vector<uint8_t> payload(2 + reason_len);
    payload[0] = static_cast<uint8_t>((code >> 8) & 0xFF);
    payload[1] = static_cast<uint8_t>(code & 0xFF);
    if (reason_len > 0) {
        memcpy(payload.data() + 2, reason, reason_len);
    }
    return encode(WSOP_CLOSE, payload.data(), payload.size(), true, mask);
}

BinaryNode* WebSocketFrameEncoder::encodePong(const void* ping_payload, size_t len, bool mask) {
    return encode(WSOP_PONG, ping_payload, len, true, mask);
}

// ---------------------------------------------------------------------------
// WebSocketFrameReassembler
// ---------------------------------------------------------------------------

WebSocketFrameReassembler::WebSocketFrameReassembler() = default;

WebSocketFrameReassembler::~WebSocketFrameReassembler() {
    if (frag_buf) {
        ExceptionSink xsink;
        frag_buf->deref(&xsink);
    }
}

bool WebSocketFrameReassembler::processFrame(const WsFrame& frame, ExceptionSink* xsink) {
    if (frame.opcode >= WSOP_CLOSE) {
        // Control frames are never fragmented per RFC 6455 -- pass through
        return true;
    }

    if (frame.opcode == WSOP_CONTINUATION) {
        if (!frag_buf) {
            xsink->raiseException("WEBSOCKET-PROTOCOL-ERROR",
                "received CONTINUATION frame without initial fragment");
            return false;
        }
        if (frame.payload && frame.payload->size() > 0) {
            frag_buf->append(frame.payload->getPtr(), frame.payload->size());
        }
        if (frame.fin) {
            return true;  // Complete message ready
        }
        return false;  // More fragments expected
    }

    // TEXT or BINARY
    if (!frame.fin) {
        // First fragment of a new message
        if (frag_buf) {
            xsink->raiseException("WEBSOCKET-PROTOCOL-ERROR",
                "received new data frame while previous fragmentation incomplete");
            return false;
        }
        frag_buf = new BinaryNode();
        if (frame.payload && frame.payload->size() > 0) {
            frag_buf->append(frame.payload->getPtr(), frame.payload->size());
        }
        first_opcode = frame.opcode;
        first_rsv = frame.rsv;
        return false;  // More fragments expected
    }

    // Complete (unfragmented) frame -- pass through
    return true;
}

BinaryNode* WebSocketFrameReassembler::getCompleteMessage(WsOpcode& opcode, uint8_t& rsv) {
    if (frag_buf) {
        opcode = first_opcode;
        rsv = first_rsv;
        BinaryNode* result = frag_buf;
        frag_buf = nullptr;
        first_opcode = WSOP_CONTINUATION;
        first_rsv = 0;
        return result;
    }
    opcode = WSOP_CONTINUATION;
    rsv = 0;
    return nullptr;
}

bool WebSocketFrameReassembler::isFragmenting() const {
    return frag_buf != nullptr;
}

void WebSocketFrameReassembler::clear(ExceptionSink* xsink) {
    if (frag_buf) {
        frag_buf->deref(xsink);
        frag_buf = nullptr;
    }
    first_opcode = WSOP_CONTINUATION;
    first_rsv = 0;
}
