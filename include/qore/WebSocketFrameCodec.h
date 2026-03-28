/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    WebSocketFrameCodec.h

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

#ifndef _QORE_WEBSOCKETFRAMECODEC_H
#define _QORE_WEBSOCKETFRAMECODEC_H

#include <qore/common.h>
#include <qore/BinaryNode.h>
#include <qore/QoreStringNode.h>
#include <qore/ExceptionSink.h>

#include <cstdint>
#include <vector>

//! WebSocket opcodes (RFC 6455 Section 5.2)
enum WsOpcode : uint8_t {
    WSOP_CONTINUATION = 0x0,
    WSOP_TEXT         = 0x1,
    WSOP_BINARY       = 0x2,
    WSOP_CLOSE        = 0x8,
    WSOP_PING         = 0x9,
    WSOP_PONG         = 0xA,
};

//! WebSocket close codes (RFC 6455 Section 7.4.1)
enum WsCloseCode : uint16_t {
    WSCC_NormalClosure    = 1000,
    WSCC_GoingAway        = 1001,
    WSCC_ProtocolError    = 1002,
    WSCC_UnsupportedData  = 1003,
    WSCC_InvalidPayload   = 1007,
    WSCC_PolicyViolation  = 1008,
    WSCC_MessageTooBig    = 1009,
    WSCC_InternalError    = 1011,
};

//! Parsed WebSocket frame
struct WsFrame {
    WsOpcode opcode;
    bool fin;
    bool masked;
    uint8_t rsv;           //!< RSV1|RSV2|RSV3 bits (3 bits)
    BinaryNode* payload;   //!< ref'd payload data (caller must deref)
    uint16_t close_code;   //!< only valid for WSOP_CLOSE
};

//! WebSocket frame decoder -- accumulates data and parses complete frames
/** Thread safety: NOT thread-safe. Use from a single thread (I/O thread).
    @since %Qore 2.3
*/
class DLLEXPORT WebSocketFrameDecoder {
public:
    WebSocketFrameDecoder();
    ~WebSocketFrameDecoder();

    //! Appends received data to the internal buffer
    void appendData(const void* data, size_t len);

    //! Returns true if a complete frame is available
    bool hasCompleteFrame() const;

    //! Parses and returns the next complete frame
    /** Consumes the frame bytes from the internal buffer.
        @param xsink exception sink for protocol errors
        @return the parsed frame; caller owns the payload BinaryNode ref
    */
    WsFrame nextFrame(ExceptionSink* xsink);

    //! Returns the number of buffered bytes
    size_t bufferedBytes() const;

    //! Clears the internal buffer
    void clear();

private:
    std::vector<uint8_t> buffer;
    size_t read_pos = 0;

    //! Returns total frame size from header, or 0 if incomplete
    size_t getFrameSize() const;

    //! Compact buffer by removing consumed bytes
    void compact();
};

//! WebSocket frame encoder -- builds frame bytes from payload
/** Thread safety: all methods are stateless (thread-safe).
    @since %Qore 2.3
*/
class DLLEXPORT WebSocketFrameEncoder {
public:
    //! Encodes a WebSocket frame
    /** @param opcode the frame opcode
        @param payload the payload data (or nullptr for empty)
        @param payload_len the payload length
        @param fin the FIN flag (true = final fragment)
        @param mask if true, applies random masking (client->server)
        @param rsv RSV bits (0 for no extensions)
        @return ref'd BinaryNode containing the encoded frame
    */
    static BinaryNode* encode(WsOpcode opcode, const void* payload, size_t payload_len,
        bool fin = true, bool mask = false, uint8_t rsv = 0);

    //! Encodes a CLOSE frame with code and reason
    static BinaryNode* encodeClose(uint16_t code, const char* reason = nullptr,
        bool mask = false);

    //! Encodes a PONG frame echoing the ping payload
    static BinaryNode* encodePong(const void* ping_payload, size_t len,
        bool mask = false);
};

//! WebSocket fragmented message reassembler
/** Accumulates fragments until the final frame (FIN=1) arrives.
    @since %Qore 2.3
*/
class DLLEXPORT WebSocketFrameReassembler {
public:
    WebSocketFrameReassembler();
    ~WebSocketFrameReassembler();

    //! Process a frame -- returns true if a complete message is ready
    /** @param frame the decoded frame
        @param xsink for protocol errors
        @return true if getCompleteMessage() can be called
    */
    bool processFrame(const WsFrame& frame, ExceptionSink* xsink);

    //! Returns the reassembled message (consumes it)
    /** @param opcode set to the original message opcode (TEXT or BINARY)
        @param rsv set to the RSV bits from the first fragment
        @return ref'd BinaryNode with the complete payload
    */
    BinaryNode* getCompleteMessage(WsOpcode& opcode, uint8_t& rsv);

    //! Returns true if fragmentation is in progress
    bool isFragmenting() const;

    //! Clears any in-progress fragmentation
    void clear(ExceptionSink* xsink);

private:
    BinaryNode* frag_buf = nullptr;
    WsOpcode first_opcode = WSOP_CONTINUATION;
    uint8_t first_rsv = 0;
};

#endif // _QORE_WEBSOCKETFRAMECODEC_H
