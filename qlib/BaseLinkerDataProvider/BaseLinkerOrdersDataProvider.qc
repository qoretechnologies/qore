# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerOrdersDataProvider.qc BaseLinker orders collection data provider
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

#! Contains all public definitions in the BaseLinkerDataProvider module
public namespace BaseLinkerDataProvider {

#! BaseLinker orders collection data provider (`getOrders`)
/** Uses BaseLinker's cursor-based pagination: walks forward by bumping
    `id_from` to one past the last `order_id` seen; stops when a page returns
    fewer than @ref OrdersPageSize rows or when the caller-supplied `limit`
    is exhausted.
*/
public class BaseLinkerOrdersDataProvider inherits BaseLinkerDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "orders",
            "desc": "List and search BaseLinker orders (`getOrders`). Supports cursor-based "
                "pagination via `id_from`; supply `order_id` to fetch a single order.",
            "type": "BaseLinkerOrdersDataProvider",
            "has_record": True,
            "supports_read": True,
            "supports_native_search": True,
            "search_options": SearchOptions,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Search options exposed for `list-orders` and `get-order`
        const SearchOptions = {
            "order_id": <DataProviderOptionInfo>{
                "display_name": "Order ID",
                "short_desc": "Fetch a single order by ID",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "If supplied, only the matching order is returned.",
            },
            "date_confirmed_from": <DataProviderOptionInfo>{
                "display_name": "Date Confirmed From",
                "short_desc": "Filter by confirmation time (Unix seconds)",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Return orders confirmed at or after this Unix timestamp.",
            },
            "date_from": <DataProviderOptionInfo>{
                "display_name": "Date From",
                "short_desc": "Filter by creation time (Unix seconds)",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Return orders created at or after this Unix timestamp.",
            },
            "id_from": <DataProviderOptionInfo>{
                "display_name": "ID From",
                "short_desc": "Starting order ID for cursor pagination",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Pagination cursor: return orders with `order_id` greater than or "
                    "equal to this value.",
            },
            "get_unconfirmed_orders": <DataProviderOptionInfo>{
                "display_name": "Include Unconfirmed Orders",
                "short_desc": "Return unconfirmed orders too",
                "type": AbstractDataProviderType::get(SoftBoolOrNothingType),
                "desc": "When `True`, unconfirmed orders are included in the result set. "
                    "Default (unspecified) returns only confirmed orders.",
                "preselected": True,
            },
            "include_custom_extra_fields": <DataProviderOptionInfo>{
                "display_name": "Include Custom Extra Fields",
                "short_desc": "Include custom extra fields",
                "type": AbstractDataProviderType::get(SoftBoolOrNothingType),
                "desc": "When `True`, custom extra fields are included in the response.",
            },
            "status_id": <DataProviderOptionInfo>{
                "display_name": "Status ID",
                "short_desc": "Filter by order status",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Only return orders in the given status.",
                "ref_data": "order_statuses",
            },
            "filter_email": <DataProviderOptionInfo>{
                "display_name": "Filter Email",
                "short_desc": "Filter by customer email",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Return orders whose customer email matches the given value.",
            },
            "filter_order_source": <DataProviderOptionInfo>{
                "display_name": "Filter Order Source",
                "short_desc": "Filter by source channel",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Return orders originating from a specific source channel.",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "ebay", "display_name": "eBay"},
                    <AllowedValueInfo>{"value": "amazon", "display_name": "Amazon"},
                    <AllowedValueInfo>{"value": "allegro", "display_name": "Allegro"},
                    <AllowedValueInfo>{"value": "shop", "display_name": "Shop"},
                    <AllowedValueInfo>{"value": "personal", "display_name": "Personal"},
                ),
            },
            "filter_order_source_id": <DataProviderOptionInfo>{
                "display_name": "Filter Order Source ID",
                "short_desc": "Filter by specific source ID",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Return orders from a specific source instance ID.",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum results",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Maximum number of records to return across all cursor pages.",
                "preselected": True,
                "default_value": 100,
            },
        };
    }

    #! Creates the object with an async REST client
    constructor(*RestClientIo::RestClientIo rest) : BaseLinkerDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(BaseLinkerRestClient::BaseLinkerRestConnection conn)
            : BaseLinkerDataProviderBase(conn) {
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : BaseLinkerDataProviderBase(options) {
    }

    #! Returns the provider name
    string getName() {
        return "orders";
    }

    #! Returns the provider description
    *string getDesc() {
        return ProviderInfo.desc;
    }

    #! Returns the record type
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return (new BaseLinkerOrderDataType()).getFields();
    }

    #! Executes the search
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        list<auto> records = ();
        int remaining = search_options.limit ? search_options.limit.toInt() : 10000;
        *softint id_from = search_options.id_from;

        # Build a reusable base params hash (everything except the cursor/limit)
        hash<auto> base_params = {};
        if (exists search_options.order_id) {
            base_params.order_id = int(search_options.order_id);
        }
        if (exists search_options.date_confirmed_from) {
            base_params.date_confirmed_from = int(search_options.date_confirmed_from);
        }
        if (exists search_options.date_from) {
            base_params.date_from = int(search_options.date_from);
        }
        if (exists search_options.get_unconfirmed_orders) {
            base_params.get_unconfirmed_orders = boolean(search_options.get_unconfirmed_orders);
        }
        if (exists search_options.include_custom_extra_fields) {
            base_params.include_custom_extra_fields =
                boolean(search_options.include_custom_extra_fields);
        }
        if (exists search_options.status_id) {
            base_params.status_id = int(search_options.status_id);
        }
        if (exists search_options.filter_email) {
            base_params.filter_email = search_options.filter_email;
        }
        if (exists search_options.filter_order_source) {
            base_params.filter_order_source = search_options.filter_order_source;
        }
        if (exists search_options.filter_order_source_id) {
            base_params.filter_order_source_id = int(search_options.filter_order_source_id);
        }

        # Single-order fast path: one call, no pagination
        if (exists search_options.order_id) {
            hash<auto> response = callMethod("getOrders", base_params);
            records = response.orders ?? ();
            return new DefaultRecordIterator(new ListHashIterator(records), where_cond,
                search_options, (new BaseLinkerOrderDataType()).getFields());
        }

        while (remaining > 0) {
            hash<auto> params = base_params;
            if (exists id_from) {
                params.id_from = id_from;
            }
            hash<auto> response = callMethod("getOrders", params);
            list<auto> page = response.orders ?? ();
            if (!page) {
                break;
            }
            foreach hash<auto> order in (page) {
                records += order;
                --remaining;
                if (!remaining) {
                    break;
                }
            }
            if (page.size() < OrdersPageSize) {
                break;
            }
            # Advance cursor to one past the last order_id in the page
            id_from = int(page[page.size() - 1].order_id) + 1;
        }

        return new DefaultRecordIterator(new ListHashIterator(records), where_cond,
            search_options, (new BaseLinkerOrderDataType()).getFields());
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BaseLinkerDataProvider
