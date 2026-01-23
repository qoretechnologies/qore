# -*- mode: qore; indent-tabs-mode: nil -*-
/*  ZohoBooksItemsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o. - MIT License */

public namespace ZohoBooksDataProvider {

public class ZohoBooksItemsDataProvider inherits ZohoBooksDataProviderBase {
    public { const ProviderInfo = <DataProviderInfo>{"name": "items", "desc": "Zoho Books items", "type": "ZohoBooksItemsDataProvider", "supports_read": True, "supports_native_search": True}; }
    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest) : ZohoBooksDataProviderBase(rest) {}
    string getName() { return "items"; }
    private *AbstractDataProviderType getRecordTypeImpl() { return AbstractDataProviderTypeMap."hash"; }
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond, *hash<auto> search_options) {
        hash<auto> response = doGet("/items", search_options);
        return new DefaultRecordIterator(new ListHashIterator(response.items ?? ()), where_cond, search_options);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

public class ZohoBooksCreateItemDataProvider inherits ZohoBooksDataProviderBase {
    public { const ProviderInfo = <DataProviderInfo>{"name": "create-item", "desc": "Creates an item", "type": "ZohoBooksCreateItemDataProvider", "supports_request": True}; }
    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest) : ZohoBooksDataProviderBase(rest) {}
    string getName() { return "create-item"; }
    private *AbstractDataProviderType getRequestTypeImpl() { return AbstractDataProviderTypeMap."hash"; }
    private *AbstractDataProviderType getResponseTypeImpl() { return AbstractDataProviderTypeMap."hash"; }
    private auto doRequestImpl(auto req, *hash<auto> request_options) { return doPost("/items", req).item; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

}
