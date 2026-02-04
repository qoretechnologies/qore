# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShippoDataProvider module definition

/*  ShippoEventsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Shippo events data provider - parent for all polling triggers
public class ShippoEventsDataProvider inherits ShippoDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "events",
            "desc": "Shippo events/triggers data provider",
            "type": "ShippoEventsDataProvider",
            "supports_children": True,
            "children_can_support_observers": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Child provider map for triggers
        const ChildMap = {
            "new-parcel": Class::forName("ShippoNewParcelWatchDataProvider"),
            "new-shipment": Class::forName("ShippoNewShipmentWatchDataProvider"),
            "new-label": Class::forName("ShippoNewLabelWatchDataProvider"),
            "new-order": Class::forName("ShippoNewOrderWatchDataProvider"),
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
        return "events";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Shippo events/triggers data provider for polling-based event detection";
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

} # namespace ShippoDataProvider
