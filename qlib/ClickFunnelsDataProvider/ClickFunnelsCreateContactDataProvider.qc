# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ClickFunnelsCreateContactDataProvider.qc ClickFunnels create contact action

/*  ClickFunnelsCreateContactDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request type for creating a contact
public class ClickFunnelsCreateContactRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "email_address": {
                "type": StringType,
                "display_name": "Email Address",
                "short_desc": "The contact's email address",
                "desc": "The email address of the contact to create",
                "required": True,
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
                "type": StringOrNothingType,
                "display_name": "Phone Number",
                "short_desc": "The contact's phone number",
                "desc": "The phone number of the contact",
            },
            "time_zone": {
                "type": StringOrNothingType,
                "display_name": "Time Zone",
                "short_desc": "The contact's time zone",
                "desc": "The time zone of the contact (e.g., `America/New_York`)",
            },
            "fb_url": {
                "type": StringOrNothingType,
                "display_name": "Facebook URL",
                "short_desc": "The contact's Facebook URL",
                "desc": "The Facebook profile URL of the contact",
            },
            "twitter_url": {
                "type": StringOrNothingType,
                "display_name": "Twitter URL",
                "short_desc": "The contact's Twitter URL",
                "desc": "The Twitter profile URL of the contact",
            },
            "instagram_url": {
                "type": StringOrNothingType,
                "display_name": "Instagram URL",
                "short_desc": "The contact's Instagram URL",
                "desc": "The Instagram profile URL of the contact",
            },
            "linkedin_url": {
                "type": StringOrNothingType,
                "display_name": "LinkedIn URL",
                "short_desc": "The contact's LinkedIn URL",
                "desc": "The LinkedIn profile URL of the contact",
            },
            "website_url": {
                "type": StringOrNothingType,
                "display_name": "Website URL",
                "short_desc": "The contact's website URL",
                "desc": "The website URL of the contact",
            },
            "tag_ids": {
                "type": SoftListOrNothingType,
                "display_name": "Tag IDs",
                "short_desc": "Contact tag IDs to apply",
                "desc": "A list of contact tag IDs to apply to the contact. Any valid array will "
                    "overwrite existing values",
                "ref_data": "contact_tags",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! ClickFunnels create contact action data provider
public class ClickFunnelsCreateContactDataProvider inherits ClickFunnelsDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new contact in ClickFunnels",
            "type": "ClickFunnelsCreateContactDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        static *AbstractDataProviderType ResponseType = new ClickFunnelsContactDataType();
        static *AbstractDataProviderType RequestType = new ClickFunnelsCreateContactRequestDataType();
    }

    constructor(ClickFunnelsRestClient::ClickFunnelsRestClientIo rest)
            : ClickFunnelsDataProviderBase(rest) {
    }

    string getName() {
        return "create";
    }

    *string getDesc() {
        return "Create a new contact in ClickFunnels";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new ClickFunnelsCreateContactRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new ClickFunnelsContactDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost(getWorkspacePath("contacts"), {"contact": req});
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ClickFunnelsDataProvider
