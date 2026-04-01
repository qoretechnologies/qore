# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AftershipDataProvider module definition

/** AftershipReturnsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the AftershipDataProvider module
public namespace AftershipDataProvider {
#! Search options for listing returns
public const AftershipReturnsSearchOptions = {
    "approval_status": <DataProviderOptionInfo>{
        "display_name": "Approval Status",
        "short_desc": "Filter by approval status",
        "type": AbstractDataProviderTypeMap."*string",
        "desc": "Filter returns by approval status",
        "preselected": True,
        "allowed_values": (
            <AllowedValueInfo>{"value": "submitted", "display_name": "Submitted"},
            <AllowedValueInfo>{"value": "approved", "display_name": "Approved"},
            <AllowedValueInfo>{"value": "done", "display_name": "Done"},
            <AllowedValueInfo>{"value": "rejected", "display_name": "Rejected"},
            <AllowedValueInfo>{"value": "expired", "display_name": "Expired"},
        ),
    },
    "customer_email": <DataProviderOptionInfo>{
        "display_name": "Customer Email",
        "short_desc": "Filter by customer email",
        "type": AbstractDataProviderTypeMap."*string",
        "desc": "Filter returns by customer email address",
    },
    "outcome": <DataProviderOptionInfo>{
        "display_name": "Outcome",
        "short_desc": "Filter by return outcome",
        "type": AbstractDataProviderTypeMap."*string",
        "desc": "Filter returns by outcome type",
        "allowed_values": (
            <AllowedValueInfo>{"value": "exchange", "display_name": "Exchange"},
            <AllowedValueInfo>{"value": "refund", "display_name": "Refund"},
            <AllowedValueInfo>{"value": "upsell", "display_name": "Upsell"},
            <AllowedValueInfo>{"value": "store_credit", "display_name": "Store Credit"},
        ),
    },
    "return_method": <DataProviderOptionInfo>{
        "display_name": "Return Method",
        "short_desc": "Filter by return method",
        "type": AbstractDataProviderTypeMap."*string",
        "desc": "Filter returns by shipping method",
        "allowed_values": (
            <AllowedValueInfo>{"value": "happy_returns", "display_name": "Happy Returns"},
            <AllowedValueInfo>{"value": "retail_reworks", "display_name": "Retail Reworks"},
            <AllowedValueInfo>{"value": "retailer_label", "display_name": "Retailer Label"},
            <AllowedValueInfo>{"value": "carrier_pickup", "display_name": "Carrier Pickup"},
            <AllowedValueInfo>{"value": "customer_courier", "display_name": "Customer Courier"},
            <AllowedValueInfo>{"value": "in_store", "display_name": "In Store"},
            <AllowedValueInfo>{"value": "green_return", "display_name": "Green Return"},
            <AllowedValueInfo>{"value": "carrier_dropoff", "display_name": "Carrier Dropoff"},
        ),
    },
    "order_name": <DataProviderOptionInfo>{
        "display_name": "Order Name",
        "short_desc": "Filter by order name",
        "type": AbstractDataProviderTypeMap."*string",
        "desc": "Filter by order name (e.g., `#1001`)",
    },
    "created_at_min": <DataProviderOptionInfo>{
        "display_name": "Created After",
        "short_desc": "Filter by minimum creation date",
        "type": AbstractDataProviderType::get(DateOrNothingType),
        "desc": "Filter returns created after this date",
    },
    "created_at_max": <DataProviderOptionInfo>{
        "display_name": "Created Before",
        "short_desc": "Filter by maximum creation date",
        "type": AbstractDataProviderType::get(DateOrNothingType),
        "desc": "Filter returns created before this date",
    },
    "page": <DataProviderOptionInfo>{
        "display_name": "Page",
        "short_desc": "Page number",
        "type": AbstractDataProviderTypeMap."*int",
        "desc": "Page number for pagination (1-200, default `1`)",
    },
    "limit": <DataProviderOptionInfo>{
        "display_name": "Limit",
        "short_desc": "Number of results per page",
        "type": AbstractDataProviderTypeMap."*int",
        "desc": "Number of returns per page (1-50, default `10`)",
        "preselected": True,
    },
};

#! Data provider for listing/searching returns (DPAT_FIND)
public class AftershipReturnsDataProvider inherits AftershipDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "returns",
            "type": "AftershipReturnsDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_read": True,
            "has_record": True,
            "supports_native_search": True,
            "search_options": AftershipReturnsSearchOptions,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    constructor(RestClientIo::RestClientIo rest) : AftershipDataProviderBase(rest) {}
    constructor(*hash<auto> options) : AftershipDataProviderBase(options) {}
    string getName() { return ProviderInfo.name; }

    private *AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        hash<auto> params = {};
        if (search_options.approval_status) params.approval_status = search_options.approval_status;
        if (search_options.customer_email) params.customer_email = search_options.customer_email;
        if (search_options.outcome) params.outcomes = search_options.outcome;
        if (search_options.return_method) params.return_method = search_options.return_method;
        if (search_options.order_name) params.order_name = search_options.order_name;
        if (search_options.created_at_min) params.created_at_min = search_options.created_at_min;
        if (search_options.created_at_max) params.created_at_max = search_options.created_at_max;
        if (search_options.page) params.page = search_options.page;
        if (search_options.limit) params.limit = search_options.limit;

        string path = "returns";
        if (params) {
            path += "?" + (
                map sprintf("%s=%s", encode_url($1.key, True), encode_url(string($1.value), True)),
                    params.pairIterator()
            ).join("&");
        }
        hash<auto> response = doReturnsCommand("GET", path);
        list<hash<auto>> recs = response.body.data.returns ?? ();
        return new DefaultRecordIterator(new ListIterator(recs));
    }

    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return AftershipReturnDataType.getFields();
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}
}
