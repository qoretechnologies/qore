# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore FreshBooksDataProvider module definition

/*  FreshBooksWebhookEventDataTypes.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the FreshBooksDataProvider module
public namespace FreshBooksDataProvider {

#! Data type for FreshBooks webhook/polling event payloads
public class FreshBooksWebhookEventDataType inherits HashDataType {
    private {
        #! Field definitions for FreshBooks webhook events
        const Fields = {
            "id": {
                "type": IntOrNothingType,
                "display_name": "ID",
                "short_desc": "The record ID",
                "desc": "The unique identifier of the record that triggered the event",
            },
            "event_type": {
                "type": StringOrNothingType,
                "display_name": "Event Type",
                "short_desc": "The type of event",
                "desc": "The event type (e.g. invoice-created, client-created)",
            },
            "account_id": {
                "type": StringOrNothingType,
                "display_name": "Account ID",
                "short_desc": "The FreshBooks account ID",
                "desc": "The FreshBooks account ID where the event occurred",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("FreshBooksWebhookEvent") {
        addQoreFields(Fields);
    }
}

} # namespace FreshBooksDataProvider
