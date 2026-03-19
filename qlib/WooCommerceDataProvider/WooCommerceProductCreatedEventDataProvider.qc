# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceProductCreatedEventDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! WooCommerce product created event data provider
public class WooCommerceProductCreatedEventDataProvider inherits WooCommerceBaseEventDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_PRODUCT_CREATED,
            "desc": "Fires when a new product is created in WooCommerce",
            "type": "WooCommerceProductCreatedEventDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_observable": True,
        };
    }

    private {
        #! Event data type
        static WooCommerceProductEventDataType eventDataType();
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
        return EVENT_PRODUCT_CREATED;
    }

    #! Returns the WooCommerce webhook topic
    string getWebhookTopic() {
        return "product.created";
    }

    #! Returns the event data type
    AbstractDataProviderType getEventDataType() {
        return eventDataType;
    }

    #! Returns the API resource path
    string getResourcePath() {
        return "products";
    }

    #! Returns fake example data for this event
    hash<auto> getFakeExampleData() {
        return {
            "id": 456,
            "name": "Example Product",
            "slug": "example-product",
            "type": "simple",
            "status": "publish",
            "sku": "EX-001",
            "regular_price": "29.99",
            "sale_price": "",
            "stock_quantity": 50,
            "stock_status": "instock",
            "date_created": "2026-01-10T08:00:00",
        };
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace WooCommerceDataProvider
