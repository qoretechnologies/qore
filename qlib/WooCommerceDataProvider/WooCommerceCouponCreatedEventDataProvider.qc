# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceCouponCreatedEventDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! WooCommerce coupon created event data provider
public class WooCommerceCouponCreatedEventDataProvider inherits WooCommerceBaseEventDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_COUPON_CREATED,
            "desc": "Fires when a new coupon is created in WooCommerce",
            "type": "WooCommerceCouponCreatedEventDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_observable": True,
        };
    }

    private {
        #! Event data type
        static WooCommerceCouponEventDataType eventDataType();
    }

    #! Creates the object with a REST client
    constructor(WooCommerceRestClient::WooCommerceRestClient rest) : WooCommerceBaseEventDataProvider(rest) {
    }

    #! Creates the object with a connection
    constructor(WooCommerceRestClient::WooCommerceRestConnection conn) : WooCommerceBaseEventDataProvider(conn) {
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : WooCommerceBaseEventDataProvider(options) {
    }

    #! Returns the event type
    string getEventType() {
        return EVENT_COUPON_CREATED;
    }

    #! Returns the WooCommerce webhook topic
    string getWebhookTopic() {
        return "coupon.created";
    }

    #! Returns the event data type
    AbstractDataProviderType getEventDataType() {
        return eventDataType;
    }

    #! Returns the API resource path
    string getResourcePath() {
        return "coupons";
    }

    #! Returns fake example data for this event
    hash<auto> getFakeExampleData() {
        return {
            "id": 101,
            "code": "SUMMER2026",
            "amount": "15.00",
            "discount_type": "percent",
            "usage_count": 42,
            "date_created": "2026-03-01T09:00:00",
        };
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace WooCommerceDataProvider
