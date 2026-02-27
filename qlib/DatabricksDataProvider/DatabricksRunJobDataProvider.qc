# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DatabricksRunJobDataProvider class definition

/*  DatabricksRunJobDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request data type for the run-job action
public class DatabricksRunJobRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "job_id": {
                "type": IntType,
                "display_name": "Job ID",
                "short_desc": "The ID of the job to run",
                "desc": "The unique identifier of the job to trigger a run for",
                "required": True,
            },
            "job_parameters": {
                "type": AutoHashOrNothingType,
                "display_name": "Job Parameters",
                "short_desc": "Parameters for the job run",
                "desc": "A map of parameters to pass to the job run",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Response data type for the run-job action
public class DatabricksRunJobResponseDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "run_id": {
                "type": IntType,
                "display_name": "Run ID",
                "short_desc": "The ID of the triggered run",
                "desc": "The unique identifier of the triggered job run",
            },
            "number_in_job": {
                "type": IntOrNothingType,
                "display_name": "Number In Job",
                "short_desc": "The run number within the job",
                "desc": "The sequence number of this run among all runs of the job",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Databricks run job action data provider
public class DatabricksRunJobDataProvider inherits DatabricksDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "run-job",
            "desc": "Trigger a run of an existing Databricks job",
            "type": "DatabricksRunJobDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        const RequestType = new DatabricksRunJobRequestDataType();

        #! Response type
        const ResponseType = new DatabricksRunJobResponseDataType();
    }

    #! Creates the object with a REST client
    constructor(DatabricksRestClient::DatabricksRestClient rest) : DatabricksDataProviderBase(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return "run-job";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Trigger a run of an existing Databricks job";
    }

    #! Makes a request and returns the response
    /** @param req the request body
        @param request_options the request options

        @return the response including `run_id` and `number_in_job`
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("api/2.1/jobs/run-now", req);
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
