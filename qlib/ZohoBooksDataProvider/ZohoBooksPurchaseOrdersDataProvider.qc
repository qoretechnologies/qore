# -*- mode: qore; indent-tabs-mode: nil -*-
/*  ZohoBooksPurchaseOrdersDataProvider.qc Copyright 2026 Qore Technologies, s.r.o. - MIT License */

public namespace ZohoBooksDataProvider {

public class ZohoBooksPurchaseOrdersDataProvider inherits ZohoBooksDataProviderBase {
    public { const ProviderInfo = <DataProviderInfo>{"name": "purchaseorders", "desc": "Zoho Books purchase orders", "type": "ZohoBooksPurchaseOrdersDataProvider", "supports_read": True, "supports_native_search": True}; }
    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest) : ZohoBooksDataProviderBase(rest) {}
    string getName() { return "purchaseorders"; }
    private *AbstractDataProviderType getRecordTypeImpl() { return AbstractDataProviderTypeMap."hash"; }
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond, *hash<auto> search_options) {
        hash<auto> response = doGet("/purchaseorders", search_options);
        return new DefaultRecordIterator(new ListHashIterator(response.purchaseorders ?? ()), where_cond, search_options);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

}
