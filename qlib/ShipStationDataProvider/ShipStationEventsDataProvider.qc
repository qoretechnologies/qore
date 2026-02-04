# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShipStationDataProvider module definition

/*  ShipStationEventsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ShipStationDataProvider module
public namespace ShipStationDataProvider {

#! Container provider for ShipStation event providers (triggers)
public class ShipStationEventsDataProvider inherits ShipStationDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "events",
            "desc": "ShipStation events data provider - watch for order and shipment events via webhooks",
            "type": "ShipStationEventsDataProvider",
            "supports_children": True,
            "children_can_support_observers": True,
        };

        #! Child provider map
        const ChildMap = {
            EVENT_ORDER_NOTIFY: Class::forName("ShipStationOrderNotifyEventDataProvider"),
            EVENT_ITEM_ORDER_NOTIFY: Class::forName("ShipStationItemOrderNotifyEventDataProvider"),
            EVENT_SHIP_NOTIFY: Class::forName("ShipStationShipNotifyEventDataProvider"),
            EVENT_ITEM_SHIP_NOTIFY: Class::forName("ShipStationItemShipNotifyEventDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(ShipStationRestClient::ShipStationRestClient rest)
            : ShipStationDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(ShipStationRestClient::ShipStationRestConnection conn) : ShipStationDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "events";
    }

    #! Returns the provider description
    *string getDesc() {
        return "ShipStation events data provider - watch for order and shipment events via webhooks";
    }

    #! Returns child provider names
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns a child provider by name
    private *AbstractDataProvider getChildProviderImpl(string name) {
        switch (name) {
            case EVENT_ORDER_NOTIFY:
                return new ShipStationOrderNotifyEventDataProvider(rest);
            case EVENT_ITEM_ORDER_NOTIFY:
                return new ShipStationItemOrderNotifyEventDataProvider(rest);
            case EVENT_SHIP_NOTIFY:
                return new ShipStationShipNotifyEventDataProvider(rest);
            case EVENT_ITEM_SHIP_NOTIFY:
                return new ShipStationItemShipNotifyEventDataProvider(rest);
        }
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ShipStationDataProvider
