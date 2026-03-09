# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DeelApproveTimeOffRequestDataProvider.qc Deel approve time-off request action data provider

/*  DeelApproveTimeOffRequestDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request type for approving a time-off request
public class DeelApproveTimeOffRequestRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "request_id": {
                "type": StringType,
                "display_name": "Request ID",
                "ref_data": "time_off_requests",
                "short_desc": "The time-off request ID to approve",
                "desc": "The unique identifier of the time-off request to approve",
                "required": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Deel approve time-off request action data provider
public class DeelApproveTimeOffRequestDataProvider inherits DeelDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "approve-request",
            "desc": "Approve a pending time-off request in Deel",
            "type": "DeelApproveTimeOffRequestDataProvider",
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
        return "approve-request";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Approve a pending time-off request in Deel";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new DeelApproveTimeOffRequestRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new DeelTimeOffRequestDataType();
    }

    #! Performs the request
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string request_id = remove req.request_id;
        return doPost("time-offs/requests/" + encode_url(request_id, True) + "/approve", req).data;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace DeelDataProvider
