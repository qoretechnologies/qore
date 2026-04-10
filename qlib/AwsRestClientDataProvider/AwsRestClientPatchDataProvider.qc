# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AwsRestClientDataProvider module definition

/** AwsRestPatchDataProvider.qc Copyright 2023 - 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the AwsRestClientDataProvider module
public namespace AwsRestClientDataProvider {
#! The AWS REST client PATCH data provider class
public class AwsRestClientPatchDataProvider inherits AwsRestClientDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "patch",
            "desc": "AWS REST PATCH data provider; makes a `PATCH` request to a REST server and returns the response",
            "type": "AwsRestClientPatchDataProvider",
            "constructor_options": AwsRestClientDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new RestClientCallRequestDataType();

        #! Response type
        const ResponseType = RestClientCallResponseDataType;
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        checkOptions("CONSTRUCTOR-ERROR", AwsRestClientDataProvider::ConstructorOptions, options);
        rest = AwsRestClientDataProvider::getAwsRestConnection(options);
    }

    #! Creates the object from a REST connection
    constructor(AwsRestClientIo rest) : AwsRestClientDataProviderBase(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return "patch";
    }

    #! Makes a request and returns the response
    /** @param req the request info
        @param request_options the request options after processing by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return RestClientDataProvider::makeRequest(rest, "PATCH", req.body, req.path, req.hdr);
    }

    #! Returns the description of a successful request message, if any
    /** @return the request type for this provider
    */
    private *AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the description of a response message, if this object represents a response message
    /** @return the response type for this response message
    */
    private *AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns data provider static info
    hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}
}
