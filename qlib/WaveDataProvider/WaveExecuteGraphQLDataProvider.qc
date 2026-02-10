# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WaveDataProvider module definition

/*  WaveExecuteGraphQLDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the WaveDataProvider module
public namespace WaveDataProvider {

#! Request type for executing an arbitrary GraphQL query
class WaveExecuteGraphQLRequestDataType inherits HashDataType {
    private {
        const Fields = {
            "query": {
                "type": StringType,
                "display_name": "Query",
                "short_desc": "The GraphQL query (required)",
                "desc": "The GraphQL query or mutation string to execute against the Wave API",
                "required": True,
            },
            "variables": {
                "type": AutoHashOrNothingType,
                "display_name": "Variables",
                "short_desc": "The query variables",
                "desc": "Optional variables to pass to the GraphQL query",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for executing arbitrary GraphQL queries against the Wave API
public class WaveExecuteGraphQLDataProvider inherits WaveDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "execute-graphql",
            "desc": "Execute an arbitrary GraphQL query against the Wave API",
            "type": "WaveExecuteGraphQLDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type - the GraphQL query to execute
        static WaveExecuteGraphQLRequestDataType RequestType();

        #! Response type - arbitrary hash response
        static AbstractDataProviderType ResponseType = AbstractDataProviderType::get(AutoHashType);
    }

    #! Creates the object with a REST client
    constructor(WaveRestClient::WaveRestClient rest) : WaveDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(WaveRestClient::WaveRestConnection conn) : WaveDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "execute-graphql";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Execute an arbitrary GraphQL query against the Wave API";
    }

    #! Makes a request and returns the response
    /** @param req the request data (GraphQL query and optional variables)
        @param request_options the request options

        @return the full GraphQL response data
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        @assert(req.query.val(), "query is required");
        hash<auto> result = doGraphQL(req.query, req.variables);
        return result;
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

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace WaveDataProvider
