# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShippoDataProvider module definition

/*  ShippoTransactionsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ShippoDataProvider module
public namespace ShippoDataProvider {

#! Shippo transactions collection data provider - list and search shipping labels
public class ShippoTransactionsDataProvider inherits ShippoDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "transactions",
            "desc": "Shippo transactions data provider - list and search shipping labels",
            "type": "ShippoTransactionsDataProvider",
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
            "object_created_gt": <DataProviderOptionInfo>{
                "display_name": "Created After",
                "short_desc": "Filter by creation date (after)",
                "type": AbstractDataProviderType::get(DateOrNothingType),
                "desc": "Return transactions created after this date/time",
            },
            "object_created_lt": <DataProviderOptionInfo>{
                "display_name": "Created Before",
                "short_desc": "Filter by creation date (before)",
                "type": AbstractDataProviderType::get(DateOrNothingType),
                "desc": "Return transactions created before this date/time",
            },
            "object_status": <DataProviderOptionInfo>{
                "display_name": "Status",
                "short_desc": "Filter by transaction status",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter transactions by status",
                "preselected": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "QUEUED", "desc": "Label creation pending"},
                    <AllowedValueInfo>{"value": "WAITING", "desc": "Waiting for carrier"},
                    <AllowedValueInfo>{"value": "SUCCESS", "desc": "Label created successfully"},
                    <AllowedValueInfo>{"value": "ERROR", "desc": "Label creation failed"},
                    <AllowedValueInfo>{"value": "REFUNDED", "desc": "Label has been refunded"},
                    <AllowedValueInfo>{"value": "REFUNDPENDING", "desc": "Refund pending"},
                    <AllowedValueInfo>{"value": "REFUNDREJECTED", "desc": "Refund rejected"},
                ),
            },
            "rate": <DataProviderOptionInfo>{
                "display_name": "Rate ID",
                "short_desc": "Filter by rate",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter transactions by the rate object ID used to create the label",
            },
            "tracking_status": <DataProviderOptionInfo>{
                "display_name": "Tracking Status",
                "short_desc": "Filter by tracking status",
                "type": AbstractDataProviderType::get(StringOrNothingType),
                "desc": "Filter by tracking status",
                "preselected": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "UNKNOWN", "desc": "Unknown tracking status"},
                    <AllowedValueInfo>{"value": "PRE_TRANSIT", "desc": "Pre-transit"},
                    <AllowedValueInfo>{"value": "TRANSIT", "desc": "In transit"},
                    <AllowedValueInfo>{"value": "DELIVERED", "desc": "Delivered"},
                    <AllowedValueInfo>{"value": "RETURNED", "desc": "Returned"},
                    <AllowedValueInfo>{"value": "FAILURE", "desc": "Delivery failed"},
                ),
            },
            "page": <DataProviderOptionInfo>{
                "display_name": "Page",
                "short_desc": "Page number",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Page number for pagination (starting at 1)",
            },
            "results": <DataProviderOptionInfo>{
                "display_name": "Page Size",
                "short_desc": "Results per page",
                "type": AbstractDataProviderType::get(IntOrNothingType),
                "desc": "Number of results per page (default 25, max 100)",
            },
        };
    }

    private {
        #! Record type
        static ShippoTransactionDataType recordType();
    }

    #! Creates the object with a REST client
    constructor(ShippoRestClient::ShippoRestClient rest) : ShippoDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(ShippoRestClient::ShippoRestConnection conn) : ShippoDataProviderBase(conn) {
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : ShippoDataProviderBase(options) {
    }

    #! Returns the provider name
    string getName() {
        return "transactions";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Shippo transactions data provider - list and search shipping labels";
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return recordType.getFields();
    }

    #! Searches for transactions
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        hash<auto> params;

        # Build query parameters from search options
        if (search_options.object_created_gt) {
            params.object_created_gt = search_options.object_created_gt;
        }
        if (search_options.object_created_lt) {
            params.object_created_lt = search_options.object_created_lt;
        }
        if (search_options.object_status) {
            params.object_status = search_options.object_status;
        }
        if (search_options.rate) {
            params.rate = search_options.rate;
        }
        if (search_options.tracking_status) {
            params.tracking_status = search_options.tracking_status;
        }
        if (search_options.page) {
            params.page = search_options.page;
        }
        if (search_options.results) {
            params.results = search_options.results;
        } else {
            params.results = DefaultPageSize;
        }

        @debug(log(LoggerLevel::DEBUG, "searchRecordsImpl: params=%y", params));

        hash<auto> response = doGet("/transactions", params);
        list<hash<auto>> records = response.results ?? ();

        return new DefaultRecordIterator(new ListHashIterator(records), where_cond, search_options,
            recordType.getFields());
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ShippoDataProvider
