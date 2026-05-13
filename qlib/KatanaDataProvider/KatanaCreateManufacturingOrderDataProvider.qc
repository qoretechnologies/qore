# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

public class KatanaCreateManufacturingOrderDataProvider inherits KatanaDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new manufacturing order in Katana",
            "type": "KatanaCreateManufacturingOrderDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
    }
    constructor(RestClientIo::RestClientIo rest) : KatanaDataProviderBase(rest) {}
    string getName() { return "create"; }
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new KatanaCreateManufacturingOrderRequestDataType();
    }
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new KatanaManufacturingOrderDataType();
    }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("manufacturing_orders", req);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace KatanaDataProvider
