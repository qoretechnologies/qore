# -*- mode: qore; indent-tabs-mode: nil -*-

/** RestCallWithMethodRequestDataType.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#! contains all public definitions in the RestClientDataProvider module
public namespace RestClientDataProvider {
#! Constant type for REST call requests with a method
public const RestClientCallWithMethodRequestDataType = new RestClientCallWithMethodRequestDataType();

#! Data type for REST client call requests with a specific method
public class RestClientCallWithMethodRequestDataType inherits RestClientCallRequestDataType {
    public {
        #! Field descriptions
        const Fields = RestClientCallRequestDataType::Fields + {
            "method": {
                "type": StringType,
                "desc": "The REST method to use",
                "required": True,
                "default_value": "GET",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "DELETE",
                        "short_desc": "HTTP DELETE method",
                        "desc": "HTTP `DELETE` method",
                        "value": "DELETE",
                    },
                    <AllowedValueInfo>{
                        "display_name": "GET",
                        "short_desc": "HTTP GET method",
                        "desc": "HTTP `GET` method",
                        "value": "GET",
                    },
                    <AllowedValueInfo>{
                        "display_name": "HEAD",
                        "short_desc": "HTTP HEAD method",
                        "desc": "HTTP `HEAD` method",
                        "value": "HEAD",
                    },
                    <AllowedValueInfo>{
                        "display_name": "OPTIONS",
                        "short_desc": "HTTP OPTIONS method",
                        "desc": "HTTP `OPTIONS` method",
                        "value": "OPTIONS",
                    },
                    <AllowedValueInfo>{
                        "display_name": "PATCH",
                        "short_desc": "HTTP PATCH method",
                        "desc": "HTTP `PATCH` method",
                        "value": "PATCH",
                    },
                    <AllowedValueInfo>{
                        "display_name": "POST",
                        "short_desc": "HTTP POST method",
                        "desc": "HTTP `POST` method",
                        "value": "POST",
                    },
                    <AllowedValueInfo>{
                        "display_name": "PUT",
                        "short_desc": "HTTP PUT method",
                        "desc": "HTTP `PUT` method",
                        "value": "PUT",
                    },
                    <AllowedValueInfo>{
                        "display_name": "TRACE",
                        "short_desc": "HTTP TRACE method",
                        "desc": "HTTP `TRACE` method",
                        "value": "TRACE",
                    },
                ),
            },
        };
    }

    #! Creates the object
    constructor() {
    }

    #! Returns fields to add
    private hash<auto> getConstructorFields() {
        return Fields;
    }
}
}
