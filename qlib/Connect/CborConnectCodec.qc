# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file CborConnectCodec.qc CBOR codec for the Connect protocol

/*  CborConnectCodec.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see CborConnectCodec
public namespace Connect;

%ifndef NoCbor

public namespace Connect {

#! CBOR codec implementation for the Connect protocol
/** Serializes and deserializes messages using CBOR (RFC 8949).

    Content-Types:
    - Unary: \c application/cbor
    - Streaming: \c application/connect+cbor

    CBOR is a compact binary serialization format that preserves most Qore types
    including integers, floats, strings, binary data, booleans, dates (as RFC 3339
    tagged values), and arbitrary-precision numbers (as CBOR bigdec tag 4).

    @note Relative dates (durations) are serialized as ISO 8601 duration strings.
    NaN and Inf number values are serialized as null.

    This codec is only available when the \c json module version 1.12+ is
    installed (which provides the \c make_cbor() and \c parse_cbor() functions).
*/
public class CborConnectCodec inherits AbstractConnectCodec {
    #! Returns the codec name
    /** @return \c "cbor"
    */
    string name() {
        return "cbor";
    }

    #! Serializes a value to CBOR binary
    /** @param value the value to serialize
        @return the CBOR-encoded binary data
    */
    binary serialize(auto value) {
        return make_cbor(value);
    }

    #! Deserializes CBOR binary data to a Qore value
    /** @param data the CBOR-encoded binary data
        @return the deserialized value
    */
    auto deserialize(binary data) {
        return parse_cbor(data);
    }
}

} # namespace Connect

%endif
