# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerUpdateInventoryProductsPricesDataProvider.qc bulk price update
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

#! Contains all public definitions in the BaseLinkerDataProvider module
public namespace BaseLinkerDataProvider {

#! BaseLinker `updateInventoryProductsPrices` bulk action data provider
/** Accepts up to 1000 product-price updates per call (BaseLinker limit). */
public class BaseLinkerUpdateInventoryProductsPricesDataProvider
        inherits BaseLinkerDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update-inventory-prices",
            "desc": "Bulk-update inventory product prices "
                "(`updateInventoryProductsPrices`). Accepts up to 1000 entries per call.",
            "type": "BaseLinkerUpdateInventoryProductsPricesDataProvider",
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

    string getName() { return "update-inventory-prices"; }
    *string getDesc() { return ProviderInfo.desc; }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new BaseLinkerUpdateInventoryProductsPricesRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new BaseLinkerSuccessIdResponseDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return callMethod("updateInventoryProductsPrices", req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BaseLinkerDataProvider
