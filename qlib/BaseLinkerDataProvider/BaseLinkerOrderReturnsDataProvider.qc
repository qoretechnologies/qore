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
                "pagination via `id_from` keyed on `return_id`.",
            "type": "BaseLinkerOrderReturnsDataProvider",
            "has_record": True,
            "supports_read": True,
            "supports_native_search": True,
            "supports_children": True,
            "children_can_support_apis": True,
            "search_options": SearchOptions,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Action-provider children reachable via \c getChildProvider()
        const ChildMap = {
            "add-order-return":
                Class::forName("BaseLinkerDataProvider::BaseLinkerAddOrderReturnDataProvider"),
            "set-order-return-fields":
                Class::forName("BaseLinkerDataProvider::BaseLinkerSetOrderReturnFieldsDataProvider"),
            "set-order-return-status":
                Class::forName("BaseLinkerDataProvider::BaseLinkerSetOrderReturnStatusDataProvider"),
            "set-order-return-refund":
                Class::forName("BaseLinkerDataProvider::BaseLinkerSetOrderReturnRefundDataProvider"),
        };

        #! Search options
        const SearchOptions = {
            "return_id": <DataProviderOptionInfo>{
                "display_name": "Return ID",
                "short_desc": "Fetch a single order return by ID",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "If supplied, only the matching return is returned.",
                "ref_data": "order_returns",
            },
            "order_id": <DataProviderOptionInfo>{
                "display_name": "Order ID",
                "short_desc": "Filter by the originating order",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Only returns created from the given BaseLinker order ID are returned.",
                "ref_data": "orders",
            },
            "date_from": <DataProviderOptionInfo>{
                "display_name": "Date From",
                "short_desc": "Filter by creation date/time",
                "type": AbstractDataProviderType::get(DateOrNothingType),
                "desc": "Return order returns created at or after this date/time. The date "
                    "is converted to Unix seconds before being passed to the BaseLinker API.",
            },
            "id_from": <DataProviderOptionInfo>{
                "display_name": "ID From",
                "short_desc": "Starting return ID for cursor pagination",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Pagination cursor: return records with `return_id` greater "
                    "than or equal to this value.",
            },
            "status_id": <DataProviderOptionInfo>{
                "display_name": "Status ID",
                "short_desc": "Filter by return status",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Only return returns in the given status.",
                "ref_data": "order_return_statuses",
            },
            "include_custom_extra_fields": <DataProviderOptionInfo>{
                "display_name": "Include Custom Extra Fields",
                "short_desc": "Include custom extra fields",
                "type": AbstractDataProviderType::get(SoftBoolOrNothingType),
                "desc": "When `True`, custom extra fields are included in the response.",
            },
            "filter_order_return_source": <DataProviderOptionInfo>{
                "display_name": "Filter Return Source",
                "short_desc": "Filter by return source channel",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Return records originating from a specific source channel.",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "ebay", "display_name": "eBay"},
                    <AllowedValueInfo>{"value": "amazon", "display_name": "Amazon"},
                    <AllowedValueInfo>{"value": "allegro", "display_name": "Allegro"},
                    <AllowedValueInfo>{"value": "shop", "display_name": "Shop"},
                    <AllowedValueInfo>{"value": "personal", "display_name": "Personal"},
                ),
            },
            "filter_order_return_source_id": <DataProviderOptionInfo>{
                "display_name": "Filter Return Source ID",
                "short_desc": "Filter by specific return source ID",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Return records from a specific source instance ID. Requires "
                    "`filter_order_return_source` to be set.",
            },
            "include_connect_data": <DataProviderOptionInfo>{
                "display_name": "Include Connect Data",
                "short_desc": "Include Base Connect / contractor data",
                "type": AbstractDataProviderType::get(SoftBoolOrNothingType),
                "desc": "When `True`, Base Connect and contractor data is included in "
                    "the response.",
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

    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) {
            return cls.newObject(rest);
        }
    }

    private AbstractDataProviderRecordIterator<hash<auto>> searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        list<auto> records = ();
        int remaining = search_options.limit ? search_options.limit.toInt() : 10000;
        *softint id_from = search_options.id_from;

        hash<auto> base_params = {};
        if (exists search_options.return_id) {
            base_params.return_id = int(search_options.return_id);
        }
        if (exists search_options.order_id) {
            base_params.order_id = int(search_options.order_id);
        }
        if (exists search_options.date_from) {
            base_params.date_from = int(get_epoch_seconds(search_options.date_from));
        }
        if (exists search_options.status_id) {
            base_params.status_id = int(search_options.status_id);
        }
        if (exists search_options.include_custom_extra_fields) {
            base_params.include_custom_extra_fields =
                boolean(search_options.include_custom_extra_fields);
        }
        if (exists search_options.filter_order_return_source) {
            base_params.filter_order_return_source =
                search_options.filter_order_return_source;
        }
        if (exists search_options.filter_order_return_source_id) {
            base_params.filter_order_return_source_id =
                int(search_options.filter_order_return_source_id);
        }
        if (exists search_options.include_connect_data) {
            base_params.include_connect_data = boolean(search_options.include_connect_data);
        }

        # Single-return fast path
        if (exists search_options.return_id) {
            hash<auto> response = callMethod("getOrderReturns", base_params);
            records = map convertEpochFields($1), (response.returns ?? ());
            return new DefaultRecordIterator<hash<auto>>(new ListHashIterator(records), where_cond,
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
                records += convertEpochFields(ret);
                --remaining;
                if (!remaining) {
                    break;
                }
            }
            if (page.size() < OrderReturnsPageSize) {
                break;
            }
            id_from = int(page[page.size() - 1].return_id) + 1;
        }

        return new DefaultRecordIterator<hash<auto>>(new ListHashIterator(records), where_cond,
            search_options, (new BaseLinkerOrderReturnDataType()).getFields());
    }

    #! Converts BaseLinker epoch-seconds fields in a record to absolute dates
    private static hash<auto> convertEpochFields(hash<auto> ret) {
        foreach string key in (("date_add", "date_in_status")) {
            if (exists ret{key} && ret{key}.typeCode() == NT_INT) {
                ret{key} = 1970-01-01Z + seconds(ret{key});
            }
        }
        return ret;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BaseLinkerDataProvider
