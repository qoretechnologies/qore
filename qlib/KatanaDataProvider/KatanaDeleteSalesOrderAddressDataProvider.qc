# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

public class KatanaDeleteSalesOrderAddressDataProvider inherits KatanaDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete",
            "desc": "Delete a sales order address from Katana",
            "type": "KatanaDeleteSalesOrderAddressDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
    }
    constructor(RestClientIo::RestClientIo rest) : KatanaDataProviderBase(rest) {}
    string getName() { return "delete"; }
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new KatanaDeleteSalesOrderAddressRequestDataType();
    }
    private *AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderTypeMap."nothing";
    }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        doDelete(sprintf("sales_order_addresses/%d", req.id));
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace KatanaDataProvider
