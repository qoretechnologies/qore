# -*- mode: qore; indent-tabs-mode: nil -*-
/*  ZohoBooksEstimatesDataProvider.qc Copyright 2026 Qore Technologies, s.r.o. - MIT License */

public namespace ZohoBooksDataProvider {

public class ZohoBooksEstimatesDataProvider inherits ZohoBooksDataProviderBase {
    public { const ProviderInfo = <DataProviderInfo>{"name": "estimates", "desc": "Zoho Books estimates", "type": "ZohoBooksEstimatesDataProvider", "supports_read": True, "supports_native_search": True}; }
    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest) : ZohoBooksDataProviderBase(rest) {}
    string getName() { return "estimates"; }
    private *AbstractDataProviderType getRecordTypeImpl() { return AbstractDataProviderTypeMap."hash"; }
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond, *hash<auto> search_options) {
        hash<auto> response = doGet("/estimates", search_options);
        return new DefaultRecordIterator(new ListHashIterator(response.estimates ?? ()), where_cond, search_options);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

public class ZohoBooksCreateEstimateDataProvider inherits ZohoBooksDataProviderBase {
    public { const ProviderInfo = <DataProviderInfo>{"name": "create-estimate", "desc": "Creates an estimate", "type": "ZohoBooksCreateEstimateDataProvider", "supports_request": True}; }
    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest) : ZohoBooksDataProviderBase(rest) {}
    string getName() { return "create-estimate"; }
    private *AbstractDataProviderType getRequestTypeImpl() { return AbstractDataProviderTypeMap."hash"; }
    private *AbstractDataProviderType getResponseTypeImpl() { return AbstractDataProviderTypeMap."hash"; }
    private auto doRequestImpl(auto req, *hash<auto> request_options) { return doPost("/estimates", req).estimate; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

public class ZohoBooksUpdateEstimateDataProvider inherits ZohoBooksDataProviderBase {
    public { const ProviderInfo = <DataProviderInfo>{"name": "update-estimate", "desc": "Updates an estimate", "type": "ZohoBooksUpdateEstimateDataProvider", "supports_request": True}; }
    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest) : ZohoBooksDataProviderBase(rest) {}
    string getName() { return "update-estimate"; }
    private *AbstractDataProviderType getRequestTypeImpl() { return AbstractDataProviderTypeMap."hash"; }
    private *AbstractDataProviderType getResponseTypeImpl() { return AbstractDataProviderTypeMap."hash"; }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string id = req.estimate_id ?? request_options.estimate_id;
        if (!id) throw "ZOHOBOOKS-ERROR", "estimate_id required";
        return doPut(sprintf("/estimates/%s", id), req).estimate;
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

}
