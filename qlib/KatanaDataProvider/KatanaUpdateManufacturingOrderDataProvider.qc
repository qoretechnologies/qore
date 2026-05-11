# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

public class KatanaUpdateManufacturingOrderDataProvider inherits KatanaDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Update an existing manufacturingorder in Katana",
            "type": "KatanaUpdateManufacturingOrderDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
    }
    constructor(RestClientIo::RestClientIo rest) : KatanaDataProviderBase(rest) {}
    string getName() { return "update"; }
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new KatanaUpdateManufacturingOrderRequestDataType();
    }
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new KatanaManufacturingOrderDataType();
    }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = req;
        int id = body.id;
        delete body.id;
        return doPatch(sprintf("manufacturing_orders/%d", id), body);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace KatanaDataProvider
