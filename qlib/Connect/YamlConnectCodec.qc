# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file YamlConnectCodec.qc YAML codec for the Connect protocol

/*  YamlConnectCodec.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace Connect {

#! YAML codec implementation for the Connect protocol
/** Serializes and deserializes messages using YAML.

    Content-Types:
    - Unary: \c application/yaml
    - Streaming: \c application/connect+yaml

    YAML preserves full Qore type fidelity including date, number (arbitrary
    precision), duration, binary, and null types.
*/
public class YamlConnectCodec inherits AbstractConnectCodec {
    #! Returns the codec name
    /** @return \c "yaml"
    */
    string name() {
        return "yaml";
    }

    #! Serializes a value to YAML binary
    /** @param value the value to serialize
        @return the YAML-encoded binary data
    */
    binary serialize(auto value) {
        return binary(make_yaml(value));
    }

    #! Deserializes YAML binary data to a Qore value
    /** @param data the YAML-encoded binary data
        @return the deserialized value
    */
    auto deserialize(binary data) {
        return parse_yaml(data.toString("utf-8"));
    }
}

} # namespace Connect
