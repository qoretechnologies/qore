# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ClickFunnelsFormsDataProvider.qc ClickFunnels forms collection and actions

/*  ClickFunnelsFormsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

public namespace ClickFunnelsDataProvider {

public class ClickFunnelsFormsDataProvider inherits ClickFunnelsDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "forms", "desc": "ClickFunnels forms data provider",
            "type": "ClickFunnelsFormsDataProvider",
            "supports_read": True, "supports_native_search": True,
            "supports_children": True, "children_can_support_apis": True,
            "constructor_options": ConstructorOptions, "search_options": SearchOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        const SearchOptions = {
            "form_id": <DataProviderOptionInfo>{ "display_name": "Form ID", "short_desc": "Get a specific form by ID", "type": AbstractDataProviderType::get(SoftStringOrNothingType), "desc": "Filter by form ID for single record lookup" },
            "after": <DataProviderOptionInfo>{ "display_name": "After Cursor", "short_desc": "Pagination cursor", "type": AbstractDataProviderType::get(SoftStringOrNothingType), "desc": "The cursor value for fetching the next page of results" },
            "sort_order": <DataProviderOptionInfo>{ "display_name": "Sort Order", "short_desc": "Sort order for results", "type": AbstractDataProviderType::get(SoftStringOrNothingType), "desc": "Sort order for results", "allowed_values": (<AllowedValueInfo>{"value": "asc", "display_name": "Ascending"}, <AllowedValueInfo>{"value": "desc", "display_name": "Descending"}) },
            "limit": <DataProviderOptionInfo>{ "display_name": "Limit", "short_desc": "Maximum number of results", "type": AbstractDataProviderType::get(SoftIntOrNothingType), "desc": "Maximum number of results to return" },
        };
        const ChildMap = {
            "create": Class::forName("ClickFunnelsCreateFormDataProvider"),
            "update": Class::forName("ClickFunnelsUpdateFormDataProvider"),
            "delete": Class::forName("ClickFunnelsDeleteFormDataProvider"),
        };
    }

    constructor(ClickFunnelsRestClient::ClickFunnelsRestClientIo rest) : ClickFunnelsDataProviderBase(rest) {}
    string getName() { return "forms"; }
    *string getDesc() { return "ClickFunnels forms data provider"; }
    private *list<string> getChildProviderNamesImpl() { return keys ChildMap; }
    private *AbstractDataProvider getChildProviderImpl(string name) { *Class cls = ChildMap{name}; if (cls) { return cls.newObject(rest); } }
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) { return new ClickFunnelsFormDataType().getFields(); }
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond, *hash<auto> search_options) {
        if (search_options.form_id) {
            auto result = doGet("forms/" + search_options.form_id);
            return new DefaultRecordIterator(new ListHashIterator((result,)), where_cond, search_options);
        }
        hash<auto> params;
        if (search_options.after) { params.after = search_options.after; }
        if (search_options.sort_order) { params.sort_order = search_options.sort_order; }
        list<auto> records = fetchAllPages(getWorkspacePath("forms"), params, search_options.limit);
        return new DefaultRecordIterator(new ListHashIterator(records), where_cond, search_options);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

# --- Create Form ---
public class ClickFunnelsCreateFormRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "name": { "type": StringType, "display_name": "Name", "short_desc": "The form name", "desc": "The name of the form", "required": True, "example_value": "Contact Form" },
            "redirect_url": { "type": StringOrNothingType, "display_name": "Redirect URL", "short_desc": "The redirect URL", "desc": "The URL to redirect to after form submission", "example_value": "https://example.com/thank-you" },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class ClickFunnelsCreateFormDataProvider inherits ClickFunnelsDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{ "name": "create", "desc": "Create a form in ClickFunnels", "type": "ClickFunnelsCreateFormDataProvider", "supports_request": True, "constructor_options": ConstructorOptions };
        const ConstructorOptions = BaseConstructorOptions;
        static *AbstractDataProviderType ResponseType = new ClickFunnelsFormDataType();
        static *AbstractDataProviderType RequestType = new ClickFunnelsCreateFormRequestDataType();
    }
    constructor(ClickFunnelsRestClient::ClickFunnelsRestClientIo rest) : ClickFunnelsDataProviderBase(rest) {}
    string getName() { return "create"; }
    *string getDesc() { return "Create a form in ClickFunnels"; }
    private *AbstractDataProviderType getRequestTypeImpl() { return new ClickFunnelsCreateFormRequestDataType(); }
    private *AbstractDataProviderType getResponseTypeImpl() { return new ClickFunnelsFormDataType(); }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost(getWorkspacePath("forms"), {"form": req});
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

# --- Update Form ---
public class ClickFunnelsUpdateFormRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "form_id": { "type": StringType, "display_name": "Form ID", "short_desc": "The form to update", "desc": "The unique identifier of the form to update", "required": True, "ref_data": "forms" },
            "name": { "type": StringOrNothingType, "display_name": "Name", "short_desc": "The form name", "desc": "The new name for the form" },
            "redirect_url": { "type": StringOrNothingType, "display_name": "Redirect URL", "short_desc": "The redirect URL", "desc": "The new redirect URL" },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class ClickFunnelsUpdateFormDataProvider inherits ClickFunnelsDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{ "name": "update", "desc": "Update a form in ClickFunnels", "type": "ClickFunnelsUpdateFormDataProvider", "supports_request": True, "constructor_options": ConstructorOptions };
        const ConstructorOptions = BaseConstructorOptions;
        static *AbstractDataProviderType ResponseType = new ClickFunnelsFormDataType();
        static *AbstractDataProviderType RequestType = new ClickFunnelsUpdateFormRequestDataType();
    }
    constructor(ClickFunnelsRestClient::ClickFunnelsRestClientIo rest) : ClickFunnelsDataProviderBase(rest) {}
    string getName() { return "update"; }
    *string getDesc() { return "Update a form in ClickFunnels"; }
    private *AbstractDataProviderType getRequestTypeImpl() { return new ClickFunnelsUpdateFormRequestDataType(); }
    private *AbstractDataProviderType getResponseTypeImpl() { return new ClickFunnelsFormDataType(); }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string id = remove req.form_id;
        return doPut("forms/" + id, {"form": req});
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

# --- Delete Form ---
public class ClickFunnelsDeleteFormRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "form_id": { "type": StringType, "display_name": "Form ID", "short_desc": "The form to delete", "desc": "The unique identifier of the form to delete", "required": True, "ref_data": "forms" },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class ClickFunnelsDeleteFormDataProvider inherits ClickFunnelsDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{ "name": "delete", "desc": "Delete a form in ClickFunnels", "type": "ClickFunnelsDeleteFormDataProvider", "supports_request": True, "constructor_options": ConstructorOptions };
        const ConstructorOptions = BaseConstructorOptions;
    }
    constructor(ClickFunnelsRestClient::ClickFunnelsRestClientIo rest) : ClickFunnelsDataProviderBase(rest) {}
    string getName() { return "delete"; }
    *string getDesc() { return "Delete a form in ClickFunnels"; }
    private *AbstractDataProviderType getRequestTypeImpl() { return new ClickFunnelsDeleteFormRequestDataType(); }
    private *AbstractDataProviderType getResponseTypeImpl() { return AbstractDataProviderTypeMap."hash"; }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        doDelete("forms/" + req.form_id);
        return {"success": True, "id": req.form_id};
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace ClickFunnelsDataProvider
