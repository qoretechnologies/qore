# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DatabricksJobsDataProvider class definition

/*  DatabricksJobsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Databricks jobs collection data provider
public class DatabricksJobsDataProvider inherits DatabricksDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "jobs",
            "desc": "Databricks jobs data provider; list and search jobs",
            "type": "DatabricksJobsDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "has_record": True,
            "search_options": SearchOptions,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Search options
        const SearchOptions = {
            "job_id": <DataProviderOptionInfo>{
                "display_name": "Job ID",
                "short_desc": "Get a specific job by ID",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Filter by job ID for single record lookup",
            },
            "name": <DataProviderOptionInfo>{
                "display_name": "Name",
                "short_desc": "Filter by job name",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter jobs by name (partial match)",
            },
            "expand_tasks": <DataProviderOptionInfo>{
                "display_name": "Expand Tasks",
                "short_desc": "Include task details in response",
                "type": AbstractDataProviderType::get(BoolOrNothingType),
                "desc": "Whether to include task and cluster details in each job",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum total results to return",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Maximum total number of results to return (default 25)",
                "default_value": 25,
            },
        };
    }

    private {
        #! Record type
        static DatabricksJobDataType recordType();
    }

    #! Creates the object with a REST client
    constructor(DatabricksRestClient::DatabricksRestClient rest)
            : DatabricksDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "jobs";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Databricks jobs data provider; list and search jobs";
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    #! Searches for jobs
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If job_id provided, get single record
        if (search_options.job_id) {
            hash<auto> result = doGet("api/2.1/jobs/get", {"job_id": search_options.job_id});
            return new DefaultRecordIterator(new ListHashIterator((result,)),
                where_cond, search_options, recordType.getFields());
        }

        # Build query parameters from search options
        hash<auto> params;
        if (search_options.name) {
            params.name = search_options.name;
        }
        if (search_options.expand_tasks) {
            params.expand_tasks = search_options.expand_tasks;
        }
        *int max_records;
        if (search_options.limit) {
            params.limit = search_options.limit;
            max_records = search_options.limit;
        }

        list<auto> records = paginateGet("api/2.1/jobs/list", "jobs", params, max_records);
        return new DefaultRecordIterator(new ListHashIterator(records),
            where_cond, search_options, recordType.getFields());
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace DatabricksDataProvider
