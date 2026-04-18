# -*- mode: qore; indent-tabs-mode: nil -*-
/*  InsightlyUpdateOpportunityDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License - see COPYING for details
*/

public namespace InsightlyDataProvider {

public class InsightlyUpdateOpportunityDataProvider inherits InsightlyDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Update an existing opportunity in Insightly CRM",
            "type": "InsightlyUpdateOpportunityDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(RestClientIo::RestClientIo rest) : InsightlyDataProviderBase(rest) {
    }

    string getName() { return "update"; }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new InsightlyUpdateOpportunityRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new InsightlyOpportunityDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPut("Opportunities", req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace InsightlyDataProvider
