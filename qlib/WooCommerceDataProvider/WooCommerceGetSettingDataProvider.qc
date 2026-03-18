# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceGetSettingDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for getting a setting
public class WooCommerceGetSettingRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "group": {
                "display_name": "Group",
                "type": StringType,
                "desc": "The settings group ID (e.g., `general`, `products`, `tax`)",
                "required": True,
                "example_value": "general",
            },
            "setting_id": {
                "display_name": "Setting ID",
                "type": StringType,
                "desc": "The setting identifier within the group",
                "required": True,
                "example_value": "woocommerce_currency",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for getting a WooCommerce setting
public class WooCommerceGetSettingDataProvider inherits WooCommerceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-setting",
            "desc": "Gets a WooCommerce setting value",
            "type": "WooCommerceGetSettingDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static WooCommerceGetSettingRequestDataType RequestType();

        #! Response type
        static WooCommerceSettingDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(WooCommerceRestClient::WooCommerceRestClient rest)
            : WooCommerceDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(WooCommerceRestClient::WooCommerceRestConnection conn) : WooCommerceDataProviderBase(conn) {
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : WooCommerceDataProviderBase(options) {
    }

    #! Returns the provider name
    string getName() {
        return "get-setting";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Gets a WooCommerce setting value";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doGet(sprintf("settings/%s/%s", encode_url(req.group, True), encode_url(req.setting_id, True)));
    }

    #! Returns the request type
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace WooCommerceDataProvider
