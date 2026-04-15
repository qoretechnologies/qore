# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore InsightlyDataProvider module definition

/*  InsightlyAddressDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the InsightlyDataProvider module
public namespace InsightlyDataProvider {

#! Insightly address data type
public class InsightlyAddressDataType inherits HashDataType {
    public {
        #! Field definitions for Insightly addresses
        const Fields = {
            "ADDRESS_TYPE": {
                "type": SoftStringOrNothingType,
                "display_name": "Address Type",
                "short_desc": "The type of address",
                "desc": "The type of address",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "Work", "display_name": "Work",
                        "desc": "Work address"},
                    <AllowedValueInfo>{"value": "Home", "display_name": "Home",
                        "desc": "Home address"},
                    <AllowedValueInfo>{"value": "Postal", "display_name": "Postal",
                        "desc": "Postal address"},
                    <AllowedValueInfo>{"value": "Other", "display_name": "Other",
                        "desc": "Other address"},
                ),
            },
            "STREET": {
                "type": SoftStringOrNothingType,
                "display_name": "Street",
                "short_desc": "Street address",
                "desc": "The street address",
                "example_value": "123 Main Street",
            },
            "CITY": {
                "type": SoftStringOrNothingType,
                "display_name": "City",
                "short_desc": "City name",
                "desc": "The city of the address",
                "example_value": "San Francisco",
            },
            "STATE": {
                "type": SoftStringOrNothingType,
                "display_name": "State",
                "short_desc": "State or province",
                "desc": "The state or province of the address",
                "example_value": "CA",
            },
            "POSTCODE": {
                "type": SoftStringOrNothingType,
                "display_name": "Postcode",
                "short_desc": "Postal code",
                "desc": "The postal or ZIP code of the address",
                "example_value": "94105",
            },
            "COUNTRY": {
                "type": SoftStringOrNothingType,
                "display_name": "Country",
                "short_desc": "Country name",
                "desc": "The country of the address",
                "example_value": "United States",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("InsightlyAddressDataType") {
        addQoreFields(Fields);
    }
}

} # namespace InsightlyDataProvider
