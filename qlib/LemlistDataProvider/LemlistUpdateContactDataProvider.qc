# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LemlistDataProvider module definition

/*  LemlistUpdateContactDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the LemlistDataProvider module
public namespace LemlistDataProvider {

#! Request type for updating a contact
public class LemlistUpdateContactRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "email": {
                "type": StringType,
                "display_name": "Email",
                "short_desc": "Contact email address to update",
                "desc": "The email address of the contact to update (used as the unique identifier)",
                "required": True,
                "example_value": "john.smith@acme.com",
            },
            "firstName": {
                "type": SoftStringOrNothingType,
                "display_name": "First Name",
                "short_desc": "Contact first name",
                "desc": "Updated first name for the contact",
                "preselected": True,
                "example_value": "John",
            },
            "lastName": {
                "type": SoftStringOrNothingType,
                "display_name": "Last Name",
                "short_desc": "Contact last name",
                "desc": "Updated last name for the contact",
                "preselected": True,
                "example_value": "Smith",
            },
            "companyName": {
                "type": SoftStringOrNothingType,
                "display_name": "Company Name",
                "short_desc": "Contact company name",
                "desc": "Updated company name for the contact",
            },
            "phone": {
                "type": SoftStringOrNothingType,
                "display_name": "Phone",
                "short_desc": "Contact phone number",
                "desc": "Updated phone number for the contact",
            },
            "linkedinUrl": {
                "type": SoftStringOrNothingType,
                "display_name": "LinkedIn URL",
                "short_desc": "Contact LinkedIn profile URL",
                "desc": "Updated LinkedIn profile URL for the contact",
            },
            "website": {
                "type": SoftStringOrNothingType,
                "display_name": "Website",
                "short_desc": "Contact or company website",
                "desc": "Updated website URL for the contact",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for updating a contact
public class LemlistUpdateContactDataProvider inherits LemlistDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "update-contact",
            "desc": "Update an existing Lemlist contact",
            "type": "LemlistUpdateContactDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static LemlistUpdateContactRequestDataType RequestType();

        #! Response type
        static LemlistContactDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(LemlistRestClient::LemlistRestClient rest)
            : LemlistDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(LemlistRestClient::LemlistRestConnection conn) : LemlistDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "update-contact";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Update an existing Lemlist contact";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string email = req.email;
        hash<auto> body = req - ("email",);
        # Preserve @ in email - Lemlist API doesn't decode %40 in path segments
        return doPatch(sprintf("/contacts/%s", replace(encode_url(email, True), "%40", "@")), body);
    }

    #! Returns the request type
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace LemlistDataProvider
