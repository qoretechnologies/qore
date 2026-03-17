# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WooCommerceDataProvider module definition

/*  WooCommerceShippingAddressDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! WooCommerce shipping address data type
public class WooCommerceShippingAddressDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "first_name": {
                "display_name": "First Name",
                "short_desc": "Shipping first name",
                "type": StringOrNothingType,
                "desc": "The first name for the shipping address",
            },
            "last_name": {
                "display_name": "Last Name",
                "short_desc": "Shipping last name",
                "type": StringOrNothingType,
                "desc": "The last name for the shipping address",
            },
            "company": {
                "display_name": "Company",
                "short_desc": "Shipping company name",
                "type": StringOrNothingType,
                "desc": "The company name for the shipping address",
            },
            "address_1": {
                "display_name": "Address Line 1",
                "short_desc": "First line of the shipping address",
                "type": StringOrNothingType,
                "desc": "The first line of the shipping street address",
            },
            "address_2": {
                "display_name": "Address Line 2",
                "short_desc": "Second line of the shipping address",
                "type": StringOrNothingType,
                "desc": "The second line of the shipping street address",
            },
            "city": {
                "display_name": "City",
                "short_desc": "Shipping city",
                "type": StringOrNothingType,
                "desc": "The city for the shipping address",
            },
            "state": {
                "display_name": "State",
                "short_desc": "Shipping state or province",
                "type": StringOrNothingType,
                "desc": "The state or province code for the shipping address",
            },
            "postcode": {
                "display_name": "Postcode",
                "short_desc": "Shipping postal code",
                "type": StringOrNothingType,
                "desc": "The postal or ZIP code for the shipping address",
            },
            "country": {
                "display_name": "Country",
                "short_desc": "Shipping country code",
                "type": StringOrNothingType,
                "desc": "The two-letter ISO country code for the shipping address",
            },
        };
    }

    #! Creates the object
    constructor() {
        map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type, $1.value.default_value)),
            Fields.pairIterator();
    }

    #! Creates the object with a specific base type (for optional fields)
    constructor(Type base_type) : HashDataType(base_type, "WooCommerceShippingAddress") {
        map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type, $1.value.default_value)),
            Fields.pairIterator();
    }
}

} # namespace WooCommerceDataProvider
