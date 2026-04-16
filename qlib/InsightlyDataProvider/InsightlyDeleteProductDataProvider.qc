# -*- mode: qore; indent-tabs-mode: nil -*-
/*  InsightlyDeleteProductDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License - see COPYING for details
*/
public namespace InsightlyDataProvider {
public class InsightlyDeleteProductDataProvider inherits InsightlyDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete", "desc": "Delete a product from Insightly CRM",
            "type": "InsightlyDeleteProductDataProvider",
            "supports_request": True, "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
    }
    constructor(RestClientIo::RestClientIo rest) : InsightlyDataProviderBase(rest) {}
    string getName() { return "delete"; }
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new InsightlyDeleteProductRequestDataType();
    }
    private *AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderTypeMap."nothing";
    }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        doDelete("Products/" + string(req.PRODUCT_ID));
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}
} # namespace InsightlyDataProvider
