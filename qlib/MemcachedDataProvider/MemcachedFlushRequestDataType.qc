# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MemcachedFlushRequestDataType class definition

/** MemcachedFlushRequestDataType.qc Copyright 2025 Qore Technologies, s.r.o.

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
#! Data type for memcached flush requests
public class MemcachedFlushRequestDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "delay": {
                "type": IntOrNothingType,
                "display_name": "Delay",
                "short_desc": "Delay before flush",
                "desc": "Optional delay in seconds before all items are invalidated. "
                    "Default is 0 (immediate flush)",
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

#! Data type for memcached flush responses
public class MemcachedFlushResponseDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "flushed": {
                "type": BoolType,
                "display_name": "Flushed",
                "short_desc": "Cache was flushed",
                "desc": "True if the flush command was accepted by the server",
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

#! Constant for the flush request type
public const MemcachedFlushRequestDataType = new MemcachedFlushRequestDataType();

#! Constant for the flush response type
public const MemcachedFlushResponseDataType = new MemcachedFlushResponseDataType();
}
