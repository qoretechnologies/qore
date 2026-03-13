# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore InstantlyDataProvider module definition

/*  InstantlyCreateLeadDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the InstantlyDataProvider module
public namespace InstantlyDataProvider {

#! Request type for creating a lead
public class InstantlyCreateLeadRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "email": {
                "type": StringType,
                "display_name": "Email",
                "short_desc": "Lead email address",
                "desc": "The email address of the lead",
                "required": True,
                "preselected": True,
            },
            "first_name": {
                "type": StringOrNothingType,
                "display_name": "First Name",
                "short_desc": "First name",
                "desc": "The first name of the lead",
            },
            "last_name": {
                "type": StringOrNothingType,
                "display_name": "Last Name",
                "short_desc": "Last name",
                "desc": "The last name of the lead",
            },
            "company_name": {
                "type": StringOrNothingType,
                "display_name": "Company Name",
                "short_desc": "Company name",
                "desc": "The company name of the lead",
            },
            "phone": {
                "type": StringOrNothingType,
                "display_name": "Phone",
                "short_desc": "Phone number",
                "desc": "The phone number of the lead",
            },
            "website": {
                "type": StringOrNothingType,
                "display_name": "Website",
                "short_desc": "Website URL",
                "desc": "The website URL of the lead",
            },
            "campaign": {
                "type": StringOrNothingType,
                "display_name": "Campaign",
                "short_desc": "Campaign UUID",
                "desc": "The UUID of the campaign to associate with the lead",
                "ref_data": "campaigns",
                "preselected": True,
            },
            "list_id": {
                "type": StringOrNothingType,
                "display_name": "List ID",
                "short_desc": "Lead list UUID",
                "desc": "The UUID of the lead list to add the lead to",
                "ref_data": "lead_lists",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Instantly create lead action data provider
public class InstantlyCreateLeadDataProvider inherits InstantlyDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-lead",
            "desc": "Create a new lead in Instantly",
            "type": "InstantlyCreateLeadDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static InstantlyCreateLeadRequestDataType RequestType();

        #! Response type
        static InstantlyLeadDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(InstantlyRestClient::InstantlyRestClient rest) : InstantlyDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "create-lead";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Create a new lead in Instantly";
    }

    #! Makes the request
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("/leads", req);
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

} # namespace InstantlyDataProvider
