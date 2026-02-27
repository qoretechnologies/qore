# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DatabricksJobRunsDataProvider class definition

/*  DatabricksJobRunsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Databricks job runs collection data provider
public class DatabricksJobRunsDataProvider inherits DatabricksDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "job-runs",
            "desc": "Databricks job runs data provider; list and search job runs",
            "type": "DatabricksJobRunsDataProvider",
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
            "run_id": <DataProviderOptionInfo>{
                "display_name": "Run ID",
                "short_desc": "Get a specific run by ID",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Filter by run ID for single record lookup",
            },
            "job_id": <DataProviderOptionInfo>{
                "display_name": "Job ID",
                "short_desc": "Filter by parent job ID",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Filter runs by the parent job ID",
            },
            "active_only": <DataProviderOptionInfo>{
                "display_name": "Active Only",
                "short_desc": "Only return active runs",
                "type": AbstractDataProviderType::get(BoolOrNothingType),
                "desc": "If `True`, only return runs that are currently active (running or pending)",
            },
            "completed_only": <DataProviderOptionInfo>{
                "display_name": "Completed Only",
                "short_desc": "Only return completed runs",
                "type": AbstractDataProviderType::get(BoolOrNothingType),
                "desc": "If `True`, only return runs that have already completed",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum total results to return",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Maximum total number of results to return (default 25)",
                "default_value": 25,
            },
            "start_time_from": <DataProviderOptionInfo>{
                "display_name": "Start Time From",
                "short_desc": "Filter by start time (after)",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Filter runs started at or after this Unix timestamp in milliseconds",
            },
            "start_time_to": <DataProviderOptionInfo>{
                "display_name": "Start Time To",
                "short_desc": "Filter by start time (before)",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Filter runs started at or before this Unix timestamp in milliseconds",
            },
        };
    }

    private {
        #! Record type
        static DatabricksJobRunDataType recordType();
    }

    #! Creates the object with a REST client
    constructor(DatabricksRestClient::DatabricksRestClient rest)
            : DatabricksDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "job-runs";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Databricks job runs data provider; list and search job runs";
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    #! Searches for job runs
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If run_id provided, get single record
        if (search_options.run_id) {
            hash<auto> result = doGet("api/2.1/jobs/runs/get", {"run_id": search_options.run_id});
            return new DefaultRecordIterator(new ListHashIterator((result,)),
                where_cond, search_options, recordType.getFields());
        }

        # Build query parameters from search options
        hash<auto> params;
        if (search_options.job_id) {
            params.job_id = search_options.job_id;
        }
        if (search_options.active_only) {
            params.active_only = search_options.active_only;
        }
        if (search_options.completed_only) {
            params.completed_only = search_options.completed_only;
        }
        *int max_records;
        if (search_options.limit) {
            params.limit = search_options.limit;
            max_records = search_options.limit;
        }
        if (search_options.start_time_from) {
            params.start_time_from = search_options.start_time_from;
        }
        if (search_options.start_time_to) {
            params.start_time_to = search_options.start_time_to;
        }

        list<auto> records = paginateGet("api/2.1/jobs/runs/list", "runs", params, max_records);
        return new DefaultRecordIterator(new ListHashIterator(records),
            where_cond, search_options, recordType.getFields());
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace DatabricksDataProvider
