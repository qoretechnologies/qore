# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConnectCodec.qc Connect protocol codec abstraction

/*  ConnectCodec.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Abstract codec interface for the Connect protocol
/** Codecs handle serialization and deserialization of message payloads.
    The codec name is used in HTTP Content-Type headers:
    - Unary: \c application/{name} (e.g., \c application/yaml)
    - Streaming: \c application/connect+{name} (e.g., \c application/connect+yaml)
*/
public class AbstractConnectCodec {
    #! Returns the codec name used in Content-Type headers
    /** @return the codec name (e.g., \c "yaml", \c "proto", \c "json")
    */
    abstract string name();

    #! Serializes a Qore value to binary
    /** @param value the value to serialize
        @return the serialized binary data
    */
    abstract binary serialize(auto value);

    #! Deserializes binary data to a Qore value
    /** @param data the binary data to deserialize
        @return the deserialized value
    */
    abstract auto deserialize(binary data);
}

} # namespace Connect
