# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerOrderStatusesDataProvider.qc BaseLinker order statuses collection
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

#! Contains all public definitions in the BaseLinkerDataProvider module
public namespace BaseLinkerDataProvider {

#! BaseLinker order statuses data provider (`getOrderStatusList`)
public class BaseLinkerOrderStatusesDataProvider inherits BaseLinkerDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "order-statuses",
            "desc": "List BaseLinker order statuses (`getOrderStatusList`).",
            "type": "BaseLinkerOrderStatusesDataProvider",
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

    string getName() { return "order-statuses"; }
    *string getDesc() { return ProviderInfo.desc; }

    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return (new BaseLinkerOrderStatusDataType()).getFields();
    }

    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        hash<auto> response = callMethod("getOrderStatusList", NOTHING);
        list<auto> records = response.statuses ?? ();
        return new DefaultRecordIterator(new ListHashIterator(records), where_cond,
            search_options, (new BaseLinkerOrderStatusDataType()).getFields());
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BaseLinkerDataProvider
