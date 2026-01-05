# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MemcachedGetResponseDataType class definition

/** MemcachedGetResponseDataType.qc Copyright 2025 Qore Technologies, s.r.o.

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
#! Data type for memcached get responses
public class MemcachedGetResponseDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "found": {
                "type": BoolType,
                "display_name": "Found",
                "short_desc": "Key was found",
                "desc": "True if the key was found in the cache",
                "example_value": True,
            },
            "key": {
                "type": StringType,
                "display_name": "Key",
                "short_desc": "Cache key",
                "desc": "The key that was requested",
                "example_value": "user:123",
            },
            "value": {
                "type": DataOrNothingType,
                "display_name": "Value",
                "short_desc": "Cached value",
                "desc": "The cached value, or NOTHING if not found",
            },
            "flags": {
                "type": IntOrNothingType,
                "display_name": "Flags",
                "short_desc": "Stored flags",
                "desc": "The 32-bit flags stored with the item",
                "example_value": 0,
            },
            "cas": {
                "type": IntOrNothingType,
                "display_name": "CAS Token",
                "short_desc": "Compare-and-swap token",
                "desc": "Unique 64-bit CAS token for this item, used for optimistic locking",
                "example_value": 12345678901234,
            },
            "status": {
                "type": StringOrNothingType,
                "display_name": "Status",
                "short_desc": "Response status",
                "desc": "Status code if the key was not found",
                "example_value": "NOT_FOUND",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data type for memcached multi-get responses
public class MemcachedMultiGetResponseDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "items": {
                "type": new ListDataType("GetItemList", new MemcachedGetResponseDataType(), True),
                "display_name": "Items",
                "short_desc": "Retrieved items",
                "desc": "List of items retrieved from the cache",
            },
            "found": {
                "type": IntType,
                "display_name": "Found Count",
                "short_desc": "Number of items found",
                "desc": "Number of keys that were found in the cache",
                "example_value": 3,
            },
            "missing": {
                "type": IntType,
                "display_name": "Missing Count",
                "short_desc": "Number of items not found",
                "desc": "Number of keys that were not found in the cache",
                "example_value": 1,
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Constant for the get response type
public const MemcachedGetResponseDataType = new MemcachedGetResponseDataType();

#! Constant for the multi-get response type
public const MemcachedMultiGetResponseDataType = new MemcachedMultiGetResponseDataType();
}
