# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SquarePaymentsDataProvider.qc Square payments collection data provider

/*  SquarePaymentsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Square payments collection data provider
public class SquarePaymentsDataProvider inherits SquareDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "payments",
            "desc": "Square payments data provider for listing and retrieving payments",
            "type": "SquarePaymentsDataProvider",
            "supports_read": True,
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
            "payment_id": <DataProviderOptionInfo>{
                "display_name": "Payment ID",
                "short_desc": "Get a specific payment by ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by payment ID for single record lookup",
            },
            "begin_time": <DataProviderOptionInfo>{
                "display_name": "Begin Time",
                "short_desc": "Filter by start time",
                "type": AbstractDataProviderType::get(SoftDateOrNothingType),
                "desc": "Return payments created at or after this timestamp",
            },
            "end_time": <DataProviderOptionInfo>{
                "display_name": "End Time",
                "short_desc": "Filter by end time",
                "type": AbstractDataProviderType::get(SoftDateOrNothingType),
                "desc": "Return payments created before this timestamp",
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
                "desc": "Filter payments by location ID",
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
            "create": Class::forName("SquareCreatePaymentDataProvider"),
            "complete": Class::forName("SquareCompletePaymentDataProvider"),
            "refund": Class::forName("SquareRefundPaymentDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(SquareRestClient::SquareRestClient rest) : SquareDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "payments";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Square payments data provider for listing and retrieving payments";
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
    private *AbstractDataProviderType getRecordTypeImpl() {
        return new SquarePaymentDataType();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If payment_id provided, get single record
        if (search_options.payment_id) {
            hash<auto> result = doGet("payments/" + search_options.payment_id);
            return new DefaultRecordIterator(new ListHashIterator((result.payment,)),
                where_cond, search_options);
        }

        # Build query params
        hash<auto> params;
        if (search_options.begin_time) {
            params.begin_time = formatDateTimeSquare(search_options.begin_time);
        }
        if (search_options.end_time) {
            params.end_time = formatDateTimeSquare(search_options.end_time);
        }
        if (search_options.sort_order) {
            params.sort_order = search_options.sort_order;
        }
        if (search_options.location_id) {
            params.location_id = search_options.location_id;
        }

        list<hash<auto>> all_records = fetchAllPages("payments", "payments", params,
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
