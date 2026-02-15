# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BigCommerceCustomerAddressDataType.qc defines the BigCommerceCustomerAddressDataType class

/*  BigCommerceCustomerAddressDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the BigCommerceDataProvider module
public namespace BigCommerceDataProvider {

#! Data type for BigCommerce customer address records
public class BigCommerceCustomerAddressDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "id": {
                "display_name": "ID",
                "type": IntType,
                "desc": "Unique numeric ID of the address",
            },
            "customer_id": {
                "display_name": "Customer ID",
                "type": IntType,
                "desc": "ID of the customer this address belongs to",
            },
            "first_name": {
                "display_name": "First Name",
                "type": StringType,
                "desc": "First name of the address recipient",
            },
            "last_name": {
                "display_name": "Last Name",
                "type": StringType,
                "desc": "Last name of the address recipient",
            },
            "company": {
                "display_name": "Company",
                "type": SoftStringOrNothingType,
                "desc": "Company name for the address",
            },
            "address1": {
                "display_name": "Address Line 1",
                "type": StringType,
                "desc": "First line of the street address",
            },
            "address2": {
                "display_name": "Address Line 2",
                "type": SoftStringOrNothingType,
                "desc": "Second line of the street address",
            },
            "city": {
                "display_name": "City",
                "type": StringType,
                "desc": "City name",
            },
            "state_or_province": {
                "display_name": "State/Province",
                "type": StringType,
                "desc": "State or province name",
            },
            "postal_code": {
                "display_name": "Postal Code",
                "type": StringType,
                "desc": "Postal or ZIP code",
            },
            "country_code": {
                "display_name": "Country Code",
                "type": StringType,
                "desc": "ISO 3166-1 alpha-2 country code (e.g., US, GB, DE)",
            },
            "phone": {
                "display_name": "Phone",
                "type": SoftStringOrNothingType,
                "desc": "Phone number for the address",
            },
            "address_type": {
                "display_name": "Address Type",
                "type": SoftStringOrNothingType,
                "desc": "Type of address",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "residential",
                        "desc": "Residential address"},
                    <AllowedValueInfo>{"value": "commercial",
                        "desc": "Commercial address"},
                ),
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("BigCommerceCustomerAddress") {
        addQoreFields(Fields);
    }
}

} # namespace BigCommerceDataProvider
