# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file CloseCreateLeadDataProvider.qc Close create lead action data provider

/*  CloseCreateLeadDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for creating a lead
public class CloseCreateLeadRequestDataType inherits HashDataType {
    public {
        #! Field definitions for creating a lead
        const Fields = {
            "name": {
                "type": StringOrNothingType,
                "display_name": "Name",
                "short_desc": "The lead name",
                "desc": "The name of the lead (typically the company or organization name)",
                "preselected": True,
                "example_value": "Acme Corporation",
            },
            "status_id": {
                "type": StringOrNothingType,
                "display_name": "Status ID",
                "short_desc": "The lead status ID",
                "desc": "The unique identifier of the lead status to assign",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "The lead description",
                "desc": "A text description of the lead with additional context or notes",
                "example_value": "Enterprise prospect from trade show",
            },
            "url": {
                "type": StringOrNothingType,
                "display_name": "URL",
                "short_desc": "The lead URL",
                "desc": "The website URL associated with the lead",
                "example_value": "https://www.example.com",
            },
            "contacts": {
                "type": new SoftListDataType("Contacts", new CloseContactEntryDataType(), True),
                "display_name": "Contacts",
                "short_desc": "Nested contacts to create",
                "desc": "A list of contact objects to create and associate with this lead. Each "
                    "contact can include `name`, `title`, `emails`, and `phones` fields",
            },
            "addresses": {
                "type": new SoftListDataType("Addresses", new CloseAddressEntryDataType(), True),
                "display_name": "Addresses",
                "short_desc": "The lead addresses",
                "desc": "A list of address objects for the lead, each with `address_1`, `city`, "
                    "`state`, `zipcode`, and `country` fields",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! Close create lead action data provider
public class CloseCreateLeadDataProvider inherits CloseDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new lead in Close",
            "type": "CloseCreateLeadDataProvider",
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
        return "Create a new lead in Close";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new CloseCreateLeadRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new CloseLeadDataType();
    }

    #! Makes the request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("lead", req);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace CloseDataProvider
