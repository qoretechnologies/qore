# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file CloseUpdateLeadDataProvider.qc Close update lead action data provider

/*  CloseUpdateLeadDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for updating a lead
public class CloseUpdateLeadRequestDataType inherits HashDataType {
    public {
        #! Field definitions for updating a lead
        const Fields = {
            "lead_id": {
                "type": StringType,
                "display_name": "Lead ID",
                "short_desc": "The lead ID to update",
                "desc": "The unique identifier of the lead to update",
                "required": True,
            },
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "The lead name",
                "desc": "The new name of the lead (typically the company or organization name)",
                "example_value": "Acme Corporation",
            },
            "status_id": {
                "type": StringOrNothingType,
                "display_name": "Status ID",
                "short_desc": "The lead status ID",
                "desc": "The new status ID to assign to the lead",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "The lead description",
                "desc": "The new text description of the lead",
                "example_value": "Updated enterprise prospect notes",
            },
            "url": {
                "type": StringOrNothingType,
                "display_name": "URL",
                "short_desc": "The lead URL",
                "desc": "The new website URL associated with the lead",
                "example_value": "https://www.example.com",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Close update lead action data provider
public class CloseUpdateLeadDataProvider inherits CloseDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Update an existing lead in Close",
            "type": "CloseUpdateLeadDataProvider",
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
        return "update";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Update an existing lead in Close";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new CloseUpdateLeadRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new CloseLeadDataType();
    }

    #! Makes the request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string lead_id = remove req.lead_id;
        return doPut("lead/" + lead_id, req);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace CloseDataProvider
