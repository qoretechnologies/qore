# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerInventoryStockDataProvider.qc BaseLinker inventory stock collection
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

#! Contains all public definitions in the BaseLinkerDataProvider module
public namespace BaseLinkerDataProvider {

#! BaseLinker inventory stock collection data provider (`getInventoryProductsStock`)
public class BaseLinkerInventoryStockDataProvider inherits BaseLinkerDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "inventory-stock",
            "desc": "List BaseLinker inventory product stock levels "
                "(`getInventoryProductsStock`). Page-number pagination.",
            "type": "BaseLinkerInventoryStockDataProvider",
            "has_record": True,
            "supports_read": True,
            "supports_children": True,
            "children_can_support_apis": True,
            "search_options": SearchOptions,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        const ChildMap = {
            "update-inventory-stock":
                Class::forName("BaseLinkerDataProvider::BaseLinkerUpdateInventoryProductsStockDataProvider"),
        };

        const SearchOptions = {
            "inventory_id": <DataProviderOptionInfo>{
                "display_name": "Inventory ID",
                "short_desc": "BaseLinker inventory identifier",
                "type": AbstractDataProviderType::get(SoftStringType),
                "desc": "The catalog to list stock levels from.",
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

    string getName() { return "inventory-stock"; }
    *string getDesc() { return ProviderInfo.desc; }

    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return (new BaseLinkerInventoryStockDataType()).getFields();
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
            hash<auto> response = callMethod("getInventoryProductsStock", {
                "inventory_id": search_options.inventory_id,
                "page": page,
            });
            *hash<auto> products = response.products;
            if (!products) {
                break;
            }
            int page_count = 0;
            foreach string pid in (keys products) {
                auto stock_map = products{pid};
                hash<auto> rec = {"product_id": pid};
                if (stock_map.typeCode() == NT_HASH) {
                    rec += {"stock": stock_map};
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
            search_options, (new BaseLinkerInventoryStockDataType()).getFields());
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BaseLinkerDataProvider
