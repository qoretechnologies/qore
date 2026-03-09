# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DeelUpdateWorkerDataProvider.qc Deel update worker action data provider

/*  DeelUpdateWorkerDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request type for updating a worker
public class DeelUpdateWorkerRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "worker_id": {
                "type": StringType,
                "display_name": "Worker ID",
                "ref_data": "workers",
                "short_desc": "The worker ID to update",
                "desc": "The unique identifier of the worker to update",
                "required": True,
            },
            "first_name": {
                "type": StringOrNothingType,
                "display_name": "First Name",
                "short_desc": "The worker's first name",
                "desc": "The updated given (first) name of the worker",
                "example_value": "Jane",
            },
            "last_name": {
                "type": StringOrNothingType,
                "display_name": "Last Name",
                "short_desc": "The worker's last name",
                "desc": "The updated family (last) name of the worker",
                "example_value": "Smith",
            },
            "job_title": {
                "type": StringOrNothingType,
                "display_name": "Job Title",
                "short_desc": "The worker's job title",
                "desc": "The updated job title of the worker",
                "example_value": "Senior Software Engineer",
            },
            "seniority": {
                "type": StringOrNothingType,
                "display_name": "Seniority",
                "short_desc": "The worker's seniority level",
                "desc": "The updated seniority level of the worker",
                "example_value": "senior",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "junior", "desc": "Junior level"},
                    <AllowedValueInfo>{"value": "mid", "desc": "Mid level"},
                    <AllowedValueInfo>{"value": "senior", "desc": "Senior level"},
                    <AllowedValueInfo>{"value": "lead", "desc": "Lead level"},
                ),
            },
            "phone": {
                "type": StringOrNothingType,
                "display_name": "Phone",
                "short_desc": "The worker's phone number",
                "desc": "The updated phone number of the worker",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Deel update worker action data provider
public class DeelUpdateWorkerDataProvider inherits DeelDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Update an existing worker/person in Deel",
            "type": "DeelUpdateWorkerDataProvider",
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
        return "Update an existing worker/person in Deel";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new DeelUpdateWorkerRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new DeelWorkerDataType();
    }

    #! Performs the request
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string worker_id = remove req.worker_id;
        return doPatch("people/" + encode_url(worker_id, True), req).data;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace DeelDataProvider
