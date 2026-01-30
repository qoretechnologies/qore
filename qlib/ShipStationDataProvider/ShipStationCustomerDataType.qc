# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShipStationDataProvider module definition

/*  ShipStationCustomerDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ShipStationDataProvider module
public namespace ShipStationDataProvider {

#! ShipStation customer data type
public class ShipStationCustomerDataType inherits HashDataType {
    #! Creates the object
    constructor() : HashDataType("ShipStationCustomer", {
        "customerId": {
            "display_name": "Customer ID",
            "short_desc": "Unique identifier for the customer",
            "type": IntType,
            "desc": "The unique numeric identifier for the customer",
        },
        "createDate": {
            "display_name": "Create Date",
            "short_desc": "Date the customer was created",
            "type": SoftStringOrNothingType,
            "desc": "The date and time the customer was first created (ISO 8601 format)",
        },
        "modifyDate": {
            "display_name": "Modify Date",
            "short_desc": "Date the customer was last modified",
            "type": SoftStringOrNothingType,
            "desc": "The date and time the customer was last modified (ISO 8601 format)",
        },
        "name": {
            "display_name": "Name",
            "short_desc": "Customer name",
            "type": SoftStringOrNothingType,
            "desc": "The customer's full name",
        },
        "company": {
            "display_name": "Company",
            "short_desc": "Company name",
            "type": SoftStringOrNothingType,
            "desc": "The customer's company name, if applicable",
        },
        "street1": {
            "display_name": "Street 1",
            "short_desc": "First line of street address",
            "type": SoftStringOrNothingType,
            "desc": "The first line of the customer's street address",
        },
        "street2": {
            "display_name": "Street 2",
            "short_desc": "Second line of street address",
            "type": SoftStringOrNothingType,
            "desc": "The second line of the customer's street address",
        },
        "city": {
            "display_name": "City",
            "short_desc": "City name",
            "type": SoftStringOrNothingType,
            "desc": "The customer's city",
        },
        "state": {
            "display_name": "State/Province",
            "short_desc": "State or province code",
            "type": SoftStringOrNothingType,
            "desc": "The customer's state or province code",
        },
        "postalCode": {
            "display_name": "Postal Code",
            "short_desc": "ZIP or postal code",
            "type": SoftStringOrNothingType,
            "desc": "The customer's postal or ZIP code",
        },
        "countryCode": {
            "display_name": "Country Code",
            "short_desc": "Two-letter country code",
            "type": SoftStringOrNothingType,
            "desc": "The two-letter ISO country code (e.g., 'US', 'CA', 'GB')",
        },
        "phone": {
            "display_name": "Phone",
            "short_desc": "Phone number",
            "type": SoftStringOrNothingType,
            "desc": "The customer's phone number",
        },
        "email": {
            "display_name": "Email",
            "short_desc": "Email address",
            "type": SoftStringOrNothingType,
            "desc": "The customer's email address",
        },
        "addressVerified": {
            "display_name": "Address Verified",
            "short_desc": "Whether the address has been verified",
            "type": SoftStringOrNothingType,
            "desc": "The verification status of the customer's address",
        },
        "marketplaceUsernames": {
            "display_name": "Marketplace Usernames",
            "short_desc": "List of marketplace usernames",
            "type": SoftAutoListOrNothingType,
            "desc": "List of marketplace usernames associated with this customer",
        },
        "tags": {
            "display_name": "Tags",
            "short_desc": "Tags assigned to customer",
            "type": SoftAutoListOrNothingType,
            "desc": "List of tags assigned to this customer",
        },
    }) {
    }
}

} # namespace ShipStationDataProvider
