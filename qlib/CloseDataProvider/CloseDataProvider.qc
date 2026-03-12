# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CloseDataProvider module definition

/*  CloseDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the CloseDataProvider module
public namespace CloseDataProvider {

#! The main Close data provider class (root provider)
public class CloseDataProvider inherits CloseDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "close",
            "desc": "Close CRM data provider for leads, contacts, opportunities, tasks, "
                "and activities",
            "type": "CloseDataProvider",
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
            "leads": Class::forName("CloseLeadsDataProvider"),
            "contacts": Class::forName("CloseContactsDataProvider"),
            "opportunities": Class::forName("CloseOpportunitiesDataProvider"),
            "tasks": Class::forName("CloseTasksDataProvider"),
            "calls": Class::forName("CloseCallsDataProvider"),
            "emails": Class::forName("CloseEmailsDataProvider"),
            "notes": Class::forName("CloseNotesDataProvider"),
            "meetings": Class::forName("CloseMeetingsDataProvider"),
            "sms": Class::forName("CloseSmsDataProvider"),
            # Events container
            "events": Class::forName("CloseEventsDataProvider"),
        };
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : CloseDataProviderBase(options) {
    }

    #! Creates the object with a REST client
    constructor(CloseRestClient::CloseRestClient rest) : CloseDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(CloseRestClient::CloseRestConnection conn) : CloseDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "close";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Close CRM data provider for leads, contacts, opportunities, tasks, "
            "and activities";
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

} # namespace CloseDataProvider
