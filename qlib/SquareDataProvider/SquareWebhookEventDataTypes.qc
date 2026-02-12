# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SquareWebhookEventDataTypes.qc Square event data types

/*  SquareWebhookEventDataTypes.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SquareDataProvider module
public namespace SquareDataProvider {

#! Data type for Square Events API event payloads
public class SquareWebhookEventDataType inherits HashDataType {
    private {
        #! Field definitions for Square events
        const Fields = {
            "event_id": {
                "type": StringOrNothingType,
                "display_name": "Event ID",
                "short_desc": "The unique event identifier",
                "desc": "The unique identifier of the event",
            },
            "event_type": {
                "type": StringOrNothingType,
                "display_name": "Event Type",
                "short_desc": "The type of event",
                "desc": "The Square event type (e.g. 'payment.created', 'order.updated')",
            },
            "merchant_id": {
                "type": StringOrNothingType,
                "display_name": "Merchant ID",
                "short_desc": "The merchant ID",
                "desc": "The ID of the Square merchant where the event occurred",
            },
            "location_id": {
                "type": StringOrNothingType,
                "display_name": "Location ID",
                "short_desc": "The location ID",
                "desc": "The ID of the location where the event occurred (if applicable)",
            },
            "created_at": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the event occurred",
                "desc": "The timestamp when the event was created (RFC 3339 format)",
            },
            "data": {
                "type": AutoHashOrNothingType,
                "display_name": "Event Data",
                "short_desc": "The event payload data",
                "desc": "The event data containing the type, id, and object of the affected resource",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("SquareWebhookEvent") {
        addQoreFields(Fields);
    }
}

} # namespace SquareDataProvider
