# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FirecrawlMapResponseDataType.qc response type for the Firecrawl map action

/*  FirecrawlMapResponseDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the FirecrawlDataProvider module
public namespace FirecrawlDataProvider {

#! Response type for the Firecrawl map action
public class FirecrawlMapResponseDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "success": {
                "display_name": "Success",
                "short_desc": "Whether the request succeeded",
                "desc": "Whether the map request completed successfully",
                "type": SoftBoolType,
            },
            "links": {
                "display_name": "Links",
                "short_desc": "Discovered URLs",
                "desc": "List of discovered URLs from the website map",
                "type": new ListDataType("links", SoftStringType, True),
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace FirecrawlDataProvider
