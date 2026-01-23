# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MailgunDataProvider module definition

/*  MailgunMakeApiCallDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the MailgunDataProvider module
public namespace MailgunDataProvider {

#! Request type for making a generic API call
public class MailgunMakeApiCallRequestDataType inherits HashDataType {
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
                "desc": "The API path to call including the version prefix (e.g., /v3/domains, "
                    "/v3/lists/mylist@example.com/members, /v4/address/validate)",
                "example_value": "/v3/domains",
                "required": True,
            },
            "body": {
                "type": AutoHashOrNothingType,
                "display_name": "Request Body",
                "short_desc": "The request body",
                "desc": "The request body for POST/PUT requests. POST requests are form-encoded; "
                    "PUT requests are sent as JSON.",
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
public class MailgunMakeApiCallResponseDataType inherits HashDataType {
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
public class MailgunMakeApiCallDataProvider inherits MailgunDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "make-api-call",
            "desc": "Make a generic API call to the Mailgun API",
            "type": "MailgunMakeApiCallDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static MailgunMakeApiCallRequestDataType RequestType();

        #! Response type
        static MailgunMakeApiCallResponseDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(MailgunRestClient::MailgunRestClient rest, *string api_domain)
            : MailgunDataProviderBase(rest, api_domain) {
    }

    #! Creates the object with a connection
    constructor(MailgunRestClient::MailgunRestConnection conn) : MailgunDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "make-api-call";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Make a generic API call to the Mailgun API";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string method = req.method.toUpper();
        string path = req.path;

        if (!path.val()) {
            throw "MAILGUN-ERROR", "path is required";
        }

        # Ensure path starts with /
        if (!path.startsWith("/")) {
            path = "/" + path;
        }

        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: %s %s", method, path));

        hash<auto> response;

        switch (method) {
            case "GET":
                response = doGet(path, req.query_params);
                break;

            case "POST":
                if (req.body) {
                    response = doPostFormEncoded(path, req.body);
                } else {
                    response = doPost(path);
                }
                break;

            case "PUT":
                response = doPut(path, req.body);
                break;

            case "DELETE":
                response = doDelete(path);
                break;

            default:
                throw "MAILGUN-ERROR", sprintf("unsupported HTTP method: %s", method);
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

} # namespace MailgunDataProvider
