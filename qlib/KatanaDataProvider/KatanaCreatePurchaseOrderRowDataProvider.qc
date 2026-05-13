# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

public class KatanaCreatePurchaseOrderRowDataProvider inherits KatanaDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new purchase order row in Katana",
            "type": "KatanaCreatePurchaseOrderRowDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
    }
    constructor(RestClientIo::RestClientIo rest) : KatanaDataProviderBase(rest) {}
    string getName() { return "create"; }
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new KatanaCreatePurchaseOrderRowRequestDataType();
    }
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new KatanaPurchaseOrderRowRecordDataType();
    }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("purchase_order_rows", req);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace KatanaDataProvider
