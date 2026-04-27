# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerDeleteOrdersDataProvider.qc BaseLinker delete order action
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

#! Contains all public definitions in the BaseLinkerDataProvider module
public namespace BaseLinkerDataProvider {

#! BaseLinker delete-order action data provider
/** Exposes the BaseLinker `deleteOrders` method; takes an array of `order_ids`
    (up to 1000 per call).
*/
public class BaseLinkerDeleteOrdersDataProvider inherits BaseLinkerDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete-order",
            "desc": "Delete one or more BaseLinker orders via `deleteOrders`.",
            "type": "BaseLinkerDeleteOrdersDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(*RestClientIo::RestClientIo rest) : BaseLinkerDataProviderBase(rest) {
    }
    constructor(BaseLinkerRestClient::BaseLinkerRestConnection conn)
            : BaseLinkerDataProviderBase(conn) {
    }
    constructor(*hash<auto> options) : BaseLinkerDataProviderBase(options) {
    }

    string getName() { return "delete-order"; }
    *string getDesc() { return ProviderInfo.desc; }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new BaseLinkerDeleteOrdersRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new BaseLinkerSuccessIdResponseDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return callMethod("deleteOrders", {"order_ids": req.order_ids});
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BaseLinkerDataProvider
