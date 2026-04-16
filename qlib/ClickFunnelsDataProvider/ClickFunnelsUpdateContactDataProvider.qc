# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ClickFunnelsUpdateContactDataProvider.qc ClickFunnels update contact action

/*  ClickFunnelsUpdateContactDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

public namespace ClickFunnelsDataProvider {

#! Request type for updating a contact
public class ClickFunnelsUpdateContactRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "contact_id": {
                "type": StringType,
                "display_name": "Contact ID",
                "short_desc": "The contact ID to update",
                "desc": "The unique identifier of the contact to update",
                "required": True,
                "ref_data": "contacts",
            },
            "email_address": {
                "type": StringOrNothingType,
                "display_name": "Email Address",
                "short_desc": "The contact's email address",
                "desc": "The new email address for the contact",
            },
            "first_name": {
                "type": StringOrNothingType,
                "display_name": "First Name",
                "short_desc": "The contact's first name",
                "desc": "The new first name for the contact",
            },
            "last_name": {
                "type": StringOrNothingType,
                "display_name": "Last Name",
                "short_desc": "The contact's last name",
                "desc": "The new last name for the contact",
            },
            "phone_number": {
                "type": StringOrNothingType,
                "display_name": "Phone Number",
                "short_desc": "The contact's phone number",
                "desc": "The new phone number for the contact",
            },
            "time_zone": {
                "type": StringOrNothingType,
                "display_name": "Time Zone",
                "short_desc": "The contact's time zone",
                "desc": "The new time zone for the contact",
            },
            "fb_url": {
                "type": StringOrNothingType,
                "display_name": "Facebook URL",
                "short_desc": "The contact's Facebook URL",
                "desc": "The new Facebook profile URL for the contact",
            },
            "twitter_url": {
                "type": StringOrNothingType,
                "display_name": "Twitter URL",
                "short_desc": "The contact's Twitter URL",
                "desc": "The new Twitter profile URL for the contact",
            },
            "instagram_url": {
                "type": StringOrNothingType,
                "display_name": "Instagram URL",
                "short_desc": "The contact's Instagram URL",
                "desc": "The new Instagram profile URL for the contact",
            },
            "linkedin_url": {
                "type": StringOrNothingType,
                "display_name": "LinkedIn URL",
                "short_desc": "The contact's LinkedIn URL",
                "desc": "The new LinkedIn profile URL for the contact",
            },
            "website_url": {
                "type": StringOrNothingType,
                "display_name": "Website URL",
                "short_desc": "The contact's website URL",
                "desc": "The new website URL for the contact",
            },
            "tag_ids": {
                "type": SoftListOrNothingType,
                "display_name": "Tag IDs",
                "short_desc": "Contact tag IDs to apply",
                "desc": "A list of contact tag IDs to apply to the contact. Any valid array will "
                    "overwrite existing values",
                "ref_data": "contact_tags",
            },
            "custom_attributes": {
                "type": AutoHashOrNothingType,
                "display_name": "Custom Attributes",
                "short_desc": "Custom key-value attributes",
                "desc": "Custom key-value attributes for the contact. Both keys and values must "
                    "be strings. Keys with special characters are automatically converted to "
                    "`snake_case`",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! ClickFunnels update contact action data provider
public class ClickFunnelsUpdateContactDataProvider inherits ClickFunnelsDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Update an existing contact in ClickFunnels",
            "type": "ClickFunnelsUpdateContactDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        static *AbstractDataProviderType ResponseType = new ClickFunnelsContactDataType();
        static *AbstractDataProviderType RequestType = new ClickFunnelsUpdateContactRequestDataType();
    }

    constructor(ClickFunnelsRestClient::ClickFunnelsRestClientIo rest)
            : ClickFunnelsDataProviderBase(rest) {
    }

    string getName() {
        return "update";
    }

    *string getDesc() {
        return "Update an existing contact in ClickFunnels";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new ClickFunnelsUpdateContactRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new ClickFunnelsContactDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string id = remove req.contact_id;
        return doPut("contacts/" + id, {"contact": req});
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ClickFunnelsDataProvider
