# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SquareExecuteApiCallDataProvider.qc Square execute custom API call data provider

/*  SquareExecuteApiCallDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SquareDataProvider module
public namespace SquareDataProvider {

#! Square execute custom API call data provider
public class SquareExecuteApiCallDataProvider inherits SquareDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "execute-api-call",
            "desc": "Make a custom API call to Square",
            "type": "SquareExecuteApiCallDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type for custom API calls
        const RequestType = new HashDataType("SquareExecuteApiCallRequest", {
            "method": {
                "display_name": "HTTP Method",
                "type": StringType,
                "desc": "The HTTP method to use (GET, POST, PUT, DELETE)",
                "required": True,
            },
            "path": {
                "display_name": "API Path",
                "type": StringType,
                "desc": "The API path to call (relative to /v2, e.g. 'customers' or "
                    "'payments/PAYMENT_ID')",
                "required": True,
            },
            "body": {
                "display_name": "Request Body",
                "type": AutoHashOrNothingType,
                "desc": "Optional request body for POST and PUT requests",
            },
        });

        #! Response type - auto hash since the response structure depends on the API call
        const ResponseType = AbstractDataProviderTypeMap."hash";
    }

    #! Creates the object with a REST client
    constructor(SquareRestClient::SquareRestClient rest) : SquareDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "execute-api-call";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Make a custom API call to Square";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string method = req.method.upr();
        string path = req.path;

        switch (method) {
            case "GET":
                return doGet(path);
            case "POST":
                return doPost(path, req.body);
            case "PUT":
                return doPut(path, req.body);
            case "DELETE":
                return doDelete(path);
            default:
                throw "SQUARE-ERROR", sprintf("unsupported HTTP method: %s; supported methods: "
                    "GET, POST, PUT, DELETE", method);
        }
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SquareDataProvider
