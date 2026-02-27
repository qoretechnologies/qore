# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DeelUpdateContractDataProvider.qc Deel update contract action data provider

/*  DeelUpdateContractDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DeelDataProvider module
public namespace DeelDataProvider {

#! Request type for updating a contract
public class DeelUpdateContractRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "contract_id": {
                "type": StringType,
                "display_name": "Contract ID",
                "short_desc": "The contract ID to update",
                "desc": "The unique identifier of the contract to update",
                "required": True,
            },
            "title": {
                "type": StringOrNothingType,
                "display_name": "Title",
                "short_desc": "Updated contract title",
                "desc": "A new descriptive title for the contract",
                "example_value": "Senior Software Engineer",
            },
            "amount": {
                "type": NumberOrNothingType,
                "display_name": "Amount",
                "short_desc": "Updated contract amount",
                "desc": "The updated monetary amount for the contract (rate or fixed amount)",
                "example_value": 6000.00,
            },
            "scope_of_work": {
                "type": StringOrNothingType,
                "display_name": "Scope of Work",
                "short_desc": "Updated scope of work",
                "desc": "Updated description of the work to be performed under this contract",
            },
            "job_title": {
                "type": StringOrNothingType,
                "display_name": "Job Title",
                "short_desc": "Updated job title",
                "desc": "The updated job title for the contract",
                "example_value": "Senior Software Engineer",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Deel update contract action data provider
public class DeelUpdateContractDataProvider inherits DeelDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Update an existing contract in Deel",
            "type": "DeelUpdateContractDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;
    }

    #! Creates the object with a REST client
    constructor(DeelRestClient::DeelRestClient rest) : DeelDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "update";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Update an existing contract in Deel";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new DeelUpdateContractRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new DeelContractDataType();
    }

    #! Performs the request
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string contract_id = remove req.contract_id;
        return doPatch("contracts/" + encode_url(contract_id, True), {"data": req}).data;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace DeelDataProvider
