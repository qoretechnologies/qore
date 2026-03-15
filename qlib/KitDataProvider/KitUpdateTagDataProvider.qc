# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KitUpdateTagDataProvider.qc Kit update tag action data provider

/*  KitUpdateTagDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the KitDataProvider module
public namespace KitDataProvider {

#! Request type for updating a tag
public class KitUpdateTagRequestDataType inherits HashDataType {
    public {
        #! Field definitions for updating a tag
        const Fields = {
            "tag_id": {
                "type": StringType,
                "display_name": "Tag ID",
                "short_desc": "The tag ID to update",
                "desc": "The unique identifier of the tag to update",
                "required": True,
                "example_value": "12345",
                "ref_data": "tags",
            },
            "name": {
                "type": StringType,
                "display_name": "Tag Name",
                "short_desc": "The new tag name",
                "desc": "The new name for the tag",
                "required": True,
                "example_value": "VIP Subscriber",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Kit update tag action data provider
public class KitUpdateTagDataProvider inherits KitDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Update an existing tag in Kit",
            "type": "KitUpdateTagDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(KitRestClient::KitRestClient rest) : KitDataProviderBase(rest) {
    }

    string getName() {
        return "update";
    }

    *string getDesc() {
        return "Update an existing tag in Kit";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new KitUpdateTagRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new KitTagDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string id = remove req.tag_id;
        hash<auto> response = doPut("tags/" + encode_url(id, True), req);
        return response.tag;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace KitDataProvider
