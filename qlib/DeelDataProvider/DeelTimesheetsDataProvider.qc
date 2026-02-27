# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DeelTimesheetsDataProvider.qc Deel timesheets collection data provider

/*  DeelTimesheetsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Deel timesheets collection data provider
public class DeelTimesheetsDataProvider inherits DeelDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "timesheets",
            "desc": "Deel timesheets data provider for listing and searching timesheet entries",
            "type": "DeelTimesheetsDataProvider",
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
            "timesheet_id": <DataProviderOptionInfo>{
                "display_name": "Timesheet ID",
                "short_desc": "Get a specific timesheet by ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by timesheet ID for single record lookup",
            },
            "contract_id": <DataProviderOptionInfo>{
                "display_name": "Contract ID",
                "short_desc": "Filter by contract",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter timesheets by associated contract ID",
            },
            "status": <DataProviderOptionInfo>{
                "display_name": "Status",
                "short_desc": "Filter by timesheet status",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter timesheets by status (e.g., `pending`, `approved`, `rejected`)",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "pending", "desc": "Pending timesheets"},
                    <AllowedValueInfo>{"value": "approved", "desc": "Approved timesheets"},
                    <AllowedValueInfo>{"value": "rejected", "desc": "Rejected timesheets"},
                ),
            },
            "date_from": <DataProviderOptionInfo>{
                "display_name": "Date From",
                "short_desc": "Start of date range",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter timesheets from this date (YYYY-MM-DD format)",
            },
            "date_to": <DataProviderOptionInfo>{
                "display_name": "Date To",
                "short_desc": "End of date range",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter timesheets up to this date (YYYY-MM-DD format)",
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
            "approve": Class::forName("DeelApproveTimesheetDataProvider"),
            "delete": Class::forName("DeelDeleteTimesheetDataProvider"),
            "reject": Class::forName("DeelRejectTimesheetDataProvider"),
            "submit": Class::forName("DeelSubmitTimesheetDataProvider"),
            "update": Class::forName("DeelUpdateTimesheetDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(DeelRestClient::DeelRestClient rest) : DeelDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "timesheets";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Deel timesheets data provider for listing and searching timesheet entries";
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
        return new DeelTimesheetDataType().getFields();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If timesheet_id provided, get single record
        if (search_options.timesheet_id) {
            hash<auto> result = doGet("timesheets/" + encode_url(search_options.timesheet_id, True));
            list<hash<auto>> data = result.data ? (result.data,) : ();
            return new DefaultRecordIterator(new ListHashIterator(data),
                where_cond, search_options);
        }

        # Build query params for listing
        hash<auto> params;
        if (search_options.contract_id) {
            params.contractId = search_options.contract_id;
        }
        if (search_options.status) {
            params.status = search_options.status;
        }
        if (search_options.date_from) {
            params.dateFrom = search_options.date_from;
        }
        if (search_options.date_to) {
            params.dateTo = search_options.date_to;
        }

        list<hash<auto>> all_records = fetchAllPages("timesheets", "timesheets", params,
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
