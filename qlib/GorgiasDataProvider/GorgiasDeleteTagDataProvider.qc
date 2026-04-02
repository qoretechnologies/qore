# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GorgiasDeleteTagDataProvider.qc Gorgias delete tag action data provider

/*  GorgiasDeleteTagDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for deleting a tag
public class GorgiasDeleteTagRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "tag_id": {
                "type": IntType,
                "display_name": "Tag ID",
                "short_desc": "The tag to delete",
                "desc": "The ID of the tag to delete",
                "required": True,
                "ref_data": "tags",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Response type for delete operation
public class GorgiasDeleteTagResponseDataType inherits HashDataType {
    public {
        const Fields = {
            "deleted": {
                "type": BoolType,
                "display_name": "Deleted",
                "short_desc": "Whether the deletion was successful",
                "desc": "Returns `True` if the tag was successfully deleted",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Gorgias delete tag action data provider
public class GorgiasDeleteTagDataProvider inherits GorgiasDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete",
            "desc": "Delete an existing Gorgias tag",
            "type": "GorgiasDeleteTagDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        static GorgiasDeleteTagResponseDataType ResponseType();
    }

    constructor(RestClientIo::RestClientIo rest) : GorgiasDataProviderBase(rest) {
    }

    string getName() {
        return "delete";
    }

    *string getDesc() {
        return "Delete an existing Gorgias tag";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new GorgiasDeleteTagRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        doDelete("tags/" + req.tag_id);
        return {"deleted": True};
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace GorgiasDataProvider
