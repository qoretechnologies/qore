# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore InstantlyDataProvider module definition

/*  InstantlyMarkAccountFixedDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for marking an account as fixed
public class InstantlyMarkAccountFixedRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "Account ID",
                "short_desc": "The account ID to mark as fixed",
                "desc": "The unique account UUID to mark as fixed",
                "required": True,
                "preselected": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for marking an email account as fixed
public class InstantlyMarkAccountFixedDataProvider inherits InstantlyDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "mark-account-fixed",
            "desc": "Mark an Instantly email account as fixed",
            "type": "InstantlyMarkAccountFixedDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static InstantlyMarkAccountFixedRequestDataType RequestType();

        #! Response type
        static InstantlyAccountDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(InstantlyRestClient::InstantlyRestClient rest)
            : InstantlyDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "mark-account-fixed";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Mark an Instantly email account as fixed";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost(sprintf("/accounts/%s/mark-fixed", req.id));
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
