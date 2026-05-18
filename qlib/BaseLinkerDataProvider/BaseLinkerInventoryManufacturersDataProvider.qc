# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerInventoryManufacturersDataProvider.qc BaseLinker inventory manufacturers collection
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

#! Contains all public definitions in the BaseLinkerDataProvider module
public namespace BaseLinkerDataProvider {

#! BaseLinker inventory manufacturers data provider (`getInventoryManufacturers`)
public class BaseLinkerInventoryManufacturersDataProvider inherits BaseLinkerDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "inventory-manufacturers",
            "desc": "List BaseLinker inventory manufacturers (`getInventoryManufacturers`).",
            "type": "BaseLinkerInventoryManufacturersDataProvider",
            "has_record": True,
            "supports_read": True,
            "search_options": SearchOptions,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        const SearchOptions = {};
    }

    constructor(*RestClientIo::RestClientIo rest) : BaseLinkerDataProviderBase(rest) {
    }
    constructor(BaseLinkerRestClient::BaseLinkerRestConnection conn)
            : BaseLinkerDataProviderBase(conn) {
    }
    constructor(*hash<auto> options) : BaseLinkerDataProviderBase(options) {
    }

    string getName() { return "inventory-manufacturers"; }
    *string getDesc() { return ProviderInfo.desc; }

    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return (new BaseLinkerInventoryManufacturerDataType()).getFields();
    }

    private AbstractDataProviderRecordIterator<hash<auto>> searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        hash<auto> response = callMethod("getInventoryManufacturers", NOTHING);
        list<auto> records = response.manufacturers ?? ();
        return new DefaultRecordIterator<hash<auto>>(new ListHashIterator(records), where_cond,
            search_options, (new BaseLinkerInventoryManufacturerDataType()).getFields());
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BaseLinkerDataProvider
