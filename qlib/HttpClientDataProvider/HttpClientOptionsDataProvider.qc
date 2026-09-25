# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore HttpClientDataProvider module definition

/** HttpOptionsDataProvider.qc Copyright 2023 - 2026 Qore Technologies, s.r.o.

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
#! The HTTP options data provider class
public class HttpClientOptionsDataProvider inherits HttpClientDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "options",
            "desc": "HTTP OPTIONS data provider; makes an `OPTIONS` request to an HTTP server and returns the response",
            "type": "HttpClientOptionsDataProvider",
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
    /** @param options the constructor options; see @ref HttpClientDataProvider::ConstructorOptions

        @throw CONSTRUCTOR-ERROR invalid or missing constructor option
    */
    constructor(*hash<auto> options) : HttpClientDataProviderBase(options) {
    }

    #! Creates the object from an HTTP connection
    /** Requests are made with a connection manager created from the configuration of \a http; see
        @ref httpclientdataprovider_client

        @param http the synchronous HTTP client whose configuration is used; it is not used for requests
    */
    constructor(HTTPClient http) : HttpClientDataProviderBase(http) {
    }

    #! Creates the object from an HttpClientIo connection manager
    /** @param mgr the connection manager for requests; it is never closed by the data provider
        @param url the target URL
        @param options request options; see the corresponding constructor of @ref HttpClientDataProviderBase

        @throw CONSTRUCTOR-ERROR the URL has no host, or an unsupported option was given

        @since HttpClientDataProvider 1.2
    */
    constructor(HttpClientIo::HttpClientConnectionManager mgr, string url, *hash<auto> options)
            : HttpClientDataProviderBase(mgr, url, options) {
    }

    #! Returns the data provider name
    string getName() {
        return "options";
    }

    #! Makes a request and returns the response
    /** @param req the request info
        @param request_options the request options after processing by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doHttpRequest("OPTIONS", NOTHING, req.path, req.hdr);
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
