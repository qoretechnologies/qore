# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file GoHighLevelExecuteApiCallDataProvider.qc GoHighLevel execute API call action data provider

/*  GoHighLevelExecuteApiCallDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

%modern

#! Contains all public definitions in the GoHighLevelDataProvider module
public namespace GoHighLevelDataProvider {

#! Request data type for the execute API call action
public class GoHighLevelExecuteApiCallRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "method": {
                "type": StringType,
                "display_name": "HTTP Method",
                "short_desc": "The HTTP method to use",
                "desc": "HTTP method: `GET`, `POST`, `PUT`, or `DELETE`",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "GET", "display_name": "GET"},
                    <AllowedValueInfo>{"value": "POST", "display_name": "POST"},
                    <AllowedValueInfo>{"value": "PUT", "display_name": "PUT"},
                    <AllowedValueInfo>{"value": "DELETE", "display_name": "DELETE"},
                ),
            },
            "path": {
                "type": StringType,
                "display_name": "API Path",
                "short_desc": "API endpoint path",
                "desc": "API path (e.g., `contacts`, `opportunities/search`)",
                "required": True,
                "example_value": "contacts",
            },
            "body": {
                "type": AutoHashOrNothingType,
                "display_name": "Request Body",
                "short_desc": "Request body for POST/PUT requests",
                "desc": "Request body for POST/PUT requests",
            },
            "query_params": {
                "type": AutoHashOrNothingType,
                "display_name": "Query Parameters",
                "short_desc": "Query parameters to append to the URL",
                "desc": "Query parameters to append to the URL",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

#! GoHighLevel execute API call action data provider
public class GoHighLevelExecuteApiCallDataProvider inherits GoHighLevelDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "execute-api-call",
            "desc": "Make an arbitrary authorized API call to GoHighLevel",
            "type": "GoHighLevelExecuteApiCallDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;
    }

    #! Creates the object with a REST client
    constructor(GoHighLevelRestClient::GoHighLevelRestClient rest) : GoHighLevelDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "execute-api-call";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Make an arbitrary authorized API call to GoHighLevel";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new GoHighLevelExecuteApiCallRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderTypeMap."hash";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string method = req.method;
        string path = req.path;
        if (req.query_params) {
            path += "?" + buildQueryString(req.query_params);
        }
        switch (method.upr()) {
            case "GET": return doGet(path);
            case "POST": return doPost(path, req.body);
            case "PUT": return doPut(path, req.body);
            case "DELETE": doDelete(path); return {};
            default: throw "GOHIGHLEVEL-ERROR", sprintf("unsupported HTTP method: %s", method);
        }
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace GoHighLevelDataProvider
