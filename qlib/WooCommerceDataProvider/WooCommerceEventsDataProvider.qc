# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceEventsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Container provider for WooCommerce event providers (triggers)
public class WooCommerceEventsDataProvider inherits WooCommerceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "events",
            "desc": "WooCommerce events and triggers - watch for order, product, customer, and coupon events "
                "via webhooks",
            "type": "WooCommerceEventsDataProvider",
            "supports_children": True,
            "children_can_support_observers": True,
        };

        #! Constructor options
        const ConstructorOptions = WooCommerceDataProviderBase::BaseConstructorOptions;

        #! Child provider map
        const ChildMap = {
            # Built-in order events
            EVENT_ORDER_CREATED: Class::forName("WooCommerceOrderCreatedEventDataProvider"),
            EVENT_ORDER_UPDATED: Class::forName("WooCommerceOrderUpdatedEventDataProvider"),
            EVENT_ORDER_DELETED: Class::forName("WooCommerceOrderDeletedEventDataProvider"),
            EVENT_ORDER_RESTORED: Class::forName("WooCommerceOrderRestoredEventDataProvider"),

            # Built-in product events
            EVENT_PRODUCT_CREATED: Class::forName("WooCommerceProductCreatedEventDataProvider"),
            EVENT_PRODUCT_UPDATED: Class::forName("WooCommerceProductUpdatedEventDataProvider"),
            EVENT_PRODUCT_DELETED: Class::forName("WooCommerceProductDeletedEventDataProvider"),
            EVENT_PRODUCT_RESTORED: Class::forName("WooCommerceProductRestoredEventDataProvider"),

            # Built-in customer events
            EVENT_CUSTOMER_CREATED: Class::forName("WooCommerceCustomerCreatedEventDataProvider"),
            EVENT_CUSTOMER_UPDATED: Class::forName("WooCommerceCustomerUpdatedEventDataProvider"),
            EVENT_CUSTOMER_DELETED: Class::forName("WooCommerceCustomerDeletedEventDataProvider"),

            # Built-in coupon events
            EVENT_COUPON_CREATED: Class::forName("WooCommerceCouponCreatedEventDataProvider"),
            EVENT_COUPON_UPDATED: Class::forName("WooCommerceCouponUpdatedEventDataProvider"),
            EVENT_COUPON_DELETED: Class::forName("WooCommerceCouponDeletedEventDataProvider"),
            EVENT_COUPON_RESTORED: Class::forName("WooCommerceCouponRestoredEventDataProvider"),

            # Custom action: payment and order status events
            EVENT_PAYMENT_COMPLETE: Class::forName("WooCommercePaymentCompleteEventDataProvider"),
            EVENT_ORDER_STATUS_PENDING: Class::forName("WooCommerceOrderStatusPendingEventDataProvider"),
            EVENT_ORDER_STATUS_PROCESSING: Class::forName("WooCommerceOrderStatusProcessingEventDataProvider"),
            EVENT_ORDER_STATUS_COMPLETED: Class::forName("WooCommerceOrderStatusCompletedEventDataProvider"),
            EVENT_ORDER_STATUS_ON_HOLD: Class::forName("WooCommerceOrderStatusOnHoldEventDataProvider"),
            EVENT_ORDER_STATUS_REFUNDED: Class::forName("WooCommerceOrderStatusRefundedEventDataProvider"),
            EVENT_ORDER_STATUS_FAILED: Class::forName("WooCommerceOrderStatusFailedEventDataProvider"),
            EVENT_ORDER_STATUS_CANCELLED: Class::forName("WooCommerceOrderStatusCancelledEventDataProvider"),

            # Custom action: cart event
            EVENT_ADD_TO_CART: Class::forName("WooCommerceAddToCartEventDataProvider"),

            # Custom action: stock events
            EVENT_LOW_STOCK: Class::forName("WooCommerceLowStockEventDataProvider"),
            EVENT_NO_STOCK: Class::forName("WooCommerceNoStockEventDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(WooCommerceRestClient::WooCommerceRestClient rest) : WooCommerceDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(WooCommerceRestClient::WooCommerceRestConnection conn) : WooCommerceDataProviderBase(conn) {
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : WooCommerceDataProviderBase(options) {
    }

    #! Returns the provider name
    string getName() {
        return "events";
    }

    #! Returns the provider description
    *string getDesc() {
        return "WooCommerce events and triggers - watch for order, product, customer, and coupon events "
            "via webhooks";
    }

    #! Returns child provider names
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns a child provider by name
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) {
            return cls.newObject(rest);
        }
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace WooCommerceDataProvider
