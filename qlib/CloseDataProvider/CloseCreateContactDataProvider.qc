# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file CloseCreateContactDataProvider.qc Close create contact action data provider

/*  CloseCreateContactDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the CloseDataProvider module
public namespace CloseDataProvider {

#! Request type for creating a contact
public class CloseCreateContactRequestDataType inherits HashDataType {
    public {
        #! Field definitions for creating a contact
        const Fields = {
            "lead_id": {
                "type": StringType,
                "display_name": "Lead ID",
                "short_desc": "The lead to associate the contact with",
                "desc": "The unique identifier of the lead to associate this contact with",
                "required": True,
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "The contact name",
                "desc": "The full name of the contact",
                "preselected": True,
                "example_value": "Jane Smith",
            },
            "title": {
                "type": StringOrNothingType,
                "display_name": "Title",
                "short_desc": "The contact title",
                "desc": "The job title of the contact",
                "example_value": "VP of Sales",
            },
            "emails": {
                "type": new SoftListDataType("Emails", new CloseEmailEntryDataType(), True),
                "display_name": "Email Addresses",
                "short_desc": "The contact email addresses",
                "desc": "A list of email address objects, each with `type` (e.g., `office`, `home`) "
                    "and `email` fields",
            },
            "phones": {
                "type": new SoftListDataType("Phones", new ClosePhoneEntryDataType(), True),
                "display_name": "Phone Numbers",
                "short_desc": "The contact phone numbers",
                "desc": "A list of phone number objects, each with `type` (e.g., `office`, `mobile`) "
                    "and `phone` fields",
            },
            "urls": {
                "type": new SoftListDataType("URLs", new CloseUrlEntryDataType(), True),
                "display_name": "URLs",
                "short_desc": "The contact URLs",
                "desc": "A list of URL objects, each with `type` and `url` fields",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Close create contact action data provider
public class CloseCreateContactDataProvider inherits CloseDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new contact in Close",
            "type": "CloseCreateContactDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;
    }

    #! Creates the object with a REST client
    constructor(CloseRestClient::CloseRestClient rest) : CloseDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "create";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Create a new contact in Close";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new CloseCreateContactRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new CloseContactDataType();
    }

    #! Makes the request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("contact", req);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace CloseDataProvider
