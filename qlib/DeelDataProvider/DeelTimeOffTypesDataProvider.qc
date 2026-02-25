# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DeelTimeOffTypesDataProvider.qc Deel time-off types/entitlements collection data provider

/*  DeelTimeOffTypesDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Deel time-off types (entitlements) collection data provider
public class DeelTimeOffTypesDataProvider inherits DeelDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "time-off",
            "desc": "Deel time-off data provider for listing time-off types and creating requests",
            "type": "DeelTimeOffTypesDataProvider",
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
            "contract_id": <DataProviderOptionInfo>{
                "display_name": "Contract ID",
                "short_desc": "Filter by contract",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter time-off entitlements by contract ID",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum results",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Maximum number of results to return",
            },
        };

        #! Child provider map
        const ChildMap = {
            "create-request": Class::forName("DeelCreateTimeOffRequestDataProvider"),
            "approve-request": Class::forName("DeelApproveTimeOffRequestDataProvider"),
            "reject-request": Class::forName("DeelRejectTimeOffRequestDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(DeelRestClient::DeelRestClient rest) : DeelDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "time-off";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Deel time-off data provider for listing time-off types and creating requests";
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
        return new DeelTimeOffTypeDataType().getFields();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # time_offs uses pageSize (not limit) and cursor-based pagination with has_next_page/next
        int page_size = DefaultPageSize;
        *int max = search_options.limit;
        if (max && max < page_size) {
            page_size = max;
        }
        hash<auto> params = {"pageSize": page_size};

        list<hash<auto>> all_records = ();
        while (True) {
            hash<auto> response = doGet("time_offs", params);
            list<auto> page_data = response{"data"} ?? ();
            if (!page_data) {
                break;
            }
            all_records += page_data;
            if (max && all_records.size() >= max) {
                all_records = all_records[0..max - 1];
                break;
            }
            if (!response{"has_next_page"}) {
                break;
            }
            *string next_cursor = response{"next"};
            if (!next_cursor) {
                break;
            }
            params{"next"} = next_cursor;
        }
        return new DefaultRecordIterator(new ListHashIterator(all_records), where_cond,
            search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace DeelDataProvider
