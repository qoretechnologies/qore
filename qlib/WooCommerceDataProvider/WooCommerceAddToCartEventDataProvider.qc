# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceAddToCartEventDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! WooCommerce add-to-cart event data provider
/** This trigger cannot be enriched because the \c arg value is a cart item key (not a product ID).
    The raw payload is passed through as-is.
*/
public class WooCommerceAddToCartEventDataProvider inherits WooCommerceBaseEventDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_ADD_TO_CART,
            "desc": "Fires when a product is added to the cart. "
                "**Note:** The payload only contains `action` and `arg` (cart item key). "
                "The full product data is not available because the `arg` value is a cart item key, not a product ID.",
            "type": "WooCommerceAddToCartEventDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_observable": True,
        };
    }

    private {
        #! Event data type
        static WooCommerceAddToCartEventDataType eventDataType();
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
        return EVENT_ADD_TO_CART;
    }

    #! Returns the WooCommerce webhook topic
    string getWebhookTopic() {
        return "action.woocommerce_add_to_cart";
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
            "action": "woocommerce_add_to_cart",
            "arg": "a1b2c3d4e5f6g7h8i9j0",
        };
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace WooCommerceDataProvider
