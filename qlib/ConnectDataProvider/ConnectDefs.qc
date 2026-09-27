# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ConnectDefs.qc ConnectDataProvider constants and definitions

/*  ConnectDefs.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ConnectDataProvider module
public namespace ConnectDataProvider {
    #! Event ID: a message received from a Connect server stream
    public const EVENT_CONNECT_STREAM_MESSAGE = "connect-stream-message";

    #! Event ID: a Connect server stream completed successfully
    public const EVENT_CONNECT_STREAM_COMPLETE = "connect-stream-complete";

    #! Event ID: a Connect server stream error occurred
    public const EVENT_CONNECT_STREAM_ERROR = "connect-stream-error";

    #! Message ID: send a message on a bidirectional Connect stream
    public const MESSAGE_CONNECT_STREAM_SEND = "connect-stream-send";

    #! Event ID: a message received from a Connect datagram channel
    public const EVENT_CONNECT_DATAGRAM_MESSAGE = "connect-datagram-message";

    #! Event ID: a Connect datagram channel closed
    public const EVENT_CONNECT_DATAGRAM_CLOSED = "connect-datagram-closed";

    #! Event ID: a Connect datagram channel error occurred
    public const EVENT_CONNECT_DATAGRAM_ERROR = "connect-datagram-error";

    #! Message ID: send a message on a bidirectional Connect datagram channel
    public const MESSAGE_CONNECT_DATAGRAM_SEND = "connect-datagram-send";

    #! Data type for datagram closed events
    public const ConnectDatagramClosedDataType = new HashDataType("ConnectDatagramClosed");

    #! Data type for stream error events
    public const ConnectStreamErrorDataType = new HashDataType("ConnectStreamError", {
        "err": {
            "type": AbstractDataProviderType::get(StringType),
            "desc": "The error code",
        },
        "desc": {
            "type": AbstractDataProviderType::get(StringType),
            "desc": "The error description",
        },
    });

    #! Data type for stream complete events
    public const ConnectStreamCompleteDataType = new HashDataType("ConnectStreamComplete");
}
