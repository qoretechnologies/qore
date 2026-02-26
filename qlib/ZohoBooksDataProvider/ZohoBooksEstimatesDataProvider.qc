# -*- mode: qore; indent-tabs-mode: nil -*-
/*  ZohoBooksEstimatesDataProvider.qc Copyright 2026 Qore Technologies, s.r.o. - MIT License */

public namespace ZohoBooksDataProvider {

public class ZohoBooksEstimatesDataProvider inherits ZohoBooksDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "estimates",
            "desc": "Zoho Books estimates",
            "type": "ZohoBooksEstimatesDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "has_record": True,
            "search_options": SearchOptions,
        };

        const SearchOptions = {
            "organization_id": <DataProviderOptionInfo>{
                "display_name": "Organization ID",
                "short_desc": "Zoho Books organization ID",
                "type": AbstractDataProviderType::get(SoftStringType),
                "desc": "The organization ID (required for all Zoho Books API calls)",
                "required": True,
            },
        };
    }
    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest) : ZohoBooksDataProviderBase(rest) {}
    string getName() { return "estimates"; }
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) { return new ZohoBooksEstimateDataType().getFields(); }
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond, *hash<auto> search_options) {
        hash<auto> params;
        if (search_options.organization_id) {
            params.organization_id = search_options.organization_id;
        }
        hash<auto> response = doGet("/estimates", params);
        return new DefaultRecordIterator(new ListHashIterator(response.estimates ?? ()), where_cond, search_options);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

public class ZohoBooksCreateEstimateDataProvider inherits ZohoBooksDataProviderBase {
    public { const ProviderInfo = <DataProviderInfo>{"name": "create-estimate", "desc": "Creates an estimate", "type": "ZohoBooksCreateEstimateDataProvider", "supports_request": True}; }
    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest) : ZohoBooksDataProviderBase(rest) {}
    string getName() { return "create-estimate"; }
    private *AbstractDataProviderType getRequestTypeImpl() { return AbstractDataProviderTypeMap."hash"; }
    private *AbstractDataProviderType getResponseTypeImpl() { return new ZohoBooksEstimateDataType(); }
    private auto doRequestImpl(auto req, *hash<auto> request_options) { return doPost("/estimates", req).estimate; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

public class ZohoBooksUpdateEstimateDataProvider inherits ZohoBooksDataProviderBase {
    public { const ProviderInfo = <DataProviderInfo>{"name": "update-estimate", "desc": "Updates an estimate", "type": "ZohoBooksUpdateEstimateDataProvider", "supports_request": True}; }
    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest) : ZohoBooksDataProviderBase(rest) {}
    string getName() { return "update-estimate"; }
    private *AbstractDataProviderType getRequestTypeImpl() { return AbstractDataProviderTypeMap."hash"; }
    private *AbstractDataProviderType getResponseTypeImpl() { return new ZohoBooksEstimateDataType(); }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string id = req.estimate_id ?? request_options.estimate_id;
        if (!id) throw "ZOHOBOOKS-ERROR", "estimate_id required";
        return doPut(sprintf("/estimates/%s", id), req).estimate;
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

public class ZohoBooksGetEstimateDataProvider inherits ZohoBooksDataProviderBase {
    public { const ProviderInfo = <DataProviderInfo>{"name": "get-estimate", "desc": "Gets an estimate", "type": "ZohoBooksGetEstimateDataProvider", "supports_request": True}; }
    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest) : ZohoBooksDataProviderBase(rest) {}
    string getName() { return "get-estimate"; }
    private *AbstractDataProviderType getRequestTypeImpl() { return AbstractDataProviderTypeMap."hash"; }
    private *AbstractDataProviderType getResponseTypeImpl() { return new ZohoBooksEstimateDataType(); }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string id = req.estimate_id ?? request_options.estimate_id;
        if (!id) throw "ZOHOBOOKS-ERROR", "estimate_id required";
        return doGet(sprintf("/estimates/%s", id)).estimate;
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

}
