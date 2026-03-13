# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore InstantlyDataProvider module definition

/*  InstantlyAuditLogsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the InstantlyDataProvider module
public namespace InstantlyDataProvider {

#! Instantly audit logs collection data provider
public class InstantlyAuditLogsDataProvider inherits InstantlyDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "audit-logs",
            "desc": "Instantly audit logs data provider; list and search audit log entries",
            "type": "InstantlyAuditLogsDataProvider",
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
            "user_id": <DataProviderOptionInfo>{
                "display_name": "User ID",
                "short_desc": "Filter by user UUID",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter audit logs by the user UUID who performed the action",
            },
            "action": <DataProviderOptionInfo>{
                "display_name": "Action",
                "short_desc": "Filter by action type",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter audit logs by the action that was performed",
            },
            "resource_type": <DataProviderOptionInfo>{
                "display_name": "Resource Type",
                "short_desc": "Filter by resource type",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter audit logs by the type of resource affected",
            },
            "start_date": <DataProviderOptionInfo>{
                "display_name": "Start Date",
                "short_desc": "Filter from date",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter audit logs from this date (inclusive)",
            },
            "end_date": <DataProviderOptionInfo>{
                "display_name": "End Date",
                "short_desc": "Filter to date",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter audit logs up to this date (inclusive)",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum number of results to return",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "The maximum number of results to return per page",
            },
        };
    }

    private {
        #! Record type
        static InstantlyAuditLogDataType recordType();
    }

    #! Creates the object with a REST client
    constructor(InstantlyRestClient::InstantlyRestClient rest)
            : InstantlyDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "audit-logs";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Instantly audit logs data provider; list and search audit log entries";
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    #! Searches for audit log entries
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        hash<auto> params;

        if (search_options.user_id) {
            params.user_id = search_options.user_id;
        }
        if (search_options.action) {
            params.action = search_options.action;
        }
        if (search_options.resource_type) {
            params.resource_type = search_options.resource_type;
        }
        if (search_options.start_date) {
            params.start_date = search_options.start_date;
        }
        if (search_options.end_date) {
            params.end_date = search_options.end_date;
        }
        if (search_options.limit) {
            params.limit = search_options.limit;
        }

        list<auto> records = doPaginatedGet("/audit-logs", params);

        return new DefaultRecordIterator(new ListHashIterator(records), where_cond, search_options,
            recordType.getFields());
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace InstantlyDataProvider
