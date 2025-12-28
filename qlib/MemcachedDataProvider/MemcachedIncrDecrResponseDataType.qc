# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MemcachedIncrDecrResponseDataType class definition

/** MemcachedIncrDecrResponseDataType.qc Copyright 2025 Qore Technologies, s.r.o.

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

%require-types
%enable-all-warnings
%new-style
%strict-args

public namespace MemcachedDataProvider {
#! Data type for memcached incr/decr responses
public class MemcachedIncrDecrResponseDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "success": {
                "type": BoolType,
                "display_name": "Success",
                "short_desc": "Operation succeeded",
                "desc": "True if the increment/decrement operation succeeded",
                "example_value": True,
            },
            "value": {
                "type": IntOrNothingType,
                "display_name": "Value",
                "short_desc": "New value",
                "desc": "The new value after the increment/decrement operation",
                "example_value": 42,
            },
            "status": {
                "type": StringType,
                "display_name": "Status",
                "short_desc": "Response status",
                "desc": "Status code from the server",
                "example_value": "OK",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "OK", "desc": "Operation succeeded"},
                    <AllowedValueInfo>{"value": "NOT_FOUND", "desc": "Key not found"},
                    <AllowedValueInfo>{"value": "NON_NUMERIC", "desc": "Value is not numeric"},
                ),
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Constant for the incr/decr response type
public const MemcachedIncrDecrResponseDataType = new MemcachedIncrDecrResponseDataType();
}
