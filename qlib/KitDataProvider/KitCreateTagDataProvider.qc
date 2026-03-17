# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KitCreateTagDataProvider.qc Kit create tag action data provider

/*  KitCreateTagDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for creating a tag
public class KitCreateTagRequestDataType inherits HashDataType {
    public {
        #! Field definitions for creating a tag
        const Fields = {
            "name": {
                "type": StringType,
                "display_name": "Tag Name",
                "short_desc": "The tag name",
                "desc": "The name of the tag to create",
                "required": True,
                "example_value": "VIP Subscriber",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Kit create tag action data provider
public class KitCreateTagDataProvider inherits KitDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new tag in Kit",
            "type": "KitCreateTagDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(KitRestClient::KitRestClient rest) : KitDataProviderBase(rest) {
    }

    string getName() {
        return "create";
    }

    *string getDesc() {
        return "Create a new tag in Kit";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new KitCreateTagRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new KitTagDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> response = doPost("tags", req);
        return response.tag;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace KitDataProvider
