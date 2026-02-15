# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShippoDataProvider module definition

/*  ShippoMakeApiCallDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ShippoDataProvider module
public namespace ShippoDataProvider {

#! Data provider for making generic API calls to Shippo
public class ShippoMakeApiCallDataProvider inherits ShippoDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "make-api-call",
            "desc": "Make a generic API call to the Shippo API",
            "type": "ShippoMakeApiCallDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static ShippoMakeApiCallRequestDataType RequestType();

        #! Response type
        static ShippoMakeApiCallResponseDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(ShippoRestClient::ShippoRestClient rest) : ShippoDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(ShippoRestClient::ShippoRestConnection conn) : ShippoDataProviderBase(conn) {
    }

    #! Creates the object with options
    constructor(*hash<auto> options) : ShippoDataProviderBase(options) {
    }

    #! Returns the provider name
    string getName() {
        return "make-api-call";
    }

    #! Returns the data provider description
    *string getDesc() {
        return "Make a generic API call to the Shippo API";
    }

    #! Makes a request and returns the response
    /** @param req the request
        @param request_options the request options (ignored)

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string method = req.method.upr();
        string path = req.path;

        # Ensure path starts with /
        if (!path.startsWith("/")) {
            path = "/" + path;
        }

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: %s %s", method, path));

        auto response;
        switch (method) {
            case "GET":
                response = doGet(path, req.query_params);
                break;
            case "POST":
                response = doPost(path, req.body);
                break;
            case "PUT":
                response = doPut(path, req.body);
                break;
            case "DELETE":
                response = doDelete(path);
                break;
            default:
                throw "SHIPPO-ERROR", sprintf("unsupported HTTP method: %s", method);
        }

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: completed %s %s", method, path));

        return {
            "body": response,
        };
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

#! Request data type for making generic API calls
public class ShippoMakeApiCallRequestDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "method": {
                "type": StringType,
                "display_name": "HTTP Method",
                "short_desc": "The HTTP method",
                "desc": "The HTTP method to use for the request",
                "required": True,
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
                "desc": "The API path to call (e.g., /shipments, /addresses, /transactions)",
                "required": True,
            },
            "body": {
                "type": AutoHashOrNothingType,
                "display_name": "Request Body",
                "short_desc": "The request body",
                "desc": "The request body for POST/PUT requests (will be sent as JSON)",
            },
            "query_params": {
                "type": AutoHashOrNothingType,
                "display_name": "Query Parameters",
                "short_desc": "URL query parameters",
                "desc": "Query parameters to add to the URL (for GET requests)",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("ShippoMakeApiCallRequestDataType") {
        addQoreFields(Fields);
    }
}

#! Response data type for making generic API calls
public class ShippoMakeApiCallResponseDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "body": {
                "type": AbstractDataProviderType::get(AutoType),
                "display_name": "Response Body",
                "short_desc": "The API response body",
                "desc": "The response body from the Shippo API",
            },
        };
    }

    #! Creates the object
    constructor() : HashDataType("ShippoMakeApiCallResponseDataType") {
        addQoreFields(Fields);
    }
}

} # namespace ShippoDataProvider
