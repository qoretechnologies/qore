# -*- mode: qore; indent-tabs-mode: nil -*-
/*  InsightlyCreateOpportunityDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License - see COPYING for details
*/

public namespace InsightlyDataProvider {

public class InsightlyCreateOpportunityDataProvider inherits InsightlyDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new opportunity in Insightly CRM",
            "type": "InsightlyCreateOpportunityDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(RestClientIo::RestClientIo rest) : InsightlyDataProviderBase(rest) {
    }

    string getName() { return "create"; }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new InsightlyCreateOpportunityRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new InsightlyOpportunityDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("Opportunities", req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace InsightlyDataProvider
