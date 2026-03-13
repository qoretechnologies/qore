# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LemlistDataProvider module definition

/*  LemlistCreateLeadDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for creating a lead
public class LemlistCreateLeadRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "campaignId": {
                "type": StringType,
                "display_name": "Campaign ID",
                "short_desc": "Campaign to add the lead to",
                "desc": "The identifier of the campaign to add this lead to",
                "required": True,
                "ref_data": "campaigns",
                "example_value": "cam_abc123",
            },
            "email": {
                "type": StringType,
                "display_name": "Email",
                "short_desc": "Lead email address",
                "desc": "The email address of the lead (used as the unique identifier)",
                "required": True,
                "example_value": "john.smith@acme.com",
            },
            "firstName": {
                "type": SoftStringOrNothingType,
                "display_name": "First Name",
                "short_desc": "Lead first name",
                "desc": "The first name of the lead",
                "preselected": True,
                "example_value": "John",
            },
            "lastName": {
                "type": SoftStringOrNothingType,
                "display_name": "Last Name",
                "short_desc": "Lead last name",
                "desc": "The last name of the lead",
                "preselected": True,
                "example_value": "Smith",
            },
            "companyName": {
                "type": SoftStringOrNothingType,
                "display_name": "Company Name",
                "short_desc": "Lead company name",
                "desc": "The name of the company the lead works for",
            },
            "phone": {
                "type": SoftStringOrNothingType,
                "display_name": "Phone",
                "short_desc": "Lead phone number",
                "desc": "The phone number of the lead",
            },
            "linkedinUrl": {
                "type": SoftStringOrNothingType,
                "display_name": "LinkedIn URL",
                "short_desc": "Lead LinkedIn profile URL",
                "desc": "The LinkedIn profile URL of the lead",
            },
            "website": {
                "type": SoftStringOrNothingType,
                "display_name": "Website",
                "short_desc": "Lead or company website",
                "desc": "The website URL associated with the lead",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for creating a new lead
public class LemlistCreateLeadDataProvider inherits LemlistDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-lead",
            "desc": "Add a lead to a Lemlist campaign",
            "type": "LemlistCreateLeadDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static LemlistCreateLeadRequestDataType RequestType();

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
        return "create-lead";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Add a lead to a Lemlist campaign";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string campaign_id = req.campaignId;
        string email = req.email;

        # Build lead body without campaignId and email (both are in the path)
        hash<auto> body = req - ("campaignId", "email");

        # Preserve @ in email - Lemlist API doesn't decode %40 in path segments
        string encoded_email = replace(encode_url(email, True), "%40", "@");
        return doPost(sprintf("/campaigns/%s/leads/%s", encode_url(campaign_id, True), encoded_email), body);
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
