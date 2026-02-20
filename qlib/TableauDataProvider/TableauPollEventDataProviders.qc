# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file TableauPollEventDataProviders.qc Tableau polling event data providers

/*  TableauPollEventDataProviders.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Watch for new datasources in Tableau
public class TableauWatchDatasourcesDataProvider inherits TableauPollEventDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_WATCH_DATASOURCES,
            "desc": "Watch for new datasources in Tableau",
            "type": "TableauWatchDatasourcesDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(TableauRestClient::TableauRestClient rest, *hash<auto> options)
            : TableauPollEventDataProviderBase(rest, options) {
    }

    #! Returns the provider name
    string getName() {
        return EVENT_WATCH_DATASOURCES;
    }

    #! Returns the provider description
    *string getDesc() {
        return "Watch for new datasources in Tableau";
    }

    #! Fetches new datasources since the given date
    private list<hash<auto>> fetchRecords(date since) {
        return fetchAllPages("datasources", "datasources.datasource",
            {"sort": "createdAt:desc"}, 100);
    }

    #! Returns the event name
    private string getEventName() {
        return EVENT_WATCH_DATASOURCES;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns the data type for datasource event records
    private AbstractDataProviderType getRecordDataType() {
        return new TableauDatasourceDataType();
    }

    #! Returns fake example data for datasources
    private hash<auto> getFakeExampleData() {
        return TableauPollEventDataProviderBase::getFakeExampleData() + {
            "name": "Sample Datasource",
            "contentUrl": "SampleDatasource",
            "type": "excel-direct",
            "isCertified": False,
            "description": "An example Tableau datasource",
            "hasExtracts": False,
            "project": {
                "id": "a1b2c3d4-e5f6-7890-abcd-ef1234567890",
                "name": "Default",
            },
            "owner": {
                "id": "f1e2d3c4-b5a6-7890-abcd-ef1234567890",
                "name": "admin",
            },
        };
    }
}

#! Watch for new projects in Tableau
public class TableauWatchProjectsDataProvider inherits TableauPollEventDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_WATCH_PROJECTS,
            "desc": "Watch for new projects in Tableau",
            "type": "TableauWatchProjectsDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(TableauRestClient::TableauRestClient rest, *hash<auto> options)
            : TableauPollEventDataProviderBase(rest, options) {
    }

    #! Returns the provider name
    string getName() {
        return EVENT_WATCH_PROJECTS;
    }

    #! Returns the provider description
    *string getDesc() {
        return "Watch for new projects in Tableau";
    }

    #! Fetches new projects since the given date
    private list<hash<auto>> fetchRecords(date since) {
        return fetchAllPages("projects", "projects.project", NOTHING, 100);
    }

    #! Returns the event name
    private string getEventName() {
        return EVENT_WATCH_PROJECTS;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns the data type for project event records
    private AbstractDataProviderType getRecordDataType() {
        return new TableauProjectDataType();
    }

    #! Returns fake example data for projects
    private hash<auto> getFakeExampleData() {
        return TableauPollEventDataProviderBase::getFakeExampleData() + {
            "name": "Sample Project",
            "description": "An example Tableau project",
            "contentPermissions": "ManagedByOwner",
            "topLevelProject": True,
            "owner": {
                "id": "f1e2d3c4-b5a6-7890-abcd-ef1234567890",
                "name": "admin",
            },
        };
    }
}

} # namespace TableauDataProvider
