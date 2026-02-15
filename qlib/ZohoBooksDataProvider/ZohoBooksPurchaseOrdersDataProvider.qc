# -*- mode: qore; indent-tabs-mode: nil -*-
/*  ZohoBooksPurchaseOrdersDataProvider.qc Copyright 2026 Qore Technologies, s.r.o. - MIT License */

public namespace ZohoBooksDataProvider {

public class ZohoBooksPurchaseOrdersDataProvider inherits ZohoBooksDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "purchaseorders",
            "desc": "Zoho Books purchase orders",
            "type": "ZohoBooksPurchaseOrdersDataProvider",
            "supports_read": True,
            "supports_native_search": True,
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
    string getName() { return "purchaseorders"; }
    private *AbstractDataProviderType getRecordTypeImpl() { return new ZohoBooksPurchaseOrderDataType(); }
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond, *hash<auto> search_options) {
        hash<auto> params;
        if (search_options.organization_id) {
            params.organization_id = search_options.organization_id;
        }
        hash<auto> response = doGet("/purchaseorders", params);
        return new DefaultRecordIterator(new ListHashIterator(response.purchaseorders ?? ()), where_cond, search_options);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

public class ZohoBooksGetPurchaseOrderDataProvider inherits ZohoBooksDataProviderBase {
    public { const ProviderInfo = <DataProviderInfo>{"name": "get-purchaseorder", "desc": "Gets a purchase order", "type": "ZohoBooksGetPurchaseOrderDataProvider", "supports_request": True}; }
    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest) : ZohoBooksDataProviderBase(rest) {}
    string getName() { return "get-purchaseorder"; }
    private *AbstractDataProviderType getRequestTypeImpl() { return AbstractDataProviderTypeMap."hash"; }
    private *AbstractDataProviderType getResponseTypeImpl() { return new ZohoBooksPurchaseOrderDataType(); }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string id = req.purchaseorder_id ?? request_options.purchaseorder_id;
        if (!id) throw "ZOHOBOOKS-ERROR", "purchaseorder_id required";
        return doGet(sprintf("/purchaseorders/%s", id)).purchaseorder;
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

}
