# -*- mode: qore; indent-tabs-mode: nil -*-
/*  ZohoBooksPaymentsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o. - MIT License */

public namespace ZohoBooksDataProvider {

public class ZohoBooksPaymentsDataProvider inherits ZohoBooksDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "payments",
            "desc": "Zoho Books payments",
            "type": "ZohoBooksPaymentsDataProvider",
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
    string getName() { return "payments"; }
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) { return new ZohoBooksPaymentDataType().getFields(); }
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond, *hash<auto> search_options) {
        hash<auto> params;
        if (search_options.organization_id) {
            params.organization_id = search_options.organization_id;
        }
        hash<auto> response = doGet("/customerpayments", params);
        return new DefaultRecordIterator(new ListHashIterator(response.customerpayments ?? ()), where_cond, search_options);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

public class ZohoBooksCreatePaymentDataProvider inherits ZohoBooksDataProviderBase {
    public { const ProviderInfo = <DataProviderInfo>{"name": "create-payment", "desc": "Creates a payment", "type": "ZohoBooksCreatePaymentDataProvider", "supports_request": True}; }
    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest) : ZohoBooksDataProviderBase(rest) {}
    string getName() { return "create-payment"; }
    private *AbstractDataProviderType getRequestTypeImpl() { return AbstractDataProviderTypeMap."hash"; }
    private *AbstractDataProviderType getResponseTypeImpl() { return new ZohoBooksPaymentDataType(); }
    private auto doRequestImpl(auto req, *hash<auto> request_options) { return doPost("/customerpayments", req).payment; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

public class ZohoBooksUpdatePaymentDataProvider inherits ZohoBooksDataProviderBase {
    public { const ProviderInfo = <DataProviderInfo>{"name": "update-payment", "desc": "Updates a payment", "type": "ZohoBooksUpdatePaymentDataProvider", "supports_request": True}; }
    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest) : ZohoBooksDataProviderBase(rest) {}
    string getName() { return "update-payment"; }
    private *AbstractDataProviderType getRequestTypeImpl() { return AbstractDataProviderTypeMap."hash"; }
    private *AbstractDataProviderType getResponseTypeImpl() { return new ZohoBooksPaymentDataType(); }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string id = req.payment_id ?? request_options.payment_id;
        if (!id) throw "ZOHOBOOKS-ERROR", "payment_id required";
        return doPut(sprintf("/customerpayments/%s", id), req).payment;
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

public class ZohoBooksDeletePaymentDataProvider inherits ZohoBooksDataProviderBase {
    public { const ProviderInfo = <DataProviderInfo>{"name": "delete-payment", "desc": "Deletes a payment", "type": "ZohoBooksDeletePaymentDataProvider", "supports_request": True}; }
    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest) : ZohoBooksDataProviderBase(rest) {}
    string getName() { return "delete-payment"; }
    private *AbstractDataProviderType getRequestTypeImpl() { return AbstractDataProviderTypeMap."hash"; }
    private *AbstractDataProviderType getResponseTypeImpl() { return AbstractDataProviderTypeMap."hash"; }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string id = req.payment_id ?? request_options.payment_id;
        if (!id) throw "ZOHOBOOKS-ERROR", "payment_id required";
        return doDelete(sprintf("/customerpayments/%s", id));
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

public class ZohoBooksGetPaymentDataProvider inherits ZohoBooksDataProviderBase {
    public { const ProviderInfo = <DataProviderInfo>{"name": "get-payment", "desc": "Gets a payment", "type": "ZohoBooksGetPaymentDataProvider", "supports_request": True}; }
    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest) : ZohoBooksDataProviderBase(rest) {}
    string getName() { return "get-payment"; }
    private *AbstractDataProviderType getRequestTypeImpl() { return AbstractDataProviderTypeMap."hash"; }
    private *AbstractDataProviderType getResponseTypeImpl() { return new ZohoBooksPaymentDataType(); }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string id = req.payment_id ?? request_options.payment_id;
        if (!id) throw "ZOHOBOOKS-ERROR", "payment_id required";
        return doGet(sprintf("/customerpayments/%s", id)).payment;
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

}
