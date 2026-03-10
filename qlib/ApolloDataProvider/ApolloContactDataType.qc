# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ApolloDataProvider module definition

/*  ApolloContactDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ApolloDataProvider module
public namespace ApolloDataProvider {

#! Apollo contact record type
public class ApolloContactDataType inherits HashDataType {
    public {
        #! Field definitions for Apollo contacts
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "ID",
                "short_desc": "Contact ID",
                "desc": "The unique identifier of the contact",
            },
            "first_name": {
                "type": StringOrNothingType,
                "display_name": "First Name",
                "short_desc": "First name",
                "desc": "The first name of the contact",
                "example_value": "John",
            },
            "last_name": {
                "type": StringOrNothingType,
                "display_name": "Last Name",
                "short_desc": "Last name",
                "desc": "The last name of the contact",
                "example_value": "Doe",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Full Name",
                "short_desc": "Full name",
                "desc": "The full name of the contact",
            },
            "email": {
                "type": StringOrNothingType,
                "display_name": "Email",
                "short_desc": "Primary email",
                "desc": "The primary email address of the contact",
                "example_value": "john@example.com",
            },
            "title": {
                "type": StringOrNothingType,
                "display_name": "Title",
                "short_desc": "Job title",
                "desc": "The job title of the contact",
                "example_value": "Software Engineer",
            },
            "organization_name": {
                "type": StringOrNothingType,
                "display_name": "Organization Name",
                "short_desc": "Company name",
                "desc": "The name of the company the contact belongs to",
            },
            "organization_id": {
                "type": StringOrNothingType,
                "display_name": "Organization ID",
                "short_desc": "Organization ID",
                "desc": "The unique identifier of the contact's organization",
            },
            "account_id": {
                "type": StringOrNothingType,
                "display_name": "Account ID",
                "short_desc": "Associated account ID",
                "desc": "The unique identifier of the associated account",
            },
            "owner_id": {
                "type": StringOrNothingType,
                "display_name": "Owner ID",
                "short_desc": "Owner user ID",
                "desc": "The unique identifier of the user who owns this contact",
            },
            "person_id": {
                "type": StringOrNothingType,
                "display_name": "Person ID",
                "short_desc": "Person record ID",
                "desc": "The unique identifier of the person record",
            },
            "headline": {
                "type": StringOrNothingType,
                "display_name": "Headline",
                "short_desc": "Professional headline",
                "desc": "The professional headline of the contact",
            },
            "linkedin_url": {
                "type": StringOrNothingType,
                "display_name": "LinkedIn URL",
                "short_desc": "LinkedIn profile URL",
                "desc": "The LinkedIn profile URL of the contact",
            },
            "photo_url": {
                "type": StringOrNothingType,
                "display_name": "Photo URL",
                "short_desc": "Profile photo URL",
                "desc": "The URL of the contact's profile photo",
            },
            "present_raw_address": {
                "type": StringOrNothingType,
                "display_name": "Present Address",
                "short_desc": "Current location",
                "desc": "The current location or address of the contact",
            },
            "source": {
                "type": StringOrNothingType,
                "display_name": "Source",
                "short_desc": "Data source",
                "desc": "The source from which the contact data was obtained",
            },
            "email_status": {
                "type": StringOrNothingType,
                "display_name": "Email Status",
                "short_desc": "Email verification status",
                "desc": "The verification status of the contact's email address",
            },
            "contact_stage_id": {
                "type": StringOrNothingType,
                "display_name": "Contact Stage ID",
                "short_desc": "Pipeline stage ID",
                "desc": "The unique identifier of the contact's pipeline stage",
            },
            "time_zone": {
                "type": StringOrNothingType,
                "display_name": "Time Zone",
                "short_desc": "Timezone",
                "desc": "The timezone of the contact",
            },
            "sanitized_phone": {
                "type": StringOrNothingType,
                "display_name": "Phone",
                "short_desc": "Phone number",
                "desc": "The sanitized phone number of the contact",
            },
            "created_at": {
                "type": DateOrNothingType,
                "display_name": "Created At",
                "short_desc": "Created timestamp",
                "desc": "The timestamp when the contact was created",
            },
            "updated_at": {
                "type": DateOrNothingType,
                "display_name": "Updated At",
                "short_desc": "Updated timestamp",
                "desc": "The timestamp when the contact was last updated",
            },
            "label_ids": {
                "type": AutoListOrNothingType,
                "display_name": "Label IDs",
                "short_desc": "Label IDs",
                "desc": "The list of label IDs associated with the contact",
            },
            "typed_custom_fields": {
                "type": AutoHashOrNothingType,
                "display_name": "Custom Fields",
                "short_desc": "Custom fields",
                "desc": "Custom fields associated with the contact",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace ApolloDataProvider
