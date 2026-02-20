# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SquarespaceDataProvider module definition

/*  SquarespaceDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SquarespaceDataProvider module
public namespace SquarespaceDataProvider {

#! The main Squarespace data provider class (root provider)
public class SquarespaceDataProvider inherits SquarespaceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "squarespace",
            "desc": "Squarespace data provider for orders, products, inventory, and transactions",
            "type": "SquarespaceDataProvider",
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
            # Collection providers
            "orders": Class::forName("SquarespaceOrdersDataProvider"),
            "products": Class::forName("SquarespaceProductsDataProvider"),
            "product-variants": Class::forName("SquarespaceProductVariantsDataProvider"),
            "inventory": Class::forName("SquarespaceInventoryDataProvider"),
            "transactions": Class::forName("SquarespaceTransactionsDataProvider"),
            # Action providers
            "execute-api-call": Class::forName("SquarespaceExecuteApiCallDataProvider"),
            # Events container
            "events": Class::forName("SquarespaceEventsDataProvider"),
        };
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : SquarespaceDataProviderBase(options) {
    }

    #! Creates the object with a REST client
    constructor(SquarespaceRestClient::SquarespaceRestClient rest) : SquarespaceDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(SquarespaceRestClient::SquarespaceRestConnection conn) : SquarespaceDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "squarespace";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Squarespace data provider for orders, products, inventory, and transactions";
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

} # namespace SquarespaceDataProvider
