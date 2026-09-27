# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ProtobufConnectCodec.qc Protobuf codec for the Connect protocol

/*  ProtobufConnectCodec.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see ProtobufConnectCodec
public namespace Connect;

%ifndef NoGrpc

public namespace Connect {

#! Protobuf codec implementation for the Connect protocol
/** Wraps \c ProtobufSchema from the \c protobuf built-in module for protobuf
    serialization/deserialization.

    Content-Types:
    - Unary: \c application/proto
    - Streaming: \c application/connect+proto

    This codec is only available when the \c grpc binary module is installed
    (provides gRPC protocol support on top of the \c protobuf module).
*/
public class ProtobufConnectCodec inherits AbstractConnectCodec {
    private {
        #! The protobuf schema
        ProtobufSchema m_schema;
        #! Default message type for untyped calls
        string m_default_type;
    }

    #! Create a protobuf codec
    /** @param schema the protobuf schema
        @param default_type the default message type for serialize/deserialize
    */
    constructor(ProtobufSchema schema, string default_type) {
        m_schema = schema;
        m_default_type = default_type;
    }

    #! Returns the codec name
    /** @return \c "proto"
    */
    string name() {
        return "proto";
    }

    #! Serializes a value to protobuf binary using the default message type
    /** @param value the value to serialize (must be a hash)
        @return the protobuf-encoded binary data
    */
    binary serialize(auto value) {
        return m_schema.encode(m_default_type, value);
    }

    #! Deserializes protobuf binary data using the default message type
    /** @param data the protobuf-encoded binary data
        @return the deserialized hash
    */
    auto deserialize(binary data) {
        return m_schema.decode(m_default_type, data);
    }

    #! Serializes with an explicit message type
    /** @param message_type the protobuf message type name
        @param value the value to serialize
        @return the protobuf-encoded binary data
    */
    binary serializeTyped(string message_type, auto value) {
        return m_schema.encode(message_type, value);
    }

    #! Deserializes with an explicit message type
    /** @param message_type the protobuf message type name
        @param data the protobuf-encoded binary data
        @return the deserialized hash
    */
    auto deserializeTyped(string message_type, binary data) {
        return m_schema.decode(message_type, data);
    }

    #! Get the underlying protobuf schema
    /** @return the ProtobufSchema
    */
    ProtobufSchema getSchema() {
        return m_schema;
    }
}

} # namespace Connect

%endif
