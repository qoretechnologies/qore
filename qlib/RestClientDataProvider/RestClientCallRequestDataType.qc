# -*- mode: qore; indent-tabs-mode: nil -*-

/** RestCallRequestDataType.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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

#! contains all public definitions in the RestClientDataProvider module
public namespace RestClientDataProvider {
#! Constant type for REST call requests
public const RestClientCallRequestDataType = new RestClientCallRequestDataType();

#! Data type for REST call requests
public class RestClientCallRequestDataType inherits HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "body": {
                "display_name": "Body",
                "type": AutoType,
                "desc": "The message body",
            },
            "path": {
                "display_name": "URI path",
                "type": StringType,
                "desc": "The URI path",
                "default_value": "/",
            },
            "query_args": {
                "display_name": "Query Arguments",
                "type": AutoHashOrNothingType,
                "short_desc": "Any query to include in the request",
                "desc": "Any query to include in the request; these will be added to the URI path in the request",
            },
            "hdr": {
                "display_name": "Headers",
                "type": AutoHashOrNothingType,
                "desc": "Any headers to include in the request",
            },
        } + (
            map {
                $1.key: ($1.value{"display_name", "short_desc", "desc", "allowed_values", "default_value"} + {
                    "type": AbstractDataProviderTypeMap{$1.value.type == "auto" ? $1.value.type : "*" + $1.value.type},
                }),
            }, (RestConnection::ConnectionScheme.options - ("headers", "validator")).pairIterator(),
                $1.key !~ /swagger/ && $1.key !~ /ssl/ && $1.key !~ /openapi3/
        );
    }

    #! Creates the object
    constructor() {
        addQoreFields(getConstructorFields());
    }

    #! Returns fields to add
    private hash<auto> getConstructorFields() {
        return Fields;
    }
}
}
