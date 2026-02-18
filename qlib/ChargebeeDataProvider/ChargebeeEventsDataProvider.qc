# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ChargebeeDataProvider module definition

/*  ChargebeeEventsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ChargebeeDataProvider module
public namespace ChargebeeDataProvider {

#! Container provider for Chargebee event providers (triggers)
public class ChargebeeEventsDataProvider inherits ChargebeeDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "events",
            "desc": "Chargebee events data provider - watch for billing events via webhooks",
            "type": "ChargebeeEventsDataProvider",
            "supports_children": True,
            "children_can_support_observers": True,
        };

        #! Child provider map
        const ChildMap = {
            EVENT_WATCH_CUSTOMERS: Class::forName("ChargebeeWatchCustomersEventDataProvider"),
            EVENT_WATCH_EVENTS: Class::forName("ChargebeeWatchEventsEventDataProvider"),
            EVENT_WATCH_INVOICES: Class::forName("ChargebeeWatchInvoicesEventDataProvider"),
            EVENT_WATCH_SUBSCRIPTIONS: Class::forName("ChargebeeWatchSubscriptionsEventDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(ChargebeeRestClient::ChargebeeRestClient rest) : ChargebeeDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(ChargebeeRestClient::ChargebeeRestConnection conn) : ChargebeeDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "events";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Chargebee events data provider - watch for billing events via webhooks";
    }

    #! Returns child provider names
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns a child provider by name
    private *AbstractDataProvider getChildProviderImpl(string name) {
        switch (name) {
            case EVENT_WATCH_CUSTOMERS:
                return new ChargebeeWatchCustomersEventDataProvider(rest);
            case EVENT_WATCH_EVENTS:
                return new ChargebeeWatchEventsEventDataProvider(rest);
            case EVENT_WATCH_INVOICES:
                return new ChargebeeWatchInvoicesEventDataProvider(rest);
            case EVENT_WATCH_SUBSCRIPTIONS:
                return new ChargebeeWatchSubscriptionsEventDataProvider(rest);
        }
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ChargebeeDataProvider
