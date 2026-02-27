# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DatabricksDataProvider class definition

/*  DatabricksDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Root data provider for Databricks workspace management and ML operations
public class DatabricksDataProvider inherits DatabricksDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "DatabricksDataProvider",
            "supports_children": True,
            "children_can_support_records": True,
            "children_can_support_apis": True,
            "children_can_support_observers": True,
            "children_can_support_pipeline_processors": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Child provider map
        const ChildMap = {
            # Collection providers (DPAT_FIND)
            "jobs": "DatabricksJobsDataProvider",
            "job-runs": "DatabricksJobRunsDataProvider",
            "clusters": "DatabricksClustersDataProvider",
            "sql-warehouses": "DatabricksSqlWarehousesDataProvider",
            "sql-statements": "DatabricksSqlStatementsDataProvider",
            "dbfs": "DatabricksDbfsDataProvider",
            "workspace": "DatabricksWorkspaceDataProvider",
            "pipelines": "DatabricksPipelinesDataProvider",
            "unity-catalog": "DatabricksUnityCatalogDataProvider",
            "secrets": "DatabricksSecretsDataProvider",
            "identity": "DatabricksIdentityDataProvider",

            # Action providers (DPAT_API)
            "create-job": "DatabricksCreateJobDataProvider",
            "run-job": "DatabricksRunJobDataProvider",
            "cancel-job-run": "DatabricksCancelJobRunDataProvider",
            "delete-job": "DatabricksDeleteJobDataProvider",
            "create-cluster": "DatabricksCreateClusterDataProvider",
            "start-cluster": "DatabricksStartClusterDataProvider",
            "delete-cluster": "DatabricksDeleteClusterDataProvider",
            "create-sql-warehouse": "DatabricksCreateSqlWarehouseDataProvider",
            "start-sql-warehouse": "DatabricksStartSqlWarehouseDataProvider",
            "stop-sql-warehouse": "DatabricksStopSqlWarehouseDataProvider",
            "delete-sql-warehouse": "DatabricksDeleteSqlWarehouseDataProvider",
            "execute-sql": "DatabricksExecuteSqlDataProvider",
            "mkdir-dbfs": "DatabricksMkdirDbfsDataProvider",
            "delete-dbfs": "DatabricksDeleteDbfsDataProvider",
            "import-workspace": "DatabricksImportWorkspaceDataProvider",
            "delete-workspace": "DatabricksDeleteWorkspaceDataProvider",
            "create-pipeline": "DatabricksCreatePipelineDataProvider",
            "update-pipeline": "DatabricksUpdatePipelineDataProvider",
            "delete-pipeline": "DatabricksDeletePipelineDataProvider",
            "start-pipeline-update": "DatabricksStartPipelineUpdateDataProvider",
            "create-secret-scope": "DatabricksCreateSecretScopeDataProvider",
            "delete-secret-scope": "DatabricksDeleteSecretScopeDataProvider",
            "put-secret": "DatabricksPutSecretDataProvider",
            "delete-secret": "DatabricksDeleteSecretDataProvider",

            # ML Pipeline processors
            "processors": "DatabricksProcessorsDataProvider",

            # Triggers
            "events": "DatabricksEventsDataProvider",
        };
    }

    #! Creates the object with a REST client
    constructor(DatabricksRestClient::DatabricksRestClient rest) : DatabricksDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(DatabricksRestClient::DatabricksRestConnection conn)
            : DatabricksDataProviderBase(conn) {
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : DatabricksDataProviderBase(options) {
        checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
    }

    #! Returns the name of this data provider
    string getName() {
        return "databricks";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Root data provider for Databricks workspace management, data engineering, "
            "and ML operations including jobs, clusters, SQL warehouses, pipelines, "
            "Unity Catalog, and model serving";
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns a list of child data provider names
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    private *DataProvider::AbstractDataProvider getChildProviderImpl(string name) {
        *string class_name = ChildMap{name};
        if (!class_name) {
            return;
        }
        return create_object(class_name, rest);
    }
}

} # namespace DatabricksDataProvider
