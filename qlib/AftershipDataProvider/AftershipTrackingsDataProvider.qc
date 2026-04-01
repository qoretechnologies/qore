# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AftershipDataProvider module definition

/** AftershipTrackingsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Search options for listing trackings
public const AftershipTrackingsSearchOptions = {
    "keyword": <DataProviderOptionInfo>{
        "display_name": "Keyword",
        "short_desc": "Search keyword",
        "type": AbstractDataProviderTypeMap."*string",
        "desc": "Search keyword to filter trackings by tracking number, title, order ID, etc.",
        "preselected": True,
    },
    "slug": <DataProviderOptionInfo>{
        "display_name": "Courier Code",
        "short_desc": "Filter by courier code",
        "type": AbstractDataProviderTypeMap."*string",
        "desc": "Filter trackings by courier code (e.g., `fedex`, `ups`)",
    },
    "tag": <DataProviderOptionInfo>{
        "display_name": "Status Tag",
        "short_desc": "Filter by status tag",
        "type": AbstractDataProviderTypeMap."*string",
        "desc": "Filter trackings by delivery status tag",
        "preselected": True,
        "allowed_values": (
            <AllowedValueInfo>{"value": "Pending", "display_name": "Pending"},
            <AllowedValueInfo>{"value": "InfoReceived", "display_name": "Info Received"},
            <AllowedValueInfo>{"value": "InTransit", "display_name": "In Transit"},
            <AllowedValueInfo>{"value": "OutForDelivery", "display_name": "Out for Delivery"},
            <AllowedValueInfo>{"value": "AttemptFail", "display_name": "Attempt Failed"},
            <AllowedValueInfo>{"value": "Delivered", "display_name": "Delivered"},
            <AllowedValueInfo>{"value": "AvailableForPickup", "display_name": "Available for Pickup"},
            <AllowedValueInfo>{"value": "Exception", "display_name": "Exception"},
            <AllowedValueInfo>{"value": "Expired", "display_name": "Expired"},
        ),
    },
    "created_at_min": <DataProviderOptionInfo>{
        "display_name": "Created After",
        "short_desc": "Filter by minimum creation date",
        "type": AbstractDataProviderType::get(DateOrNothingType),
        "desc": "Filter trackings created after this date",
    },
    "created_at_max": <DataProviderOptionInfo>{
        "display_name": "Created Before",
        "short_desc": "Filter by maximum creation date",
        "type": AbstractDataProviderType::get(DateOrNothingType),
        "desc": "Filter trackings created before this date",
    },
    "updated_at_min": <DataProviderOptionInfo>{
        "display_name": "Updated After",
        "short_desc": "Filter by minimum update date",
        "type": AbstractDataProviderType::get(DateOrNothingType),
        "desc": "Filter trackings updated after this date",
    },
    "updated_at_max": <DataProviderOptionInfo>{
        "display_name": "Updated Before",
        "short_desc": "Filter by maximum update date",
        "type": AbstractDataProviderType::get(DateOrNothingType),
        "desc": "Filter trackings updated before this date",
    },
    "tracking_numbers": <DataProviderOptionInfo>{
        "display_name": "Tracking Numbers",
        "short_desc": "Filter by tracking numbers",
        "type": AbstractDataProviderTypeMap."*string",
        "desc": "Comma-separated tracking numbers to filter by (up to 50)",
    },
    "order_id": <DataProviderOptionInfo>{
        "display_name": "Order ID",
        "short_desc": "Filter by order ID",
        "type": AbstractDataProviderTypeMap."*string",
        "desc": "Filter trackings by the order ID",
        "preselected": True,
    },
    "shipment_tags": <DataProviderOptionInfo>{
        "display_name": "Shipment Tags",
        "short_desc": "Filter by shipment tags",
        "type": AbstractDataProviderTypeMap."*string",
        "desc": "Comma-separated shipment tags to filter by",
    },
    "return_to_sender": <DataProviderOptionInfo>{
        "display_name": "Return to Sender",
        "short_desc": "Filter return-to-sender shipments",
        "type": AbstractDataProviderTypeMap."*bool",
        "desc": "Filter trackings by return-to-sender status. Set to `True` to show only "
            "return-to-sender shipments, `False` to exclude them",
    },
    "origin": <DataProviderOptionInfo>{
        "display_name": "Origin Country",
        "short_desc": "Filter by origin country",
        "type": AbstractDataProviderTypeMap."*string",
        "desc": "ISO Alpha-3 origin country code(s), comma-separated",
    },
    "destination": <DataProviderOptionInfo>{
        "display_name": "Destination Country",
        "short_desc": "Filter by destination country",
        "type": AbstractDataProviderTypeMap."*string",
        "desc": "ISO Alpha-3 destination country code(s), comma-separated",
    },
    "cursor": <DataProviderOptionInfo>{
        "display_name": "Cursor",
        "short_desc": "Pagination cursor",
        "type": AbstractDataProviderTypeMap."*string",
        "desc": "Cursor for pagination; use the value from the previous response to get the next page",
    },
    "limit": <DataProviderOptionInfo>{
        "display_name": "Limit",
        "short_desc": "Number of results per page",
        "type": AbstractDataProviderTypeMap."*int",
        "desc": "Number of trackings per page (max 200)",
        "preselected": True,
    },
};

#! Data provider for listing/searching trackings (DPAT_FIND)
public class AftershipTrackingsDataProvider inherits AftershipDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "trackings",
            "type": "AftershipTrackingsDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_read": True,
            "has_record": True,
            "supports_native_search": True,
            "search_options": AftershipTrackingsSearchOptions,
        };

        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    constructor(RestClientIo::RestClientIo rest) : AftershipDataProviderBase(rest) {}
    constructor(*hash<auto> options) : AftershipDataProviderBase(options) {}
    string getName() { return ProviderInfo.name; }

    #! Returns the record type
    private *AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        hash<auto> params = {};
        if (search_options.keyword) params.keyword = search_options.keyword;
        if (search_options.slug) params.slug = search_options.slug;
        if (search_options.tag) params.tag = search_options.tag;
        if (search_options.created_at_min) params.created_at_min = search_options.created_at_min;
        if (search_options.created_at_max) params.created_at_max = search_options.created_at_max;
        if (search_options.updated_at_min) params.updated_at_min = search_options.updated_at_min;
        if (search_options.updated_at_max) params.updated_at_max = search_options.updated_at_max;
        if (search_options.tracking_numbers) params.tracking_numbers = search_options.tracking_numbers;
        if (search_options.order_id) params.order_id = search_options.order_id;
        if (search_options.shipment_tags) params.shipment_tags = search_options.shipment_tags;
        if (exists search_options.return_to_sender) params.return_to_sender = search_options.return_to_sender;
        if (search_options.origin) params.origin = search_options.origin;
        if (search_options.destination) params.destination = search_options.destination;
        if (search_options.cursor) params.cursor = search_options.cursor;
        if (search_options.limit) params.limit = search_options.limit;

        string path = "trackings";
        if (params) {
            path += "?" + (
                map sprintf("%s=%s", encode_url($1.key, True), encode_url(string($1.value), True)),
                    params.pairIterator()
            ).join("&");
        }
        hash<auto> response = doTrackingCommand("GET", path);
        list<hash<auto>> trackings = response.body.data.trackings ?? ();
        return new DefaultRecordIterator(new ListIterator(trackings));
    }

    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return AftershipTrackingDataType.getFields();
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}
}
