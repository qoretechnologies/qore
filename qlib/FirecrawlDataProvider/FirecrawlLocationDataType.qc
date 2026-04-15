# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FirecrawlLocationDataType.qc geolocation settings type for Firecrawl requests

/*  FirecrawlLocationDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Geolocation settings type for Firecrawl requests
public class FirecrawlLocationDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "country": {
                "display_name": "Country",
                "short_desc": "Country code",
                "desc": "ISO 3166-1 alpha-2 country code (e.g., `US`, `AU`, `DE`, `JP`). "
                    "Defaults to `US`",
                "type": StringOrNothingType,
                "example_value": "US",
            },
            "languages": {
                "display_name": "Languages",
                "short_desc": "Preferred languages",
                "desc": "List of preferred languages/locales in priority order. Defaults to "
                    "the language of the specified location",
                "type": SoftListOrNothingType,
                "example_value": ("en",),
            },
        };
    }

    #! Creates the type
    constructor(*Type base_type) : HashDataType(base_type ?? AutoHashType) {
        addQoreFields(Fields);
    }
}

} # namespace FirecrawlDataProvider
