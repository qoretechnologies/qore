# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DatabricksStartPipelineUpdateDataProvider class definition

/*  DatabricksStartPipelineUpdateDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request data type for the start-pipeline-update action
public class DatabricksStartPipelineUpdateRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "pipeline_id": {
                "type": StringType,
                "display_name": "Pipeline ID",
                "short_desc": "The ID of the pipeline to update",
                "desc": "The unique identifier of the Delta Live Tables pipeline to start an update for",
                "required": True,
            },
            "full_refresh": {
                "type": BoolOrNothingType,
                "display_name": "Full Refresh",
                "short_desc": "Perform a full refresh",
                "desc": "Whether to perform a full refresh of all tables in the pipeline",
            },
            "full_refresh_selection": {
                "type": AutoListOrNothingType,
                "display_name": "Full Refresh Selection",
                "short_desc": "Tables to fully refresh",
                "desc": "A list of specific tables to fully refresh while incrementally "
                    "updating the rest",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Response data type for the start-pipeline-update action
public class DatabricksStartPipelineUpdateResponseDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "update_id": {
                "type": StringType,
                "display_name": "Update ID",
                "short_desc": "The ID of the started update",
                "desc": "The unique identifier of the started pipeline update",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Databricks start pipeline update action data provider
public class DatabricksStartPipelineUpdateDataProvider inherits DatabricksDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "start-pipeline-update",
            "desc": "Start a pipeline update for a Delta Live Tables pipeline in Databricks",
            "type": "DatabricksStartPipelineUpdateDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        const RequestType = new DatabricksStartPipelineUpdateRequestDataType();

        #! Response type
        const ResponseType = new DatabricksStartPipelineUpdateResponseDataType();
    }

    #! Creates the object with a REST client
    constructor(DatabricksRestClient::DatabricksRestClient rest) : DatabricksDataProviderBase(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return "start-pipeline-update";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Start a pipeline update for a Delta Live Tables pipeline in Databricks";
    }

    #! Makes a request and returns the response
    /** @param req the request body
        @param request_options the request options

        @return the response including the `update_id` of the started update
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost(sprintf("api/2.0/pipelines/%s/updates", req.pipeline_id), req - "pipeline_id");
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
