# -*- mode: qore; indent-tabs-mode: nil -*-

/*  ConstantContactContactDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace ConstantContactDataProvider {

#! Constant Contact contact data type
public class ConstantContactContactDataType inherits HashDataType {
    public {
        const Fields = {
            "contact_id": {
                "type": StringType,
                "display_name": "Contact ID",
                "short_desc": "The unique contact identifier",
                "desc": "The unique identifier for the contact (UUID format)",
            },
            "email_address": {
                "type": AutoHashOrNothingType,
                "display_name": "Email Address",
                "short_desc": "The contact's email address object",
                "desc": "The contact's email address object containing `address`, "
                    "`permission_to_send`, `created_at`, `updated_at`, `opt_in_source`, "
                    "and `opt_in_date` fields",
            },
            "first_name": {
                "type": StringOrNothingType,
                "display_name": "First Name",
                "short_desc": "The contact's first name",
                "desc": "The first name of the contact",
                "example_value": "John",
            },
            "last_name": {
                "type": StringOrNothingType,
                "display_name": "Last Name",
                "short_desc": "The contact's last name",
                "desc": "The last name of the contact",
                "example_value": "Doe",
            },
            "job_title": {
                "type": StringOrNothingType,
                "display_name": "Job Title",
                "short_desc": "The contact's job title",
                "desc": "The job title of the contact",
            },
            "company_name": {
                "type": StringOrNothingType,
                "display_name": "Company Name",
                "short_desc": "The contact's company name",
                "desc": "The company name associated with the contact",
            },
            "birthday_month": {
                "type": IntOrNothingType,
                "display_name": "Birthday Month",
                "short_desc": "The birthday month (1-12)",
                "desc": "The birthday month of the contact (1-12)",
            },
            "birthday_day": {
                "type": IntOrNothingType,
                "display_name": "Birthday Day",
                "short_desc": "The birthday day (1-31)",
                "desc": "The birthday day of the contact (1-31)",
            },
            "anniversary": {
                "type": StringOrNothingType,
                "display_name": "Anniversary",
                "short_desc": "Anniversary date",
                "desc": "The anniversary date of the contact in `YYYY-MM-DD` format",
            },
            "update_source": {
                "type": StringOrNothingType,
                "display_name": "Update Source",
                "short_desc": "Source of last update",
                "desc": "The source of the last update to the contact. Values: `Account`, "
                    "`Contact`",
            },
            "create_source": {
                "type": StringOrNothingType,
                "display_name": "Create Source",
                "short_desc": "Source of contact creation",
                "desc": "The source of the contact creation. Values: `Account`, `Contact`",
            },
            "created_at": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the contact was created",
                "desc": "The timestamp when the contact was created (ISO-8601 format)",
            },
            "updated_at": {
                "type": StringOrNothingType,
                "display_name": "Updated At",
                "short_desc": "When the contact was last updated",
                "desc": "The timestamp when the contact was last updated (ISO-8601 format)",
            },
            "deleted_at": {
                "type": StringOrNothingType,
                "display_name": "Deleted At",
                "short_desc": "When the contact was deleted",
                "desc": "The timestamp when the contact was deleted (ISO-8601 format), "
                    "if applicable",
            },
            "custom_fields": {
                "type": AutoListOrNothingType,
                "display_name": "Custom Fields",
                "short_desc": "Contact custom field values",
                "desc": "A list of custom field value objects for the contact, each containing "
                    "`custom_field_id` and `value`",
            },
            "phone_numbers": {
                "type": AutoListOrNothingType,
                "display_name": "Phone Numbers",
                "short_desc": "Contact phone numbers",
                "desc": "A list of phone number objects for the contact",
            },
            "street_addresses": {
                "type": AutoListOrNothingType,
                "display_name": "Street Addresses",
                "short_desc": "Contact street addresses",
                "desc": "A list of street address objects for the contact",
            },
            "list_memberships": {
                "type": AutoListOrNothingType,
                "display_name": "List Memberships",
                "short_desc": "List IDs the contact belongs to",
                "desc": "A list of contact list IDs that the contact is a member of",
            },
            "taggings": {
                "type": AutoListOrNothingType,
                "display_name": "Tags",
                "short_desc": "Tag IDs assigned to the contact",
                "desc": "A list of tag IDs assigned to the contact",
            },
            "notes": {
                "type": AutoListOrNothingType,
                "display_name": "Notes",
                "short_desc": "Contact notes",
                "desc": "A list of note objects associated with the contact",
            },
            "sms_channel": {
                "type": AutoHashOrNothingType,
                "display_name": "SMS Channel",
                "short_desc": "SMS channel details",
                "desc": "SMS channel consent and details for the contact",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Constant Contact create contact request data type
public class ConstantContactCreateContactRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "email_address": {
                "type": AutoHashOrNothingType,
                "display_name": "Email Address",
                "short_desc": "The contact's email address",
                "desc": "The contact's email address object. Must contain an `address` field "
                    "with the email address string and an optional `permission_to_send` field",
                "example_value": {"address": "john@example.com", "permission_to_send": "implicit"},
            },
            "first_name": {
                "type": StringOrNothingType,
                "display_name": "First Name",
                "short_desc": "The contact's first name",
                "desc": "The first name of the contact",
                "example_value": "John",
            },
            "last_name": {
                "type": StringOrNothingType,
                "display_name": "Last Name",
                "short_desc": "The contact's last name",
                "desc": "The last name of the contact",
                "example_value": "Doe",
            },
            "job_title": {
                "type": StringOrNothingType,
                "display_name": "Job Title",
                "short_desc": "The contact's job title",
                "desc": "The job title of the contact",
            },
            "company_name": {
                "type": StringOrNothingType,
                "display_name": "Company Name",
                "short_desc": "The contact's company name",
                "desc": "The company name associated with the contact",
            },
            "birthday_month": {
                "type": IntOrNothingType,
                "display_name": "Birthday Month",
                "short_desc": "The birthday month (1-12)",
                "desc": "The birthday month of the contact (1-12)",
            },
            "birthday_day": {
                "type": IntOrNothingType,
                "display_name": "Birthday Day",
                "short_desc": "The birthday day (1-31)",
                "desc": "The birthday day of the contact (1-31)",
            },
            "anniversary": {
                "type": StringOrNothingType,
                "display_name": "Anniversary",
                "short_desc": "Anniversary date (YYYY-MM-DD)",
                "desc": "The anniversary date of the contact in `YYYY-MM-DD` format",
            },
            "create_source": {
                "type": StringOrNothingType,
                "display_name": "Create Source",
                "short_desc": "Source of contact creation",
                "desc": "The source of the contact creation. Values: `Account`, `Contact`",
            },
            "custom_fields": {
                "type": AutoListOrNothingType,
                "display_name": "Custom Fields",
                "short_desc": "Custom field values",
                "desc": "A list of custom field value objects, each containing "
                    "`custom_field_id` and `value`",
            },
            "phone_numbers": {
                "type": AutoListOrNothingType,
                "display_name": "Phone Numbers",
                "short_desc": "Phone numbers",
                "desc": "A list of phone number objects for the contact",
            },
            "street_addresses": {
                "type": AutoListOrNothingType,
                "display_name": "Street Addresses",
                "short_desc": "Street addresses",
                "desc": "A list of street address objects for the contact",
            },
            "list_memberships": {
                "type": AutoListOrNothingType,
                "display_name": "List Memberships",
                "short_desc": "Contact list IDs to add contact to",
                "desc": "A list of contact list IDs to add the contact to",
            },
            "taggings": {
                "type": AutoListOrNothingType,
                "display_name": "Tags",
                "short_desc": "Tag IDs to assign",
                "desc": "A list of tag IDs to assign to the contact",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Constant Contact update contact request data type
public class ConstantContactUpdateContactRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "contact_id": {
                "type": StringType,
                "display_name": "Contact ID",
                "short_desc": "The contact ID to update",
                "desc": "The unique identifier of the contact to update",
            },
        } + ConstantContactCreateContactRequestDataType::Fields + {
            "update_source": {
                "type": StringOrNothingType,
                "display_name": "Update Source",
                "short_desc": "Source of the update",
                "desc": "The source of the update. Values: `Account`, `Contact`",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace ConstantContactDataProvider
