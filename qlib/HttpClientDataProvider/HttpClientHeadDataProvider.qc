# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore HttpClientDataProvider module definition

/** HttpHeadDataProvider.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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

#! contains all public definitions in the HttpClientDataProvider module
public namespace HttpClientDataProvider {
#! The HTTP head data provider class
public class HttpClientHeadDataProvider inherits AbstractDataProvider {
    public {
        #! HTTP connection
        HTTPClient http;

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "head",
            "desc": "HTTP HEAD data provider; makes a `HEAD` request to an HTTP server and returns the response",
            "type": "HttpClientHeadDataProvider",
            "constructor_options": HttpClientDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new HttpClientCallWithoutBodyRequestDataType();

        #! Response type
        const ResponseType = new HttpClientCallResponseDataType();
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        checkOptions("CONSTRUCTOR-ERROR", HttpClientDataProvider::ConstructorOptions, options);
        http = HttpClientDataProvider::getHttpConnection(options);
    }

    #! Creates the object from an HTTP connection
    constructor(HTTPClient http) {
        self.http = http;
    }

    #! Returns the data provider name
    string getName() {
        return "head";
    }

    #! Makes a request and returns the response
    /** @param req the request info
        @param request_options the request options after processing by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return HttpClientDataProvider::makeRequest(http, "HEAD", NOTHING, req.path, req.hdr);
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
