# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DatabricksUpdatePipelineDataProvider class definition

/*  DatabricksUpdatePipelineDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request data type for the update-pipeline action
public class DatabricksUpdatePipelineRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "pipeline_id": {
                "type": StringType,
                "display_name": "Pipeline ID",
                "short_desc": "The ID of the pipeline to update",
                "desc": "The unique identifier of the Delta Live Tables pipeline to update",
                "required": True,
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Pipeline Name",
                "short_desc": "New name for the pipeline",
                "desc": "A new human-readable name for the pipeline",
            },
            "libraries": {
                "type": AutoListOrNothingType,
                "display_name": "Libraries",
                "short_desc": "Updated list of notebook libraries",
                "desc": "An updated list of library specifications (notebooks) for the pipeline",
            },
            "clusters": {
                "type": AutoListOrNothingType,
                "display_name": "Clusters",
                "short_desc": "Updated cluster configurations",
                "desc": "An updated list of cluster configuration objects for the pipeline",
            },
            "continuous": {
                "type": BoolOrNothingType,
                "display_name": "Continuous",
                "short_desc": "Enable or disable continuous mode",
                "desc": "Whether to run the pipeline in continuous mode",
            },
            "catalog": {
                "type": StringOrNothingType,
                "display_name": "Catalog",
                "short_desc": "Unity Catalog name",
                "desc": "The name of the Unity Catalog to publish pipeline data to",
            },
            "target": {
                "type": StringOrNothingType,
                "display_name": "Target",
                "short_desc": "Target schema or database",
                "desc": "The target schema or database to publish pipeline data to",
            },
            "storage": {
                "type": StringOrNothingType,
                "display_name": "Storage",
                "short_desc": "DBFS storage location for output data",
                "desc": "The DBFS root storage location for pipeline output data and metadata",
            },
            "configuration": {
                "type": AutoHashOrNothingType,
                "display_name": "Configuration",
                "short_desc": "Key-value pipeline configuration",
                "desc": "A map of key-value pairs for pipeline-specific configuration settings",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Databricks update pipeline action data provider
public class DatabricksUpdatePipelineDataProvider inherits DatabricksDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "update-pipeline",
            "desc": "Update an existing Delta Live Tables pipeline in Databricks",
            "type": "DatabricksUpdatePipelineDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        const RequestType = new DatabricksUpdatePipelineRequestDataType();

        #! Response type
        const ResponseType = AbstractDataProviderType::get(AutoHashType);
    }

    #! Creates the object with a REST client
    constructor(DatabricksRestClient::DatabricksRestClient rest) : DatabricksDataProviderBase(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return "update-pipeline";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Update an existing Delta Live Tables pipeline in Databricks";
    }

    #! Makes a request and returns the response
    /** @param req the request body
        @param request_options the request options

        @return an empty hash on success
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        doPut(sprintf("api/2.0/pipelines/%s", req.pipeline_id), req - "pipeline_id");
        return {};
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace DatabricksDataProvider
