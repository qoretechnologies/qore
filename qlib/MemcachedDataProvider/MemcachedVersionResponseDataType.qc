# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MemcachedVersionResponseDataType class definition

/** MemcachedVersionResponseDataType.qc Copyright 2025 Qore Technologies, s.r.o.

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
#! Data type for memcached version responses
public class MemcachedVersionResponseDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "version": {
                "type": StringOrNothingType,
                "display_name": "Version",
                "short_desc": "Server version",
                "desc": "The memcached server version string",
                "example_value": "1.6.22",
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

#! Constant for the version response type
public const MemcachedVersionResponseDataType = new MemcachedVersionResponseDataType();
}
