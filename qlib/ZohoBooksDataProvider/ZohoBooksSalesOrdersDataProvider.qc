# -*- mode: qore; indent-tabs-mode: nil -*-
/*  ZohoBooksSalesOrdersDataProvider.qc Copyright 2026 Qore Technologies, s.r.o. - MIT License */

public namespace ZohoBooksDataProvider {

public class ZohoBooksSalesOrdersDataProvider inherits ZohoBooksDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "salesorders",
            "desc": "Zoho Books sales orders",
            "type": "ZohoBooksSalesOrdersDataProvider",
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
    string getName() { return "salesorders"; }
    private *AbstractDataProviderType getRecordTypeImpl() { return new ZohoBooksSalesOrderDataType(); }
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond, *hash<auto> search_options) {
        hash<auto> params;
        if (search_options.organization_id) {
            params.organization_id = search_options.organization_id;
        }
        hash<auto> response = doGet("/salesorders", params);
        return new DefaultRecordIterator(new ListHashIterator(response.salesorders ?? ()), where_cond, search_options);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

public class ZohoBooksCreateSalesOrderDataProvider inherits ZohoBooksDataProviderBase {
    public { const ProviderInfo = <DataProviderInfo>{"name": "create-salesorder", "desc": "Creates a sales order", "type": "ZohoBooksCreateSalesOrderDataProvider", "supports_request": True}; }
    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest) : ZohoBooksDataProviderBase(rest) {}
    string getName() { return "create-salesorder"; }
    private *AbstractDataProviderType getRequestTypeImpl() { return AbstractDataProviderTypeMap."hash"; }
    private *AbstractDataProviderType getResponseTypeImpl() { return AbstractDataProviderTypeMap."hash"; }
    private auto doRequestImpl(auto req, *hash<auto> request_options) { return doPost("/salesorders", req).salesorder; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

public class ZohoBooksUpdateSalesOrderDataProvider inherits ZohoBooksDataProviderBase {
    public { const ProviderInfo = <DataProviderInfo>{"name": "update-salesorder", "desc": "Updates a sales order", "type": "ZohoBooksUpdateSalesOrderDataProvider", "supports_request": True}; }
    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest) : ZohoBooksDataProviderBase(rest) {}
    string getName() { return "update-salesorder"; }
    private *AbstractDataProviderType getRequestTypeImpl() { return AbstractDataProviderTypeMap."hash"; }
    private *AbstractDataProviderType getResponseTypeImpl() { return AbstractDataProviderTypeMap."hash"; }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string id = req.salesorder_id ?? request_options.salesorder_id;
        if (!id) throw "ZOHOBOOKS-ERROR", "salesorder_id required";
        return doPut(sprintf("/salesorders/%s", id), req).salesorder;
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

public class ZohoBooksDeleteSalesOrderDataProvider inherits ZohoBooksDataProviderBase {
    public { const ProviderInfo = <DataProviderInfo>{"name": "delete-salesorder", "desc": "Deletes a sales order", "type": "ZohoBooksDeleteSalesOrderDataProvider", "supports_request": True}; }
    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest) : ZohoBooksDataProviderBase(rest) {}
    string getName() { return "delete-salesorder"; }
    private *AbstractDataProviderType getRequestTypeImpl() { return AbstractDataProviderTypeMap."hash"; }
    private *AbstractDataProviderType getResponseTypeImpl() { return AbstractDataProviderTypeMap."hash"; }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string id = req.salesorder_id ?? request_options.salesorder_id;
        if (!id) throw "ZOHOBOOKS-ERROR", "salesorder_id required";
        return doDelete(sprintf("/salesorders/%s", id));
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

public class ZohoBooksGetSalesOrderDataProvider inherits ZohoBooksDataProviderBase {
    public { const ProviderInfo = <DataProviderInfo>{"name": "get-salesorder", "desc": "Gets a sales order", "type": "ZohoBooksGetSalesOrderDataProvider", "supports_request": True}; }
    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest) : ZohoBooksDataProviderBase(rest) {}
    string getName() { return "get-salesorder"; }
    private *AbstractDataProviderType getRequestTypeImpl() { return AbstractDataProviderTypeMap."hash"; }
    private *AbstractDataProviderType getResponseTypeImpl() { return new ZohoBooksSalesOrderDataType(); }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string id = req.salesorder_id ?? request_options.salesorder_id;
        if (!id) throw "ZOHOBOOKS-ERROR", "salesorder_id required";
        return doGet(sprintf("/salesorders/%s", id)).salesorder;
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

}
