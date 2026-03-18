# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceWebhookDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the WooCommerceDataProvider module
public namespace WooCommerceDataProvider {

#! WooCommerce webhook data type
public class WooCommerceWebhookDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "id": {
                "display_name": "ID",
                "short_desc": "Unique identifier for the webhook",
                "type": IntOrNothingType,
                "desc": "The unique identifier for the webhook",
            },
            "name": {
                "display_name": "Name",
                "short_desc": "Webhook name",
                "type": StringOrNothingType,
                "desc": "The friendly name for the webhook",
            },
            "status": {
                "display_name": "Status",
                "short_desc": "Webhook status",
                "type": StringOrNothingType,
                "desc": "Webhook status: `active`, `paused`, `disabled`",
            },
            "topic": {
                "display_name": "Topic",
                "short_desc": "Webhook topic",
                "type": StringOrNothingType,
                "desc": "The webhook topic (e.g., `order.created`, `product.updated`)",
            },
            "resource": {
                "display_name": "Resource",
                "short_desc": "Webhook resource type",
                "type": StringOrNothingType,
                "desc": "The resource type for the webhook (e.g., `order`, `product`)",
            },
            "event": {
                "display_name": "Event",
                "short_desc": "Webhook event type",
                "type": StringOrNothingType,
                "desc": "The event type for the webhook (e.g., `created`, `updated`, `deleted`)",
            },
            "delivery_url": {
                "display_name": "Delivery URL",
                "short_desc": "Webhook delivery URL",
                "type": StringOrNothingType,
                "desc": "The URL where the webhook payload is delivered",
            },
            "secret": {
                "display_name": "Secret",
                "short_desc": "Webhook secret",
                "type": StringOrNothingType,
                "desc": "The secret used to generate the webhook delivery signature",
            },
            "date_created": {
                "display_name": "Date Created",
                "short_desc": "Date the webhook was created",
                "type": DateOrNothingType,
                "desc": "The date and time the webhook was created",
            },
            "date_modified": {
                "display_name": "Date Modified",
                "short_desc": "Date the webhook was last modified",
                "type": DateOrNothingType,
                "desc": "The date and time the webhook was last modified",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace WooCommerceDataProvider
