# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

public class KatanaUpdateCustomerDataProvider inherits KatanaDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Update an existing customer in Katana",
            "type": "KatanaUpdateCustomerDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
    }
    constructor(RestClientIo::RestClientIo rest) : KatanaDataProviderBase(rest) {}
    string getName() { return "update"; }
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new KatanaUpdateCustomerRequestDataType();
    }
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new KatanaCustomerDataType();
    }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = req;
        int id = body.id;
        delete body.id;
        return doPatch(sprintf("customers/%d", id), body);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace KatanaDataProvider
