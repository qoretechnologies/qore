# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerInventoryCategoriesDataProvider.qc BaseLinker inventory categories collection
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

#! Contains all public definitions in the BaseLinkerDataProvider module
public namespace BaseLinkerDataProvider {

#! BaseLinker inventory categories data provider (`getInventoryCategories`)
/** When `inventory_id` is supplied, only categories within that catalog are
    returned; otherwise categories across all catalogs are returned.
*/
public class BaseLinkerInventoryCategoriesDataProvider inherits BaseLinkerDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "inventory-categories",
            "desc": "List BaseLinker inventory categories (`getInventoryCategories`).",
            "type": "BaseLinkerInventoryCategoriesDataProvider",
            "has_record": True,
            "supports_read": True,
            "search_options": SearchOptions,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        const SearchOptions = {
            "inventory_id": <DataProviderOptionInfo>{
                "display_name": "Inventory ID",
                "short_desc": "BaseLinker inventory (catalog) identifier",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "When provided, only categories from the given catalog are "
                    "returned. When omitted, categories across all catalogs are returned.",
                "ref_data": "inventories",
                "preselected": True,
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

    string getName() { return "inventory-categories"; }
    *string getDesc() { return ProviderInfo.desc; }

    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return (new BaseLinkerInventoryCategoryDataType()).getFields();
    }

    private AbstractDataProviderRecordIterator<hash<auto>> searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        *hash<auto> params;
        if (exists search_options.inventory_id) {
            params = {"inventory_id": search_options.inventory_id};
        }
        hash<auto> response = callMethod("getInventoryCategories", params);
        list<auto> records = response.categories ?? ();
        return new DefaultRecordIterator<hash<auto>>(new ListHashIterator(records), where_cond,
            search_options, (new BaseLinkerInventoryCategoryDataType()).getFields());
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BaseLinkerDataProvider
