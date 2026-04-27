# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerCourierAccountsDataProvider.qc BaseLinker courier accounts collection
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

#! Contains all public definitions in the BaseLinkerDataProvider module
public namespace BaseLinkerDataProvider {

#! BaseLinker courier accounts data provider (`getCourierAccounts`)
public class BaseLinkerCourierAccountsDataProvider inherits BaseLinkerDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "courier-accounts",
            "desc": "List accounts configured for a specific courier (`getCourierAccounts`).",
            "type": "BaseLinkerCourierAccountsDataProvider",
            "has_record": True,
            "supports_read": True,
            "search_options": SearchOptions,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        const SearchOptions = {
            "courier_code": <DataProviderOptionInfo>{
                "display_name": "Courier Code",
                "short_desc": "BaseLinker courier code",
                "type": AbstractDataProviderType::get(SoftStringType),
                "desc": "The courier whose accounts should be listed.",
                "required": True,
                "ref_data": "couriers",
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

    string getName() { return "courier-accounts"; }
    *string getDesc() { return ProviderInfo.desc; }

    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return (new BaseLinkerCourierAccountDataType()).getFields();
    }

    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        if (!exists search_options.courier_code) {
            throw "BASELINKER-SEARCH-ERROR", "`courier_code` search option is required";
        }
        hash<auto> response = callMethod("getCourierAccounts", {
            "courier_code": search_options.courier_code,
        });
        list<auto> records = response.accounts ?? ();
        return new DefaultRecordIterator(new ListHashIterator(records), where_cond,
            search_options, (new BaseLinkerCourierAccountDataType()).getFields());
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BaseLinkerDataProvider
