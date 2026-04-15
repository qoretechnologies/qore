# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore InsightlyDataProvider module definition

/*  InsightlyContactDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the InsightlyDataProvider module
public namespace InsightlyDataProvider {

#! Insightly contact record type
public class InsightlyContactDataType inherits HashDataType {
    public {
        #! Field definitions for Insightly contacts
        const Fields = {
            "CONTACT_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Contact ID",
                "short_desc": "The unique contact identifier",
                "desc": "The unique identifier of the contact",
            },
            "FIRST_NAME": {
                "type": SoftStringOrNothingType,
                "display_name": "First Name",
                "short_desc": "Contact's first name",
                "desc": "The first name of the contact",
            },
            "LAST_NAME": {
                "type": SoftStringOrNothingType,
                "display_name": "Last Name",
                "short_desc": "Contact's last name",
                "desc": "The last name of the contact",
            },
            "SALUTATION": {
                "type": SoftStringOrNothingType,
                "display_name": "Salutation",
                "short_desc": "Contact's salutation",
                "desc": "The salutation for the contact (e.g., `Mr`, `Ms`, `Dr`)",
            },
            "TITLE": {
                "type": SoftStringOrNothingType,
                "display_name": "Title",
                "short_desc": "Job title",
                "desc": "The job title of the contact",
            },
            "EMAIL_ADDRESS": {
                "type": SoftStringOrNothingType,
                "display_name": "Email Address",
                "short_desc": "Primary email address",
                "desc": "The primary email address of the contact",
            },
            "PHONE": {
                "type": SoftStringOrNothingType,
                "display_name": "Phone",
                "short_desc": "Phone number",
                "desc": "The phone number of the contact",
            },
            "MOBILE": {
                "type": SoftStringOrNothingType,
                "display_name": "Mobile",
                "short_desc": "Mobile phone number",
                "desc": "The mobile phone number of the contact",
            },
            "FAX": {
                "type": SoftStringOrNothingType,
                "display_name": "Fax",
                "short_desc": "Fax number",
                "desc": "The fax number of the contact",
            },
            "BACKGROUND": {
                "type": SoftStringOrNothingType,
                "display_name": "Background",
                "short_desc": "Background information",
                "desc": "Background information about the contact",
            },
            "IMAGE_URL": {
                "type": SoftStringOrNothingType,
                "display_name": "Image URL",
                "short_desc": "Profile image URL",
                "desc": "The URL of the contact's profile image",
            },
            "OWNER_USER_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Owner User ID",
                "short_desc": "The owner user identifier",
                "desc": "The ID of the user who owns this contact",
            },
            "DATE_OF_BIRTH": {
                "type": SoftDateOrNothingType,
                "display_name": "Date of Birth",
                "short_desc": "Contact's date of birth",
                "desc": "The date of birth of the contact",
            },
            "ORGANISATION_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Organisation ID",
                "short_desc": "Linked organisation identifier",
                "desc": "The ID of the organisation this contact belongs to",
            },
            "VISIBLE_TO": {
                "type": SoftStringOrNothingType,
                "display_name": "Visible To",
                "short_desc": "Visibility setting",
                "desc": "Who can see this contact",
                "allowed_values": VisibleToAllowedValues,
            },
            "VISIBLE_TEAM_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Visible Team ID",
                "short_desc": "Team visibility identifier",
                "desc": "The ID of the team that can see this contact when `VISIBLE_TO` is "
                    "`TEAM`",
            },
            "CUSTOMFIELDS": {
                "type": new ListDataType("InsightlyContactCustomFieldList",
                    new InsightlyCustomFieldDataType(), True),
                "display_name": "Custom Fields",
                "short_desc": "Custom field values",
                "desc": "A list of custom field values for the contact",
            },
            "ADDRESSES": {
                "type": new ListDataType("InsightlyContactAddressList",
                    new InsightlyAddressDataType(), True),
                "display_name": "Addresses",
                "short_desc": "Contact addresses",
                "desc": "A list of addresses associated with the contact",
            },
            "CONTACTINFOS": {
                "type": new ListDataType("InsightlyContactContactInfoList",
                    new InsightlyContactInfoDataType(), True),
                "display_name": "Contact Infos",
                "short_desc": "Additional contact information",
                "desc": "A list of additional contact information entries (phone, email, "
                    "website, social)",
            },
            "LINKS": {
                "type": new ListDataType("InsightlyContactLinkList",
                    new InsightlyLinkDataType(), True),
                "display_name": "Links",
                "short_desc": "Related object links",
                "desc": "A list of links to related objects",
            },
            "TAGS": {
                "type": new ListDataType("InsightlyContactTagList",
                    new InsightlyTagDataType(), True),
                "display_name": "Tags",
                "short_desc": "Contact tags",
                "desc": "A list of tags applied to the contact",
            },
            "DATES": {
                "type": new ListDataType("InsightlyContactDateList",
                    new InsightlyDateDataType(), True),
                "display_name": "Dates",
                "short_desc": "Important dates",
                "desc": "A list of important dates (occasions) for the contact",
            },
            "DATE_CREATED_UTC": {
                "type": SoftDateOrNothingType,
                "display_name": "Date Created",
                "short_desc": "Creation timestamp (UTC)",
                "desc": "The UTC timestamp when the contact was created",
            },
            "DATE_UPDATED_UTC": {
                "type": SoftDateOrNothingType,
                "display_name": "Date Updated",
                "short_desc": "Last update timestamp (UTC)",
                "desc": "The UTC timestamp when the contact was last updated",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Insightly contact request data type for create operations
public class InsightlyCreateContactRequestDataType inherits HashDataType {
    public {
        #! Field definitions for creating a contact
        const Fields = {
            "LAST_NAME": {
                "type": SoftStringType,
                "display_name": "Last Name",
                "required": True,
                "short_desc": "Contact's last name",
                "desc": "The last name of the contact (required)",
                "example_value": "Smith",
            },
            "FIRST_NAME": {
                "type": SoftStringType,
                "display_name": "First Name",
                "required": True,
                "short_desc": "Contact's first name",
                "desc": "The first name of the contact (required)",
                "example_value": "Jane",
            },
            "SALUTATION": {
                "type": SoftStringOrNothingType,
                "display_name": "Salutation",
                "short_desc": "Contact's salutation",
                "desc": "The salutation for the contact (e.g., `Mr`, `Ms`, `Dr`)",
            },
            "TITLE": {
                "type": SoftStringOrNothingType,
                "display_name": "Title",
                "short_desc": "Job title",
                "desc": "The job title of the contact",
                "preselected": True,
            },
            "EMAIL_ADDRESS": {
                "type": SoftStringOrNothingType,
                "display_name": "Email Address",
                "short_desc": "Primary email address",
                "desc": "The primary email address of the contact",
                "preselected": True,
                "example_value": "jane.smith@example.com",
            },
            "PHONE": {
                "type": SoftStringOrNothingType,
                "display_name": "Phone",
                "short_desc": "Phone number",
                "desc": "The phone number of the contact",
                "preselected": True,
            },
            "MOBILE": {
                "type": SoftStringOrNothingType,
                "display_name": "Mobile",
                "short_desc": "Mobile phone number",
                "desc": "The mobile phone number of the contact",
            },
            "FAX": {
                "type": SoftStringOrNothingType,
                "display_name": "Fax",
                "short_desc": "Fax number",
                "desc": "The fax number of the contact",
            },
            "BACKGROUND": {
                "type": SoftStringOrNothingType,
                "display_name": "Background",
                "short_desc": "Background information",
                "desc": "Background information about the contact",
            },
            "DATE_OF_BIRTH": {
                "type": SoftDateOrNothingType,
                "display_name": "Date of Birth",
                "short_desc": "Contact's date of birth",
                "desc": "The date of birth of the contact",
            },
            "ORGANISATION_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Organisation ID",
                "short_desc": "Linked organisation identifier",
                "desc": "The ID of the organisation this contact belongs to",
                "ref_data": "organisations",
                "preselected": True,
            },
            "OWNER_USER_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Owner User ID",
                "short_desc": "The owner user identifier",
                "desc": "The ID of the user who owns this contact",
                "ref_data": "users",
            },
            "VISIBLE_TO": {
                "type": SoftStringOrNothingType,
                "display_name": "Visible To",
                "short_desc": "Visibility setting",
                "desc": "Who can see this contact",
                "allowed_values": VisibleToAllowedValues,
            },
            "VISIBLE_TEAM_ID": {
                "type": SoftIntOrNothingType,
                "display_name": "Visible Team ID",
                "short_desc": "Team visibility identifier",
                "desc": "The ID of the team that can see this contact when `VISIBLE_TO` is "
                    "`TEAM`",
                "ref_data": "teams",
            },
            "CUSTOMFIELDS": {
                "type": new ListDataType("InsightlyCustomFieldList",
                    new InsightlyCustomFieldDataType(), True),
                "display_name": "Custom Fields",
                "short_desc": "Custom field values",
                "desc": "A list of custom field values for the contact",
            },
            "ADDRESSES": {
                "type": new ListDataType("InsightlyAddressList",
                    new InsightlyAddressDataType(), True),
                "display_name": "Addresses",
                "short_desc": "Contact addresses",
                "desc": "A list of addresses for the contact",
            },
            "CONTACTINFOS": {
                "type": new ListDataType("InsightlyContactInfoList",
                    new InsightlyContactInfoDataType(), True),
                "display_name": "Contact Infos",
                "short_desc": "Additional contact information",
                "desc": "A list of additional contact information entries",
            },
            "LINKS": {
                "type": new ListDataType("InsightlyLinkList",
                    new InsightlyLinkDataType(), True),
                "display_name": "Links",
                "short_desc": "Related object links",
                "desc": "A list of links to related objects",
            },
            "TAGS": {
                "type": new ListDataType("InsightlyTagList",
                    new InsightlyTagDataType(), True),
                "display_name": "Tags",
                "short_desc": "Contact tags",
                "desc": "A list of tags to apply to the contact",
            },
            "DATES": {
                "type": new ListDataType("InsightlyDateList",
                    new InsightlyDateDataType(), True),
                "display_name": "Dates",
                "short_desc": "Important dates",
                "desc": "A list of important dates for the contact",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Insightly contact request data type for update operations
public class InsightlyUpdateContactRequestDataType inherits HashDataType {
    public {
        #! Field definitions for updating a contact
        const Fields = {
            "CONTACT_ID": {
                "type": SoftIntType,
                "display_name": "Contact ID",
                "required": True,
                "short_desc": "The contact ID to update",
                "desc": "The unique identifier of the contact to update",
                "ref_data": "contacts",
            },
        } + InsightlyDataProviderBase::makeFieldsOptional(
            InsightlyCreateContactRequestDataType::Fields);
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Insightly contact request data type for delete operations
public class InsightlyDeleteContactRequestDataType inherits HashDataType {
    public {
        #! Field definitions for deleting a contact
        const Fields = {
            "CONTACT_ID": {
                "type": SoftIntType,
                "display_name": "Contact ID",
                "required": True,
                "short_desc": "The contact ID to delete",
                "desc": "The unique identifier of the contact to delete",
                "ref_data": "contacts",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace InsightlyDataProvider
