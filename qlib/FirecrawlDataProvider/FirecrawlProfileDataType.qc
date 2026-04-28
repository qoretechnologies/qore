# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FirecrawlProfileDataType.qc browser profile sub-type for Firecrawl scrape requests

/*  FirecrawlProfileDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Browser profile configuration for persistent scrape sessions
/** Identifies a named browser profile so cookies, local storage, and other
    session state are preserved across scrape requests.
*/
public class FirecrawlProfileDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "name": {
                "display_name": "Profile Name",
                "short_desc": "Name of the browser profile",
                "desc": "Name of the browser profile to use. The first scrape with a given "
                    "name creates the profile; subsequent scrapes reuse its session state",
                "type": StringOrNothingType,
                "example_value": "default",
            },
            "saveChanges": {
                "display_name": "Save Changes",
                "short_desc": "Persist changes back to the profile",
                "desc": "If `True`, changes made during the scrape (cookies, local storage, "
                    "etc.) are saved back to the named profile. Defaults to `False`",
                "type": SoftBoolOrNothingType,
            },
        };
    }

    #! Creates the type
    constructor(*Type base_type) : HashDataType(base_type ?? AutoHashType) {
        addQoreFields(Fields);
    }
}

} # namespace FirecrawlDataProvider
