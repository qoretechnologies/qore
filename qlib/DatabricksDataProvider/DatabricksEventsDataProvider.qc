# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DatabricksEventsDataProvider class definition

/*  DatabricksEventsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DatabricksDataProvider module
public namespace DatabricksDataProvider {

#! Container data provider for Databricks event/trigger providers
public class DatabricksEventsDataProvider inherits DatabricksDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "DatabricksEventsDataProvider",
            "supports_children": True,
            "children_can_support_observers": True,
        };

        #! Child provider map
        const Children = {
            "watch-job-runs": "DatabricksWatchJobRunsDataProvider",
            "watch-cluster-state": "DatabricksWatchClusterStateDataProvider",
            "watch-pipeline-updates": "DatabricksWatchPipelineUpdatesDataProvider",
        };
    }

    #! Creates the object with a REST client
    constructor(DatabricksRestClient::DatabricksRestClient rest)
            : DatabricksDataProviderBase(rest) {
    }

    #! Returns the name of this data provider
    string getName() {
        return "events";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Databricks event/trigger providers for watching job runs, cluster state changes, "
            "and pipeline updates";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns a list of child data provider names
    private *list<string> getChildProviderNamesImpl() {
        return keys Children;
    }

    #! Returns a child data provider by name
    private *DataProvider::AbstractDataProvider getChildProviderImpl(string name) {
        *string class_name = Children{name};
        if (!class_name) {
            return;
        }
        return create_object(class_name, rest);
    }
}

} # namespace DatabricksDataProvider
