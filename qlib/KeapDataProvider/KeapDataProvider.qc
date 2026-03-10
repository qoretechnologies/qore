# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore KeapDataProvider module definition

/*  KeapDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the KeapDataProvider module
public namespace KeapDataProvider {

#! The main Keap data provider class (root provider)
public class KeapDataProvider inherits KeapDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "keap",
            "desc": "Keap data provider for contacts, companies, opportunities, orders, "
                "tags, tasks, notes, emails, and campaigns",
            "type": "KeapDataProvider",
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
            "contacts": Class::forName("KeapContactsDataProvider"),
            "companies": Class::forName("KeapCompaniesDataProvider"),
            "opportunities": Class::forName("KeapOpportunitiesDataProvider"),
            "orders": Class::forName("KeapOrdersDataProvider"),
            "tags": Class::forName("KeapTagsDataProvider"),
            "notes": Class::forName("KeapNotesDataProvider"),
            "tasks": Class::forName("KeapTasksDataProvider"),
            "emails": Class::forName("KeapEmailsDataProvider"),
            "files": Class::forName("KeapFilesDataProvider"),
            "products": Class::forName("KeapProductsDataProvider"),
            "campaigns": Class::forName("KeapCampaignsDataProvider"),
            "affiliates": Class::forName("KeapAffiliatesDataProvider"),
            # Action providers
            "execute-api-call": Class::forName("KeapExecuteApiCallDataProvider"),
            # Events container
            "events": Class::forName("KeapEventsDataProvider"),
        };
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : KeapDataProviderBase(options) {
    }

    #! Creates the object with a REST client
    constructor(KeapRestClient::KeapRestClient rest) : KeapDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(KeapRestClient::KeapRestConnection conn) : KeapDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "keap";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Keap data provider for contacts, companies, opportunities, orders, "
            "tags, tasks, notes, emails, and campaigns";
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

} # namespace KeapDataProvider
