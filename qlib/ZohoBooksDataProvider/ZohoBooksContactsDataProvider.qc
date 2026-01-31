# -*- mode: qore; indent-tabs-mode: nil -*-
/*  ZohoBooksContactsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o. - MIT License */

public namespace ZohoBooksDataProvider {

public class ZohoBooksContactsDataProvider inherits ZohoBooksDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "contacts",
            "desc": "Zoho Books contacts",
            "type": "ZohoBooksContactsDataProvider",
            "supports_read": True,
            "supports_native_search": True,
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
            "contact_type": <DataProviderOptionInfo>{
                "display_name": "Contact Type",
                "short_desc": "Filter by contact type",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter contacts by type (customer, vendor)",
            },
        };
    }

    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest) : ZohoBooksDataProviderBase(rest) {}
    string getName() { return "contacts"; }
    private *AbstractDataProviderType getRecordTypeImpl() { return new ZohoBooksContactDataType(); }

    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond, *hash<auto> search_options) {
        hash<auto> params;
        if (search_options.organization_id) {
            params.organization_id = search_options.organization_id;
        }
        if (search_options.contact_type) {
            params.contact_type = search_options.contact_type;
        }
        hash<auto> response = doGet("/contacts", params);
        return new DefaultRecordIterator(new ListHashIterator(response.contacts ?? ()), where_cond, search_options);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

public class ZohoBooksCreateContactDataProvider inherits ZohoBooksDataProviderBase {
    public { const ProviderInfo = <DataProviderInfo>{"name": "create-contact", "desc": "Creates a contact", "type": "ZohoBooksCreateContactDataProvider", "supports_request": True}; }
    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest) : ZohoBooksDataProviderBase(rest) {}
    string getName() { return "create-contact"; }
    private *AbstractDataProviderType getRequestTypeImpl() { return AbstractDataProviderTypeMap."hash"; }
    private *AbstractDataProviderType getResponseTypeImpl() { return AbstractDataProviderTypeMap."hash"; }
    private auto doRequestImpl(auto req, *hash<auto> request_options) { return doPost("/contacts", req).contact; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

public class ZohoBooksUpdateContactDataProvider inherits ZohoBooksDataProviderBase {
    public { const ProviderInfo = <DataProviderInfo>{"name": "update-contact", "desc": "Updates a contact", "type": "ZohoBooksUpdateContactDataProvider", "supports_request": True}; }
    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest) : ZohoBooksDataProviderBase(rest) {}
    string getName() { return "update-contact"; }
    private *AbstractDataProviderType getRequestTypeImpl() { return AbstractDataProviderTypeMap."hash"; }
    private *AbstractDataProviderType getResponseTypeImpl() { return AbstractDataProviderTypeMap."hash"; }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string id = req.contact_id ?? request_options.contact_id;
        if (!id) throw "ZOHOBOOKS-ERROR", "contact_id required";
        return doPut(sprintf("/contacts/%s", id), req).contact;
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

public class ZohoBooksGetContactDataProvider inherits ZohoBooksDataProviderBase {
    public { const ProviderInfo = <DataProviderInfo>{"name": "get-contact", "desc": "Gets a contact", "type": "ZohoBooksGetContactDataProvider", "supports_request": True}; }
    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest) : ZohoBooksDataProviderBase(rest) {}
    string getName() { return "get-contact"; }
    private *AbstractDataProviderType getRequestTypeImpl() { return AbstractDataProviderTypeMap."hash"; }
    private *AbstractDataProviderType getResponseTypeImpl() { return new ZohoBooksContactDataType(); }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string id = req.contact_id ?? request_options.contact_id;
        if (!id) throw "ZOHOBOOKS-ERROR", "contact_id required";
        return doGet(sprintf("/contacts/%s", id)).contact;
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

}
