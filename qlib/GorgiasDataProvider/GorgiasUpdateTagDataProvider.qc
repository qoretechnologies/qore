# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GorgiasUpdateTagDataProvider.qc Gorgias update tag action data provider

/*  GorgiasUpdateTagDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the GorgiasDataProvider module
public namespace GorgiasDataProvider {

#! Request type for updating a tag
public class GorgiasUpdateTagRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "tag_id": {
                "type": IntType,
                "display_name": "Tag ID",
                "short_desc": "The tag to update",
                "desc": "The ID of the tag to update",
                "required": True,
                "ref_data": "tags",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "Tag name",
                "desc": "The new name of the tag (min 1, max 256 characters)",
                "preselected": True,
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "Tag description",
                "desc": "A short description of the tag",
            },
            "decoration": {
                "type": new GorgiasTagDecorationDataType(AutoHashOrNothingType),
                "display_name": "Decoration",
                "short_desc": "Visual decoration",
                "desc": "Visual decoration settings for the tag",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Gorgias update tag action data provider
public class GorgiasUpdateTagDataProvider inherits GorgiasDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Update an existing Gorgias tag",
            "type": "GorgiasUpdateTagDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(RestClientIo::RestClientIo rest) : GorgiasDataProviderBase(rest) {
    }

    string getName() {
        return "update";
    }

    *string getDesc() {
        return "Update an existing Gorgias tag";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new GorgiasUpdateTagRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new GorgiasTagDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        int tag_id = remove req.tag_id;
        return doPut("tags/" + tag_id, req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace GorgiasDataProvider
