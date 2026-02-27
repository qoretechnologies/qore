# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DeelCreateWorkerDataProvider.qc Deel create worker action data provider

/*  DeelCreateWorkerDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request type for creating a worker
public class DeelCreateWorkerRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "first_name": {
                "type": StringType,
                "display_name": "First Name",
                "short_desc": "The worker's first name",
                "desc": "The given (first) name of the worker",
                "required": True,
                "example_value": "Jane",
            },
            "last_name": {
                "type": StringType,
                "display_name": "Last Name",
                "short_desc": "The worker's last name",
                "desc": "The family (last) name of the worker",
                "required": True,
                "example_value": "Smith",
            },
            "email": {
                "type": StringType,
                "display_name": "Email",
                "short_desc": "The worker's email address",
                "desc": "The email address of the worker",
                "required": True,
                "example_value": "jane.smith@example.com",
            },
            "country_code": {
                "type": StringType,
                "display_name": "Country Code",
                "short_desc": "The worker's country",
                "desc": "The ISO 3166-1 alpha-2 country code for the worker's location",
                "required": True,
                "example_value": "US",
            },
            "job_title": {
                "type": StringOrNothingType,
                "display_name": "Job Title",
                "short_desc": "The worker's job title",
                "desc": "The job title of the worker",
                "example_value": "Software Engineer",
            },
            "seniority": {
                "type": StringOrNothingType,
                "display_name": "Seniority",
                "short_desc": "The worker's seniority level",
                "desc": "The seniority level of the worker (e.g., `junior`, `mid`, `senior`)",
                "example_value": "senior",
            },
            "phone": {
                "type": StringOrNothingType,
                "display_name": "Phone",
                "short_desc": "The worker's phone number",
                "desc": "The phone number of the worker",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Deel create worker action data provider
public class DeelCreateWorkerDataProvider inherits DeelDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new worker/person in Deel",
            "type": "DeelCreateWorkerDataProvider",
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
        return "Create a new worker/person in Deel";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new DeelCreateWorkerRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new DeelWorkerDataType();
    }

    #! Performs the request
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("people", {"data": req}).data;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace DeelDataProvider
