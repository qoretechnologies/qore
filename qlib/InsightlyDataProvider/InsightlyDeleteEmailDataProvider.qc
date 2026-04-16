# -*- mode: qore; indent-tabs-mode: nil -*-
/*  InsightlyDeleteEmailDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License - see COPYING for details
*/
public namespace InsightlyDataProvider {
public class InsightlyDeleteEmailDataProvider inherits InsightlyDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete", "desc": "Delete an email from Insightly CRM",
            "type": "InsightlyDeleteEmailDataProvider",
            "supports_request": True, "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
    }
    constructor(RestClientIo::RestClientIo rest) : InsightlyDataProviderBase(rest) {}
    string getName() { return "delete"; }
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new InsightlyDeleteEmailRequestDataType();
    }
    private *AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderTypeMap."nothing";
    }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        doDelete("Emails/" + string(req.EMAIL_ID));
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}
} # namespace InsightlyDataProvider
