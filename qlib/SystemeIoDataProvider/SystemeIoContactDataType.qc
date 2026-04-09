# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SystemeIoDataProvider module definition

/*  SystemeIoContactDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SystemeIoDataProvider module
public namespace SystemeIoDataProvider {

#! Systeme.io contact custom field value type (slug + value pair)
public class SystemeIoContactFieldValueDataType inherits HashDataType {
    public {
        const Fields = {
            "slug": {
                "type": StringType,
                "display_name": "Slug",
                "short_desc": "Field slug identifier",
                "desc": "The unique slug identifier of the custom contact field",
            },
            "value": {
                "type": StringOrNothingType,
                "display_name": "Value",
                "short_desc": "Field value",
                "desc": "The value of the custom contact field",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Systeme.io contact record type
public class SystemeIoContactDataType inherits HashDataType {
    public {
        #! Field definitions for Systeme.io contacts
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "ID",
                "short_desc": "The contact ID",
                "desc": "The unique identifier of the contact",
            },
            "email": {
                "type": StringOrNothingType,
                "display_name": "Email",
                "short_desc": "Email address",
                "desc": "The email address of the contact",
            },
            "firstName": {
                "type": StringOrNothingType,
                "display_name": "First Name",
                "short_desc": "Contact's first name",
                "desc": "The first name of the contact",
            },
            "lastName": {
                "type": StringOrNothingType,
                "display_name": "Last Name",
                "short_desc": "Contact's last name",
                "desc": "The last name of the contact",
            },
            "phoneNumber": {
                "type": StringOrNothingType,
                "display_name": "Phone Number",
                "short_desc": "Phone number",
                "desc": "The phone number of the contact",
            },
            "locale": {
                "type": StringOrNothingType,
                "display_name": "Locale",
                "short_desc": "Contact locale",
                "desc": "The locale of the contact (e.g., `en` for English)",
            },
            "tags": {
                "type": new ListDataType("Tags", new SystemeIoTagDataType(), True),
                "display_name": "Tags",
                "short_desc": "Assigned tags",
                "desc": "List of tags assigned to the contact",
            },
            "fields": {
                "type": new ListDataType("Fields", new SystemeIoContactFieldValueDataType(), True),
                "display_name": "Custom Fields",
                "short_desc": "Custom field values",
                "desc": "List of custom field values, each with `slug` and `value` properties",
            },
            "createdAt": {
                "type": DateOrNothingType,
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "The date and time when the contact was created",
            },
            "updatedAt": {
                "type": DateOrNothingType,
                "display_name": "Updated At",
                "short_desc": "Last update timestamp",
                "desc": "The date and time when the contact was last updated",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Systeme.io create contact request data type
public class SystemeIoCreateContactRequestDataType inherits HashDataType {
    public {
        #! Field definitions for creating a contact
        const Fields = {
            "email": {
                "type": StringType,
                "display_name": "Email",
                "short_desc": "Email address",
                "desc": "The email address of the contact to create",
                "required": True,
                "example_value": "john@example.com",
            },
            "firstName": {
                "type": StringOrNothingType,
                "display_name": "First Name",
                "short_desc": "Contact's first name",
                "desc": "The first name of the contact",
                "attr": {"preselected": True},
                "example_value": "John",
            },
            "lastName": {
                "type": StringOrNothingType,
                "display_name": "Last Name",
                "short_desc": "Contact's last name",
                "desc": "The last name of the contact",
                "attr": {"preselected": True},
                "example_value": "Doe",
            },
            "phoneNumber": {
                "type": StringOrNothingType,
                "display_name": "Phone Number",
                "short_desc": "Phone number",
                "desc": "The phone number of the contact",
                "example_value": "+1-555-0100",
            },
            "locale": {
                "type": StringOrNothingType,
                "display_name": "Locale",
                "short_desc": "Contact locale",
                "desc": "The locale of the contact (e.g., `en` for English, `fr` for French)",
                "example_value": "en",
            },
            "fields": {
                "type": new ListDataType("Fields", new SystemeIoContactFieldValueDataType(), True),
                "display_name": "Custom Fields",
                "short_desc": "Custom field values",
                "desc": "List of custom field values to set on the contact. Each entry should be "
                    "a hash with `slug` and `value` properties.\n\n"
                    "**Example:**\n"
                    "```\n"
                    "[{\"slug\": \"company\", \"value\": \"Acme Corp\"}]\n"
                    "```",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Systeme.io update contact request data type
public class SystemeIoUpdateContactRequestDataType inherits HashDataType {
    public {
        #! Field definitions for updating a contact
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "Contact ID",
                "short_desc": "The contact ID to update",
                "desc": "The unique identifier of the contact to update",
                "required": True,
                "ref_data": "contacts",
            },
        } + SystemeIoDataProviderBase::makeFieldsOptional(
            SystemeIoCreateContactRequestDataType::Fields);
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Systeme.io delete contact request data type
public class SystemeIoDeleteContactRequestDataType inherits HashDataType {
    public {
        #! Field definitions for deleting a contact
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "Contact ID",
                "short_desc": "The contact ID to delete",
                "desc": "The unique identifier of the contact to delete",
                "required": True,
                "ref_data": "contacts",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace SystemeIoDataProvider
