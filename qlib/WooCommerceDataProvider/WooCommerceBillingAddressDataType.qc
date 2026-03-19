# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceBillingAddressDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! WooCommerce billing address data type
public class WooCommerceBillingAddressDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "first_name": {
                "display_name": "First Name",
                "short_desc": "Billing first name",
                "type": StringOrNothingType,
                "desc": "The first name for the billing address",
                "example_value": "John",
            },
            "last_name": {
                "display_name": "Last Name",
                "short_desc": "Billing last name",
                "type": StringOrNothingType,
                "desc": "The last name for the billing address",
                "example_value": "Doe",
            },
            "company": {
                "display_name": "Company",
                "short_desc": "Billing company name",
                "type": StringOrNothingType,
                "desc": "The company name for the billing address",
                "example_value": "Acme Corp",
            },
            "address_1": {
                "display_name": "Address Line 1",
                "short_desc": "First line of the billing address",
                "type": StringOrNothingType,
                "desc": "The first line of the billing street address",
                "example_value": "969 Market",
            },
            "address_2": {
                "display_name": "Address Line 2",
                "short_desc": "Second line of the billing address",
                "type": StringOrNothingType,
                "desc": "The second line of the billing street address",
                "example_value": "Suite 200",
            },
            "city": {
                "display_name": "City",
                "short_desc": "Billing city",
                "type": StringOrNothingType,
                "desc": "The city for the billing address",
                "example_value": "San Francisco",
            },
            "state": {
                "display_name": "State",
                "short_desc": "Billing state or province",
                "type": StringOrNothingType,
                "desc": "The state or province code for the billing address",
                "example_value": "CA",
            },
            "postcode": {
                "display_name": "Postcode",
                "short_desc": "Billing postal code",
                "type": StringOrNothingType,
                "desc": "The postal or ZIP code for the billing address",
                "example_value": "94103",
            },
            "country": {
                "display_name": "Country",
                "short_desc": "Billing country code",
                "type": StringOrNothingType,
                "desc": "The two-letter ISO country code for the billing address",
                "example_value": "US",
            },
            "email": {
                "display_name": "Email",
                "short_desc": "Billing email address",
                "type": StringOrNothingType,
                "desc": "The email address for the billing contact",
                "example_value": "john.doe@example.com",
            },
            "phone": {
                "display_name": "Phone",
                "short_desc": "Billing phone number",
                "type": StringOrNothingType,
                "desc": "The phone number for the billing contact",
                "example_value": "(555) 555-5555",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }

    #! Creates the object with a specific base type (for optional fields)
    constructor(Type base_type) : HashDataType(base_type, "WooCommerceBillingAddress") {
        addQoreFields(Fields);
    }
}

} # namespace WooCommerceDataProvider
