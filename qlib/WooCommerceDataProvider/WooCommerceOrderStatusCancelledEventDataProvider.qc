# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceOrderStatusCancelledEventDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! WooCommerce order status cancelled event data provider
public class WooCommerceOrderStatusCancelledEventDataProvider inherits WooCommerceCustomActionEventDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_ORDER_STATUS_CANCELLED,
            "desc": "Fires when an order status changes to `cancelled` in WooCommerce",
            "type": "WooCommerceOrderStatusCancelledEventDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_observable": True,
        };
    }

    private {
        #! Event data type
        static WooCommerceOrderEventDataType eventDataType();
    }

    #! Creates the object with a REST client
    constructor(WooCommerceRestClient::WooCommerceRestClient rest) : WooCommerceCustomActionEventDataProvider(rest) {
    }

    #! Creates the object with a connection
    constructor(WooCommerceRestClient::WooCommerceRestConnection conn) : WooCommerceCustomActionEventDataProvider(conn) {
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : WooCommerceCustomActionEventDataProvider(options) {
    }

    #! Returns the event type
    string getEventType() {
        return EVENT_ORDER_STATUS_CANCELLED;
    }

    #! Returns the WooCommerce webhook topic
    string getWebhookTopic() {
        return "action.woocommerce_order_status_cancelled";
    }

    #! Returns the event data type
    AbstractDataProviderType getEventDataType() {
        return eventDataType;
    }

    #! Returns the API resource path for enrichment
    string getEnrichmentResource() {
        return "orders";
    }

    #! Returns fake example data for this event
    hash<auto> getFakeExampleData() {
        return {
            "id": 1234,
            "status": "processing",
            "currency": "USD",
            "total": "99.99",
            "customer_id": 5,
            "billing": {"first_name": "John", "last_name": "Doe", "email": "john@example.com"},
            "shipping": {"first_name": "John", "last_name": "Doe"},
            "line_items": ({"name": "Example Product", "quantity": 1, "total": "99.99"},),
            "payment_method": "stripe",
            "date_created": "2026-01-15T10:30:00",
        };
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace WooCommerceDataProvider
