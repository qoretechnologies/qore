# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DatabricksPipelinesDataProvider class definition

/*  DatabricksPipelinesDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Databricks pipelines collection data provider
public class DatabricksPipelinesDataProvider inherits DatabricksDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "pipelines",
            "desc": "Databricks Delta Live Tables pipelines data provider; list and search pipelines",
            "type": "DatabricksPipelinesDataProvider",
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
            "pipeline_id": <DataProviderOptionInfo>{
                "display_name": "Pipeline ID",
                "short_desc": "Get a specific pipeline by ID",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter by pipeline ID for single record lookup",
            },
            "filter": <DataProviderOptionInfo>{
                "display_name": "Filter",
                "short_desc": "Filter pipelines by name",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter pipelines by name (partial match)",
            },
            "max_results": <DataProviderOptionInfo>{
                "display_name": "Max Results",
                "short_desc": "Maximum number of results",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Maximum number of pipelines to return per page",
            },
        };
    }

    private {
        #! Record type
        static DatabricksPipelineDataType recordType();
    }

    #! Creates the object with a REST client
    constructor(DatabricksRestClient::DatabricksRestClient rest)
            : DatabricksDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "pipelines";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Databricks Delta Live Tables pipelines data provider; list and search pipelines";
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    #! Searches for pipelines
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If pipeline_id provided, get single record
        if (search_options.pipeline_id) {
            hash<auto> result = doGet("api/2.0/pipelines/" + search_options.pipeline_id);
            return new DefaultRecordIterator(new ListHashIterator((result,)),
                where_cond, search_options, recordType.getFields());
        }

        # Build query parameters from search options
        hash<auto> params;
        if (search_options.filter) {
            params.filter = search_options.filter;
        }
        *int max_records;
        if (search_options.max_results) {
            params.max_results = search_options.max_results;
            max_records = search_options.max_results;
        }

        list<auto> records = paginateGet("api/2.0/pipelines", "statuses", params, max_records);
        return new DefaultRecordIterator(new ListHashIterator(records),
            where_cond, search_options, recordType.getFields());
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace DatabricksDataProvider
