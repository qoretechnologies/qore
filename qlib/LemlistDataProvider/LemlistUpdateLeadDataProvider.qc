# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LemlistDataProvider module definition

/*  LemlistUpdateLeadDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for updating a lead
public class LemlistUpdateLeadRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "email": {
                "type": StringType,
                "display_name": "Email",
                "short_desc": "Lead email address to update",
                "desc": "The email address of the lead to update (used as identifier)",
                "required": True,
                "example_value": "john.smith@acme.com",
            },
            "firstName": {
                "type": SoftStringOrNothingType,
                "display_name": "First Name",
                "short_desc": "Lead first name",
                "desc": "Updated first name for the lead",
                "preselected": True,
            },
            "lastName": {
                "type": SoftStringOrNothingType,
                "display_name": "Last Name",
                "short_desc": "Lead last name",
                "desc": "Updated last name for the lead",
                "preselected": True,
            },
            "companyName": {
                "type": SoftStringOrNothingType,
                "display_name": "Company Name",
                "short_desc": "Lead company name",
                "desc": "Updated company name for the lead",
            },
            "phone": {
                "type": SoftStringOrNothingType,
                "display_name": "Phone",
                "short_desc": "Lead phone number",
                "desc": "Updated phone number for the lead",
            },
            "linkedinUrl": {
                "type": SoftStringOrNothingType,
                "display_name": "LinkedIn URL",
                "short_desc": "Lead LinkedIn profile URL",
                "desc": "Updated LinkedIn profile URL for the lead",
            },
            "website": {
                "type": SoftStringOrNothingType,
                "display_name": "Website",
                "short_desc": "Lead or company website",
                "desc": "Updated website URL for the lead",
            },
            "isPaused": {
                "type": SoftBoolOrNothingType,
                "display_name": "Is Paused",
                "short_desc": "Pause or resume outreach to this lead",
                "desc": "Set to `True` to pause outreach to this lead, `False` to resume",
            },
            "isInterested": {
                "type": SoftBoolOrNothingType,
                "display_name": "Is Interested",
                "short_desc": "Mark the lead as interested or not",
                "desc": "Set to `True` to mark this lead as interested in your offer",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for updating a lead
public class LemlistUpdateLeadDataProvider inherits LemlistDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "update-lead",
            "desc": "Update an existing Lemlist lead",
            "type": "LemlistUpdateLeadDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static LemlistUpdateLeadRequestDataType RequestType();

        #! Response type
        static LemlistLeadDataType ResponseType();
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
        return "update-lead";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Update an existing Lemlist lead";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string email = req.email;
        hash<auto> body = req - ("email",);
        # Preserve @ in email - Lemlist API doesn't decode %40 in path segments
        return doPatch(sprintf("/leads/%s", replace(encode_url(email, True), "%40", "@")), body);
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
