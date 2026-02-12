# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SquareRefundsDataProvider.qc Square refunds collection data provider

/*  SquareRefundsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SquareDataProvider module
public namespace SquareDataProvider {

#! Square refunds collection data provider
public class SquareRefundsDataProvider inherits SquareDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "refunds",
            "desc": "Square refunds data provider for listing refunds",
            "type": "SquareRefundsDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "constructor_options": ConstructorOptions,
            "search_options": SearchOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Search options
        const SearchOptions = {
            "begin_time": <DataProviderOptionInfo>{
                "display_name": "Begin Time",
                "short_desc": "Filter by start time",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Return refunds created at or after this timestamp (RFC 3339 format)",
            },
            "end_time": <DataProviderOptionInfo>{
                "display_name": "End Time",
                "short_desc": "Filter by end time",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Return refunds created before this timestamp (RFC 3339 format)",
            },
            "sort_order": <DataProviderOptionInfo>{
                "display_name": "Sort Order",
                "short_desc": "Sort direction",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Sort direction (ASC, DESC)",
            },
            "location_id": <DataProviderOptionInfo>{
                "display_name": "Location ID",
                "short_desc": "Filter by location",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter refunds by location ID",
            },
            "status": <DataProviderOptionInfo>{
                "display_name": "Status",
                "short_desc": "Filter by refund status",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter refunds by status (PENDING, COMPLETED, REJECTED, FAILED)",
            },
            "source_type": <DataProviderOptionInfo>{
                "display_name": "Source Type",
                "short_desc": "Filter by source type",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter refunds by source type (CARD, BANK_ACCOUNT)",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum results",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Maximum number of results to return",
            },
        };
    }

    #! Creates the object with a REST client
    constructor(SquareRestClient::SquareRestClient rest) : SquareDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "refunds";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Square refunds data provider for listing refunds";
    }

    #! Returns the record type
    private *AbstractDataProviderType getRecordTypeImpl() {
        return new SquareRefundDataType();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # Build query params
        hash<auto> params;
        if (search_options.begin_time) {
            params.begin_time = search_options.begin_time;
        }
        if (search_options.end_time) {
            params.end_time = search_options.end_time;
        }
        if (search_options.sort_order) {
            params.sort_order = search_options.sort_order;
        }
        if (search_options.location_id) {
            params.location_id = search_options.location_id;
        }
        if (search_options.status) {
            params.status = search_options.status;
        }
        if (search_options.source_type) {
            params.source_type = search_options.source_type;
        }

        list<hash<auto>> all_records = fetchAllPages("refunds", "refunds", params,
            search_options.limit);
        return new DefaultRecordIterator(new ListHashIterator(all_records), where_cond,
            search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SquareDataProvider
