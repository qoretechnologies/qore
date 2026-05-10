# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceUpdateTaxRateDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for updating a tax rate
public class WooCommerceUpdateTaxRateRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Tax Rate ID",
                "type": IntType,
                "desc": "The unique identifier for the tax rate to update",
                "required": True,
                "example_value": 72,
                "ref_data": "tax_rates",
            },
            "rate": {
                "display_name": "Rate",
                "type": StringOrNothingType,
                "desc": "The tax rate as a percentage",
                "example_value": "8.5",
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
            "postcode": {
                "display_name": "Postcode",
                "type": StringOrNothingType,
                "desc": "Single postcode this rate applies to",
            },
            "city": {
                "display_name": "City",
                "type": StringOrNothingType,
                "desc": "Single city this rate applies to",
            },
            "postcodes": {
                "display_name": "Postcodes",
                "type": new ListDataType("Postcodes", SoftStringType, True),
                "desc": "List of postcodes this rate applies to",
            },
            "cities": {
                "display_name": "Cities",
                "type": new ListDataType("Cities", SoftStringType, True),
                "desc": "List of cities this rate applies to",
            },
            "name": {
                "display_name": "Name",
                "type": StringOrNothingType,
                "desc": "The tax rate name",
            },
            "priority": {
                "display_name": "Priority",
                "type": IntOrNothingType,
                "desc": "The priority of this tax rate",
            },
            "order": {
                "display_name": "Display Order",
                "type": IntOrNothingType,
                "desc": "Display order of this tax rate",
            },
            "compound": {
                "display_name": "Compound",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, the tax rate is compounded",
            },
            "shipping": {
                "display_name": "Shipping",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, the tax rate applies to shipping",
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

#! Data provider for updating a WooCommerce tax rate
public class WooCommerceUpdateTaxRateDataProvider inherits WooCommerceDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update-tax-rate",
            "desc": "Updates a WooCommerce tax rate",
            "type": "WooCommerceUpdateTaxRateDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        static WooCommerceUpdateTaxRateRequestDataType RequestType();

        static WooCommerceTaxRateDataType ResponseType();
    }

    constructor(WooCommerceRestClient::WooCommerceRestClient rest)
            : WooCommerceDataProviderBase(rest) {
    }

    constructor(WooCommerceRestClient::WooCommerceRestConnection conn) : WooCommerceDataProviderBase(conn) {
    }

    constructor(*hash<auto> options) : WooCommerceDataProviderBase(options) {
    }

    string getName() {
        return "update-tax-rate";
    }

    *string getDesc() {
        return "Updates a WooCommerce tax rate";
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        int id = remove req.id;
        hash<auto> body = {} + req;
        # Map tax_class -> class for WooCommerce API
        if (body.tax_class) {
            body."class" = remove body.tax_class;
        }
        return doPut(sprintf("taxes/%d", id), body);
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
