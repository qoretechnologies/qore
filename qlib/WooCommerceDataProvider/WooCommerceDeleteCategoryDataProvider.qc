# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceDeleteCategoryDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for deleting a category
public class WooCommerceDeleteCategoryRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Category ID",
                "type": IntType,
                "desc": "The unique identifier for the category to delete",
                "required": True,
                "example_value": 15,
                "ref_data": "categories",
            },
            "force": {
                "display_name": "Force",
                "type": SoftBoolOrNothingType,
                "desc": "Required to be `True` since categories do not support trash",
                "default_value": True,
                "example_value": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for deleting a WooCommerce product category
public class WooCommerceDeleteCategoryDataProvider inherits WooCommerceDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete-category",
            "desc": "Deletes a WooCommerce product category",
            "type": "WooCommerceDeleteCategoryDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        static WooCommerceDeleteCategoryRequestDataType RequestType();

        static WooCommerceCategoryDataType ResponseType();
    }

    constructor(WooCommerceRestClient::WooCommerceRestClient rest)
            : WooCommerceDataProviderBase(rest) {
    }

    constructor(WooCommerceRestClient::WooCommerceRestConnection conn) : WooCommerceDataProviderBase(conn) {
    }

    constructor(*hash<auto> options) : WooCommerceDataProviderBase(options) {
    }

    string getName() {
        return "delete-category";
    }

    *string getDesc() {
        return "Deletes a WooCommerce product category";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doDelete(sprintf("products/categories/%d", req.id),
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
