# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerInventoryPricesDataProvider.qc BaseLinker inventory prices collection
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

#! Contains all public definitions in the BaseLinkerDataProvider module
public namespace BaseLinkerDataProvider {

#! BaseLinker inventory prices collection data provider (`getInventoryProductsPrices`)
public class BaseLinkerInventoryPricesDataProvider inherits BaseLinkerDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "inventory-prices",
            "desc": "List BaseLinker inventory product prices "
                "(`getInventoryProductsPrices`). Page-number pagination.",
            "type": "BaseLinkerInventoryPricesDataProvider",
            "has_record": True,
            "supports_read": True,
            "supports_children": True,
            "children_can_support_apis": True,
            "search_options": SearchOptions,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        const ChildMap = {
            "update-inventory-prices":
                Class::forName("BaseLinkerDataProvider::BaseLinkerUpdateInventoryProductsPricesDataProvider"),
        };

        const SearchOptions = {
            "inventory_id": <DataProviderOptionInfo>{
                "display_name": "Inventory ID",
                "short_desc": "BaseLinker inventory identifier",
                "type": AbstractDataProviderType::get(SoftStringType),
                "desc": "The catalog to list prices from.",
                "required": True,
                "ref_data": "inventories",
            },
            "page": <DataProviderOptionInfo>{
                "display_name": "Page",
                "short_desc": "Starting page number",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Page number for pagination (starting at 1).",
                "preselected": True,
                "default_value": 1,
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum results across pages",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Overall maximum number of records returned.",
            },
        };
    }

    constructor(*RestClientIo::RestClientIo rest) : BaseLinkerDataProviderBase(rest) {
    }
    constructor(BaseLinkerRestClient::BaseLinkerRestConnection conn)
            : BaseLinkerDataProviderBase(conn) {
    }
    constructor(*hash<auto> options) : BaseLinkerDataProviderBase(options) {
    }

    string getName() { return "inventory-prices"; }
    *string getDesc() { return ProviderInfo.desc; }

    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return (new BaseLinkerInventoryPriceDataType()).getFields();
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

    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        if (!exists search_options.inventory_id) {
            throw "BASELINKER-SEARCH-ERROR", "`inventory_id` search option is required";
        }
        list<auto> records = ();
        int remaining = search_options.limit ? search_options.limit.toInt() : 1000000;
        int page = search_options.page ? search_options.page.toInt() : 1;

        while (remaining > 0) {
            hash<auto> response = callMethod("getInventoryProductsPrices", {
                "inventory_id": search_options.inventory_id,
                "page": page,
            });
            *hash<auto> products = response.products;
            if (!products) {
                break;
            }
            int page_count = 0;
            foreach string pid in (keys products) {
                auto price_map = products{pid};
                hash<auto> rec = {"product_id": pid};
                if (price_map.typeCode() == NT_HASH) {
                    rec += {"prices": price_map};
                }
                records += rec;
                ++page_count;
                --remaining;
                if (!remaining) {
                    break;
                }
            }
            if (page_count < InventoryProductsPageSize) {
                break;
            }
            ++page;
        }

        return new DefaultRecordIterator(new ListHashIterator(records), where_cond,
            search_options, (new BaseLinkerInventoryPriceDataType()).getFields());
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BaseLinkerDataProvider
