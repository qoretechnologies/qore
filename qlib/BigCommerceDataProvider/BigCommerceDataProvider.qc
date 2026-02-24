# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BigCommerceDataProvider.qc defines the root BigCommerceDataProvider class

/*  BigCommerceDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Root data provider for BigCommerce
public class BigCommerceDataProvider inherits BigCommerceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "bigcommerce",
            "desc": "BigCommerce data provider; provides access to BigCommerce e-commerce APIs",
            "type": "BigCommerceDataProvider",
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
            "blog-posts": Class::forName("BigCommerceBlogPostsDataProvider"),
            "customer-addresses": Class::forName("BigCommerceCustomerAddressesDataProvider"),
            "customers": Class::forName("BigCommerceCustomersDataProvider"),
            "events": Class::forName("BigCommerceEventsDataProvider"),
            "execute-api-call": Class::forName("BigCommerceExecuteApiCallDataProvider"),
            "orders": Class::forName("BigCommerceOrdersDataProvider"),
            "products": Class::forName("BigCommerceProductsDataProvider"),
            "shipments": Class::forName("BigCommerceShipmentsDataProvider"),
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
        return "bigcommerce";
    }

    #! Returns the provider description
    *string getDesc() {
        return "BigCommerce data provider; provides access to BigCommerce e-commerce APIs";
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

} # namespace BigCommerceDataProvider
