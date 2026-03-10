# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore KeapDataProvider module definition

/*  KeapShippingInfoDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Keap shipping address information data type (AddressInformation schema)
public class KeapShippingInfoDataType inherits HashDataType {
    public {
        #! Field definitions for shipping address information
        const Fields = {
            "first_name": {
                "type": StringOrNothingType,
                "display_name": "First Name",
                "short_desc": "Recipient first name",
                "desc": "The first name of the shipping recipient",
                "example_value": "John",
            },
            "middle_name": {
                "type": StringOrNothingType,
                "display_name": "Middle Name",
                "short_desc": "Recipient middle name",
                "desc": "The middle name of the shipping recipient",
            },
            "last_name": {
                "type": StringOrNothingType,
                "display_name": "Last Name",
                "short_desc": "Recipient last name",
                "desc": "The last name of the shipping recipient",
            },
            "company": {
                "type": StringOrNothingType,
                "display_name": "Company",
                "short_desc": "Shipping company name",
                "desc": "The company name for the shipping address",
            },
            "line1": {
                "type": StringOrNothingType,
                "display_name": "Address Line 1",
                "short_desc": "The first address line",
                "desc": "The first line of the shipping address",
                "example_value": "123 Main Street",
            },
            "line2": {
                "type": StringOrNothingType,
                "display_name": "Address Line 2",
                "short_desc": "The second address line",
                "desc": "The second line of the shipping address",
            },
            "locality": {
                "type": StringOrNothingType,
                "display_name": "City",
                "short_desc": "The shipping city",
                "desc": "The city of the shipping address",
                "example_value": "San Francisco",
            },
            "region": {
                "type": StringOrNothingType,
                "display_name": "State/Province",
                "short_desc": "The shipping state or province",
                "desc": "The state or province of the shipping address",
                "example_value": "California",
            },
            "country_code": {
                "type": StringOrNothingType,
                "display_name": "Country Code",
                "short_desc": "The shipping country code",
                "desc": "The country code of the shipping address",
                "example_value": "US",
            },
            "zip_code": {
                "type": StringOrNothingType,
                "display_name": "ZIP Code",
                "short_desc": "The shipping postal code",
                "desc": "The ZIP or postal code of the shipping address",
                "example_value": "94105",
            },
            "phone": {
                "type": StringOrNothingType,
                "display_name": "Phone",
                "short_desc": "Recipient phone number",
                "desc": "The phone number of the shipping recipient",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("KeapShippingInfoDataType") {
        addQoreFields(Fields);
    }

    #! Creates the type with a base type (for optional fields)
    constructor(Type base_type) : HashDataType(base_type, "KeapShippingInfoDataType") {
        addQoreFields(Fields);
    }
}

} # namespace KeapDataProvider
