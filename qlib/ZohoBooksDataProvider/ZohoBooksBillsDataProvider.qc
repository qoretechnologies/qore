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
            "has_record": True,
            "constructor_options": BaseConstructorOptions,
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
            "status": <DataProviderOptionInfo>{
                "display_name": "Status",
                "short_desc": "Filter by bill status",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter bills by status",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "open", "desc": "Open bills"},
                    <AllowedValueInfo>{"value": "overdue", "desc": "Overdue bills"},
                    <AllowedValueInfo>{"value": "paid", "desc": "Paid bills"},
                    <AllowedValueInfo>{"value": "void", "desc": "Voided bills"},
                    <AllowedValueInfo>{"value": "partially_paid", "desc": "Partially paid bills"},
                ),
            },
            "vendor_id": <DataProviderOptionInfo>{
                "display_name": "Vendor ID",
                "short_desc": "Filter by vendor",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter bills by vendor ID",
            },
        };
    }

    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest) : ZohoBooksDataProviderBase(rest) {
    }

    string getName() { return "bills"; }
    *string getDesc() { return "Zoho Books bills data provider for listing bills"; }

    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new ZohoBooksBillDataType().getFields();
    }

    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond, *hash<auto> search_options) {
        hash<auto> params;
        if (search_options.organization_id) {
            params.organization_id = search_options.organization_id;
        }
        if (search_options.status) {
            params.status = search_options.status;
        }
        if (search_options.vendor_id) {
            params.vendor_id = search_options.vendor_id;
        }
        hash<auto> response = doGet("/bills", params);
        return new DefaultRecordIterator(new ListHashIterator(response.bills ?? ()), where_cond, search_options);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

public class ZohoBooksCreateBillDataProvider inherits ZohoBooksDataProviderBase {
    public { const ProviderInfo = <DataProviderInfo>{"name": "create-bill", "desc": "Creates a bill", "type": "ZohoBooksCreateBillDataProvider", "supports_request": True}; }
    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest) : ZohoBooksDataProviderBase(rest) {}
    string getName() { return "create-bill"; }
    private *AbstractDataProviderType getRequestTypeImpl() { return AbstractDataProviderTypeMap."hash"; }
    private *AbstractDataProviderType getResponseTypeImpl() { return new ZohoBooksBillDataType(); }
    private auto doRequestImpl(auto req, *hash<auto> request_options) { return doPost("/bills", req).bill; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

public class ZohoBooksUpdateBillDataProvider inherits ZohoBooksDataProviderBase {
    public { const ProviderInfo = <DataProviderInfo>{"name": "update-bill", "desc": "Updates a bill", "type": "ZohoBooksUpdateBillDataProvider", "supports_request": True}; }
    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest) : ZohoBooksDataProviderBase(rest) {}
    string getName() { return "update-bill"; }
    private *AbstractDataProviderType getRequestTypeImpl() { return AbstractDataProviderTypeMap."hash"; }
    private *AbstractDataProviderType getResponseTypeImpl() { return new ZohoBooksBillDataType(); }
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
    private *AbstractDataProviderType getResponseTypeImpl() { return new ZohoBooksBillDataType(); }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string id = req.bill_id ?? request_options.bill_id;
        if (!id) throw "ZOHOBOOKS-ERROR", "bill_id required";
        return doGet(sprintf("/bills/%s", id)).bill;
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

}
