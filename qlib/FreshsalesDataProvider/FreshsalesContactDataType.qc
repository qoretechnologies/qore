# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore FreshsalesDataProvider module definition

/*  FreshsalesContactDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the FreshsalesDataProvider module
public namespace FreshsalesDataProvider {

#! Freshsales contact record type
public class FreshsalesContactDataType inherits HashDataType {
    public {
        #! Field definitions for Freshsales contacts
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "ID",
                "short_desc": "The contact ID",
                "desc": "The unique identifier of the contact",
            },
            "first_name": {
                "type": StringOrNothingType,
                "display_name": "First Name",
                "short_desc": "Contact's first name",
                "desc": "The first name of the contact",
            },
            "last_name": {
                "type": StringOrNothingType,
                "display_name": "Last Name",
                "short_desc": "Contact's last name",
                "desc": "The last name of the contact",
            },
            "email": {
                "type": StringOrNothingType,
                "display_name": "Email",
                "short_desc": "Primary email address",
                "desc": "The primary email address of the contact",
            },
            "mobile_number": {
                "type": StringOrNothingType,
                "display_name": "Mobile Number",
                "short_desc": "Mobile phone number",
                "desc": "The mobile phone number of the contact",
            },
            "work_number": {
                "type": StringOrNothingType,
                "display_name": "Work Number",
                "short_desc": "Work phone number",
                "desc": "The work phone number of the contact",
            },
            "job_title": {
                "type": StringOrNothingType,
                "display_name": "Job Title",
                "short_desc": "Professional title",
                "desc": "The job title or professional designation of the contact",
            },
            "address": {
                "type": StringOrNothingType,
                "display_name": "Address",
                "short_desc": "Street address",
                "desc": "The street address of the contact",
            },
            "city": {
                "type": StringOrNothingType,
                "display_name": "City",
                "short_desc": "City",
                "desc": "The city of the contact",
            },
            "state": {
                "type": StringOrNothingType,
                "display_name": "State",
                "short_desc": "State or province",
                "desc": "The state or province of the contact",
            },
            "zipcode": {
                "type": StringOrNothingType,
                "display_name": "Zip Code",
                "short_desc": "Postal code",
                "desc": "The postal or zip code of the contact",
            },
            "country": {
                "type": StringOrNothingType,
                "display_name": "Country",
                "short_desc": "Country name",
                "desc": "The country of the contact",
            },
            "facebook": {
                "type": StringOrNothingType,
                "display_name": "Facebook",
                "short_desc": "Facebook profile",
                "desc": "The Facebook username or profile URL of the contact",
            },
            "twitter": {
                "type": StringOrNothingType,
                "display_name": "Twitter",
                "short_desc": "Twitter handle",
                "desc": "The Twitter handle of the contact",
            },
            "linkedin": {
                "type": StringOrNothingType,
                "display_name": "LinkedIn",
                "short_desc": "LinkedIn profile",
                "desc": "The LinkedIn profile URL of the contact",
            },
            "owner_id": {
                "type": IntOrNothingType,
                "display_name": "Owner ID",
                "short_desc": "Assigned user ID",
                "desc": "The ID of the user who owns this contact",
            },
            "lead_source_id": {
                "type": IntOrNothingType,
                "display_name": "Lead Source ID",
                "short_desc": "Lead source identifier",
                "desc": "The ID of the lead source for this contact",
            },
            "campaign_id": {
                "type": IntOrNothingType,
                "display_name": "Campaign ID",
                "short_desc": "Campaign identifier",
                "desc": "The ID of the campaign associated with this contact",
            },
            "territory_id": {
                "type": IntOrNothingType,
                "display_name": "Territory ID",
                "short_desc": "Territory identifier",
                "desc": "The ID of the territory this contact belongs to",
            },
            "contact_status_id": {
                "type": IntOrNothingType,
                "display_name": "Contact Status ID",
                "short_desc": "Status identifier",
                "desc": "The ID of the contact's status classification",
            },
            "lifecycle_stage_id": {
                "type": IntOrNothingType,
                "display_name": "Lifecycle Stage ID",
                "short_desc": "Lifecycle stage identifier",
                "desc": "The ID of the contact's lifecycle stage",
            },
            "external_id": {
                "type": StringOrNothingType,
                "display_name": "External ID",
                "short_desc": "External reference ID",
                "desc": "An external reference identifier for the contact",
            },
            "created_at": {
                "type": StringOrNothingType,
                "display_name": "Created At",
                "short_desc": "Creation timestamp",
                "desc": "The timestamp when the contact was created",
            },
            "updated_at": {
                "type": StringOrNothingType,
                "display_name": "Updated At",
                "short_desc": "Last update timestamp",
                "desc": "The timestamp when the contact was last updated",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Freshsales contact request data type for create operations
public class FreshsalesCreateContactRequestDataType inherits HashDataType {
    public {
        #! Field definitions for creating a contact
        const Fields = {
            "first_name": {
                "type": StringType,
                "display_name": "First Name",
                "required": True,
                "short_desc": "Contact's first name",
                "desc": "The first name of the contact",
                "example_value": "Jane",
            },
            "last_name": {
                "type": StringType,
                "display_name": "Last Name",
                "required": True,
                "short_desc": "Contact's last name",
                "desc": "The last name of the contact",
                "example_value": "Smith",
            },
            "email": {
                "type": StringOrNothingType,
                "display_name": "Email",
                "short_desc": "Primary email address",
                "desc": "The primary email address of the contact. At least one of `email`, "
                    "`mobile_number`, or `twitter` is recommended",
                "preselected": True,
                "example_value": "jane.smith@example.com",
            },
            "mobile_number": {
                "type": StringOrNothingType,
                "display_name": "Mobile Number",
                "short_desc": "Mobile phone number",
                "desc": "The mobile phone number of the contact",
                "preselected": True,
            },
            "work_number": {
                "type": StringOrNothingType,
                "display_name": "Work Number",
                "short_desc": "Work phone number",
                "desc": "The work phone number of the contact",
            },
            "job_title": {
                "type": StringOrNothingType,
                "display_name": "Job Title",
                "short_desc": "Professional title",
                "desc": "The job title or professional designation of the contact",
                "preselected": True,
            },
            "address": {
                "type": StringOrNothingType,
                "display_name": "Address",
                "short_desc": "Street address",
                "desc": "The street address of the contact",
            },
            "city": {
                "type": StringOrNothingType,
                "display_name": "City",
                "short_desc": "City",
                "desc": "The city of the contact",
            },
            "state": {
                "type": StringOrNothingType,
                "display_name": "State",
                "short_desc": "State or province",
                "desc": "The state or province of the contact",
            },
            "zipcode": {
                "type": StringOrNothingType,
                "display_name": "Zip Code",
                "short_desc": "Postal code",
                "desc": "The postal or zip code of the contact",
            },
            "country": {
                "type": StringOrNothingType,
                "display_name": "Country",
                "short_desc": "Country name",
                "desc": "The country of the contact",
            },
            "facebook": {
                "type": StringOrNothingType,
                "display_name": "Facebook",
                "short_desc": "Facebook profile",
                "desc": "The Facebook username or profile URL of the contact",
            },
            "twitter": {
                "type": StringOrNothingType,
                "display_name": "Twitter",
                "short_desc": "Twitter handle",
                "desc": "The Twitter handle of the contact",
            },
            "linkedin": {
                "type": StringOrNothingType,
                "display_name": "LinkedIn",
                "short_desc": "LinkedIn profile",
                "desc": "The LinkedIn profile URL of the contact",
            },
            "external_id": {
                "type": StringOrNothingType,
                "display_name": "External ID",
                "short_desc": "External reference ID",
                "desc": "An external reference identifier for the contact",
            },
            "owner_id": {
                "type": SoftIntOrNothingType,
                "display_name": "Owner ID",
                "short_desc": "Assigned user ID",
                "desc": "The ID of the user to assign as owner",
                "ref_data": "owners",
            },
            "lead_source_id": {
                "type": SoftIntOrNothingType,
                "display_name": "Lead Source ID",
                "short_desc": "Lead source identifier",
                "desc": "The ID of the lead source",
                "ref_data": "lead_sources",
            },
            "campaign_id": {
                "type": SoftIntOrNothingType,
                "display_name": "Campaign ID",
                "short_desc": "Campaign identifier",
                "desc": "The ID of the campaign associated with this contact",
                "ref_data": "campaigns",
            },
            "territory_id": {
                "type": SoftIntOrNothingType,
                "display_name": "Territory ID",
                "short_desc": "Territory identifier",
                "desc": "The ID of the territory this contact belongs to",
                "ref_data": "territories",
            },
            "contact_status_id": {
                "type": SoftIntOrNothingType,
                "display_name": "Contact Status ID",
                "short_desc": "Status identifier",
                "desc": "The ID of the contact's status classification",
                "ref_data": "contact_statuses",
            },
            "lifecycle_stage_id": {
                "type": SoftIntOrNothingType,
                "display_name": "Lifecycle Stage ID",
                "short_desc": "Lifecycle stage identifier",
                "desc": "The ID of the contact's lifecycle stage",
                "ref_data": "lifecycle_stages",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Freshsales contact request data type for update operations
public class FreshsalesUpdateContactRequestDataType inherits HashDataType {
    public {
        #! Field definitions for updating a contact
        const Fields = {
            "id": {
                "type": SoftIntType,
                "display_name": "Contact ID",
                "required": True,
                "short_desc": "The contact ID to update",
                "desc": "The unique identifier of the contact to update",
            },
        } + FreshsalesDataProviderBase::makeFieldsOptional(
            FreshsalesCreateContactRequestDataType::Fields);
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Freshsales contact request data type for delete operations
public class FreshsalesDeleteContactRequestDataType inherits HashDataType {
    public {
        #! Field definitions for deleting a contact
        const Fields = {
            "id": {
                "type": SoftIntType,
                "display_name": "Contact ID",
                "required": True,
                "short_desc": "The contact ID to delete",
                "desc": "The unique identifier of the contact to delete",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace FreshsalesDataProvider
