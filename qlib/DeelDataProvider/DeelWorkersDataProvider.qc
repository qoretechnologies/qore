# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DeelWorkersDataProvider.qc Deel workers/people collection data provider

/*  DeelWorkersDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DeelDataProvider module
public namespace DeelDataProvider {

#! Deel workers (people) collection data provider
public class DeelWorkersDataProvider inherits DeelDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "workers",
            "desc": "Deel workers data provider for listing and searching workers/people",
            "type": "DeelWorkersDataProvider",
            "supports_read": True,
            "has_record": True,
            "supports_native_search": True,
            "supports_children": True,
            "children_can_support_apis": True,
            "constructor_options": ConstructorOptions,
            "search_options": SearchOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Search options
        const SearchOptions = {
            "worker_id": <DataProviderOptionInfo>{
                "display_name": "Worker ID",
                "short_desc": "Get a specific worker by ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by worker ID for single record lookup",
            },
            "country_code": <DataProviderOptionInfo>{
                "display_name": "Country Code",
                "short_desc": "Filter by country",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter workers by ISO 3166-1 alpha-2 country code (e.g., `US`, `GB`)",
            },
            "employment_status": <DataProviderOptionInfo>{
                "display_name": "Employment Status",
                "short_desc": "Filter by employment status",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter workers by employment status",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "active", "desc": "Active workers"},
                    <AllowedValueInfo>{"value": "inactive", "desc": "Inactive workers"},
                    <AllowedValueInfo>{"value": "pending", "desc": "Pending workers"},
                ),
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum results",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Maximum number of results to return",
            },
            "offset": <DataProviderOptionInfo>{
                "display_name": "Offset",
                "short_desc": "Results offset",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Number of results to skip (for manual pagination)",
            },
        };

        #! Child provider map
        const ChildMap = {
            "create": Class::forName("DeelCreateWorkerDataProvider"),
            "update": Class::forName("DeelUpdateWorkerDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(DeelRestClient::DeelRestClient rest) : DeelDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "workers";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Deel workers data provider for listing and searching workers/people";
    }

    #! Returns child provider names
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns a child provider by name
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) {
            return cls.newObject(rest);
        }
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new DeelWorkerDataType().getFields();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If worker_id provided, get single record
        if (search_options.worker_id) {
            hash<auto> result = doGet("people/" + encode_url(search_options.worker_id, True));
            list<hash<auto>> data = result.data ? (result.data,) : ();
            return new DefaultRecordIterator(new ListHashIterator(data),
                where_cond, search_options);
        }

        # Build query params for listing
        hash<auto> params;
        if (search_options.country_code) {
            params.country = search_options.country_code;
        }
        if (search_options.employment_status) {
            params.status = search_options.employment_status;
        }

        list<hash<auto>> all_records = fetchAllPages("people", "people", params,
            search_options.limit);
        return new DefaultRecordIterator(new ListHashIterator(all_records), where_cond,
            search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace DeelDataProvider
