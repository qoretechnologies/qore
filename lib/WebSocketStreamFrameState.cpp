/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    WebSocketStreamFrameState.cpp

    Qore Programming Language

    Copyright 2026 Qore Technologies, s.r.o.

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

#include "qore/intern/WebSocketStreamFrameState.h"
#include "qore/intern/QC_Queue.h"

WebSocketStreamFrameState::WebSocketStreamFrameState(Queue* msg_queue_, SendCallback send_cb_,
        bool is_server_)
    : msg_queue(msg_queue_),
      send_cb(std::move(send_cb_)),
      is_server(is_server_) {
}

WebSocketStreamFrameState::~WebSocketStreamFrameState() {
    // Caller transfers one Queue reference to us at construction; drop it
    // here so the Queue is released when the last shared_ptr to this state
    // dies (typically when the stream is torn down).  decoder and
    // reassembler clean up their own state.
    if (msg_queue) {
        ExceptionSink xs;
        msg_queue->deref(&xs);
        xs.clear();
    }
}

int WebSocketStreamFrameState::feedData(const void* data, size_t len) {
    if (error_state || peer_closed) {
        // Ignore further data once the stream is done — control-frame
        // responses have already been dispatched (or never will be).
        return 0;
    }
    if (data && len) {
        decoder.appendData(data, len);
    }
    return processFrames();
}

int WebSocketStreamFrameState::processFrames() {
    int pushed_before = messages_pushed;
    while (decoder.hasCompleteFrame() && !error_state) {
        ExceptionSink xs;
        WsFrame frame = decoder.nextFrame(&xs);
        if (xs) {
            // Protocol-level decode error (reserved opcode, bad header, etc.)
            const QoreStringNode* desc = xs.getExceptionDesc().get<const QoreStringNode>();
            std::string m = desc ? desc->c_str() : "WebSocket decode error";
            xs.clear();
            if (frame.payload) {
                ExceptionSink dx;
                frame.payload->deref(&dx);
                dx.clear();
            }
            setError(m, WSCC_ProtocolError);
            break;
        }

        // Enforce per-message size limit against decoded frame size before
        // handing off to the reassembler (reassembler checks post-merge size
        // at every append, but this guards the single-frame fast-path too)
        if (frame.payload && frame.payload->size() > max_message_size) {
            ExceptionSink dx;
            frame.payload->deref(&dx);
            dx.clear();
            setError("WebSocket frame exceeds max message size", WSCC_MessageTooBig);
            break;
        }

        handleFrame(frame);

        if (frame.payload) {
            ExceptionSink dx;
            frame.payload->deref(&dx);
            dx.clear();
        }
    }
    return messages_pushed - pushed_before;
}

void WebSocketStreamFrameState::handleFrame(WsFrame& frame) {
    // Control frames (never fragmented per RFC 6455 §5.5)
    if (frame.opcode >= WSOP_CLOSE) {
        switch (frame.opcode) {
            case WSOP_PING: {
                // Auto-pong: encode a pong carrying the ping payload and
                // hand it to the transport.  Server-side sends unmasked;
                // client-side would mask (is_server == false).
                SimpleRefHolder<BinaryNode> pong(
                    WebSocketFrameEncoder::encodePong(
                        frame.payload ? frame.payload->getPtr() : nullptr,
                        frame.payload ? frame.payload->size() : 0,
                        !is_server  /* mask = client-side only */
                    )
                );
                if (send_cb && *pong) {
                    BinaryNode* bn = pong.release();
                    send_cb(bn);
                }
                // Notify the handler that a ping arrived; payload ref'd
                pushMessage(MessageKind::Ping,
                    frame.payload
                        ? static_cast<BinaryNode*>(frame.payload->refSelf())
                        : nullptr);
                break;
            }

            case WSOP_PONG: {
                pushMessage(MessageKind::Pong,
                    frame.payload
                        ? static_cast<BinaryNode*>(frame.payload->refSelf())
                        : nullptr);
                break;
            }

            case WSOP_CLOSE: {
                uint16_t code = WSCC_NormalClosure;
                std::string reason;
                if (frame.payload && frame.payload->size() >= 2) {
                    const uint8_t* d = static_cast<const uint8_t*>(frame.payload->getPtr());
                    code = (static_cast<uint16_t>(d[0]) << 8) | d[1];
                    if (frame.payload->size() > 2) {
                        reason.assign(reinterpret_cast<const char*>(d + 2),
                            frame.payload->size() - 2);
                    }
                }
                close_code = code;
                close_reason = reason;
                peer_closed = true;
                pushMessage(MessageKind::Close, nullptr, code, reason.c_str());
                break;
            }

            default:
                // Reserved control opcode — protocol violation
                setError("reserved WebSocket control opcode", WSCC_ProtocolError);
                break;
        }
        return;
    }

    // Data frames — feed to reassembler for fragmentation support
    ExceptionSink xs;
    bool complete = reassembler.processFrame(frame, &xs);
    if (xs) {
        const QoreStringNode* desc = xs.getExceptionDesc().get<const QoreStringNode>();
        std::string m = desc ? desc->c_str() : "WebSocket fragmentation error";
        xs.clear();
        setError(m, WSCC_ProtocolError);
        return;
    }

    if (!complete) {
        return;  // waiting for more fragments
    }

    WsOpcode msg_opcode = frame.opcode;
    uint8_t msg_rsv = frame.rsv;
    SimpleRefHolder<BinaryNode> payload;
    if (reassembler.isFragmenting() || frame.opcode == WSOP_CONTINUATION) {
        payload = reassembler.getCompleteMessage(msg_opcode, msg_rsv);
    } else if (frame.payload) {
        // Single-frame message: copy the payload so the reassembler's
        // ownership model (caller derefs in processFrames) stays clean.
        BinaryNode* b = new BinaryNode();
        if (frame.payload->size()) {
            b->append(frame.payload->getPtr(), frame.payload->size());
        }
        payload = b;
    } else {
        // Empty single-frame message
        payload = new BinaryNode();
    }

    if (payload && payload->size() > max_message_size) {
        setError("WebSocket message exceeds max message size", WSCC_MessageTooBig);
        return;
    }

    MessageKind kind = (msg_opcode == WSOP_TEXT) ? MessageKind::Text : MessageKind::Binary;
    pushMessage(kind, payload.release());
}

void WebSocketStreamFrameState::pushMessage(MessageKind kind, BinaryNode* payload,
        uint16_t code, const char* reason) {
    ExceptionSink xs;
    ReferenceHolder<QoreHashNode> h(new QoreHashNode(autoTypeInfo), &xs);
    const char* type_str = nullptr;
    switch (kind) {
        case MessageKind::Text:   type_str = "text"; break;
        case MessageKind::Binary: type_str = "binary"; break;
        case MessageKind::Ping:   type_str = "ping"; break;
        case MessageKind::Pong:   type_str = "pong"; break;
        case MessageKind::Close:  type_str = "close"; break;
        case MessageKind::Error:  type_str = "error"; break;
    }
    h->setKeyValue("type", new QoreStringNode(type_str), &xs);
    if (payload) {
        h->setKeyValue("data", payload, &xs);  // transfers ref to hash
    }
    if (kind == MessageKind::Close) {
        h->setKeyValue("code", static_cast<int64_t>(code), &xs);
        h->setKeyValue("reason", new QoreStringNode(reason ? reason : ""), &xs);
    }
    if (!xs && msg_queue) {
        msg_queue->push(&xs, h.release());
        if (!xs) {
            ++messages_pushed;
        }
    }
    xs.clear();
}

int WebSocketStreamFrameState::getAndClearMessagesPushed() {
    int n = messages_pushed;
    messages_pushed = 0;
    return n;
}

void WebSocketStreamFrameState::setError(const std::string& msg, uint16_t wire_code) {
    if (error_state) {
        return;
    }
    error_state = true;
    error_msg = msg;
    // Notify the handler
    pushMessage(MessageKind::Error, nullptr, wire_code, msg.c_str());
    // Attempt to send a close frame to the peer so they see a proper
    // termination rather than an abrupt stream reset.  Caller may also
    // tear down the transport.
    if (!close_sent && send_cb) {
        SimpleRefHolder<BinaryNode> close_frame(
            WebSocketFrameEncoder::encodeClose(wire_code, msg.c_str(),
                !is_server /* mask = client-side only */)
        );
        if (*close_frame) {
            BinaryNode* bn = close_frame.release();
            send_cb(bn);
            close_sent = true;
        }
    }
}

void WebSocketStreamFrameState::sendText(const char* text, size_t len) {
    if (close_sent || error_state || !send_cb) {
        return;
    }
    SimpleRefHolder<BinaryNode> f(
        WebSocketFrameEncoder::encode(WSOP_TEXT, text, len, true,
            !is_server /* mask = client-side only */)
    );
    if (*f) {
        send_cb(f.release());
    }
}

void WebSocketStreamFrameState::sendBinary(const void* data, size_t len) {
    if (close_sent || error_state || !send_cb) {
        return;
    }
    SimpleRefHolder<BinaryNode> f(
        WebSocketFrameEncoder::encode(WSOP_BINARY, data, len, true,
            !is_server)
    );
    if (*f) {
        send_cb(f.release());
    }
}

void WebSocketStreamFrameState::sendPing(const void* data, size_t len) {
    if (close_sent || error_state || !send_cb) {
        return;
    }
    SimpleRefHolder<BinaryNode> f(
        WebSocketFrameEncoder::encode(WSOP_PING, data, len, true, !is_server)
    );
    if (*f) {
        send_cb(f.release());
    }
}

void WebSocketStreamFrameState::sendClose(uint16_t code, const char* reason) {
    if (close_sent || !send_cb) {
        return;
    }
    SimpleRefHolder<BinaryNode> f(
        WebSocketFrameEncoder::encodeClose(code, reason, !is_server)
    );
    if (*f) {
        send_cb(f.release());
        close_sent = true;
    }
}

void WebSocketStreamFrameState::pushCloseSentinel() {
    if (!msg_queue) {
        return;
    }
    ExceptionSink xs;
    msg_queue->pushAndTakeRef(QoreValue());
    ++messages_pushed;
    xs.clear();
}
