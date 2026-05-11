# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

public class KatanaCreateSalesOrderAddressDataProvider inherits KatanaDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new sales order address in Katana",
            "type": "KatanaCreateSalesOrderAddressDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
    }
    constructor(RestClientIo::RestClientIo rest) : KatanaDataProviderBase(rest) {}
    string getName() { return "create"; }
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new KatanaCreateSalesOrderAddressRequestDataType();
    }
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new KatanaSalesOrderAddressDataType();
    }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("sales_order_addresses", req);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace KatanaDataProvider
