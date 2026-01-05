# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MemcachedStorageResponseDataType class definition

/** MemcachedStorageResponseDataType.qc Copyright 2025 Qore Technologies, s.r.o.

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
#! Data type for memcached storage responses
public class MemcachedStorageResponseDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "stored": {
                "type": BoolType,
                "display_name": "Stored",
                "short_desc": "Value was stored",
                "desc": "True if the value was successfully stored",
                "example_value": True,
            },
            "status": {
                "type": StringType,
                "display_name": "Status",
                "short_desc": "Response status",
                "desc": "Status code from the server: OK, NOT_STORED, EXISTS, NOT_FOUND, etc.",
                "example_value": "OK",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "OK", "desc": "Operation succeeded"},
                    <AllowedValueInfo>{"value": "NOT_STORED", "desc": "Item was not stored (add/replace condition failed)"},
                    <AllowedValueInfo>{"value": "EXISTS", "desc": "CAS value mismatch"},
                    <AllowedValueInfo>{"value": "NOT_FOUND", "desc": "Key not found (for replace/append/prepend)"},
                    <AllowedValueInfo>{"value": "VALUE_TOO_LARGE", "desc": "Value exceeds maximum size"},
                ),
            },
            "cas": {
                "type": IntOrNothingType,
                "display_name": "CAS Token",
                "short_desc": "Compare-and-swap token",
                "desc": "The CAS token for the stored item, can be used for subsequent CAS operations",
                "example_value": 12345678901234,
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Constant for the storage response type
public const MemcachedStorageResponseDataType = new MemcachedStorageResponseDataType();
}
