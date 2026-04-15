# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file UnbounceDataProvider.qc Unbounce root data provider

/*  UnbounceDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the UnbounceDataProvider module
public namespace UnbounceDataProvider {

#! The main Unbounce data provider class (root provider)
public class UnbounceDataProvider inherits UnbounceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "unbounce",
            "desc": "Unbounce data provider for landing pages, leads, accounts, sub-accounts, "
                "domains, page groups, users, and events",
            "type": "UnbounceDataProvider",
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
            "accounts": Class::forName("UnbounceAccountsDataProvider"),
            "sub-accounts": Class::forName("UnbounceSubAccountsDataProvider"),
            "domains": Class::forName("UnbounceDomainsDataProvider"),
            "page-groups": Class::forName("UnbouncePageGroupsDataProvider"),
            "pages": Class::forName("UnbouncePagesDataProvider"),
            "leads": Class::forName("UnbounceLeadsDataProvider"),
            "lead-deletion-requests": Class::forName("UnbounceLeadDeletionRequestsDataProvider"),
            "users": Class::forName("UnbounceUsersDataProvider"),
            "events": Class::forName("UnbounceEventsDataProvider"),
        };
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : UnbounceDataProviderBase(options) {
    }

    #! Creates the object with a REST client
    constructor(*RestClientIo::RestClientIo rest) : UnbounceDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "unbounce";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Unbounce data provider for landing pages, leads, accounts, sub-accounts, "
            "domains, page groups, users, and events";
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

} # namespace UnbounceDataProvider
