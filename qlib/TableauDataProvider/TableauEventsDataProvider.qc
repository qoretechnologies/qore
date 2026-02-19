# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file TableauEventsDataProvider.qc Tableau events container data provider

/*  TableauEventsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Tableau events data provider - parent for all event triggers
public class TableauEventsDataProvider inherits TableauDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "events",
            "desc": "Tableau events/triggers data provider",
            "type": "TableauEventsDataProvider",
            "supports_children": True,
            "children_can_support_observers": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Child provider map for triggers
        const ChildMap = {
            EVENT_DATASOURCE_CREATED: Class::forName("TableauWatchDatasourceCreatedDataProvider"),
            EVENT_DATASOURCE_DELETED: Class::forName("TableauWatchDatasourceDeletedDataProvider"),
            EVENT_DATASOURCE_UPDATED: Class::forName("TableauWatchDatasourceUpdatedDataProvider"),
            EVENT_DATASOURCE_REFRESH_STARTED: Class::forName("TableauWatchDatasourceRefreshStartedDataProvider"),
            EVENT_DATASOURCE_REFRESH_SUCCEEDED: Class::forName("TableauWatchDatasourceRefreshSucceededDataProvider"),
            EVENT_DATASOURCE_REFRESH_FAILED: Class::forName("TableauWatchDatasourceRefreshFailedDataProvider"),
            EVENT_WATCH_DATASOURCES: Class::forName("TableauWatchDatasourcesDataProvider"),
            EVENT_WATCH_PROJECTS: Class::forName("TableauWatchProjectsDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(TableauRestClient::TableauRestClient rest) : TableauDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "events";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Tableau events/triggers data provider";
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
