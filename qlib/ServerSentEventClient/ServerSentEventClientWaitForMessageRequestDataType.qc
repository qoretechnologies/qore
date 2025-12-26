# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ServerSentEventClientWaitForMessageRequestDataType.qc SSE connection client

/*  ServerSentEventClientWaitForMessageRequestDataType.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! the ServerSentEventClient namespace contains all the objects in the ServerSentEventClient module
public namespace ServerSentEventClient {
#! Data type for wait for message requests
public class ServerSentEventClientWaitForMessageRequestDataType inherits HashDataType {
    public {
        #! Field descriptions
        const Fields = {
            "timeout": {
                "type": IntType,
                "desc": "The number of milliseconds to wait for an incoming message; after this point, if no "
                    "SSE message has been received, an exception will be thrown",
                "required": True,
                "default_value": 10000,
            },
        };
    }

    #! Creates the object
    constructor() {
        map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type, $1.value.default_value)),
            Fields.pairIterator();
    }
}

#! Data type for wait for message requests
public const ServerSentEventClientWaitForMessageRequestDataType =
    new ServerSentEventClientWaitForMessageRequestDataType();
}
