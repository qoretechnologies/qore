# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerCouriersDataProvider.qc BaseLinker couriers collection
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

#! Contains all public definitions in the BaseLinkerDataProvider module
public namespace BaseLinkerDataProvider {

#! BaseLinker couriers list data provider (`getCouriersList`)
public class BaseLinkerCouriersDataProvider inherits BaseLinkerDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "couriers",
            "desc": "List configured BaseLinker couriers (`getCouriersList`).",
            "type": "BaseLinkerCouriersDataProvider",
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

    string getName() { return "couriers"; }
    *string getDesc() { return ProviderInfo.desc; }

    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return (new BaseLinkerCourierDataType()).getFields();
    }

    private AbstractDataProviderRecordIterator<hash<auto>> searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        hash<auto> response = callMethod("getCouriersList", NOTHING);
        list<auto> records = response.couriers ?? ();
        return new DefaultRecordIterator<hash<auto>>(new ListHashIterator(records), where_cond,
            search_options, (new BaseLinkerCourierDataType()).getFields());
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BaseLinkerDataProvider
