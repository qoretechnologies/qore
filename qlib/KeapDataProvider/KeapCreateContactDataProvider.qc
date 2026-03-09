# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KeapCreateContactDataProvider.qc Keap create contact action data provider

/*  KeapCreateContactDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for creating a contact
public class KeapCreateContactRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "email": {
                "type": StringOrNothingType,
                "display_name": "Email",
                "short_desc": "The contact's email address",
                "desc": "The contact's email address. Converted to the API's `email_addresses` "
                    "array format automatically",
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
            "phone_number": {
                "type": StringOrNothingType,
                "display_name": "Phone Number",
                "short_desc": "The contact's phone number",
                "desc": "The contact's phone number. Converted to the API's `phone_numbers` "
                    "array format automatically",
                "example_value": "+1-555-123-4567",
            },
            "middle_name": {
                "type": StringOrNothingType,
                "display_name": "Middle Name",
                "short_desc": "The contact's middle name",
                "desc": "The middle name of the contact",
            },
            "company": {
                "type": AutoHashOrNothingType,
                "display_name": "Company",
                "short_desc": "The associated company",
                "desc": "The company to associate with the contact. Provide an object with "
                    "either `id` (existing company ID) or `company_name` (company name)",
            },
            "job_title": {
                "type": StringOrNothingType,
                "display_name": "Job Title",
                "short_desc": "The contact's job title",
                "desc": "The job title of the contact",
                "example_value": "Software Engineer",
            },
            "owner_id": {
                "type": StringOrNothingType,
                "display_name": "Owner ID",
                "short_desc": "The owner user ID",
                "desc": "The ID of the user who owns this contact. Must not be an empty string; "
                    "omit if not setting an owner",
            },
            "leadsource_id": {
                "type": StringOrNothingType,
                "display_name": "Lead Source ID",
                "short_desc": "The lead source ID",
                "desc": "The ID of the lead source for this contact. Must not be an empty string; "
                    "omit if not setting a lead source",
            },
            "website": {
                "type": StringOrNothingType,
                "display_name": "Website",
                "short_desc": "The contact's website",
                "desc": "The website URL of the contact",
                "example_value": "https://example.com",
            },
            "source_type": {
                "type": StringOrNothingType,
                "display_name": "Source Type",
                "short_desc": "How the contact was created",
                "desc": "The source type indicating how the contact was created",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "API", "desc": "Created via API"},
                    <AllowedValueInfo>{"value": "WEBFORM", "desc": "Created via web form"},
                    <AllowedValueInfo>{"value": "IMPORT", "desc": "Created via import"},
                    <AllowedValueInfo>{"value": "MANUAL", "desc": "Created manually"},
                    <AllowedValueInfo>{"value": "LANDING_PAGE", "desc": "Created via landing page"},
                    <AllowedValueInfo>{"value": "OTHER", "desc": "Other source"},
                ),
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
                "short_desc": "Name prefix (e.g., Mr., Dr.)",
                "desc": "The name prefix of the contact (e.g., `Mr.`, `Mrs.`, `Dr.`)",
            },
            "suffix": {
                "type": StringOrNothingType,
                "display_name": "Suffix",
                "short_desc": "Name suffix (e.g., Jr., III)",
                "desc": "The name suffix of the contact (e.g., `Jr.`, `Sr.`, `III`)",
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
                "desc": "The time zone of the contact (e.g., `US/Eastern`)",
                "example_value": "US/Eastern",
            },
            "preferred_locale": {
                "type": StringOrNothingType,
                "display_name": "Preferred Locale",
                "short_desc": "The contact's locale",
                "desc": "The preferred locale of the contact (e.g., `en_US`)",
                "example_value": "en_US",
            },
            "contact_type": {
                "type": StringOrNothingType,
                "display_name": "Contact Type",
                "short_desc": "The type of contact",
                "desc": "The type of the contact",
            },
            "addresses": {
                "type": new ListDataType(new KeapAddressDataType(AutoHashOrNothingType), True),
                "display_name": "Addresses",
                "short_desc": "The contact's addresses",
                "desc": "A list of address objects for the contact",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Keap create contact action data provider
public class KeapCreateContactDataProvider inherits KeapDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new contact in Keap",
            "type": "KeapCreateContactDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        static *AbstractDataProviderType ResponseType = new KeapContactDataType();
        static *AbstractDataProviderType RequestType = new KeapCreateContactRequestDataType();
    }

    constructor(KeapRestClient::KeapRestClient rest) : KeapDataProviderBase(rest) {
    }

    string getName() {
        return "create";
    }

    *string getDesc() {
        return "Create a new contact in Keap";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new KeapCreateContactRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new KeapContactDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = {} + req;
        # Transform simple email → email_addresses array
        if (body.email) {
            body.email_addresses = ({"email": remove body.email, "field": "EMAIL1"},);
        } else {
            remove body.email;
        }
        # Transform simple phone_number → phone_numbers array
        if (body.phone_number) {
            body.phone_numbers = ({"number": remove body.phone_number, "field": "PHONE1"},);
        } else {
            remove body.phone_number;
        }
        hash<auto> result = doPost("contacts", body);
        # Create response is minimal; fetch full contact with optional properties
        string opt_props_qs = (map sprintf("optional_properties=%s", $1),
            KeapContactsDataProvider::OptionalContactProperties).join("&");
        hash<auto> full = doRestRequest("GET",
            "contacts/" + result.id + "?" + opt_props_qs);
        return transformContactResponse(full);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace KeapDataProvider
