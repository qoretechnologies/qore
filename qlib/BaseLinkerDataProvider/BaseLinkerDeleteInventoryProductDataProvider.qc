# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerDeleteInventoryProductDataProvider.qc BaseLinker deleteInventoryProduct action
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

#! Contains all public definitions in the BaseLinkerDataProvider module
public namespace BaseLinkerDataProvider {

#! BaseLinker `deleteInventoryProduct` action data provider
public class BaseLinkerDeleteInventoryProductDataProvider inherits BaseLinkerDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete-inventory-product",
            "desc": "Delete an inventory product (`deleteInventoryProduct`).",
            "type": "BaseLinkerDeleteInventoryProductDataProvider",
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

    string getName() { return "delete-inventory-product"; }
    *string getDesc() { return ProviderInfo.desc; }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new BaseLinkerDeleteInventoryProductRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new BaseLinkerSuccessIdResponseDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        # inventory_id is only used to drive the cascading product_id dropdown;
        # the BaseLinker API only accepts product_id
        return callMethod("deleteInventoryProduct", {"product_id": req.product_id});
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BaseLinkerDataProvider
