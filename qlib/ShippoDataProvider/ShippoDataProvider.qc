# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShippoDataProvider module definition

/*  ShippoDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ShippoDataProvider module
public namespace ShippoDataProvider {

#! Root data provider for Shippo
/** This is the main entry point for the Shippo data provider, providing access to
    shipping, labels, orders, and other Shippo functionality.
*/
public class ShippoDataProvider inherits ShippoDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "shippo",
            "desc": "Shippo data provider for shipping, labels, and order management",
            "type": "ShippoDataProvider",
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
            # Collection/Search providers
            "shipments": Class::forName("ShippoShipmentsDataProvider"),
            "transactions": Class::forName("ShippoTransactionsDataProvider"),
            "orders": Class::forName("ShippoOrdersDataProvider"),

            # Action providers
            "create-shipment": Class::forName("ShippoCreateShipmentDataProvider"),
            "create-order": Class::forName("ShippoCreateOrderDataProvider"),
            "create-label-by-rate": Class::forName("ShippoCreateLabelByRateDataProvider"),
            "get-shipment": Class::forName("ShippoGetShipmentDataProvider"),
            "get-transaction": Class::forName("ShippoGetTransactionDataProvider"),
            "get-order": Class::forName("ShippoGetOrderDataProvider"),
            "validate-address": Class::forName("ShippoValidateAddressDataProvider"),
            # Events container
            "events": Class::forName("ShippoEventsDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(ShippoRestClient::ShippoRestClient rest) : ShippoDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(ShippoRestClient::ShippoRestConnection conn) : ShippoDataProviderBase(conn) {
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : ShippoDataProviderBase(options) {
    }

    #! Returns the provider name
    string getName() {
        return "shippo";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Shippo data provider for shipping, labels, and order management";
    }

    #! Return data provider summary info
    *list<hash<DataProvider::DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map cast<hash<DataProviderSummaryInfo>>({
            "name": $1,
            "desc": ChildMap{$1}.getConstant("ProviderInfo").getValue().desc,
            "type": ChildMap{$1}.getConstant("ProviderInfo").getValue().type,
            "supports_children": ChildMap{$1}.getConstant("ProviderInfo").getValue().supports_children ?? False,
            "children_can_support_apis": ChildMap{$1}.getConstant("ProviderInfo").getValue()
                .children_can_support_apis ?? False,
            "children_can_support_records": ChildMap{$1}.getConstant("ProviderInfo").getValue()
                .children_can_support_records ?? False,
            "children_can_support_observers": ChildMap{$1}.getConstant("ProviderInfo").getValue()
                .children_can_support_observers ?? False,
        }), keys ChildMap;
    }

    #! Returns a list of child data provider names
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
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

} # namespace ShippoDataProvider
