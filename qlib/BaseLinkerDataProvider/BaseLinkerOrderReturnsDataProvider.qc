# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerOrderReturnsDataProvider.qc BaseLinker order returns collection
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

#! Contains all public definitions in the BaseLinkerDataProvider module
public namespace BaseLinkerDataProvider {

#! BaseLinker order returns collection data provider (`getOrderReturns`)
public class BaseLinkerOrderReturnsDataProvider inherits BaseLinkerDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "order-returns",
            "desc": "List and search BaseLinker order returns (`getOrderReturns`). Cursor "
                "pagination via `id_from` keyed on `order_return_id`.",
            "type": "BaseLinkerOrderReturnsDataProvider",
            "has_record": True,
            "supports_read": True,
            "supports_native_search": True,
            "search_options": SearchOptions,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Search options
        const SearchOptions = {
            "order_return_id": <DataProviderOptionInfo>{
                "display_name": "Order Return ID",
                "short_desc": "Fetch a single order return by ID",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "If supplied, only the matching return is returned.",
            },
            "date_confirmed_from": <DataProviderOptionInfo>{
                "display_name": "Date Confirmed From",
                "short_desc": "Filter by confirmation time (Unix seconds)",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Return order returns confirmed at or after this Unix timestamp.",
            },
            "date_from": <DataProviderOptionInfo>{
                "display_name": "Date From",
                "short_desc": "Filter by creation time (Unix seconds)",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Return order returns created at or after this Unix timestamp.",
            },
            "id_from": <DataProviderOptionInfo>{
                "display_name": "ID From",
                "short_desc": "Starting return ID for cursor pagination",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Pagination cursor: return records with `order_return_id` greater "
                    "than or equal to this value.",
            },
            "status_id": <DataProviderOptionInfo>{
                "display_name": "Status ID",
                "short_desc": "Filter by return status",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Only return returns in the given status.",
                "ref_data": "order_return_statuses",
            },
            "get_unconfirmed": <DataProviderOptionInfo>{
                "display_name": "Include Unconfirmed Returns",
                "short_desc": "Return unconfirmed order returns too",
                "type": AbstractDataProviderType::get(SoftBoolOrNothingType),
                "desc": "When `True`, unconfirmed returns are included.",
                "preselected": True,
            },
            "include_custom_extra_fields": <DataProviderOptionInfo>{
                "display_name": "Include Custom Extra Fields",
                "short_desc": "Include custom extra fields",
                "type": AbstractDataProviderType::get(SoftBoolOrNothingType),
                "desc": "When `True`, custom extra fields are included in the response.",
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

    string getName() { return "order-returns"; }
    *string getDesc() { return ProviderInfo.desc; }

    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return (new BaseLinkerOrderReturnDataType()).getFields();
    }

    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        list<auto> records = ();
        int remaining = search_options.limit ? search_options.limit.toInt() : 10000;
        *softint id_from = search_options.id_from;

        hash<auto> base_params = {};
        if (exists search_options.order_return_id) {
            base_params.order_return_id = int(search_options.order_return_id);
        }
        if (exists search_options.date_confirmed_from) {
            base_params.date_confirmed_from = int(search_options.date_confirmed_from);
        }
        if (exists search_options.date_from) {
            base_params.date_from = int(search_options.date_from);
        }
        if (exists search_options.status_id) {
            base_params.status_id = int(search_options.status_id);
        }
        if (exists search_options.get_unconfirmed) {
            base_params.get_unconfirmed = boolean(search_options.get_unconfirmed);
        }
        if (exists search_options.include_custom_extra_fields) {
            base_params.include_custom_extra_fields =
                boolean(search_options.include_custom_extra_fields);
        }

        # Single-return fast path
        if (exists search_options.order_return_id) {
            hash<auto> response = callMethod("getOrderReturns", base_params);
            records = response.returns ?? ();
            return new DefaultRecordIterator(new ListHashIterator(records), where_cond,
                search_options, (new BaseLinkerOrderReturnDataType()).getFields());
        }

        while (remaining > 0) {
            hash<auto> params = base_params;
            if (exists id_from) {
                params.id_from = id_from;
            }
            hash<auto> response = callMethod("getOrderReturns", params);
            list<auto> page = response.returns ?? ();
            if (!page) {
                break;
            }
            foreach hash<auto> ret in (page) {
                records += ret;
                --remaining;
                if (!remaining) {
                    break;
                }
            }
            if (page.size() < OrderReturnsPageSize) {
                break;
            }
            id_from = int(page[page.size() - 1].order_return_id) + 1;
        }

        return new DefaultRecordIterator(new ListHashIterator(records), where_cond,
            search_options, (new BaseLinkerOrderReturnDataType()).getFields());
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BaseLinkerDataProvider
