# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ClickFunnelsContactTagActions.qc ClickFunnels contact tag CRUD actions

/*  ClickFunnelsContactTagActions.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

# --- Create Contact Tag ---

public class ClickFunnelsCreateContactTagRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "The tag name",
                "desc": "The name of the contact tag to create",
                "required": True,
                "example_value": "VIP",
            },
            "color": {
                "type": StringOrNothingType,
                "display_name": "Color",
                "short_desc": "The tag color",
                "desc": "The color code for the tag",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class ClickFunnelsCreateContactTagDataProvider inherits ClickFunnelsDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new contact tag in ClickFunnels",
            "type": "ClickFunnelsCreateContactTagDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static *AbstractDataProviderType ResponseType = new ClickFunnelsContactTagDataType();
        static *AbstractDataProviderType RequestType = new ClickFunnelsCreateContactTagRequestDataType();
    }

    constructor(ClickFunnelsRestClient::ClickFunnelsRestClientIo rest)
            : ClickFunnelsDataProviderBase(rest) {}
    string getName() { return "create"; }
    *string getDesc() { return "Create a new contact tag in ClickFunnels"; }
    private *AbstractDataProviderType getRequestTypeImpl() { return new ClickFunnelsCreateContactTagRequestDataType(); }
    private *AbstractDataProviderType getResponseTypeImpl() { return new ClickFunnelsContactTagDataType(); }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost(getWorkspacePath("contacts/tags"), {"contacts_tag": req});
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

# --- Update Contact Tag ---

public class ClickFunnelsUpdateContactTagRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "tag_id": {
                "type": StringType,
                "display_name": "Tag ID",
                "short_desc": "The tag ID to update",
                "desc": "The unique identifier of the tag to update",
                "required": True,
                "ref_data": "contact_tags",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "The tag name",
                "desc": "The new name for the tag",
            },
            "color": {
                "type": StringOrNothingType,
                "display_name": "Color",
                "short_desc": "The tag color",
                "desc": "The new color code for the tag",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class ClickFunnelsUpdateContactTagDataProvider inherits ClickFunnelsDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Update a contact tag in ClickFunnels",
            "type": "ClickFunnelsUpdateContactTagDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        static *AbstractDataProviderType ResponseType = new ClickFunnelsContactTagDataType();
        static *AbstractDataProviderType RequestType = new ClickFunnelsUpdateContactTagRequestDataType();
    }

    constructor(ClickFunnelsRestClient::ClickFunnelsRestClientIo rest)
            : ClickFunnelsDataProviderBase(rest) {}
    string getName() { return "update"; }
    *string getDesc() { return "Update a contact tag in ClickFunnels"; }
    private *AbstractDataProviderType getRequestTypeImpl() { return new ClickFunnelsUpdateContactTagRequestDataType(); }
    private *AbstractDataProviderType getResponseTypeImpl() { return new ClickFunnelsContactTagDataType(); }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string id = remove req.tag_id;
        return doPut("contacts/tags/" + id, {"contacts_tag": req});
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

# --- Delete Contact Tag ---

public class ClickFunnelsDeleteContactTagRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "tag_id": {
                "type": StringType,
                "display_name": "Tag ID",
                "short_desc": "The tag to delete",
                "desc": "The unique identifier of the tag to delete",
                "required": True,
                "ref_data": "contact_tags",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class ClickFunnelsDeleteContactTagDataProvider inherits ClickFunnelsDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete",
            "desc": "Delete a contact tag in ClickFunnels",
            "type": "ClickFunnelsDeleteContactTagDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(ClickFunnelsRestClient::ClickFunnelsRestClientIo rest)
            : ClickFunnelsDataProviderBase(rest) {}
    string getName() { return "delete"; }
    *string getDesc() { return "Delete a contact tag in ClickFunnels"; }
    private *AbstractDataProviderType getRequestTypeImpl() { return new ClickFunnelsDeleteContactTagRequestDataType(); }
    private *AbstractDataProviderType getResponseTypeImpl() { return AbstractDataProviderTypeMap."hash"; }
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        doDelete("contacts/tags/" + req.tag_id);
        return {"success": True, "id": req.tag_id};
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace ClickFunnelsDataProvider
