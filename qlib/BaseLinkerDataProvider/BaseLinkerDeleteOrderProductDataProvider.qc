# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerDeleteOrderProductDataProvider.qc BaseLinker deleteOrderProduct action
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

#! Contains all public definitions in the BaseLinkerDataProvider module
public namespace BaseLinkerDataProvider {

#! BaseLinker `deleteOrderProduct` action data provider
public class BaseLinkerDeleteOrderProductDataProvider inherits BaseLinkerDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete-order-product",
            "desc": "Remove a line item from an order (`deleteOrderProduct`).",
            "type": "BaseLinkerDeleteOrderProductDataProvider",
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

    string getName() { return "delete-order-product"; }
    *string getDesc() { return ProviderInfo.desc; }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new BaseLinkerDeleteOrderProductRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new BaseLinkerSuccessIdResponseDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return callMethod("deleteOrderProduct", req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BaseLinkerDataProvider
