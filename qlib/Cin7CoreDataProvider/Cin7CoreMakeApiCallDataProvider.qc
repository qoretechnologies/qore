# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Cin7CoreDataProvider module definition

/*  Cin7CoreMakeApiCallDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the Cin7CoreDataProvider module
public namespace Cin7CoreDataProvider {

#! Data provider for making a generic API call
public class Cin7CoreMakeApiCallDataProvider inherits Cin7CoreDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "make-api-call",
            "desc": "Makes a generic API call to the Cin7Core API",
            "type": "Cin7CoreMakeApiCallDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static Cin7CoreMakeApiCallRequestDataType RequestType();

        #! Response type (any hash)
        static HashDataType ResponseType("Cin7CoreApiResponse");
    }

    #! Creates the object with a REST client
    constructor(Cin7CoreRestClient::Cin7CoreRestClient rest) : Cin7CoreDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(Cin7CoreRestClient::Cin7CoreRestConnection conn) : Cin7CoreDataProviderBase(conn) {
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : Cin7CoreDataProviderBase(options) {
    }

    #! Returns the provider name
    string getName() {
        return "make-api-call";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Makes a generic API call to the Cin7Core API";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string method = req.method ?? "GET";
        string path = req.path;
        @assert(path.val(), "path is required");

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: %s %s", method, path));

        hash<auto> response;
        switch (method.upr()) {
            case "GET":
                response.body = doGet(path, req.query_params);
                break;
            case "POST":
                response.body = doPost(path, req.body);
                break;
            case "PUT":
                response.body = doPut(path, req.body);
                break;
            case "DELETE":
                response.body = doDelete(path, req.query_params);
                break;
            default:
                throw "CIN7CORE-METHOD-ERROR", sprintf("Unsupported HTTP method: %y", method);
        }

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: success"));
        return response;
    }

    #! Returns the request type
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Request data type for making a generic API call
public class Cin7CoreMakeApiCallRequestDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "method": {
                "type": SoftStringOrNothingType,
                "display_name": "HTTP Method",
                "short_desc": "The HTTP method",
                "desc": "The HTTP method to use (GET, POST, PUT, DELETE). Defaults to GET",
                "default_value": "GET",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "GET", "desc": "HTTP GET request"},
                    <AllowedValueInfo>{"value": "POST", "desc": "HTTP POST request"},
                    <AllowedValueInfo>{"value": "PUT", "desc": "HTTP PUT request"},
                    <AllowedValueInfo>{"value": "DELETE", "desc": "HTTP DELETE request"},
                ),
            },
            "path": {
                "type": StringType,
                "display_name": "API Path",
                "short_desc": "The API endpoint path",
                "desc": "The API endpoint path (e.g., /Customer, /SaleList)",
                "required": True,
            },
            "query_params": {
                "type": AutoHashOrNothingType,
                "display_name": "Query Parameters",
                "short_desc": "Query string parameters",
                "desc": "Optional query string parameters to include in the request",
            },
            "body": {
                "type": AutoHashOrNothingType,
                "display_name": "Request Body",
                "short_desc": "Request body",
                "desc": "Optional request body for POST/PUT requests",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("Cin7CoreMakeApiCallRequestDataType") {
        addQoreFields(Fields);
    }
}

} # namespace Cin7CoreDataProvider
