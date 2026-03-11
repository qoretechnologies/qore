# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file CloseUrlEntryDataType.qc Close URL entry data type

/*  CloseUrlEntryDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the CloseDataProvider module
public namespace CloseDataProvider {

#! Close URL entry data type for contact URLs
public class CloseUrlEntryDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "type": {
                "type": StringOrNothingType,
                "display_name": "Type",
                "short_desc": "The URL type",
                "desc": "The type of URL (e.g., `url`, `office`, `home`)",
                "example_value": "url",
            },
            "url": {
                "type": StringType,
                "display_name": "URL",
                "short_desc": "The URL value",
                "desc": "The URL value",
                "example_value": "https://www.example.com",
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace CloseDataProvider
