# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MemcachedTouchRequestDataType class definition

/** MemcachedTouchRequestDataType.qc Copyright 2025 Qore Technologies, s.r.o.

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
#! Data type for memcached touch requests
public class MemcachedTouchRequestDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "key": {
                "type": StringType,
                "display_name": "Key",
                "short_desc": "Cache key",
                "desc": "The key to update the expiration time for",
                "example_value": "user:123",
            },
            "exptime": {
                "type": IntType,
                "display_name": "Expiration Time",
                "short_desc": "New TTL in seconds",
                "desc": "New expiration time in seconds. 0 means never expire. "
                    "Values over 30 days are treated as absolute UNIX timestamps",
                "example_value": 3600,
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data type for memcached touch responses
public class MemcachedTouchResponseDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "touched": {
                "type": BoolType,
                "display_name": "Touched",
                "short_desc": "Expiration was updated",
                "desc": "True if the expiration time was successfully updated",
                "example_value": True,
            },
            "status": {
                "type": StringType,
                "display_name": "Status",
                "short_desc": "Response status",
                "desc": "Status code from the server",
                "example_value": "OK",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Constant for the touch request type
public const MemcachedTouchRequestDataType = new MemcachedTouchRequestDataType();

#! Constant for the touch response type
public const MemcachedTouchResponseDataType = new MemcachedTouchResponseDataType();
}
