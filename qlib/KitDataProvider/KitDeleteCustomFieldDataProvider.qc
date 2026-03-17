# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KitDeleteCustomFieldDataProvider.qc Kit delete custom field action data provider

/*  KitDeleteCustomFieldDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for deleting a custom field
public class KitDeleteCustomFieldRequestDataType inherits HashDataType {
    public {
        #! Field definitions for deleting a custom field
        const Fields = {
            "custom_field_id": {
                "type": StringType,
                "display_name": "Custom Field ID",
                "short_desc": "The custom field ID to delete",
                "desc": "The unique identifier of the custom field to delete",
                "required": True,
                "example_value": "12345",
                "ref_data": "custom_fields",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Kit delete custom field action data provider
public class KitDeleteCustomFieldDataProvider inherits KitDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete",
            "desc": "Delete a custom field in Kit",
            "type": "KitDeleteCustomFieldDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(KitRestClient::KitRestClient rest) : KitDataProviderBase(rest) {
    }

    string getName() {
        return "delete";
    }

    *string getDesc() {
        return "Delete a custom field in Kit";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new KitDeleteCustomFieldRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new HashDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string id = remove req.custom_field_id;
        return doDelete("custom_fields/" + encode_url(id, True));
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace KitDataProvider
