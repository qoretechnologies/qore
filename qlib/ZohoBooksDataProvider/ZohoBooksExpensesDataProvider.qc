# -*- mode: qore; indent-tabs-mode: nil -*-
/*  ZohoBooksExpensesDataProvider.qc Copyright 2026 Qore Technologies, s.r.o. - MIT License */

public namespace ZohoBooksDataProvider {

public class ZohoBooksExpensesDataProvider inherits ZohoBooksDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "expenses",
            "desc": "Zoho Books expenses",
            "type": "ZohoBooksExpensesDataProvider",
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
    string getName() { return "expenses"; }
    private *AbstractDataProviderType getRecordTypeImpl() { return new ZohoBooksExpenseDataType(); }
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond, *hash<auto> search_options) {
        hash<auto> params;
        if (search_options.organization_id) {
            params.organization_id = search_options.organization_id;
        }
        hash<auto> response = doGet("/expenses", params);
        return new DefaultRecordIterator(new ListHashIterator(response.expenses ?? ()), where_cond, search_options);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

public class ZohoBooksCreateExpenseDataProvider inherits ZohoBooksDataProviderBase {
    public { const ProviderInfo = <DataProviderInfo>{"name": "create-expense", "desc": "Creates an expense", "type": "ZohoBooksCreateExpenseDataProvider", "supports_request": True}; }
    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest) : ZohoBooksDataProviderBase(rest) {}
    string getName() { return "create-expense"; }
    private *AbstractDataProviderType getRequestTypeImpl() { return AbstractDataProviderTypeMap."hash"; }
    private *AbstractDataProviderType getResponseTypeImpl() { return AbstractDataProviderTypeMap."hash"; }
    private auto doRequestImpl(auto req, *hash<auto> request_options) { return doPost("/expenses", req).expense; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

public class ZohoBooksUpdateExpenseDataProvider inherits ZohoBooksDataProviderBase {
    public { const ProviderInfo = <DataProviderInfo>{"name": "update-expense", "desc": "Updates an expense", "type": "ZohoBooksUpdateExpenseDataProvider", "supports_request": True}; }
    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest) : ZohoBooksDataProviderBase(rest) {}
    string getName() { return "update-expense"; }
    private *AbstractDataProviderType getRequestTypeImpl() { return AbstractDataProviderTypeMap."hash"; }
    private *AbstractDataProviderType getResponseTypeImpl() { return AbstractDataProviderTypeMap."hash"; }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string id = req.expense_id ?? request_options.expense_id;
        if (!id) throw "ZOHOBOOKS-ERROR", "expense_id required";
        return doPut(sprintf("/expenses/%s", id), req).expense;
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

public class ZohoBooksGetExpenseDataProvider inherits ZohoBooksDataProviderBase {
    public { const ProviderInfo = <DataProviderInfo>{"name": "get-expense", "desc": "Gets an expense", "type": "ZohoBooksGetExpenseDataProvider", "supports_request": True}; }
    constructor(ZohoBooksRestClient::ZohoBooksRestClient rest) : ZohoBooksDataProviderBase(rest) {}
    string getName() { return "get-expense"; }
    private *AbstractDataProviderType getRequestTypeImpl() { return AbstractDataProviderTypeMap."hash"; }
    private *AbstractDataProviderType getResponseTypeImpl() { return new ZohoBooksExpenseDataType(); }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string id = req.expense_id ?? request_options.expense_id;
        if (!id) throw "ZOHOBOOKS-ERROR", "expense_id required";
        return doGet(sprintf("/expenses/%s", id)).expense;
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

}
