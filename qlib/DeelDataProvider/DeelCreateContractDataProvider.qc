# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DeelCreateContractDataProvider.qc Deel create contract action data provider

/*  DeelCreateContractDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request type for creating a contract
public class DeelCreateContractRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "type": {
                "type": StringType,
                "display_name": "Contract Type",
                "short_desc": "The type of contract",
                "desc": "The contract type (e.g., `pay_as_you_go_time_based`, `fixed`, `milestone`, "
                    "`employee`)",
                "required": True,
                "example_value": "fixed",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "pay_as_you_go_time_based",
                        "desc": "Pay-as-you-go time-based contract"},
                    <AllowedValueInfo>{"value": "fixed", "desc": "Fixed-rate contract"},
                    <AllowedValueInfo>{"value": "milestone", "desc": "Milestone-based contract"},
                    <AllowedValueInfo>{"value": "employee", "desc": "Employee contract"},
                ),
            },
            "title": {
                "type": StringType,
                "display_name": "Title",
                "short_desc": "The contract title",
                "desc": "A descriptive title for the contract",
                "required": True,
                "example_value": "Software Engineer - Backend",
            },
            "worker_email": {
                "type": StringType,
                "display_name": "Worker Email",
                "short_desc": "The worker's email address",
                "desc": "The email address of the contractor or employee",
                "required": True,
                "example_value": "worker@example.com",
            },
            "start_date": {
                "type": StringType,
                "display_name": "Start Date",
                "short_desc": "The contract start date",
                "desc": "The date the contract starts (YYYY-MM-DD format)",
                "required": True,
                "example_value": "2026-03-01",
            },
            "currency": {
                "type": StringOrNothingType,
                "display_name": "Currency",
                "short_desc": "The contract currency",
                "desc": "The ISO 4217 currency code for payments (e.g., `USD`, `EUR`)",
                "example_value": "USD",
            },
            "amount": {
                "type": NumberOrNothingType,
                "display_name": "Amount",
                "short_desc": "The contract amount",
                "desc": "The monetary amount for the contract (rate or fixed amount)",
                "example_value": 5000.00,
            },
            "payment_cycle": {
                "type": StringOrNothingType,
                "display_name": "Payment Cycle",
                "short_desc": "How often payments are made",
                "desc": "The payment cycle for the contract (e.g., `monthly`, `weekly`, `biweekly`)",
                "example_value": "monthly",
            },
            "scope_of_work": {
                "type": StringOrNothingType,
                "display_name": "Scope of Work",
                "short_desc": "Description of the work",
                "desc": "A detailed description of the work to be performed under this contract",
            },
            "country": {
                "type": StringOrNothingType,
                "display_name": "Country",
                "short_desc": "The worker's country",
                "desc": "The ISO 3166-1 alpha-2 country code for the worker's location",
                "example_value": "US",
            },
            "job_title": {
                "type": StringOrNothingType,
                "display_name": "Job Title",
                "short_desc": "The worker's job title",
                "desc": "The job title of the contractor or employee",
                "example_value": "Software Engineer",
            },
            "end_date": {
                "type": StringOrNothingType,
                "display_name": "End Date",
                "short_desc": "The contract end date",
                "desc": "The date the contract ends (YYYY-MM-DD format), if applicable",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Deel create contract action data provider
public class DeelCreateContractDataProvider inherits DeelDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new contract in Deel",
            "type": "DeelCreateContractDataProvider",
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
        return "create";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Create a new contract in Deel";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new DeelCreateContractRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new DeelContractDataType();
    }

    #! Performs the request
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("contracts", {"data": req}).data;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace DeelDataProvider
