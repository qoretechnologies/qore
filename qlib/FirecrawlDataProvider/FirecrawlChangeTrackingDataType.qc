# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FirecrawlChangeTrackingDataType.qc change tracking result type for Firecrawl scrape responses

/*  FirecrawlChangeTrackingDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Change tracking result type for Firecrawl scrape responses
/** Contains change tracking information comparing the current scrape to a previous
    scrape of the same URL. Available when `changeTracking` is in the `formats` list.
*/
public class FirecrawlChangeTrackingDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "previousScrapeAt": {
                "display_name": "Previous Scrape At",
                "short_desc": "Timestamp of the previous scrape",
                "desc": "ISO 8601 timestamp of the previous scrape. `NOTHING` on the first "
                    "scrape of a URL",
                "type": StringOrNothingType,
            },
            "changeStatus": {
                "display_name": "Change Status",
                "short_desc": "Change detection status",
                "desc": "The change detection status compared to the previous scrape",
                "type": StringOrNothingType,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "new", "display_name": "New"},
                    <AllowedValueInfo>{"value": "same", "display_name": "Same"},
                    <AllowedValueInfo>{"value": "changed", "display_name": "Changed"},
                    <AllowedValueInfo>{"value": "removed", "display_name": "Removed"},
                ),
            },
            "visibility": {
                "display_name": "Visibility",
                "short_desc": "Page visibility status",
                "desc": "Whether the page is currently visible or hidden",
                "type": StringOrNothingType,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "visible", "display_name": "Visible"},
                    <AllowedValueInfo>{"value": "hidden", "display_name": "Hidden"},
                ),
            },
            "diff": {
                "display_name": "Diff",
                "short_desc": "Git-diff formatted changes",
                "desc": "Git-diff formatted text showing changes between the previous and "
                    "current scrape. Present only when `git-diff` mode is used and the "
                    "change status is `changed`",
                "type": StringOrNothingType,
            },
            "json": {
                "display_name": "JSON",
                "short_desc": "Field-level change comparisons",
                "desc": "Field-level comparisons with `previous` and `current` values. "
                    "Present only when `json` mode is used and the change status is "
                    "`changed`",
                "type": AutoHashOrNothingType,
            },
        };
    }

    #! Creates the type
    constructor(*Type base_type) : HashDataType(base_type ?? AutoHashType) {
        addQoreFields(Fields);
    }
}

} # namespace FirecrawlDataProvider
