# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShipStationDataProvider module definition

/*  ShipStationMakeApiCallDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ShipStationDataProvider module
public namespace ShipStationDataProvider {

#! Request type for making a generic API call
public class ShipStationMakeApiCallRequestDataType inherits HashDataType {
    private {
        const Fields = {
            "method": {
                "type": StringType,
                "display_name": "HTTP Method",
                "short_desc": "The HTTP method",
                "desc": "The HTTP method to use for the request",
                "example_value": "GET",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "GET",
                        "desc": "HTTP GET request",
                    },
                    <AllowedValueInfo>{
                        "value": "POST",
                        "desc": "HTTP POST request",
                    },
                    <AllowedValueInfo>{
                        "value": "PUT",
                        "desc": "HTTP PUT request",
                    },
                    <AllowedValueInfo>{
                        "value": "DELETE",
                        "desc": "HTTP DELETE request",
                    },
                ),
            },
            "path": {
                "type": StringType,
                "display_name": "API Path",
                "short_desc": "The API endpoint path",
                "desc": "The API path to call (e.g., /orders, /carriers, /stores/123)",
                "example_value": "/orders",
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
                "desc": "Query parameters to add to the URL",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Response type for generic API call
public class ShipStationMakeApiCallResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "body": {
                "type": AutoHashOrNothingType,
                "display_name": "Response Body",
                "short_desc": "The response body",
                "desc": "The JSON response body from the API. Errors throw exceptions, so successful "
                    "responses always indicate a 2xx status code.",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for making a generic API call
public class ShipStationMakeApiCallDataProvider inherits ShipStationDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "make-api-call",
            "desc": "Make a generic API call to the ShipStation API",
            "type": "ShipStationMakeApiCallDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static ShipStationMakeApiCallRequestDataType RequestType();

        #! Response type
        static ShipStationMakeApiCallResponseDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(ShipStationRestClient::ShipStationRestClient rest)
            : ShipStationDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(ShipStationRestClient::ShipStationRestConnection conn) : ShipStationDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "make-api-call";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Make a generic API call to the ShipStation API";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string method = req.method.upr();
        string path = req.path;

        if (!path.val()) {
            throw "SHIPSTATION-ERROR", "path is required";
        }

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
                throw "SHIPSTATION-ERROR", sprintf("unsupported HTTP method: %s", method);
        }

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: response=%y", response));

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

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ShipStationDataProvider
