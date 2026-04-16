# -*- mode: qore; indent-tabs-mode: nil -*-
/*  InsightlyUpdateProductDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License - see COPYING for details
*/
public namespace InsightlyDataProvider {
public class InsightlyUpdateProductDataProvider inherits InsightlyDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update", "desc": "Update an existing product in Insightly CRM",
            "type": "InsightlyUpdateProductDataProvider",
            "supports_request": True, "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
    }
    constructor(RestClientIo::RestClientIo rest) : InsightlyDataProviderBase(rest) {}
    string getName() { return "update"; }
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new InsightlyUpdateProductRequestDataType();
    }
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new InsightlyProductDataType();
    }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPut("Products", req);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}
} # namespace InsightlyDataProvider
