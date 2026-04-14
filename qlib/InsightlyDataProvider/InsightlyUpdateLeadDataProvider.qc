# -*- mode: qore; indent-tabs-mode: nil -*-
/*  InsightlyUpdateLeadDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License - see COPYING for details
*/

public namespace InsightlyDataProvider {

public class InsightlyUpdateLeadDataProvider inherits InsightlyDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Update an existing lead in Insightly CRM",
            "type": "InsightlyUpdateLeadDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(RestClientIo::RestClientIo rest) : InsightlyDataProviderBase(rest) {
    }

    string getName() { return "update"; }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new InsightlyUpdateLeadRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new InsightlyLeadDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPut("Leads", req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace InsightlyDataProvider
