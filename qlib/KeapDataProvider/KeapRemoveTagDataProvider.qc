# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KeapRemoveTagDataProvider.qc Keap remove tag from contact action data provider

/*  KeapRemoveTagDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the KeapDataProvider module
public namespace KeapDataProvider {

#! Request type for removing a tag from a contact
public class KeapRemoveTagRequestDataType inherits HashDataType {
    private {
        const Fields = {
            "contact_id": {
                "type": StringType,
                "display_name": "Contact ID",
                "short_desc": "The contact to remove the tag from",
                "desc": "The unique identifier of the contact to remove the tag from",
                "required": True,
            },
            "tag_id": {
                "type": StringType,
                "display_name": "Tag ID",
                "short_desc": "The tag to remove",
                "desc": "The unique identifier of the tag to remove from the contact",
                "required": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Keap remove tag from contact action data provider
public class KeapRemoveTagDataProvider inherits KeapDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "remove",
            "desc": "Remove a tag from a contact in Keap",
            "type": "KeapRemoveTagDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(KeapRestClient::KeapRestClient rest) : KeapDataProviderBase(rest) {
    }

    string getName() {
        return "remove";
    }

    *string getDesc() {
        return "Remove a tag from a contact in Keap";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new KeapRemoveTagRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderTypeMap."hash";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        # v2 API: POST /tags/{tag_id}/contacts:removeTags with {"contact_ids": [...]}
        if (!req.contact_id.val() || !regex(req.contact_id, "^[0-9]+$")) {
            throw "KEAP-ERROR", sprintf("contact_id must be a numeric value; got: %y",
                req.contact_id);
        }
        return doPost("tags/" + req.tag_id + "/contacts:removeTags",
            {"contact_ids": (req.contact_id.toInt(),)});
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace KeapDataProvider
