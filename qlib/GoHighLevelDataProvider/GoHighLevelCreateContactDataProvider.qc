# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GoHighLevelCreateContactDataProvider.qc GoHighLevel create contact action data provider

/*  GoHighLevelCreateContactDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

%modern

#! Contains all public definitions in the GoHighLevelDataProvider module
public namespace GoHighLevelDataProvider {

#! Request data type for creating a contact
public class GoHighLevelCreateContactRequestDataType inherits HashDataType {
    public {
        #! Field definitions for creating a contact
        const Fields = {
            "firstName": {
                "type": StringOrNothingType,
                "display_name": "First Name",
                "short_desc": "The contact's first name",
                "desc": "The contact's first name",
            },
            "lastName": {
                "type": StringOrNothingType,
                "display_name": "Last Name",
                "short_desc": "The contact's last name",
                "desc": "The contact's last name",
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Full Name",
                "short_desc": "The contact's full name",
                "desc": "The contact's full name",
            },
            "email": {
                "type": StringOrNothingType,
                "display_name": "Email",
                "short_desc": "The contact's email address",
                "desc": "The contact's email address (e.g. `john@example.com`)",
                "example_value": "john@example.com",
            },
            "phone": {
                "type": StringOrNothingType,
                "display_name": "Phone",
                "short_desc": "The contact's phone number",
                "desc": "The contact's phone number",
            },
            "companyName": {
                "type": StringOrNothingType,
                "display_name": "Company Name",
                "short_desc": "The contact's company name",
                "desc": "The contact's company name",
            },
            "address1": {
                "type": StringOrNothingType,
                "display_name": "Address",
                "short_desc": "The contact's street address",
                "desc": "The contact's street address",
            },
            "city": {
                "type": StringOrNothingType,
                "display_name": "City",
                "short_desc": "The contact's city",
                "desc": "The contact's city",
            },
            "state": {
                "type": StringOrNothingType,
                "display_name": "State",
                "short_desc": "The contact's state or province",
                "desc": "The contact's state or province",
            },
            "country": {
                "type": StringOrNothingType,
                "display_name": "Country",
                "short_desc": "The contact's country",
                "desc": "The contact's country (2-letter ISO country code)",
            },
            "postalCode": {
                "type": StringOrNothingType,
                "display_name": "Postal Code",
                "short_desc": "The contact's postal/zip code",
                "desc": "The contact's postal/zip code",
            },
            "website": {
                "type": StringOrNothingType,
                "display_name": "Website",
                "short_desc": "The contact's website URL",
                "desc": "The contact's website URL",
            },
            "timezone": {
                "type": StringOrNothingType,
                "display_name": "Timezone",
                "short_desc": "The contact's timezone",
                "desc": "The contact's timezone",
            },
            "tags": {
                "type": SoftListOrNothingType,
                "display_name": "Tags",
                "short_desc": "Tags to assign to the contact",
                "desc": "Tags to assign to the contact",
            },
            "source": {
                "type": StringOrNothingType,
                "display_name": "Source",
                "short_desc": "The source of the contact",
                "desc": "The source of the contact",
            },
            "assignedTo": {
                "type": StringOrNothingType,
                "display_name": "Assigned To",
                "short_desc": "User ID to assign the contact to",
                "desc": "User ID to assign the contact to",
            },
            "dnd": {
                "type": SoftBoolOrNothingType,
                "display_name": "Do Not Disturb",
                "short_desc": "Enable do-not-disturb for the contact",
                "desc": "Enable do-not-disturb for the contact",
            },
            "customFields": {
                "type": new SoftListDataType(new GoHighLevelCustomFieldDataType(), True),
                "display_name": "Custom Fields",
                "short_desc": "Custom field values for the contact",
                "desc": "Custom field values for the contact; each entry has an `id` "
                    "(custom field ID) and `field_value`",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! GoHighLevel create contact action data provider
public class GoHighLevelCreateContactDataProvider inherits GoHighLevelDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new contact in GoHighLevel",
            "type": "GoHighLevelCreateContactDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;
    }

    #! Creates the object with a REST client
    constructor(GoHighLevelRestClient::GoHighLevelRestClient rest) : GoHighLevelDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "create";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Create a new contact in GoHighLevel";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new GoHighLevelCreateContactRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new GoHighLevelContactDataType();
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("contacts", req).contact;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace GoHighLevelDataProvider
