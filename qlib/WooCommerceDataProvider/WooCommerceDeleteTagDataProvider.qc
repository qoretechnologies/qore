# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceDeleteTagDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the WooCommerceDataProvider module
public namespace WooCommerceDataProvider {

#! Request type for deleting a tag
public class WooCommerceDeleteTagRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Tag ID",
                "type": IntType,
                "desc": "The unique identifier for the tag to delete",
                "required": True,
                "example_value": 7,
                "ref_data": "tags",
            },
            "force": {
                "display_name": "Force",
                "type": SoftBoolOrNothingType,
                "desc": "Required to be `True` since tags do not support trash",
                "default_value": True,
                "example_value": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for deleting a WooCommerce product tag
public class WooCommerceDeleteTagDataProvider inherits WooCommerceDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete-tag",
            "desc": "Deletes a WooCommerce product tag",
            "type": "WooCommerceDeleteTagDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        static WooCommerceDeleteTagRequestDataType RequestType();

        static WooCommerceTagDataType ResponseType();
    }

    constructor(WooCommerceRestClient::WooCommerceRestClient rest)
            : WooCommerceDataProviderBase(rest) {
    }

    constructor(WooCommerceRestClient::WooCommerceRestConnection conn) : WooCommerceDataProviderBase(conn) {
    }

    constructor(*hash<auto> options) : WooCommerceDataProviderBase(options) {
    }

    string getName() {
        return "delete-tag";
    }

    *string getDesc() {
        return "Deletes a WooCommerce product tag";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doDelete(sprintf("products/tags/%d", req.id),
            {"force": req.force ?? True ? "true" : "false"});
    }

    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace WooCommerceDataProvider
