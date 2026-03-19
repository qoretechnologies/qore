# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KitUpdateCustomFieldDataProvider.qc Kit update custom field action data provider

/*  KitUpdateCustomFieldDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for updating a custom field
public class KitUpdateCustomFieldRequestDataType inherits HashDataType {
    public {
        #! Field definitions for updating a custom field
        const Fields = {
            "custom_field_id": {
                "type": StringType,
                "display_name": "Custom Field ID",
                "short_desc": "The custom field ID to update",
                "desc": "The unique identifier of the custom field to update",
                "required": True,
                "example_value": "12345",
                "ref_data": "custom_fields",
            },
            "label": {
                "type": StringType,
                "display_name": "Label",
                "short_desc": "The new custom field label",
                "desc": "The new display label for the custom field",
                "required": True,
                "example_value": "Company Name",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Kit update custom field action data provider
public class KitUpdateCustomFieldDataProvider inherits KitDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Update an existing custom field in Kit",
            "type": "KitUpdateCustomFieldDataProvider",
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
        return "Update an existing custom field in Kit";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new KitUpdateCustomFieldRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new KitCustomFieldDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string id = remove req.custom_field_id;
        hash<auto> response = doPut("custom_fields/" + encode_url(id, True), req);
        return response.custom_field;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace KitDataProvider
