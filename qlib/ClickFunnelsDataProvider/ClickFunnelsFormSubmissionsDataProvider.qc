# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ClickFunnelsFormSubmissionsDataProvider.qc ClickFunnels form submissions collection and actions

/*  ClickFunnelsFormSubmissionsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

public class ClickFunnelsFormSubmissionsDataProvider inherits ClickFunnelsDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "form-submissions", "desc": "ClickFunnels form submissions data provider",
            "type": "ClickFunnelsFormSubmissionsDataProvider",
            "supports_read": True, "supports_native_search": True,
            "supports_children": True, "children_can_support_apis": True,
            "constructor_options": ConstructorOptions, "search_options": SearchOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        const SearchOptions = ClickFunnelsDataProviderBase::WorkspaceIdSearchOption + {
            "form_id": <DataProviderOptionInfo>{ "display_name": "Form ID", "short_desc": "The form to list submissions for", "type": AbstractDataProviderType::get(SoftStringOrNothingType), "desc": "The form ID to list submissions for (required for listing)", "ref_data": "forms" },
            "submission_id": <DataProviderOptionInfo>{ "display_name": "Submission ID", "short_desc": "Get a specific submission", "type": AbstractDataProviderType::get(SoftStringOrNothingType), "desc": "Filter by submission ID for single record lookup", "ref_data": "form_submissions" },
            "after": <DataProviderOptionInfo>{ "display_name": "After Cursor", "short_desc": "Pagination cursor", "type": AbstractDataProviderType::get(SoftStringOrNothingType), "desc": "The cursor value for fetching the next page of results" },
            "sort_order": <DataProviderOptionInfo>{ "display_name": "Sort Order", "short_desc": "Sort order for results", "type": AbstractDataProviderType::get(SoftStringOrNothingType), "desc": "Sort order for results", "allowed_values": (<AllowedValueInfo>{"value": "asc", "display_name": "Ascending"}, <AllowedValueInfo>{"value": "desc", "display_name": "Descending"}) },
            "limit": <DataProviderOptionInfo>{ "display_name": "Limit", "short_desc": "Maximum number of results", "type": AbstractDataProviderType::get(SoftIntOrNothingType), "desc": "Maximum number of results to return" },
        };
        const ChildMap = {
            "create": Class::forName("ClickFunnelsCreateFormSubmissionDataProvider"),
            "update": Class::forName("ClickFunnelsUpdateFormSubmissionDataProvider"),
            "delete": Class::forName("ClickFunnelsDeleteFormSubmissionDataProvider"),
        };
    }

    constructor(ClickFunnelsRestClient::ClickFunnelsRestClientIo rest) : ClickFunnelsDataProviderBase(rest) {}
    string getName() { return "form-submissions"; }
    *string getDesc() { return "ClickFunnels form submissions data provider"; }
    private *list<string> getChildProviderNamesImpl() { return keys ChildMap; }
    private *AbstractDataProvider getChildProviderImpl(string name) { *Class cls = ChildMap{name}; if (cls) { return cls.newObject(rest); } }
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) { return new ClickFunnelsFormSubmissionDataType().getFields(); }
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond, *hash<auto> search_options) {
        setWorkspaceIdFromSearchOpts(search_options);
        if (search_options.submission_id) {
            auto result = doGet("forms/submissions/" + search_options.submission_id);
            return new DefaultRecordIterator(new ListHashIterator((result,)), where_cond, search_options);
        }
        if (!search_options.form_id) {
            throw "CLICKFUNNELS-ERROR", "form_id is required to list form submissions";
        }
        hash<auto> params;
        if (search_options.after) { params.after = search_options.after; }
        if (search_options.sort_order) { params.sort_order = search_options.sort_order; }
        list<auto> records = fetchAllPages("forms/" + search_options.form_id + "/submissions", params, search_options.limit);
        return new DefaultRecordIterator(new ListHashIterator(records), where_cond, search_options);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

# --- Create Form Submission ---
public class ClickFunnelsCreateFormSubmissionRequestDataType inherits HashDataType {
    public {
        const Fields = ClickFunnelsDataProviderBase::WorkspaceIdField + {
            "form_id": { "type": StringType, "display_name": "Form ID", "short_desc": "The form to submit to", "desc": "The ID of the form to create a submission for", "required": True, "ref_data": "forms" },
            "contact_id": { "type": StringOrNothingType, "display_name": "Contact ID", "short_desc": "The submitting contact", "desc": "The ID of the contact submitting the form", "ref_data": "contacts" },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class ClickFunnelsCreateFormSubmissionDataProvider inherits ClickFunnelsDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{ "name": "create", "desc": "Create a form submission in ClickFunnels", "type": "ClickFunnelsCreateFormSubmissionDataProvider", "supports_request": True, "constructor_options": ConstructorOptions };
        const ConstructorOptions = BaseConstructorOptions;
        static *AbstractDataProviderType ResponseType = new ClickFunnelsFormSubmissionDataType();
        static *AbstractDataProviderType RequestType = new ClickFunnelsCreateFormSubmissionRequestDataType();
    }
    constructor(ClickFunnelsRestClient::ClickFunnelsRestClientIo rest) : ClickFunnelsDataProviderBase(rest) {}
    string getName() { return "create"; }
    *string getDesc() { return "Create a form submission in ClickFunnels"; }
    private *AbstractDataProviderType getRequestTypeImpl() { return new ClickFunnelsCreateFormSubmissionRequestDataType(); }
    private *AbstractDataProviderType getResponseTypeImpl() { return new ClickFunnelsFormSubmissionDataType(); }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        req = setWorkspaceIdFromReq(req);
        string form_id = remove req.form_id;
        return doPost("forms/" + form_id + "/submissions", {"form_submission": req});
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

# --- Update Form Submission ---
public class ClickFunnelsUpdateFormSubmissionRequestDataType inherits HashDataType {
    public {
        const Fields = ClickFunnelsDataProviderBase::WorkspaceIdField + {
            "submission_id": { "type": StringType, "display_name": "Submission ID", "short_desc": "The submission to update", "desc": "The unique identifier of the submission to update", "required": True, "ref_data": "form_submissions" },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class ClickFunnelsUpdateFormSubmissionDataProvider inherits ClickFunnelsDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{ "name": "update", "desc": "Update a form submission in ClickFunnels", "type": "ClickFunnelsUpdateFormSubmissionDataProvider", "supports_request": True, "constructor_options": ConstructorOptions };
        const ConstructorOptions = BaseConstructorOptions;
        static *AbstractDataProviderType ResponseType = new ClickFunnelsFormSubmissionDataType();
        static *AbstractDataProviderType RequestType = new ClickFunnelsUpdateFormSubmissionRequestDataType();
    }
    constructor(ClickFunnelsRestClient::ClickFunnelsRestClientIo rest) : ClickFunnelsDataProviderBase(rest) {}
    string getName() { return "update"; }
    *string getDesc() { return "Update a form submission in ClickFunnels"; }
    private *AbstractDataProviderType getRequestTypeImpl() { return new ClickFunnelsUpdateFormSubmissionRequestDataType(); }
    private *AbstractDataProviderType getResponseTypeImpl() { return new ClickFunnelsFormSubmissionDataType(); }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        req = setWorkspaceIdFromReq(req);
        string id = remove req.submission_id;
        return doPut("forms/submissions/" + id, {"form_submission": req});
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

# --- Delete Form Submission ---
public class ClickFunnelsDeleteFormSubmissionRequestDataType inherits HashDataType {
    public {
        const Fields = ClickFunnelsDataProviderBase::WorkspaceIdField + {
            "submission_id": { "type": StringType, "display_name": "Submission ID", "short_desc": "The submission to delete", "desc": "The unique identifier of the submission to delete", "required": True, "ref_data": "form_submissions" },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class ClickFunnelsDeleteFormSubmissionDataProvider inherits ClickFunnelsDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{ "name": "delete", "desc": "Delete a form submission in ClickFunnels", "type": "ClickFunnelsDeleteFormSubmissionDataProvider", "supports_request": True, "constructor_options": ConstructorOptions };
        const ConstructorOptions = BaseConstructorOptions;
    }
    constructor(ClickFunnelsRestClient::ClickFunnelsRestClientIo rest) : ClickFunnelsDataProviderBase(rest) {}
    string getName() { return "delete"; }
    *string getDesc() { return "Delete a form submission in ClickFunnels"; }
    private *AbstractDataProviderType getRequestTypeImpl() { return new ClickFunnelsDeleteFormSubmissionRequestDataType(); }
    private *AbstractDataProviderType getResponseTypeImpl() { return AbstractDataProviderTypeMap."hash"; }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        req = setWorkspaceIdFromReq(req);
        doDelete("forms/submissions/" + req.submission_id);
        return {"success": True, "id": req.submission_id};
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace ClickFunnelsDataProvider
