# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BaseLinkerAddInventoryProductDataProvider.qc BaseLinker addInventoryProduct action
/*  Copyright (C) 2026 Qore Technologies, s.r.o. MIT licensed. */

#! Contains all public definitions in the BaseLinkerDataProvider module
public namespace BaseLinkerDataProvider {

#! BaseLinker `addInventoryProduct` action data provider (upsert)
/** A single action handles both create and update: omit `product_id` to
    create a new product; supply it to update the existing one. This matches
    BaseLinker's upsert shape for this method.
*/
public class BaseLinkerAddInventoryProductDataProvider inherits BaseLinkerDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "add-inventory-product",
            "desc": "Create or update an inventory product (`addInventoryProduct`). "
                "Omit `product_id` to create; supply it to update.",
            "type": "BaseLinkerAddInventoryProductDataProvider",
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

    string getName() { return "add-inventory-product"; }
    *string getDesc() { return ProviderInfo.desc; }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new BaseLinkerAddInventoryProductRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new BaseLinkerSuccessIdResponseDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return callMethod("addInventoryProduct", req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BaseLinkerDataProvider
