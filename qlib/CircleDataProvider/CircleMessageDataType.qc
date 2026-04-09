# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file CircleMessageDataType.qc defines data types for Circle messages

/*  CircleMessageDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the CircleDataProvider module
public namespace CircleDataProvider {

#! Circle send message request data type
public class CircleSendMessageRequestDataType inherits HashDataType {
    public {
        #! Field definitions for sending a Circle message
        const Fields = {
            "recipient_id": {
                "type": IntType,
                "display_name": "Recipient ID",
                "short_desc": "ID of the message recipient",
                "desc": "The ID of the community member to send the message to",
                "required": True,
                "ref_data": "members",
                "example_value": 12345,
            },
            "body": {
                "type": StringType,
                "display_name": "Body",
                "short_desc": "Message content",
                "desc": "The body content of the message",
                "required": True,
                "example_value": "Hello, how are you?",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Circle message response data type
public class CircleMessageResponseDataType inherits HashDataType {
    public {
        #! Field definitions for a Circle message response
        const Fields = {
            "id": {
                "type": IntOrNothingType,
                "display_name": "Message ID",
                "short_desc": "Unique identifier for the message",
                "desc": "The unique identifier for the message",
            },
            "body": {
                "type": StringOrNothingType,
                "display_name": "Body",
                "short_desc": "Message content",
                "desc": "The body content of the message",
            },
            "chat_room_id": {
                "type": IntOrNothingType,
                "display_name": "Chat Room ID",
                "short_desc": "ID of the chat room",
                "desc": "The ID of the chat room the message was sent in",
            },
            "created_at": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "The date and time the message was created",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace CircleDataProvider
