# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerCourierServicesDataProvider.qc BaseLinker courier services collection
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

#! Contains all public definitions in the BaseLinkerDataProvider module
public namespace BaseLinkerDataProvider {

#! BaseLinker courier services data provider (`getCourierServices`)
/** The API returns services as a flat hash `{service_id: service_name, ...}`;
    this provider flattens each entry into a `{service_id, service_name}` record.
*/
public class BaseLinkerCourierServicesDataProvider inherits BaseLinkerDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "courier-services",
            "desc": "List services offered by a specific courier (`getCourierServices`).",
            "type": "BaseLinkerCourierServicesDataProvider",
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
                "desc": "The courier whose services should be listed.",
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

    string getName() { return "courier-services"; }
    *string getDesc() { return ProviderInfo.desc; }

    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return (new BaseLinkerCourierServiceDataType()).getFields();
    }

    private AbstractDataProviderRecordIterator<hash<auto>> searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        if (!exists search_options.courier_code) {
            throw "BASELINKER-SEARCH-ERROR", "`courier_code` search option is required";
        }
        hash<auto> response = callMethod("getCourierServices", {
            "courier_code": search_options.courier_code,
        });
        list<auto> records = ();
        *hash<auto> services = response.services;
        foreach string sid in (keys services) {
            records += {
                "service_id": sid,
                "service_name": string(services{sid}),
            };
        }
        return new DefaultRecordIterator<hash<auto>>(new ListHashIterator(records), where_cond,
            search_options, (new BaseLinkerCourierServiceDataType()).getFields());
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BaseLinkerDataProvider
