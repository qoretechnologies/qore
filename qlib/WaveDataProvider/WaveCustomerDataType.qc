# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WaveDataProvider module definition

/*  WaveCustomerDataType.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the WaveDataProvider module
public namespace WaveDataProvider {

#! Wave customer record type
public class WaveCustomerDataType inherits HashDataType {
    private {
        #! Field definitions for Wave customers
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The unique identifier of the customer",
                "desc": "The unique identifier of the customer",
            },
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "The customer name",
                "desc": "The full name of the customer",
                "example_value": "Acme Corp",
            },
            "firstName": {
                "type": StringOrNothingType,
                "display_name": "First Name",
                "short_desc": "The customer first name",
                "desc": "The first name of the customer contact",
                "example_value": "John",
            },
            "lastName": {
                "type": StringOrNothingType,
                "display_name": "Last Name",
                "short_desc": "The customer last name",
                "desc": "The last name of the customer contact",
                "example_value": "Doe",
            },
            "email": {
                "type": StringOrNothingType,
                "display_name": "Email",
                "short_desc": "The customer email",
                "desc": "The email address of the customer",
                "example_value": "john@acme.com",
            },
            "phone": {
                "type": StringOrNothingType,
                "display_name": "Phone",
                "short_desc": "The customer phone number",
                "desc": "The primary phone number of the customer",
                "example_value": "+1-555-0100",
            },
            "fax": {
                "type": StringOrNothingType,
                "display_name": "Fax",
                "short_desc": "The customer fax number",
                "desc": "The fax number of the customer",
                "example_value": "+1-555-0101",
            },
            "mobile": {
                "type": StringOrNothingType,
                "display_name": "Mobile",
                "short_desc": "The customer mobile number",
                "desc": "The mobile phone number of the customer",
                "example_value": "+1-555-0102",
            },
            "tollFree": {
                "type": StringOrNothingType,
                "display_name": "Toll Free",
                "short_desc": "The toll-free number",
                "desc": "The toll-free phone number of the customer",
                "example_value": "1-800-555-0100",
            },
            "website": {
                "type": StringOrNothingType,
                "display_name": "Website",
                "short_desc": "The customer website",
                "desc": "The website URL of the customer",
                "example_value": "https://www.acme.com",
            },
            "internalNotes": {
                "type": StringOrNothingType,
                "display_name": "Internal Notes",
                "short_desc": "Internal notes about the customer",
                "desc": "Internal notes about the customer (not visible to the customer)",
                "example_value": "Preferred payment: wire transfer",
            },
            "address": {
                "type": new WaveAddressRefDataType(),
                "display_name": "Address",
                "short_desc": "The customer address",
                "desc": "The billing address of the customer",
            },
            "shippingDetails": {
                "type": new WaveShippingDetailsRefDataType(),
                "display_name": "Shipping Details",
                "short_desc": "The shipping details",
                "desc": "The shipping details for the customer (name, phone, instructions, address)",
            },
            "currency": {
                "type": new WaveCurrencyRefDataType(),
                "display_name": "Currency",
                "short_desc": "The customer currency",
                "desc": "The default currency for the customer (code)",
            },
            "createdAt": {
                "type": DateOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the customer was created",
                "desc": "The date and time when the customer was created",
            },
            "modifiedAt": {
                "type": DateOrNothingType,
                "display_name": "Modified At",
                "short_desc": "When the customer was last modified",
                "desc": "The date and time when the customer was last modified",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Wave customer create input type
public class WaveCustomerCreateDataType inherits HashDataType {
    private {
        #! Field definitions for creating customers
        const Fields = {
            "businessId": {
                "type": StringType,
                "display_name": "Business ID",
                "short_desc": "The business ID (required)",
                "desc": "The ID of the business to create the customer in",
                "required": True,
            },
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "The customer name (required)",
                "desc": "The full name of the customer",
                "example_value": "Acme Corp",
                "required": True,
            },
            "firstName": {
                "type": StringOrNothingType,
                "display_name": "First Name",
                "short_desc": "The customer first name",
                "desc": "The first name of the customer contact",
                "example_value": "John",
            },
            "lastName": {
                "type": StringOrNothingType,
                "display_name": "Last Name",
                "short_desc": "The customer last name",
                "desc": "The last name of the customer contact",
                "example_value": "Doe",
            },
            "email": {
                "type": StringOrNothingType,
                "display_name": "Email",
                "short_desc": "The customer email",
                "desc": "The email address of the customer",
                "example_value": "john@acme.com",
            },
            "phone": {
                "type": StringOrNothingType,
                "display_name": "Phone",
                "short_desc": "The customer phone number",
                "desc": "The primary phone number of the customer",
                "example_value": "+1-555-0100",
            },
            "fax": {
                "type": StringOrNothingType,
                "display_name": "Fax",
                "short_desc": "The customer fax number",
                "desc": "The fax number of the customer",
                "example_value": "+1-555-0101",
            },
            "mobile": {
                "type": StringOrNothingType,
                "display_name": "Mobile",
                "short_desc": "The customer mobile number",
                "desc": "The mobile phone number of the customer",
                "example_value": "+1-555-0102",
            },
            "tollFree": {
                "type": StringOrNothingType,
                "display_name": "Toll Free",
                "short_desc": "The toll-free number",
                "desc": "The toll-free phone number of the customer",
                "example_value": "1-800-555-0100",
            },
            "website": {
                "type": StringOrNothingType,
                "display_name": "Website",
                "short_desc": "The customer website",
                "desc": "The website URL of the customer",
                "example_value": "https://www.acme.com",
            },
            "internalNotes": {
                "type": StringOrNothingType,
                "display_name": "Internal Notes",
                "short_desc": "Internal notes about the customer",
                "desc": "Internal notes about the customer (not visible to the customer)",
                "example_value": "Preferred payment: wire transfer",
            },
            "currency": {
                "type": StringOrNothingType,
                "display_name": "Currency",
                "short_desc": "The currency code",
                "desc": "The ISO 4217 currency code for the customer (e.g., USD, CAD)",
                "example_value": "USD",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Wave customer update input type
public class WaveCustomerUpdateDataType inherits HashDataType {
    private {
        #! Field definitions for updating customers
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The customer ID (required)",
                "desc": "The unique identifier of the customer to update",
                "required": True,
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "The customer name",
                "desc": "The new full name of the customer",
                "example_value": "Acme Corp",
            },
            "firstName": {
                "type": StringOrNothingType,
                "display_name": "First Name",
                "short_desc": "The customer first name",
                "desc": "The new first name of the customer contact",
                "example_value": "John",
            },
            "lastName": {
                "type": StringOrNothingType,
                "display_name": "Last Name",
                "short_desc": "The customer last name",
                "desc": "The new last name of the customer contact",
                "example_value": "Doe",
            },
            "email": {
                "type": StringOrNothingType,
                "display_name": "Email",
                "short_desc": "The customer email",
                "desc": "The new email address of the customer",
                "example_value": "john@acme.com",
            },
            "phone": {
                "type": StringOrNothingType,
                "display_name": "Phone",
                "short_desc": "The customer phone number",
                "desc": "The new primary phone number of the customer",
                "example_value": "+1-555-0100",
            },
            "fax": {
                "type": StringOrNothingType,
                "display_name": "Fax",
                "short_desc": "The customer fax number",
                "desc": "The new fax number of the customer",
                "example_value": "+1-555-0101",
            },
            "mobile": {
                "type": StringOrNothingType,
                "display_name": "Mobile",
                "short_desc": "The customer mobile number",
                "desc": "The new mobile phone number of the customer",
                "example_value": "+1-555-0102",
            },
            "tollFree": {
                "type": StringOrNothingType,
                "display_name": "Toll Free",
                "short_desc": "The toll-free number",
                "desc": "The new toll-free phone number of the customer",
                "example_value": "1-800-555-0100",
            },
            "website": {
                "type": StringOrNothingType,
                "display_name": "Website",
                "short_desc": "The customer website",
                "desc": "The new website URL of the customer",
                "example_value": "https://www.acme.com",
            },
            "internalNotes": {
                "type": StringOrNothingType,
                "display_name": "Internal Notes",
                "short_desc": "Internal notes about the customer",
                "desc": "The new internal notes about the customer",
                "example_value": "Preferred payment: wire transfer",
            },
            "currency": {
                "type": StringOrNothingType,
                "display_name": "Currency",
                "short_desc": "The currency code",
                "desc": "The new ISO 4217 currency code for the customer",
                "example_value": "USD",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace WaveDataProvider
