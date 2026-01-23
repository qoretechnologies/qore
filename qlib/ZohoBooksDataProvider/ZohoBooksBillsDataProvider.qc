# -*- mode: qore; indent-tabs-mode: nil -*-
/*  ZohoBooksBillsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o. - MIT License */

public namespace ZohoBooksDataProvider {

public class ZohoBooksBillsDataProvider inherits ZohoBooksDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "bills",
            "desc": "Zoho Books bills data provider",
            "type": "ZohoBooksBillsDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "constructor_options": BaseConstructorOptions,
        };
    }

    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest) : ZohoBooksDataProviderBase(rest) {
    }

    string getName() { return "bills"; }
    *string getDesc() { return "Zoho Books bills data provider for listing bills"; }

    private *AbstractDataProviderType getRecordTypeImpl() {
        return AbstractDataProviderTypeMap."hash";
    }

    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond, *hash<auto> search_options) {
        hash<auto> response = doGet("/bills", search_options);
        return new DefaultRecordIterator(new ListHashIterator(response.bills ?? ()), where_cond, search_options);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

public class ZohoBooksCreateBillDataProvider inherits ZohoBooksDataProviderBase {
    public { const ProviderInfo = <DataProviderInfo>{"name": "create-bill", "desc": "Creates a bill", "type": "ZohoBooksCreateBillDataProvider", "supports_request": True}; }
    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest) : ZohoBooksDataProviderBase(rest) {}
    string getName() { return "create-bill"; }
    private *AbstractDataProviderType getRequestTypeImpl() { return AbstractDataProviderTypeMap."hash"; }
    private *AbstractDataProviderType getResponseTypeImpl() { return AbstractDataProviderTypeMap."hash"; }
    private auto doRequestImpl(auto req, *hash<auto> request_options) { return doPost("/bills", req).bill; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

public class ZohoBooksUpdateBillDataProvider inherits ZohoBooksDataProviderBase {
    public { const ProviderInfo = <DataProviderInfo>{"name": "update-bill", "desc": "Updates a bill", "type": "ZohoBooksUpdateBillDataProvider", "supports_request": True}; }
    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest) : ZohoBooksDataProviderBase(rest) {}
    string getName() { return "update-bill"; }
    private *AbstractDataProviderType getRequestTypeImpl() { return AbstractDataProviderTypeMap."hash"; }
    private *AbstractDataProviderType getResponseTypeImpl() { return AbstractDataProviderTypeMap."hash"; }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string id = req.bill_id ?? request_options.bill_id;
        if (!id) throw "ZOHOBOOKS-ERROR", "bill_id required";
        return doPut(sprintf("/bills/%s", id), req).bill;
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

public class ZohoBooksDeleteBillDataProvider inherits ZohoBooksDataProviderBase {
    public { const ProviderInfo = <DataProviderInfo>{"name": "delete-bill", "desc": "Deletes a bill", "type": "ZohoBooksDeleteBillDataProvider", "supports_request": True}; }
    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest) : ZohoBooksDataProviderBase(rest) {}
    string getName() { return "delete-bill"; }
    private *AbstractDataProviderType getRequestTypeImpl() { return AbstractDataProviderTypeMap."hash"; }
    private *AbstractDataProviderType getResponseTypeImpl() { return AbstractDataProviderTypeMap."hash"; }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string id = req.bill_id ?? request_options.bill_id;
        if (!id) throw "ZOHOBOOKS-ERROR", "bill_id required";
        return doDelete(sprintf("/bills/%s", id));
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

public class ZohoBooksGetBillDataProvider inherits ZohoBooksDataProviderBase {
    public { const ProviderInfo = <DataProviderInfo>{"name": "get-bill", "desc": "Gets a bill", "type": "ZohoBooksGetBillDataProvider", "supports_request": True}; }
    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest) : ZohoBooksDataProviderBase(rest) {}
    string getName() { return "get-bill"; }
    private *AbstractDataProviderType getRequestTypeImpl() { return AbstractDataProviderTypeMap."hash"; }
    private *AbstractDataProviderType getResponseTypeImpl() { return AbstractDataProviderTypeMap."hash"; }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string id = req.bill_id ?? request_options.bill_id;
        if (!id) throw "ZOHOBOOKS-ERROR", "bill_id required";
        return doGet(sprintf("/bills/%s", id)).bill;
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

}
