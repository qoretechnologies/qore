# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MemcachedGetRequestDataType class definition

/** MemcachedGetRequestDataType.qc Copyright 2025 Qore Technologies, s.r.o.

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
#! Data type for memcached get requests
public class MemcachedGetRequestDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "key": {
                "type": StringType,
                "display_name": "Key",
                "short_desc": "Cache key",
                "desc": "The key to retrieve (max 250 bytes)",
                "example_value": "user:123",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data type for memcached multi-get requests
public class MemcachedMultiGetRequestDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "keys": {
                "type": new ListDataType("KeyList", StringType, True),
                "display_name": "Keys",
                "short_desc": "Cache keys to retrieve",
                "desc": "List of keys to retrieve in a single batch operation",
                "example_value": ("user:1", "user:2", "user:3"),
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Constant for the get request type
public const MemcachedGetRequestDataType = new MemcachedGetRequestDataType();

#! Constant for the multi-get request type
public const MemcachedMultiGetRequestDataType = new MemcachedMultiGetRequestDataType();
}
