# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceSystemStatusSettingsDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! WooCommerce system status settings data type
public class WooCommerceSystemStatusSettingsDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "api_enabled": {
                "display_name": "API Enabled",
                "short_desc": "REST API enabled",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, the WooCommerce REST API is enabled",
            },
            "force_ssl": {
                "display_name": "Force SSL",
                "short_desc": "Force SSL for checkout",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, SSL is forced for checkout pages",
            },
            "currency": {
                "display_name": "Currency",
                "short_desc": "Store currency code",
                "type": StringOrNothingType,
                "desc": "The store currency code (e.g., `USD`, `EUR`)",
            },
            "currency_symbol": {
                "display_name": "Currency Symbol",
                "short_desc": "Store currency symbol",
                "type": StringOrNothingType,
                "desc": "The store currency symbol (e.g., `$`, `€`)",
            },
            "currency_position": {
                "display_name": "Currency Position",
                "short_desc": "Currency symbol position",
                "type": StringOrNothingType,
                "desc": "The position of the currency symbol (`left`, `right`, `left_space`, `right_space`)",
            },
            "thousand_separator": {
                "display_name": "Thousand Separator",
                "short_desc": "Thousand separator character",
                "type": StringOrNothingType,
                "desc": "The character used as the thousand separator (e.g., `,`)",
            },
            "decimal_separator": {
                "display_name": "Decimal Separator",
                "short_desc": "Decimal separator character",
                "type": StringOrNothingType,
                "desc": "The character used as the decimal separator (e.g., `.`)",
            },
            "number_of_decimals": {
                "display_name": "Number of Decimals",
                "short_desc": "Number of decimal places",
                "type": IntOrNothingType,
                "desc": "The number of decimal places used for prices",
            },
            "geolocation_enabled": {
                "display_name": "Geolocation Enabled",
                "short_desc": "Geolocation enabled",
                "type": SoftBoolOrNothingType,
                "desc": "If `True`, customer geolocation is enabled",
            },
            "taxonomies": {
                "display_name": "Taxonomies",
                "short_desc": "Product taxonomies",
                "type": AutoHashOrNothingType,
                "desc": "Product visibility and other taxonomy configuration",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }

    #! Creates the object with a specific base type (for optional fields)
    constructor(Type base_type) : HashDataType(base_type, "WooCommerceSystemStatusSettings") {
        addQoreFields(Fields);
    }
}

} # namespace WooCommerceDataProvider
