# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SquareOrdersDataProvider.qc Square orders collection data provider

/*  SquareOrdersDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Square orders collection data provider
public class SquareOrdersDataProvider inherits SquareDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "orders",
            "desc": "Square orders data provider for searching and retrieving orders",
            "type": "SquareOrdersDataProvider",
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
            "order_id": <DataProviderOptionInfo>{
                "display_name": "Order ID",
                "short_desc": "Get a specific order by ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by order ID for single record lookup",
            },
            "location_ids": <DataProviderOptionInfo>{
                "display_name": "Location IDs",
                "short_desc": "Filter by location IDs",
                "type": AbstractDataProviderType::get(new Type("softlist<string>")),
                "desc": "Filter orders by location IDs (required for search, auto-detected "
                    "if not provided)",
            },
            "state": <DataProviderOptionInfo>{
                "display_name": "State",
                "short_desc": "Filter by order state",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter orders by state (OPEN, COMPLETED, CANCELED, DRAFT)",
            },
            "created_at_start": <DataProviderOptionInfo>{
                "display_name": "Created After",
                "short_desc": "Filter by creation start date",
                "type": AbstractDataProviderType::get(SoftDateOrNothingType),
                "desc": "Return orders created at or after this timestamp (RFC 3339 format)",
            },
            "created_at_end": <DataProviderOptionInfo>{
                "display_name": "Created Before",
                "short_desc": "Filter by creation end date",
                "type": AbstractDataProviderType::get(SoftDateOrNothingType),
                "desc": "Return orders created before this timestamp (RFC 3339 format)",
            },
            "updated_at_start": <DataProviderOptionInfo>{
                "display_name": "Updated After",
                "short_desc": "Filter by update start date",
                "type": AbstractDataProviderType::get(SoftDateOrNothingType),
                "desc": "Return orders updated at or after this timestamp (RFC 3339 format)",
            },
            "updated_at_end": <DataProviderOptionInfo>{
                "display_name": "Updated Before",
                "short_desc": "Filter by update end date",
                "type": AbstractDataProviderType::get(SoftDateOrNothingType),
                "desc": "Return orders updated before this timestamp (RFC 3339 format)",
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
            "create": Class::forName("SquareCreateOrderDataProvider"),
            "create-paid": Class::forName("SquareCreatePaidOrderDataProvider"),
            "batch-get": Class::forName("SquareGetOrdersDataProvider"),
            "cancel": Class::forName("SquareCancelOrderDataProvider"),
        };
    }

    #! Creates the object with a REST client
    constructor(SquareRestClient::SquareRestClient rest) : SquareDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "orders";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Square orders data provider for searching and retrieving orders";
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
        return new SquareOrderDataType();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If order_id provided, get single record
        if (search_options.order_id) {
            hash<auto> result = doGet("orders/" + search_options.order_id);
            return new DefaultRecordIterator(new ListHashIterator((result.order,)),
                where_cond, search_options);
        }

        # Build search body for POST /orders/search
        hash<auto> body = {};

        # Get location IDs (required for search)
        list<string> location_ids;
        if (search_options.location_ids) {
            location_ids = search_options.location_ids;
        } else {
            location_ids = getLocationIds();
        }
        body.location_ids = location_ids;

        # Build query filter
        hash<auto> query_filter;
        if (search_options.state) {
            query_filter.state_filter = {"states": (search_options.state,)};
        }
        # Date filter for created_at
        if (search_options.created_at_start || search_options.created_at_end) {
            hash<auto> date_time_filter;
            if (search_options.created_at_start) {
                date_time_filter.created_at = {
                    "start_at": formatDateTimeSquare(search_options.created_at_start),
                };
            }
            if (search_options.created_at_end) {
                date_time_filter.created_at.end_at =
                    formatDateTimeSquare(search_options.created_at_end);
            }
            query_filter.date_time_filter = date_time_filter;
        }
        # Date filter for updated_at
        if (search_options.updated_at_start || search_options.updated_at_end) {
            hash<auto> date_time_filter = query_filter.date_time_filter ?? {};
            if (search_options.updated_at_start) {
                date_time_filter.updated_at = {
                    "start_at": formatDateTimeSquare(search_options.updated_at_start),
                };
            }
            if (search_options.updated_at_end) {
                date_time_filter.updated_at.end_at =
                    formatDateTimeSquare(search_options.updated_at_end);
            }
            query_filter.date_time_filter = date_time_filter;
        }
        if (query_filter) {
            body.query = {"filter": query_filter};
        }
        if (search_options.limit) {
            body.limit = search_options.limit;
        }

        list<hash<auto>> all_records = fetchAllPagesPost("orders/search", "orders", body,
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
