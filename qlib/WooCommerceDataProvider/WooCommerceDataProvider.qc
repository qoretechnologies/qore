# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! The main WooCommerce data provider class (root provider)
public class WooCommerceDataProvider inherits WooCommerceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "woocommerce",
            "desc": "WooCommerce data provider for e-commerce management",
            "type": "WooCommerceDataProvider",
            "supports_children": True,
            "children_can_support_apis": True,
            "children_can_support_records": True,
            "children_can_support_observers": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Child provider map
        const ChildMap = {
            # Collection/List providers
            "orders": Class::forName("WooCommerceOrdersDataProvider"),
            "products": Class::forName("WooCommerceProductsDataProvider"),
            "customers": Class::forName("WooCommerceCustomersDataProvider"),
            "coupons": Class::forName("WooCommerceCouponsDataProvider"),
            "webhooks": Class::forName("WooCommerceWebhooksDataProvider"),
            "categories": Class::forName("WooCommerceCategoriesDataProvider"),
            "tags": Class::forName("WooCommerceTagsDataProvider"),
            "tax-rates": Class::forName("WooCommerceTaxRatesDataProvider"),

            # Action providers - Orders
            "create-order": Class::forName("WooCommerceCreateOrderDataProvider"),
            "update-order": Class::forName("WooCommerceUpdateOrderDataProvider"),
            "delete-order": Class::forName("WooCommerceDeleteOrderDataProvider"),
            "get-order": Class::forName("WooCommerceGetOrderDataProvider"),
            "update-order-status": Class::forName("WooCommerceUpdateOrderStatusDataProvider"),
            "create-order-note": Class::forName("WooCommerceCreateOrderNoteDataProvider"),
            "list-order-notes": Class::forName("WooCommerceListOrderNotesDataProvider"),
            "delete-order-note": Class::forName("WooCommerceDeleteOrderNoteDataProvider"),
            "create-refund": Class::forName("WooCommerceCreateRefundDataProvider"),
            "list-refunds": Class::forName("WooCommerceListRefundsDataProvider"),

            # Action providers - Products
            "create-product": Class::forName("WooCommerceCreateProductDataProvider"),
            "update-product": Class::forName("WooCommerceUpdateProductDataProvider"),
            "delete-product": Class::forName("WooCommerceDeleteProductDataProvider"),
            "get-product": Class::forName("WooCommerceGetProductDataProvider"),
            "update-stock-quantity": Class::forName("WooCommerceUpdateStockQuantityDataProvider"),
            "create-variation": Class::forName("WooCommerceCreateVariationDataProvider"),
            "update-variation": Class::forName("WooCommerceUpdateVariationDataProvider"),
            "list-variations": Class::forName("WooCommerceListVariationsDataProvider"),
            "create-category": Class::forName("WooCommerceCreateCategoryDataProvider"),
            "update-category": Class::forName("WooCommerceUpdateCategoryDataProvider"),
            "create-tag": Class::forName("WooCommerceCreateTagDataProvider"),

            # Action providers - Customers
            "create-customer": Class::forName("WooCommerceCreateCustomerDataProvider"),
            "update-customer": Class::forName("WooCommerceUpdateCustomerDataProvider"),
            "delete-customer": Class::forName("WooCommerceDeleteCustomerDataProvider"),
            "get-customer": Class::forName("WooCommerceGetCustomerDataProvider"),

            # Action providers - Coupons
            "create-coupon": Class::forName("WooCommerceCreateCouponDataProvider"),
            "update-coupon": Class::forName("WooCommerceUpdateCouponDataProvider"),
            "delete-coupon": Class::forName("WooCommerceDeleteCouponDataProvider"),
            "get-coupon": Class::forName("WooCommerceGetCouponDataProvider"),

            # Action providers - Webhooks
            "create-webhook": Class::forName("WooCommerceCreateWebhookDataProvider"),
            "update-webhook": Class::forName("WooCommerceUpdateWebhookDataProvider"),
            "delete-webhook": Class::forName("WooCommerceDeleteWebhookDataProvider"),

            # Action providers - Reports
            "sales-report": Class::forName("WooCommerceSalesReportDataProvider"),
            "top-sellers-report": Class::forName("WooCommerceTopSellersReportDataProvider"),
            "orders-totals": Class::forName("WooCommerceOrdersTotalsDataProvider"),
            "products-totals": Class::forName("WooCommerceProductsTotalsDataProvider"),
            "customers-totals": Class::forName("WooCommerceCustomersTotalsDataProvider"),

            # Action providers - Shipping/Tax/Settings/System
            "list-shipping-zones": Class::forName("WooCommerceListShippingZonesDataProvider"),
            "list-shipping-methods": Class::forName("WooCommerceListShippingMethodsDataProvider"),
            "create-tax-rate": Class::forName("WooCommerceCreateTaxRateDataProvider"),
            "get-setting": Class::forName("WooCommerceGetSettingDataProvider"),
            "update-setting": Class::forName("WooCommerceUpdateSettingDataProvider"),
            "system-status": Class::forName("WooCommerceSystemStatusDataProvider"),

            # Event providers
            "events": Class::forName("WooCommerceEventsDataProvider"),
        };
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : WooCommerceDataProviderBase(options) {
    }

    #! Creates the object with a REST client
    constructor(WooCommerceRestClient::WooCommerceRestClient rest)
            : WooCommerceDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(WooCommerceRestClient::WooCommerceRestConnection conn) : WooCommerceDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "woocommerce";
    }

    #! Returns the provider description
    *string getDesc() {
        return "WooCommerce data provider for e-commerce management";
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
