# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SquareDataProvider module definition

/*  SquareCustomerDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SquareDataProvider module
public namespace SquareDataProvider {

#! Square customer record type
public class SquareCustomerDataType inherits HashDataType {
    private {
        #! Field definitions for Square customers
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The customer ID",
                "desc": "The unique identifier of the customer",
            },
            "created_at": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the customer was created",
                "desc": "The timestamp when the customer was created (RFC 3339 format)",
            },
            "updated_at": {
                "type": StringOrNothingType,
                "display_name": "Updated At",
                "short_desc": "When the customer was last updated",
                "desc": "The timestamp when the customer was last updated (RFC 3339 format)",
            },
            "given_name": {
                "type": StringOrNothingType,
                "display_name": "First Name",
                "short_desc": "The customer's first name",
                "desc": "The given (first) name of the customer",
                "example_value": "John",
            },
            "family_name": {
                "type": StringOrNothingType,
                "display_name": "Last Name",
                "short_desc": "The customer's last name",
                "desc": "The family (last) name of the customer",
                "example_value": "Doe",
            },
            "email_address": {
                "type": StringOrNothingType,
                "display_name": "Email",
                "short_desc": "The customer's email address",
                "desc": "The email address of the customer",
                "example_value": "john@example.com",
            },
            "phone_number": {
                "type": StringOrNothingType,
                "display_name": "Phone Number",
                "short_desc": "The customer's phone number",
                "desc": "The phone number of the customer",
            },
            "company_name": {
                "type": StringOrNothingType,
                "display_name": "Company Name",
                "short_desc": "The customer's company name",
                "desc": "The company name associated with the customer",
            },
            "nickname": {
                "type": StringOrNothingType,
                "display_name": "Nickname",
                "short_desc": "The customer's nickname",
                "desc": "A nickname for the customer",
            },
            "address": {
                "type": AutoHashOrNothingType,
                "display_name": "Address",
                "short_desc": "The customer's address",
                "desc": "The physical address of the customer (hash with address_line_1, "
                    "address_line_2, locality, administrative_district_level_1, postal_code, country)",
            },
            "note": {
                "type": StringOrNothingType,
                "display_name": "Note",
                "short_desc": "Notes about the customer",
                "desc": "A note about the customer",
            },
            "birthday": {
                "type": StringOrNothingType,
                "display_name": "Birthday",
                "short_desc": "The customer's birthday",
                "desc": "The customer's birthday in YYYY-MM-DD format",
            },
            "reference_id": {
                "type": StringOrNothingType,
                "display_name": "Reference ID",
                "short_desc": "External reference ID",
                "desc": "An optional reference ID for the customer from an external system",
            },
            "version": {
                "type": IntOrNothingType,
                "display_name": "Version",
                "short_desc": "The current version of the customer",
                "desc": "The current version of the customer profile (for optimistic concurrency)",
            },
            "creation_source": {
                "type": StringOrNothingType,
                "display_name": "Creation Source",
                "short_desc": "How the customer was created",
                "desc": "The source that created the customer (e.g., THIRD_PARTY, DIRECTORY, API)",
            },
            "group_ids": {
                "type": AutoListOrNothingType,
                "display_name": "Group IDs",
                "short_desc": "Customer group IDs",
                "desc": "The IDs of customer groups the customer belongs to",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace SquareDataProvider
