# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ApolloUpdateContactDataProvider.qc Apollo update contact action data provider

/*  ApolloUpdateContactDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request type for updating a contact
public class ApolloUpdateContactRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "contact_id": {
                "type": StringType,
                "display_name": "Contact ID",
                "short_desc": "The contact ID to update",
                "desc": "The unique identifier of the contact to update",
                "required": True,
            },
            "first_name": {
                "type": StringOrNothingType,
                "display_name": "First Name",
                "short_desc": "The contact's first name",
                "desc": "The new first name of the contact",
            },
            "last_name": {
                "type": StringOrNothingType,
                "display_name": "Last Name",
                "short_desc": "The contact's last name",
                "desc": "The new last name of the contact",
            },
            "email": {
                "type": StringOrNothingType,
                "display_name": "Email",
                "short_desc": "The contact's email address",
                "desc": "The new email address of the contact",
            },
            "title": {
                "type": StringOrNothingType,
                "display_name": "Job Title",
                "short_desc": "The contact's job title",
                "desc": "The new job title of the contact",
            },
            "organization_name": {
                "type": StringOrNothingType,
                "display_name": "Organization Name",
                "short_desc": "The company name",
                "desc": "The new company name for the contact",
            },
            "owner_id": {
                "type": StringOrNothingType,
                "display_name": "Owner ID",
                "short_desc": "The owner user ID",
                "desc": "The new owner user ID for the contact",
            },
            "phone_number": {
                "type": StringOrNothingType,
                "display_name": "Phone Number",
                "short_desc": "The contact's phone number",
                "desc": "The new phone number of the contact",
            },
            "present_raw_address": {
                "type": StringOrNothingType,
                "display_name": "Present Address",
                "short_desc": "The contact's current location",
                "desc": "The new current location or address of the contact",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Apollo update contact action data provider
public class ApolloUpdateContactDataProvider inherits ApolloDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Update an existing contact in Apollo",
            "type": "ApolloUpdateContactDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        static *AbstractDataProviderType ResponseType = new ApolloContactDataType();
        static *AbstractDataProviderType RequestType = new ApolloUpdateContactRequestDataType();
    }

    constructor(ApolloRestClient::ApolloRestClient rest) : ApolloDataProviderBase(rest) {
    }

    string getName() {
        return "update";
    }

    *string getDesc() {
        return "Update an existing contact in Apollo";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new ApolloUpdateContactRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new ApolloContactDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        hash<auto> body = {} + req;
        string id = remove body.contact_id;
        hash<auto> result = doPatch("contacts/" + id, body);
        return result.contact ?? result;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ApolloDataProvider
