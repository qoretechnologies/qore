# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DatabricksCreateJobDataProvider class definition

/*  DatabricksCreateJobDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request data type for the create-job action
public class DatabricksCreateJobRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "name": {
                "type": StringType,
                "display_name": "Job Name",
                "short_desc": "The name of the job",
                "desc": "The name of the job to create",
                "required": True,
            },
            "tasks": {
                "type": AutoListType,
                "display_name": "Tasks",
                "short_desc": "List of task specifications",
                "desc": "A list of task specifications for the job to execute",
                "required": True,
            },
            "schedule": {
                "type": AutoHashOrNothingType,
                "display_name": "Schedule",
                "short_desc": "Cron schedule for the job",
                "desc": "An optional periodic schedule for the job using cron syntax",
            },
            "max_concurrent_runs": {
                "type": IntOrNothingType,
                "display_name": "Max Concurrent Runs",
                "short_desc": "Maximum concurrent job runs",
                "desc": "An optional maximum number of concurrent runs for the job",
            },
            "timeout_seconds": {
                "type": IntOrNothingType,
                "display_name": "Timeout Seconds",
                "short_desc": "Job timeout in seconds",
                "desc": "An optional timeout in seconds applied to each run of the job",
            },
            "tags": {
                "type": AutoHashOrNothingType,
                "display_name": "Tags",
                "short_desc": "Custom tags for the job",
                "desc": "A map of tags associated with the job for filtering and organization",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Response data type for the create-job action
public class DatabricksCreateJobResponseDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "job_id": {
                "type": IntType,
                "display_name": "Job ID",
                "short_desc": "The ID of the created job",
                "desc": "The unique identifier of the newly created job",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Databricks create job action data provider
public class DatabricksCreateJobDataProvider inherits DatabricksDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-job",
            "desc": "Create a new job in the Databricks workspace",
            "type": "DatabricksCreateJobDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        const RequestType = new DatabricksCreateJobRequestDataType();

        #! Response type
        const ResponseType = new DatabricksCreateJobResponseDataType();
    }

    #! Creates the object with a REST client
    constructor(DatabricksRestClient::DatabricksRestClient rest) : DatabricksDataProviderBase(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return "create-job";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Create a new job in the Databricks workspace";
    }

    #! Makes a request and returns the response
    /** @param req the request body
        @param request_options the request options

        @return the response including the `job_id` of the created job
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("api/2.1/jobs/create", req);
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
