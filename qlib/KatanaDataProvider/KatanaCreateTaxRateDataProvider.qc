# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

public class KatanaCreateTaxRateDataProvider inherits KatanaDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new tax rate in Katana",
            "type": "KatanaCreateTaxRateDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
    }
    constructor(RestClientIo::RestClientIo rest) : KatanaDataProviderBase(rest) {}
    string getName() { return "create"; }
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new KatanaCreateTaxRateRequestDataType();
    }
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new KatanaTaxRateDataType();
    }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("tax_rates", req);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace KatanaDataProvider
