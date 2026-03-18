# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceTaxRateDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! WooCommerce tax rate data type
public class WooCommerceTaxRateDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "id": {
                "display_name": "ID",
                "short_desc": "Unique identifier for the tax rate",
                "type": IntOrNothingType,
                "desc": "The unique identifier for the tax rate",
            },
            "country": {
                "display_name": "Country",
                "short_desc": "Country code",
                "type": StringOrNothingType,
                "desc": "The two-letter ISO country code for the tax rate",
            },
            "state": {
                "display_name": "State",
                "short_desc": "State code",
                "type": StringOrNothingType,
                "desc": "The state or province code for the tax rate",
            },
            "postcode": {
                "display_name": "Postcode",
                "short_desc": "Postal code",
                "type": StringOrNothingType,
                "desc": "The postal or ZIP code for the tax rate",
            },
            "city": {
                "display_name": "City",
                "short_desc": "City name",
                "type": StringOrNothingType,
                "desc": "The city for the tax rate",
            },
            "rate": {
                "display_name": "Rate",
                "short_desc": "Tax rate percentage",
                "type": StringOrNothingType,
                "desc": "The tax rate percentage",
            },
            "name": {
                "display_name": "Name",
                "short_desc": "Tax rate name",
                "type": StringOrNothingType,
                "desc": "The tax rate name",
            },
            "priority": {
                "display_name": "Priority",
                "short_desc": "Tax rate priority",
                "type": IntOrNothingType,
                "desc": "The priority of the tax rate for calculation order",
            },
            "compound": {
                "display_name": "Compound",
                "short_desc": "Whether the tax rate is compound",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, the tax rate is compounded (applied on top of other taxes)",
            },
            "shipping": {
                "display_name": "Shipping",
                "short_desc": "Whether the tax rate applies to shipping",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, the tax rate applies to shipping charges",
            },
            "order": {
                "display_name": "Order",
                "short_desc": "Tax rate sort order",
                "type": IntOrNothingType,
                "desc": "The sort order of the tax rate",
            },
            "tax_class": {
                "display_name": "Tax Class",
                "short_desc": "Tax class slug",
                "type": StringOrNothingType,
                "desc": "The tax class slug (corresponds to the `class` field in the API)",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace WooCommerceDataProvider
