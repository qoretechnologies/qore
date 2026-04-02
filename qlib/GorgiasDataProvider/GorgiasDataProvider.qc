# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GorgiasDataProvider module definition

/*  GorgiasDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the GorgiasDataProvider module
public namespace GorgiasDataProvider {

#! The main Gorgias data provider class (root provider)
public class GorgiasDataProvider inherits GorgiasDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "gorgias",
            "desc": "Gorgias data provider for customer support ticket management, "
                "customer data, and helpdesk operations",
            "type": "GorgiasDataProvider",
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
            "tickets": Class::forName("GorgiasTicketsDataProvider"),
            "customers": Class::forName("GorgiasCustomersDataProvider"),
            "tags": Class::forName("GorgiasTagsDataProvider"),
            "messages": Class::forName("GorgiasMessagesDataProvider"),
            "users": Class::forName("GorgiasUsersDataProvider"),
            "events": Class::forName("GorgiasEventsDataProvider"),
        };
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : GorgiasDataProviderBase(options) {
    }

    #! Creates the object with a REST client (async)
    constructor(RestClientIo::RestClientIo rest) : GorgiasDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(GorgiasRestClient::GorgiasRestConnection conn) : GorgiasDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "gorgias";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Gorgias data provider for customer support ticket management, "
            "customer data, and helpdesk operations";
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

} # namespace GorgiasDataProvider
