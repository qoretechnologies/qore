# -*- mode: qore; indent-tabs-mode: nil -*-
/*  InsightlyDeleteOpportunityDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License - see COPYING for details
*/

public namespace InsightlyDataProvider {

public class InsightlyDeleteOpportunityDataProvider inherits InsightlyDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete",
            "desc": "Delete an opportunity from Insightly CRM",
            "type": "InsightlyDeleteOpportunityDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(RestClientIo::RestClientIo rest) : InsightlyDataProviderBase(rest) {
    }

    string getName() { return "delete"; }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new InsightlyDeleteOpportunityRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderTypeMap."nothing";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        doDelete("Opportunities/" + string(req.OPPORTUNITY_ID));
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace InsightlyDataProvider
