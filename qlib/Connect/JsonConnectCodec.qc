# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file JsonConnectCodec.qc JSON codec for the Connect protocol

/*  JsonConnectCodec.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! @see JsonConnectCodec
public namespace Connect;

%ifndef NoJson

public namespace Connect {

#! JSON codec implementation for the Connect protocol
/** Serializes and deserializes messages using JSON.

    Content-Types:
    - Unary: \c application/json
    - Streaming: \c application/connect+json

    @note JSON does not preserve full Qore type fidelity; dates are serialized
    as ISO 8601 strings, arbitrary-precision numbers may lose precision, and
    binary values are base64-encoded.  Use the YAML codec when type fidelity is
    required.

    This codec is only available when the \c json module is installed.
*/
public class JsonConnectCodec inherits AbstractConnectCodec {
    #! Returns the codec name
    /** @return \c "json"
    */
    string name() {
        return "json";
    }

    #! Serializes a value to JSON binary
    /** @param value the value to serialize
        @return the JSON-encoded binary data (UTF-8)
    */
    binary serialize(auto value) {
        return binary(make_json(value));
    }

    #! Deserializes JSON binary data to a Qore value
    /** @param data the JSON-encoded binary data
        @return the deserialized value
    */
    auto deserialize(binary data) {
        return parse_json(data.toString("utf-8"));
    }
}

} # namespace Connect

%endif
