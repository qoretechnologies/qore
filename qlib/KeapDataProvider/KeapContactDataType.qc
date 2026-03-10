# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore KeapDataProvider module definition

/*  KeapContactDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Keap contact record type
public class KeapContactDataType inherits HashDataType {
    private {
        #! Field definitions for Keap contacts
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "The contact ID",
                "desc": "The unique identifier of the contact",
            },
            "email": {
                "type": StringOrNothingType,
                "display_name": "Email",
                "short_desc": "The contact's email address",
                "desc": "The primary email address of the contact",
                "example_value": "john@example.com",
            },
            "given_name": {
                "type": StringOrNothingType,
                "display_name": "First Name",
                "short_desc": "The contact's first name",
                "desc": "The given (first) name of the contact",
                "example_value": "John",
            },
            "family_name": {
                "type": StringOrNothingType,
                "display_name": "Last Name",
                "short_desc": "The contact's last name",
                "desc": "The family (last) name of the contact",
                "example_value": "Doe",
            },
            "phone1": {
                "type": StringOrNothingType,
                "display_name": "Phone",
                "short_desc": "The contact's phone number",
                "desc": "The primary phone number of the contact",
            },
            "company_id": {
                "type": IntOrNothingType,
                "display_name": "Company ID",
                "short_desc": "The associated company ID",
                "desc": "The ID of the company associated with this contact",
            },
            "job_title": {
                "type": StringOrNothingType,
                "display_name": "Job Title",
                "short_desc": "The contact's job title",
                "desc": "The job title of the contact",
                "example_value": "Software Engineer",
            },
            "middle_name": {
                "type": StringOrNothingType,
                "display_name": "Middle Name",
                "short_desc": "The contact's middle name",
                "desc": "The middle name of the contact",
            },
            "preferred_name": {
                "type": StringOrNothingType,
                "display_name": "Preferred Name",
                "short_desc": "The contact's preferred name",
                "desc": "The preferred display name of the contact",
            },
            "prefix": {
                "type": StringOrNothingType,
                "display_name": "Prefix",
                "short_desc": "Name prefix",
                "desc": "The name prefix (e.g., `Mr.`, `Mrs.`, `Dr.`)",
            },
            "suffix": {
                "type": StringOrNothingType,
                "display_name": "Suffix",
                "short_desc": "Name suffix",
                "desc": "The name suffix (e.g., `Jr.`, `Sr.`, `III`)",
            },
            "owner_id": {
                "type": IntOrNothingType,
                "display_name": "Owner ID",
                "short_desc": "The owner user ID",
                "desc": "The ID of the user who owns this contact",
            },
            "website": {
                "type": StringOrNothingType,
                "display_name": "Website",
                "short_desc": "The contact's website",
                "desc": "The website URL of the contact",
            },
            "birth_date": {
                "type": DateOrNothingType,
                "display_name": "Birthday",
                "short_desc": "The contact's birthday",
                "desc": "The birth date of the contact",
            },
            "anniversary_date": {
                "type": DateOrNothingType,
                "display_name": "Anniversary",
                "short_desc": "The contact's anniversary",
                "desc": "The anniversary date of the contact",
            },
            "spouse_name": {
                "type": StringOrNothingType,
                "display_name": "Spouse Name",
                "short_desc": "The contact's spouse name",
                "desc": "The name of the contact's spouse",
            },
            "time_zone": {
                "type": StringOrNothingType,
                "display_name": "Time Zone",
                "short_desc": "The contact's time zone",
                "desc": "The time zone of the contact",
            },
            "date_created": {
                "type": DateOrNothingType,
                "display_name": "Date Created",
                "short_desc": "When the contact was created",
                "desc": "The timestamp when the contact was created",
            },
            "last_updated": {
                "type": DateOrNothingType,
                "display_name": "Last Updated",
                "short_desc": "When the contact was last updated",
                "desc": "The timestamp when the contact was last updated",
            },
            "addresses": {
                "type": new ListDataType(new KeapAddressDataType(AutoHashOrNothingType), True),
                "display_name": "Addresses",
                "short_desc": "The contact's addresses",
                "desc": "A list of addresses associated with the contact",
            },
            "custom_fields": {
                "type": AutoListOrNothingType,
                "display_name": "Custom Fields",
                "short_desc": "Custom field values",
                "desc": "A list of custom field values for the contact",
            },
            "tag_ids": {
                "type": AutoListOrNothingType,
                "display_name": "Tag IDs",
                "short_desc": "Applied tag IDs",
                "desc": "A list of tag IDs applied to the contact",
            },
            "lead_source_id": {
                "type": IntOrNothingType,
                "display_name": "Lead Source ID",
                "short_desc": "The lead source ID",
                "desc": "The ID of the lead source for this contact",
            },
            "opt_in_reason": {
                "type": StringOrNothingType,
                "display_name": "Opt-In Reason",
                "short_desc": "The opt-in reason",
                "desc": "The reason the contact opted in to communications",
            },
            "source_type": {
                "type": StringOrNothingType,
                "display_name": "Source Type",
                "short_desc": "The contact source type",
                "desc": "The source type indicating how the contact was created",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "API", "desc": "Created via API"},
                    <AllowedValueInfo>{"value": "WEBFORM", "desc": "Created via web form"},
                    <AllowedValueInfo>{"value": "IMPORT", "desc": "Created via import"},
                ),
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace KeapDataProvider
