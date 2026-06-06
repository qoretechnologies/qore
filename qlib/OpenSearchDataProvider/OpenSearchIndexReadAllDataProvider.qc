# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OpenSearchDataProvider module definition

/** OpenSearchIndexReadAllDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the OpenSearchDataProvider module
public namespace OpenSearchDataProvider {
#! The OpenSearch read all indices API data provider
/** This class implements an API that return the descriptions of all known indices in the OpenSearch server.
*/
public class OpenSearchIndexReadAllDataProvider inherits OpenSearchDataProviderBase {
    public {
        #! Provider info
        const hash<DataProviderInfo> ProviderInfo = <DataProviderInfo>{
            "name": "read-all",
            "desc": "OpenSearch index read-all API data provider",
            "type": "OpenSearchIndexReadAllDataProvider",
            "constructor_options": OpenSearchDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const hash<DataProviderSummaryInfo> ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const AbstractDataProviderType RequestType = new OpenSearchIndexReadAllRequestDataType();

        #! Response type
        const AbstractDataProviderType ResponseType = new OpenSearchIndexReadResponseDataType();

        #! Query args
        const list<string> QueryArgs = OpenSearchIndexReadDataProvider::QueryArgs;
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        checkOptions("CONSTRUCTOR-ERROR", OpenSearchDataProvider::ConstructorOptions, options);
        rest = OpenSearchDataProvider::getRestConnection(options);
    }

    #! Creates the object from a REST connection
    constructor(RestClient::AbstractRestClient rest) : OpenSearchDataProviderBase(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("OpenSearch data index %s provider for `%s`", ProviderInfo.name, rest.getSafeUrl());
    }

    #! Makes a request and returns the response
    /** @param req the request to serialize and make according to the request type
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        bool q;
        string uri_path = "*";
        foreach string arg in (QueryArgs) {
            if (!exists req{arg}) {
                continue;
            }
            if (!q) {
                uri_path += "?";
                q = True;
            } else {
                uri_path += "&";
            }
            uri_path += sprintf("%s=%s", arg, remove req{arg});
        }
        return rest.restGet(uri_path).body;
    }

    #! Returns the description of a successful request message, if any
    /** @return the request type for this provider
    */
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the description of a response message, if this object represents a response message
    /** @return the response type for this response message
    */
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns data provider static info
    hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! OpenSearch index read-all API request
public class OpenSearchIndexReadAllRequestDataType inherits DataProvider::HashDataType {
    public {
        #! Field descriptions
        const hash<string, AbstractDataField> Fields = OpenSearchIndexReadRequestDataType::Fields - "index";
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}
}
