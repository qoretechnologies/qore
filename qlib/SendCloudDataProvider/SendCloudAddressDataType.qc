# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SendCloudAddressDataType.qc SendCloud recipient address data type

/*  SendCloudAddressDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SendCloudDataProvider module
public namespace SendCloudDataProvider {

#! Recipient address (shared shape) — used as a sub-document on parcels and returns
public class SendCloudAddressDataType inherits HashDataType {
    public {
        const Fields = {
            "name": {
                "type": SoftStringType,
                "display_name": "Name",
                "short_desc": "Recipient full name",
                "desc": "The full name of the recipient",
                "required": True,
                "example_value": "John Doe",
            },
            "company_name": {
                "type": SoftStringOrNothingType,
                "display_name": "Company Name",
                "short_desc": "Recipient company",
                "desc": "Optional recipient company name",
            },
            "address": {
                "type": SoftStringType,
                "display_name": "Street Address",
                "short_desc": "Primary street address line",
                "desc": "The street name (and optionally number, if house_number is not used)",
                "required": True,
                "example_value": "Stadhuisplein",
            },
            "house_number": {
                "type": SoftStringOrNothingType,
                "display_name": "House Number",
                "short_desc": "Street number",
                "desc": "The house or street number",
                "example_value": "10",
            },
            "address_2": {
                "type": SoftStringOrNothingType,
                "display_name": "Address Line 2",
                "short_desc": "Secondary address line",
                "desc": "Apartment, suite, or other secondary address information",
            },
            "city": {
                "type": SoftStringType,
                "display_name": "City",
                "short_desc": "City name",
                "desc": "The city of the recipient",
                "required": True,
                "example_value": "Eindhoven",
            },
            "postal_code": {
                "type": SoftStringType,
                "display_name": "Postal Code",
                "short_desc": "Postal / ZIP code",
                "desc": "The postal code of the recipient",
                "required": True,
                "example_value": "5611 EM",
            },
            "country": {
                "type": SoftStringType,
                "display_name": "Country",
                "short_desc": "ISO 3166-1 alpha-2 country code",
                "desc": "Two-letter ISO country code of the recipient address",
                "required": True,
                "ref_data": "countries",
                "example_value": "NL",
            },
            "country_state": {
                "type": SoftStringOrNothingType,
                "display_name": "State / Region",
                "short_desc": "Country state or region code",
                "desc": "State or region code (required for countries that use them, e.g., US, CA)",
            },
            "telephone": {
                "type": SoftStringOrNothingType,
                "display_name": "Telephone",
                "short_desc": "Recipient phone number",
                "desc": "Phone number of the recipient (required by some carriers)",
            },
            "email": {
                "type": SoftStringOrNothingType,
                "display_name": "Email",
                "short_desc": "Recipient email address",
                "desc": "Email of the recipient (used for tracking notifications)",
            },
        };
    }

    constructor() : HashDataType("SendCloudAddressDataType") {
        addQoreFields(Fields);
    }
}

} # namespace SendCloudDataProvider
