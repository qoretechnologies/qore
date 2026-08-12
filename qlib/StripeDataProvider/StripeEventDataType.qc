# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore StripeDataProvider module definition

/*  StripeEventDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the StripeDataProvider module
public namespace StripeDataProvider {
#! The data carried by a Stripe webhook event
/** Every Stripe event has the same envelope; what differs is the object inside \c data.
*/
public class StripeEventDataTypeBase inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "id": {
                "display_name": "Event ID",
                "short_desc": "The unique ID of the event",
                "type": StringType,
                "desc": "The unique ID of the event (`evt_…`).\n\n"
                    "Stripe can deliver the same event more than once, so record this and ignore an ID "
                    "already processed; that is what makes handling **idempotent**.",
            },
            "type": {
                "display_name": "Event Type",
                "short_desc": "The type of event",
                "type": StringType,
                "desc": "The type of event, such as `charge.succeeded` or `invoice.payment_failed`",
            },
            "created": {
                "display_name": "Created",
                "short_desc": "When the event occurred",
                "type": IntType,
                "desc": "When the event occurred, as a count of seconds since the epoch",
            },
            "api_version": {
                "display_name": "API Version",
                "short_desc": "The Stripe API version the payload is formatted to",
                "type": StringOrNothingType,
                "desc": "The Stripe API version the payload is formatted to.\n\n"
                    "The webhook endpoint is created with an explicit version, so this is the version this "
                    "application works with rather than the account's default.",
            },
            "livemode": {
                "display_name": "Live Mode",
                "short_desc": "Whether the event came from live data",
                "type": SoftBoolOrNothingType,
                "desc": "`True` when the event describes live data, `False` when it describes test data",
            },
            "data": {
                "display_name": "Data",
                "short_desc": "The object the event is about",
                "type": AutoHashOrNothingType,
                "desc": "The object the event is about, under `object`.\n\n"
                    "For an update, `previous_attributes` holds **only the fields that changed**, which is "
                    "the reliable way to tell what actually happened.",
            },
            "request": {
                "display_name": "Request",
                "short_desc": "The API request that caused the event, if any",
                "type": AutoHashOrNothingType,
                "desc": "The API request that caused the event, if it was caused by one.\n\n"
                    "`idempotency_key` is echoed here, which lets an integration recognize an event caused "
                    "by its own call.",
            },
            "pending_webhooks": {
                "display_name": "Pending Webhooks",
                "short_desc": "How many endpoints have yet to receive this event",
                "type": SoftIntOrNothingType,
                "desc": "How many webhook endpoints have still to receive this event successfully",
            },
        };
    }

    #! Creates the object
    constructor() {
        map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type, NOTHING, NOTHING, NOTHING)
            .setAttributes({"display_name": $1.value.display_name, "short_desc": $1.value.short_desc})),
            Fields.pairIterator();
    }
}

#! The data carried by a Stripe webhook event
public const StripeEventDataType = new StripeEventDataTypeBase();
}
