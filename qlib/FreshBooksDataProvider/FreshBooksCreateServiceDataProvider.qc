# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FreshBooksCreateServiceDataProvider.qc FreshBooks create service action data provider

/*  FreshBooksCreateServiceDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the FreshBooksDataProvider module
public namespace FreshBooksDataProvider {

#! FreshBooks create service action data provider
public class FreshBooksCreateServiceDataProvider inherits FreshBooksDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-service",
            "desc": "Create a new service in FreshBooks",
            "type": "FreshBooksCreateServiceDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;
    }

    #! Creates the object with a REST client
    constructor(FreshBooksRestClient::FreshBooksRestClient rest) : FreshBooksDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "create-service";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Create a new service in FreshBooks";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new FreshBooksServiceCreateDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new FreshBooksServiceDataType();
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        req = setBusinessIdFromReq(req);
        return doPost(businessPath("/service"), {"service": req}).service;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace FreshBooksDataProvider
