# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore FreshsalesDataProvider module definition

/*  FreshsalesDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the FreshsalesDataProvider module
public namespace FreshsalesDataProvider {

#! The main Freshsales data provider class (root provider)
public class FreshsalesDataProvider inherits FreshsalesDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "freshsales",
            "desc": "Freshsales data provider for managing contacts, accounts, deals, tasks, "
                "appointments, and notes in Freshsales CRM",
            "type": "FreshsalesDataProvider",
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
            "contacts": Class::forName("FreshsalesContactsDataProvider"),
            "accounts": Class::forName("FreshsalesAccountsDataProvider"),
            "deals": Class::forName("FreshsalesDealsDataProvider"),
            "tasks": Class::forName("FreshsalesTasksDataProvider"),
            "appointments": Class::forName("FreshsalesAppointmentsDataProvider"),
            "notes": Class::forName("FreshsalesNotesDataProvider"),
            "events": Class::forName("FreshsalesEventsDataProvider"),
        };
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : FreshsalesDataProviderBase(options) {
    }

    #! Creates the object with a REST client (async)
    constructor(RestClientIo::RestClientIo rest) : FreshsalesDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(FreshsalesRestClient::FreshsalesRestConnection conn)
            : FreshsalesDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "freshsales";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Freshsales data provider for managing contacts, accounts, deals, tasks, "
            "appointments, and notes in Freshsales CRM";
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

} # namespace FreshsalesDataProvider
