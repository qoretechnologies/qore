# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ClickFunnelsAppliedTagsDataProvider.qc ClickFunnels applied tags collection and actions

/*  ClickFunnelsAppliedTagsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

public class ClickFunnelsAppliedTagsDataProvider inherits ClickFunnelsDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "applied-tags",
            "desc": "ClickFunnels applied tags data provider",
            "type": "ClickFunnelsAppliedTagsDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "supports_children": True,
            "children_can_support_apis": True,
            "constructor_options": ConstructorOptions,
            "search_options": SearchOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        const SearchOptions = ClickFunnelsDataProviderBase::WorkspaceIdSearchOption + {
            "contact_id": <DataProviderOptionInfo>{
                "display_name": "Contact ID",
                "short_desc": "The contact to list tags for",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "The contact ID to list applied tags for (required for listing)",
            },
            "applied_tag_id": <DataProviderOptionInfo>{
                "display_name": "Applied Tag ID",
                "short_desc": "Get a specific applied tag",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by applied tag ID for single record lookup",
            },
            "tag_id": <DataProviderOptionInfo>{
                "display_name": "Tag ID",
                "short_desc": "Filter by tag",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter applied tags by tag ID",
                "ref_data": "contact_tags",
            },
            "after": <DataProviderOptionInfo>{
                "display_name": "After Cursor",
                "short_desc": "Pagination cursor",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "The cursor value for fetching the next page of results",
            },
            "sort_order": <DataProviderOptionInfo>{
                "display_name": "Sort Order",
                "short_desc": "Sort order for results",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Sort order for results",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "asc", "display_name": "Ascending"},
                    <AllowedValueInfo>{"value": "desc", "display_name": "Descending"},
                ),
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "Maximum number of results",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Maximum number of results to return",
            },
        };

        const ChildMap = {
            "apply": Class::forName("ClickFunnelsCreateAppliedTagDataProvider"),
            "remove": Class::forName("ClickFunnelsDeleteAppliedTagDataProvider"),
        };
    }

    constructor(ClickFunnelsRestClient::ClickFunnelsRestClientIo rest)
            : ClickFunnelsDataProviderBase(rest) {}
    string getName() { return "applied-tags"; }
    *string getDesc() { return "ClickFunnels applied tags data provider"; }
    private *list<string> getChildProviderNamesImpl() { return keys ChildMap; }
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) { return cls.newObject(rest); }
    }
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new ClickFunnelsAppliedTagDataType().getFields();
    }

    private AbstractDataProviderRecordIterator<hash<auto>> searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        setWorkspaceIdFromSearchOpts(search_options);
        if (search_options.applied_tag_id) {
            auto result = doGet("contacts/applied_tags/" + search_options.applied_tag_id);
            return new DefaultRecordIterator<hash<auto>>(new ListHashIterator((result,)),
                where_cond, search_options);
        }
        if (!search_options.contact_id) {
            throw "CLICKFUNNELS-ERROR", "contact_id is required to list applied tags";
        }
        hash<auto> params;
        if (search_options.tag_id) { params.tag_id = search_options.tag_id; }
        if (search_options.after) { params.after = search_options.after; }
        if (search_options.sort_order) { params.sort_order = search_options.sort_order; }
        list<auto> records = fetchAllPages(
            "contacts/" + search_options.contact_id + "/applied_tags",
            params, search_options.limit);
        return new DefaultRecordIterator<hash<auto>>(new ListHashIterator(records), where_cond,
            search_options);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

# --- Apply Tag ---

public class ClickFunnelsCreateAppliedTagRequestDataType inherits HashDataType {
    public {
        const Fields = ClickFunnelsDataProviderBase::WorkspaceIdField + {
            "contact_id": {
                "type": StringType,
                "display_name": "Contact ID",
                "short_desc": "The contact to tag",
                "desc": "The ID of the contact to apply the tag to",
                "required": True,
                "ref_data": "contacts",
            },
            "tag_id": {
                "type": StringType,
                "display_name": "Tag ID",
                "short_desc": "The tag to apply",
                "desc": "The ID of the tag to apply to the contact",
                "required": True,
                "ref_data": "contact_tags",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class ClickFunnelsCreateAppliedTagDataProvider inherits ClickFunnelsDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "apply",
            "desc": "Apply a tag to a contact in ClickFunnels",
            "type": "ClickFunnelsCreateAppliedTagDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static *AbstractDataProviderType ResponseType = new ClickFunnelsAppliedTagDataType();
        static *AbstractDataProviderType RequestType = new ClickFunnelsCreateAppliedTagRequestDataType();
    }

    constructor(ClickFunnelsRestClient::ClickFunnelsRestClientIo rest)
            : ClickFunnelsDataProviderBase(rest) {}
    string getName() { return "apply"; }
    *string getDesc() { return "Apply a tag to a contact in ClickFunnels"; }
    private *AbstractDataProviderType getRequestTypeImpl() { return new ClickFunnelsCreateAppliedTagRequestDataType(); }
    private *AbstractDataProviderType getResponseTypeImpl() { return new ClickFunnelsAppliedTagDataType(); }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        req = setWorkspaceIdFromReq(req);
        string contact_id = remove req.contact_id;
        return doPost("contacts/" + contact_id + "/applied_tags",
            {"contacts_applied_tag": {"tag_id": req.tag_id}});
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

# --- Remove Applied Tag ---

public class ClickFunnelsDeleteAppliedTagRequestDataType inherits HashDataType {
    public {
        const Fields = ClickFunnelsDataProviderBase::WorkspaceIdField + {
            "contact_id": {
                "type": StringType,
                "display_name": "Contact",
                "short_desc": "The contact to remove a tag from",
                "desc": "Select the contact whose applied tag you want to remove",
                "required": True,
                "ref_data": "contacts",
                "has_dependents": True,
                "on_change": ("refetch",),
            },
            "applied_tag_id": {
                "type": StringType,
                "display_name": "Applied Tag",
                "short_desc": "The applied tag to remove",
                "desc": "The applied tag to remove from the contact",
                "required": True,
                "depends_on": ("contact_id",),
                "ref_data": "applied_tags",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class ClickFunnelsDeleteAppliedTagDataProvider inherits ClickFunnelsDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "remove",
            "desc": "Remove an applied tag from a contact in ClickFunnels",
            "type": "ClickFunnelsDeleteAppliedTagDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(ClickFunnelsRestClient::ClickFunnelsRestClientIo rest)
            : ClickFunnelsDataProviderBase(rest) {}
    string getName() { return "remove"; }
    *string getDesc() { return "Remove an applied tag from a contact in ClickFunnels"; }
    private *AbstractDataProviderType getRequestTypeImpl() { return new ClickFunnelsDeleteAppliedTagRequestDataType(); }
    private *AbstractDataProviderType getResponseTypeImpl() { return AbstractDataProviderTypeMap."hash"; }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        req = setWorkspaceIdFromReq(req);
        remove req.contact_id;
        doDelete("contacts/applied_tags/" + req.applied_tag_id);
        return {"success": True, "id": req.applied_tag_id};
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace ClickFunnelsDataProvider
