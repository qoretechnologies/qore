# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerCourierPackagesDataProvider.qc BaseLinker courier packages collection
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

#! Contains all public definitions in the BaseLinkerDataProvider module
public namespace BaseLinkerDataProvider {

#! BaseLinker courier packages collection data provider (`getOrderPackages`)
/** Returns all packages associated with a single order; no pagination. */
public class BaseLinkerCourierPackagesDataProvider inherits BaseLinkerDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "order-packages",
            "desc": "List courier packages (shipments) associated with an order "
                "(`getOrderPackages`).",
            "type": "BaseLinkerCourierPackagesDataProvider",
            "has_record": True,
            "supports_read": True,
            "search_options": SearchOptions,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        const SearchOptions = {
            "order_id": <DataProviderOptionInfo>{
                "display_name": "Order ID",
                "short_desc": "BaseLinker order identifier",
                "type": AbstractDataProviderType::get(SoftIntType),
                "desc": "The order whose packages should be listed.",
                "required": True,
                "ref_data": "orders",
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

    string getName() { return "order-packages"; }
    *string getDesc() { return ProviderInfo.desc; }

    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return (new BaseLinkerCourierPackageDataType()).getFields();
    }

    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        if (!exists search_options.order_id) {
            throw "BASELINKER-SEARCH-ERROR", "`order_id` search option is required";
        }
        hash<auto> response = callMethod("getOrderPackages", {
            "order_id": int(search_options.order_id),
        });
        list<auto> records = response.packages ?? ();
        return new DefaultRecordIterator(new ListHashIterator(records), where_cond,
            search_options, (new BaseLinkerCourierPackageDataType()).getFields());
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BaseLinkerDataProvider
