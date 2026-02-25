# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DeelTerminateContractDataProvider.qc Deel terminate contract action data provider

/*  DeelTerminateContractDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request type for terminating a contract
public class DeelTerminateContractRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "contract_id": {
                "type": StringType,
                "display_name": "Contract ID",
                "short_desc": "The contract ID to terminate",
                "desc": "The unique identifier of the contract to terminate",
                "required": True,
            },
            "termination_reason": {
                "type": StringType,
                "display_name": "Termination Reason",
                "short_desc": "The reason for termination",
                "desc": "The reason for terminating the contract",
                "required": True,
                "example_value": "Project completed",
            },
            "last_day_of_work": {
                "type": StringOrNothingType,
                "display_name": "Last Day of Work",
                "short_desc": "The worker's last working day",
                "desc": "The last day the worker will be active (YYYY-MM-DD format)",
                "example_value": "2026-03-31",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Deel terminate contract action data provider
public class DeelTerminateContractDataProvider inherits DeelDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "terminate",
            "desc": "Terminate an existing contract in Deel",
            "type": "DeelTerminateContractDataProvider",
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
        return "terminate";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Terminate an existing contract in Deel";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new DeelTerminateContractRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new DeelContractDataType();
    }

    #! Performs the request
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string contract_id = remove req.contract_id;
        return doPost("contracts/" + encode_url(contract_id, True) + "/terminate", req).data;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace DeelDataProvider
