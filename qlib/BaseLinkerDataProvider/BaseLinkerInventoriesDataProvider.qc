# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerInventoriesDataProvider.qc BaseLinker inventories collection
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

#! Contains all public definitions in the BaseLinkerDataProvider module
public namespace BaseLinkerDataProvider {

#! BaseLinker inventories collection data provider (`getInventories`)
/** Returns the (small) list of configured inventories/catalogs. No pagination. */
public class BaseLinkerInventoriesDataProvider inherits BaseLinkerDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "inventories",
            "desc": "List BaseLinker inventories (catalogs) via `getInventories`.",
            "type": "BaseLinkerInventoriesDataProvider",
            "has_record": True,
            "supports_read": True,
            "search_options": SearchOptions,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        #! No filters; BaseLinker returns all inventories unconditionally
        const SearchOptions = {};
    }

    constructor(*RestClientIo::RestClientIo rest) : BaseLinkerDataProviderBase(rest) {
    }
    constructor(BaseLinkerRestClient::BaseLinkerRestConnection conn)
            : BaseLinkerDataProviderBase(conn) {
    }
    constructor(*hash<auto> options) : BaseLinkerDataProviderBase(options) {
    }

    string getName() { return "inventories"; }
    *string getDesc() { return ProviderInfo.desc; }

    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return (new BaseLinkerInventoryDataType()).getFields();
    }

    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        hash<auto> response = callMethod("getInventories", NOTHING);
        list<auto> records = response.inventories ?? ();
        return new DefaultRecordIterator(new ListHashIterator(records), where_cond,
            search_options, (new BaseLinkerInventoryDataType()).getFields());
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BaseLinkerDataProvider
