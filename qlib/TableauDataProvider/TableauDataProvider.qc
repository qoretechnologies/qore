# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file TableauDataProvider.qc Tableau root data provider

/*  TableauDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the TableauDataProvider module
public namespace TableauDataProvider {

#! The main Tableau data provider class (root provider)
public class TableauDataProvider inherits TableauDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "tableau",
            "desc": "Tableau data provider for analytics, datasources, projects, users, "
                "and extract refresh tasks",
            "type": "TableauDataProvider",
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
            "datasources": Class::forName("TableauDatasourcesDataProvider"),
            "projects": Class::forName("TableauProjectsDataProvider"),
            "users": Class::forName("TableauUsersDataProvider"),
            "extract-refresh-tasks": Class::forName("TableauExtractRefreshTasksDataProvider"),
            # Action providers
            "execute-api-call": Class::forName("TableauExecuteApiCallDataProvider"),
            # Events container
            "events": Class::forName("TableauEventsDataProvider"),
        };
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : TableauDataProviderBase(options) {
    }

    #! Creates the object with a REST client
    constructor(TableauRestClient::TableauRestClient rest) : TableauDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(TableauRestClient::TableauRestConnection conn) : TableauDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "tableau";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Tableau data provider for analytics, datasources, projects, users, "
            "and extract refresh tasks";
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

} # namespace TableauDataProvider
