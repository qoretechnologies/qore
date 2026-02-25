# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BigCommerceEventsDataProvider.qc container for BigCommerce event/trigger data providers

/*  BigCommerceEventsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the BigCommerceDataProvider module
public namespace BigCommerceDataProvider {

#! Container for BigCommerce event/trigger data providers
public class BigCommerceEventsDataProvider inherits BigCommerceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "events",
            "desc": "BigCommerce events container - polling-based triggers for BigCommerce changes",
            "type": "BigCommerceEventsDataProvider",
            "supports_children": True,
            "children_can_support_observers": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Child event map
        const ChildMap = {
            EVENT_CART_ABANDONED:
                Class::forName("BigCommerceDataProvider::BigCommerceWatchAbandonedCartDataProvider"),
            EVENT_CART_CREATED:
                Class::forName("BigCommerceDataProvider::BigCommerceWatchNewCartDataProvider"),
            EVENT_CUSTOMER_CREATED:
                Class::forName("BigCommerceDataProvider::BigCommerceWatchNewCustomerDataProvider"),
            EVENT_CUSTOMER_ADDRESS_CREATED:
                Class::forName("BigCommerceDataProvider::BigCommerceWatchNewCustomerAddressDataProvider"),
            EVENT_CUSTOMER_ADDRESS_UPDATED:
                Class::forName("BigCommerceDataProvider::BigCommerceWatchUpdatedCustomerAddressDataProvider"),
            EVENT_CUSTOMER_UPDATED:
                Class::forName("BigCommerceDataProvider::BigCommerceWatchUpdatedCustomerDataProvider"),
            EVENT_ORDER_CREATED:
                Class::forName("BigCommerceDataProvider::BigCommerceWatchNewOrderDataProvider"),
            EVENT_ORDER_STATUS_UPDATED:
                Class::forName("BigCommerceDataProvider::BigCommerceWatchUpdatedOrderStatusDataProvider"),
            EVENT_ORDER_UPDATED:
                Class::forName("BigCommerceDataProvider::BigCommerceWatchUpdatedOrderDataProvider"),
            EVENT_PRODUCT_CREATED:
                Class::forName("BigCommerceDataProvider::BigCommerceWatchNewProductDataProvider"),
            EVENT_SHIPMENT_CREATED:
                Class::forName("BigCommerceDataProvider::BigCommerceWatchNewShipmentDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(BigCommerceRestClient::BigCommerceRestClient rest)
            : BigCommerceDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(BigCommerceRestClient::BigCommerceRestConnection conn)
            : BigCommerceDataProviderBase(conn) {
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : BigCommerceDataProviderBase(options) {
    }

    #! Returns the provider name
    string getName() {
        return "events";
    }

    #! Returns the provider description
    *string getDesc() {
        return "BigCommerce events container - polling-based triggers for BigCommerce changes";
    }

    #! Returns a list of child data provider names
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns a child data provider
    private *DataProvider::AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (!cls) {
            return;
        }
        return cls.newObject(rest);
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BigCommerceDataProvider
