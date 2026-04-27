# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerUpdateInventoryProductsStockDataProvider.qc bulk stock update
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

#! Contains all public definitions in the BaseLinkerDataProvider module
public namespace BaseLinkerDataProvider {

#! BaseLinker `updateInventoryProductsStock` bulk action data provider
/** Accepts up to 1000 product-stock updates per call (BaseLinker limit). */
public class BaseLinkerUpdateInventoryProductsStockDataProvider
        inherits BaseLinkerDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update-inventory-stock",
            "desc": "Bulk-update inventory product stock levels "
                "(`updateInventoryProductsStock`). Accepts up to 1000 entries per call.",
            "type": "BaseLinkerUpdateInventoryProductsStockDataProvider",
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

    string getName() { return "update-inventory-stock"; }
    *string getDesc() { return ProviderInfo.desc; }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new BaseLinkerUpdateInventoryProductsStockRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new BaseLinkerSuccessIdResponseDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return callMethod("updateInventoryProductsStock", req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BaseLinkerDataProvider
