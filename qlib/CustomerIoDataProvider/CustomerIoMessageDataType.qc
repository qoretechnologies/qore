# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CustomerIoDataProvider module definition

/*  CustomerIoMessageDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the CustomerIoDataProvider module
public namespace CustomerIoDataProvider {

#! Customer.io message record type
/** Represents a message in Customer.io.
*/
public class CustomerIoMessageDataType inherits HashDataType {
    public {
        #! Field definitions for Customer.io messages
        const Fields = {
            "id": {
                "type": StringOrNothingType,
                "display_name": "ID",
                "short_desc": "Message ID",
                "desc": "The unique identifier for the message",
            },
            "metric": {
                "type": StringOrNothingType,
                "display_name": "Metric",
                "short_desc": "Event metric",
                "desc": "The event metric (e.g., `email_sent`)",
            },
            "subject": {
                "type": StringOrNothingType,
                "display_name": "Subject",
                "short_desc": "Subject",
                "desc": "The subject line of the message",
            },
            "recipient": {
                "type": StringOrNothingType,
                "display_name": "Recipient",
                "short_desc": "Recipient",
                "desc": "The recipient of the message",
            },
            "customer_id": {
                "type": StringOrNothingType,
                "display_name": "Customer ID",
                "short_desc": "Customer ID",
                "desc": "The ID of the customer associated with this message",
            },
            "type": {
                "type": StringOrNothingType,
                "display_name": "Type",
                "short_desc": "Message type",
                "desc": "The type of message (email, sms, push, slack)",
            },
            "created": {
                "type": IntOrNothingType,
                "display_name": "Created",
                "short_desc": "Created timestamp",
                "desc": "Unix timestamp when the message was created",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace CustomerIoDataProvider
