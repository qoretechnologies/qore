# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GoHighLevelDataProvider module definition

/*  GoHighLevelDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the GoHighLevelDataProvider module
public namespace GoHighLevelDataProvider {

#! The main GoHighLevel data provider class (root provider)
public class GoHighLevelDataProvider inherits GoHighLevelDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "gohighlevel",
            "desc": "GoHighLevel data provider for CRM, contacts, opportunities, tasks, "
                "and marketing automation",
            "type": "GoHighLevelDataProvider",
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
            "contacts": Class::forName("GoHighLevelContactsDataProvider"),
            "opportunities": Class::forName("GoHighLevelOpportunitiesDataProvider"),
            "tasks": Class::forName("GoHighLevelTasksDataProvider"),
            "create-account": Class::forName("GoHighLevelCreateAccountDataProvider"),
            "execute-api-call": Class::forName("GoHighLevelExecuteApiCallDataProvider"),
            "events": Class::forName("GoHighLevelEventsDataProvider"),
        };
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : GoHighLevelDataProviderBase(options) {
    }

    #! Creates the object with a REST client
    constructor(GoHighLevelRestClient::GoHighLevelRestClient rest) : GoHighLevelDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(GoHighLevelRestClient::GoHighLevelRestConnection conn) : GoHighLevelDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "gohighlevel";
    }

    #! Returns the provider description
    *string getDesc() {
        return "GoHighLevel data provider for CRM, contacts, opportunities, tasks, "
            "and marketing automation";
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

} # namespace GoHighLevelDataProvider
