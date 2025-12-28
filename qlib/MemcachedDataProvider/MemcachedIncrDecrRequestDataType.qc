# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MemcachedIncrDecrRequestDataType class definition

/** MemcachedIncrDecrRequestDataType.qc Copyright 2025 Qore Technologies, s.r.o.

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
#! Data type for memcached incr/decr requests
public class MemcachedIncrDecrRequestDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "key": {
                "type": StringType,
                "display_name": "Key",
                "short_desc": "Cache key",
                "desc": "The key of the numeric value to increment/decrement",
                "example_value": "counter:page_views",
            },
            "delta": {
                "type": IntType,
                "display_name": "Delta",
                "short_desc": "Amount to add/subtract",
                "desc": "The amount to increment or decrement by (unsigned 64-bit integer)",
                "example_value": 1,
                "default_value": 1,
            },
            "initial": {
                "type": IntOrNothingType,
                "display_name": "Initial Value",
                "short_desc": "Initial value if key missing",
                "desc": "Initial value if the key does not exist. "
                    "If not specified and key doesn't exist, operation fails",
                "example_value": 0,
            },
            "exptime": {
                "type": IntOrNothingType,
                "display_name": "Expiration Time",
                "short_desc": "TTL for new keys",
                "desc": "Expiration time for newly created keys (when initial is used)",
                "example_value": 0,
                "default_value": 0,
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Constant for the incr/decr request type
public const MemcachedIncrDecrRequestDataType = new MemcachedIncrDecrRequestDataType();
}
