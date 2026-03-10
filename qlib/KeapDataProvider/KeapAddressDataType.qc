# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore KeapDataProvider module definition

/*  KeapAddressDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the KeapDataProvider module
public namespace KeapDataProvider {

#! Keap address data type
public class KeapAddressDataType inherits HashDataType {
    public {
        #! Field definitions for Keap addresses
        const Fields = {
            "line1": {
                "type": StringOrNothingType,
                "display_name": "Address Line 1",
                "short_desc": "The first address line",
                "desc": "The first line of the street address",
                "example_value": "123 Main Street",
            },
            "line2": {
                "type": StringOrNothingType,
                "display_name": "Address Line 2",
                "short_desc": "The second address line",
                "desc": "The second line of the street address (suite, unit, etc.)",
            },
            "locality": {
                "type": StringOrNothingType,
                "display_name": "City",
                "short_desc": "The city or locality",
                "desc": "The city or locality of the address",
                "example_value": "San Francisco",
            },
            "region": {
                "type": StringOrNothingType,
                "display_name": "State/Region",
                "short_desc": "The state or region",
                "desc": "The state, province, or region of the address",
                "example_value": "CA",
            },
            "zip_code": {
                "type": StringOrNothingType,
                "display_name": "ZIP Code",
                "short_desc": "The postal code",
                "desc": "The ZIP or postal code of the address",
                "example_value": "94105",
            },
            "country_code": {
                "type": StringOrNothingType,
                "display_name": "Country Code",
                "short_desc": "The country code",
                "desc": "The ISO 3166-1 alpha-2 country code (e.g., `US`, `GB`, `DE`)",
                "example_value": "US",
            },
            "field": {
                "type": StringOrNothingType,
                "display_name": "Address Type",
                "short_desc": "The address type",
                "desc": "The type of address",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "BILLING", "desc": "Billing address"},
                    <AllowedValueInfo>{"value": "SHIPPING", "desc": "Shipping address"},
                    <AllowedValueInfo>{"value": "OTHER", "desc": "Other address"},
                ),
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("KeapAddressDataType") {
        addQoreFields(Fields);
    }

    #! Creates the type with a base type (for optional fields)
    constructor(Type base_type) : HashDataType(base_type, "KeapAddressDataType") {
        addQoreFields(Fields);
    }
}

} # namespace KeapDataProvider
