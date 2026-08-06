# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ClickFunnelsContactDataType.qc ClickFunnels contact data type

/*  ClickFunnelsContactDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ClickFunnelsDataProvider module
public namespace ClickFunnelsDataProvider {

#! ClickFunnels contact tag entry type (for tags array in contact response)
public class ClickFunnelsContactTagEntryDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "type": IntOrNothingType,
                "display_name": "ID",
                "short_desc": "The tag ID",
                "desc": "The unique identifier of the tag",
            },
            "public_id": {
                "type": StringOrNothingType,
                "display_name": "Public ID",
                "short_desc": "The public identifier",
                "desc": "The public-facing unique identifier of the tag",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "The tag name",
                "desc": "The name of the tag",
            },
            "color": {
                "type": StringOrNothingType,
                "display_name": "Color",
                "short_desc": "The tag color",
                "desc": "The color of the tag",
            },
            "applied_at": {
                "type": DateOrNothingType,
                "display_name": "Applied At",
                "short_desc": "When the tag was applied",
                "desc": "The timestamp when the tag was applied to the contact",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! ClickFunnels contact record type
public class ClickFunnelsContactDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "type": IntType,
                "display_name": "ID",
                "short_desc": "The contact ID",
                "desc": "The unique identifier of the contact",
            },
            "public_id": {
                "type": StringOrNothingType,
                "display_name": "Public ID",
                "short_desc": "The public identifier",
                "desc": "The public-facing unique identifier of the contact",
            },
            "workspace_id": {
                "type": IntOrNothingType,
                "display_name": "Workspace ID",
                "short_desc": "The workspace ID",
                "desc": "The ID of the workspace this contact belongs to",
            },
            "email_address": {
                "type": EmailOrNothingType,
                "display_name": "Email Address",
                "short_desc": "The contact's email address",
                "desc": "The email address of the contact",
                "example_value": "john@example.com",
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
            "phone_number": {
                "type": PhoneOrNothingType,
                "display_name": "Phone Number",
                "short_desc": "The contact's phone number",
                "desc": "The phone number of the contact",
            },
            "time_zone": {
                "type": StringOrNothingType,
                "display_name": "Time Zone",
                "short_desc": "The contact's time zone",
                "desc": "The time zone of the contact",
            },
            "uuid": {
                "type": StringOrNothingType,
                "display_name": "UUID",
                "short_desc": "The contact UUID",
                "desc": "The universally unique identifier of the contact",
            },
            "unsubscribed_at": {
                "type": DateOrNothingType,
                "display_name": "Unsubscribed At",
                "short_desc": "When the contact unsubscribed",
                "desc": "The timestamp when the contact unsubscribed from communications",
            },
            "last_contacted_at": {
                "type": DateOrNothingType,
                "display_name": "Last Contacted At",
                "short_desc": "When the contact was last contacted",
                "desc": "The timestamp when the contact was last contacted",
            },
            "fb_url": {
                "type": UriOrNothingType,
                "display_name": "Facebook URL",
                "short_desc": "The contact's Facebook URL",
                "desc": "The Facebook profile URL of the contact",
            },
            "twitter_url": {
                "type": UriOrNothingType,
                "display_name": "Twitter URL",
                "short_desc": "The contact's Twitter URL",
                "desc": "The Twitter profile URL of the contact",
            },
            "instagram_url": {
                "type": UriOrNothingType,
                "display_name": "Instagram URL",
                "short_desc": "The contact's Instagram URL",
                "desc": "The Instagram profile URL of the contact",
            },
            "linkedin_url": {
                "type": UriOrNothingType,
                "display_name": "LinkedIn URL",
                "short_desc": "The contact's LinkedIn URL",
                "desc": "The LinkedIn profile URL of the contact",
            },
            "website_url": {
                "type": UriOrNothingType,
                "display_name": "Website URL",
                "short_desc": "The contact's website URL",
                "desc": "The website URL of the contact",
            },
            "tags": {
                "type": new ListDataType("tags", new ClickFunnelsContactTagEntryDataType(), True),
                "display_name": "Tags",
                "short_desc": "Applied tags",
                "desc": "The tags applied to this contact. Each tag is an object with `id`, "
                    "`public_id`, `name`, `color`, and `applied_at` fields",
            },
            "created_at": {
                "type": DateOrNothingType,
                "display_name": "Created At",
                "short_desc": "When the contact was created",
                "desc": "The timestamp when the contact was created",
            },
            "updated_at": {
                "type": DateOrNothingType,
                "display_name": "Updated At",
                "short_desc": "When the contact was last updated",
                "desc": "The timestamp when the contact was last updated",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace ClickFunnelsDataProvider
