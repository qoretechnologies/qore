# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SquareDataProvider module definition

/*  SquareDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SquareDataProvider module
public namespace SquareDataProvider {

#! The main Square data provider class (root provider)
public class SquareDataProvider inherits SquareDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "square",
            "desc": "Square data provider for payments, orders, invoices, customers, "
                "inventory, and bookings",
            "type": "SquareDataProvider",
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
            "customers": Class::forName("SquareCustomersDataProvider"),
            "orders": Class::forName("SquareOrdersDataProvider"),
            "payments": Class::forName("SquarePaymentsDataProvider"),
            "invoices": Class::forName("SquareInvoicesDataProvider"),
            "bookings": Class::forName("SquareBookingsDataProvider"),
            "catalog": Class::forName("SquareCatalogDataProvider"),
            "services": Class::forName("SquareServicesDataProvider"),
            "refunds": Class::forName("SquareRefundsDataProvider"),
            "inventory": Class::forName("SquareInventoryDataProvider"),
            # Action providers
            "execute-api-call": Class::forName("SquareExecuteApiCallDataProvider"),
            # Events container
            "events": Class::forName("SquareEventsDataProvider"),
        };
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : SquareDataProviderBase(options) {
    }

    #! Creates the object with a REST client
    constructor(SquareRestClient::SquareRestClient rest) : SquareDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(SquareRestClient::SquareRestConnection conn) : SquareDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "square";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Square data provider for payments, orders, invoices, customers, "
            "inventory, and bookings";
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

} # namespace SquareDataProvider
