# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WaveDataProvider module definition

/*  WaveWebhookEventDataTypes.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the WaveDataProvider module
public namespace WaveDataProvider {

#! Wave webhook event type constant
public const WaveEventTypeInvoice = "invoice";

#! Wave webhook action constant
public const WaveActionCreated = "invoice.created";

#! Wave webhook event base data type
public class WaveWebhookEventBaseDataType inherits HashDataType {
    private {
        const Fields = {
            "business_id": {
                "type": StringOrNothingType,
                "display_name": "Business ID",
                "short_desc": "The business that triggered the event",
                "desc": "The ID of the business where the event occurred",
                "example_value": "QnVzaW5lc3M6YWJjZGVm",
            },
            "event": {
                "type": StringType,
                "display_name": "Event",
                "short_desc": "The event type",
                "desc": "The type of event (e.g., invoice.created)",
                "example_value": "invoice.created",
            },
            "triggered_at": {
                "type": DateOrNothingType,
                "display_name": "Triggered At",
                "short_desc": "When the event was triggered",
                "desc": "The timestamp when the event was triggered",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Invoice webhook event data type
public class WaveInvoiceWebhookEventDataType inherits WaveWebhookEventBaseDataType {
    constructor() : WaveWebhookEventBaseDataType() {
        addField(new QoreDataField({
            "name": "data",
            "display_name": "Invoice Data",
            "short_desc": "The invoice data",
            "desc": "The complete invoice data from the webhook event",
            "type": new WaveInvoiceDataType(),
        }));
    }
}

} # namespace WaveDataProvider
