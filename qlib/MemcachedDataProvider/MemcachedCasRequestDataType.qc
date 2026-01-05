# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MemcachedCasRequestDataType class definition

/** MemcachedCasRequestDataType.qc Copyright 2025 Qore Technologies, s.r.o.

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

%modern

public namespace MemcachedDataProvider {
#! Data type for memcached CAS (compare-and-swap) requests
public class MemcachedCasRequestDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "key": {
                "type": StringType,
                "display_name": "Key",
                "short_desc": "Cache key",
                "desc": "The key to update",
                "example_value": "user:123",
            },
            "value": {
                "type": DataType,
                "display_name": "Value",
                "short_desc": "New value",
                "desc": "The new value to store",
            },
            "cas": {
                "type": IntType,
                "display_name": "CAS Token",
                "short_desc": "Expected CAS token",
                "desc": "The CAS token from a previous gets operation. The update only succeeds "
                    "if this matches the current CAS token on the server (optimistic locking)",
                "example_value": 12345678901234,
            },
            "flags": {
                "type": IntOrNothingType,
                "display_name": "Flags",
                "short_desc": "Arbitrary flags",
                "desc": "Arbitrary 32-bit unsigned integer flags stored with the item",
                "example_value": 0,
                "default_value": 0,
            },
            "exptime": {
                "type": IntOrNothingType,
                "display_name": "Expiration Time",
                "short_desc": "TTL in seconds",
                "desc": "Expiration time in seconds",
                "example_value": 3600,
                "default_value": 0,
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Constant for the CAS request type
public const MemcachedCasRequestDataType = new MemcachedCasRequestDataType();
}
