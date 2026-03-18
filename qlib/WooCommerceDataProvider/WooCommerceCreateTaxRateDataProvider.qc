# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceCreateTaxRateDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for creating a tax rate
public class WooCommerceCreateTaxRateRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "rate": {
                "display_name": "Rate",
                "type": StringType,
                "desc": "The tax rate as a percentage",
                "example_value": "8.5",
                "required": True,
            },
            "country": {
                "display_name": "Country",
                "type": StringOrNothingType,
                "desc": "The two-letter country code",
                "example_value": "US",
            },
            "state": {
                "display_name": "State",
                "type": StringOrNothingType,
                "desc": "The state code",
                "example_value": "CA",
            },
            "name": {
                "display_name": "Name",
                "type": StringOrNothingType,
                "desc": "The tax rate name",
                "example_value": "State Tax",
            },
            "priority": {
                "display_name": "Priority",
                "type": IntOrNothingType,
                "desc": "The priority of this tax rate",
                "example_value": 1,
            },
            "compound": {
                "display_name": "Compound",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, the tax rate is compounded",
                "example_value": False,
            },
            "shipping": {
                "display_name": "Shipping",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, the tax rate applies to shipping",
                "example_value": True,
            },
            "tax_class": {
                "display_name": "Tax Class",
                "type": StringOrNothingType,
                "desc": "The tax class this rate belongs to",
                "example_value": "standard",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for creating a new WooCommerce tax rate
public class WooCommerceCreateTaxRateDataProvider inherits WooCommerceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-tax-rate",
            "desc": "Creates a new WooCommerce tax rate",
            "type": "WooCommerceCreateTaxRateDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static WooCommerceCreateTaxRateRequestDataType RequestType();

        #! Response type
        static WooCommerceTaxRateDataType ResponseType();
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
        return "create-tax-rate";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Creates a new WooCommerce tax rate";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = {} + req;
        # Map tax_class to class for WooCommerce API
        if (body.tax_class) {
            body."class" = remove body.tax_class;
        }
        return doPost("taxes", body);
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
