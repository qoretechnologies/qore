# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file CircleExecuteApiCallDataProvider.qc Circle execute API call action data provider

/*  CircleExecuteApiCallDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the CircleDataProvider module
public namespace CircleDataProvider {

#! Request type for executing a raw API call
public class CircleExecuteApiCallRequestDataType inherits HashDataType {
    public {
        #! Field definitions for making a raw API call
        const Fields = {
            "method": {
                "type": StringType,
                "display_name": "HTTP Method",
                "short_desc": "HTTP method to use",
                "desc": "The HTTP method to use for the API call",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "GET", "display_name": "GET"},
                    <AllowedValueInfo>{"value": "POST", "display_name": "POST"},
                    <AllowedValueInfo>{"value": "PUT", "display_name": "PUT"},
                    <AllowedValueInfo>{"value": "PATCH", "display_name": "PATCH"},
                    <AllowedValueInfo>{"value": "DELETE", "display_name": "DELETE"},
                ),
            },
            "path": {
                "type": StringType,
                "display_name": "API Path",
                "short_desc": "API path relative to /api/admin/v2",
                "desc": "The API path relative to `/api/admin/v2`. For example, `posts` "
                    "or `community_members/123`",
                "required": True,
                "example_value": "posts",
            },
            "body": {
                "type": AutoHashOrNothingType,
                "display_name": "Request Body",
                "short_desc": "Request body for POST/PUT/PATCH",
                "desc": "Optional request body for `POST`, `PUT`, and `PATCH` requests",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Circle execute API call action data provider
public class CircleExecuteApiCallDataProvider inherits CircleDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "execute-api-call",
            "desc": "Performs an arbitrary authorized API call to the Circle REST API",
            "type": "CircleExecuteApiCallDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        static CircleExecuteApiCallRequestDataType RequestType();
    }

    constructor(RestClientIo::RestClientIo rest) : CircleDataProviderBase(rest) {
    }

    string getName() {
        return "execute-api-call";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderTypeMap."hash";
    }

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
            case "PATCH":
                return doPatch(path, req.body);
            case "DELETE":
                return doDelete(path);
            default:
                throw "CIRCLE-ERROR", sprintf("unsupported HTTP method: %s", method);
        }
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace CircleDataProvider
