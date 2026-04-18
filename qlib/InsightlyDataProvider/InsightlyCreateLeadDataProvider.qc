# -*- mode: qore; indent-tabs-mode: nil -*-
/*  InsightlyCreateLeadDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License - see COPYING for details
*/

public namespace InsightlyDataProvider {

public class InsightlyCreateLeadDataProvider inherits InsightlyDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new lead in Insightly CRM",
            "type": "InsightlyCreateLeadDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(RestClientIo::RestClientIo rest) : InsightlyDataProviderBase(rest) {
    }

    string getName() { return "create"; }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new InsightlyCreateLeadRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new InsightlyLeadDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("Leads", req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace InsightlyDataProvider
