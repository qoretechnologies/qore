# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FirecrawlCrawlResponseDataType.qc response type for the Firecrawl crawl action

/*  FirecrawlCrawlResponseDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Response type for the Firecrawl crawl action (also used for get-crawl-status)
public class FirecrawlCrawlResponseDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "success": {
                "display_name": "Success",
                "short_desc": "Whether the request succeeded",
                "desc": "Whether the crawl request completed successfully",
                "type": SoftBoolType,
            },
            "status": {
                "display_name": "Status",
                "short_desc": "Crawl job status",
                "desc": "The current status of the crawl job (e.g., `scraping`, `completed`, `failed`)",
                "type": StringOrNothingType,
            },
            "total": {
                "display_name": "Total",
                "short_desc": "Total pages discovered",
                "desc": "Total number of pages discovered during crawling",
                "type": SoftIntOrNothingType,
            },
            "completed": {
                "display_name": "Completed",
                "short_desc": "Pages completed",
                "desc": "Number of pages that have been successfully scraped",
                "type": SoftIntOrNothingType,
            },
            "creditsUsed": {
                "display_name": "Credits Used",
                "short_desc": "API credits consumed",
                "desc": "Number of API credits consumed by this crawl operation",
                "type": SoftIntOrNothingType,
            },
            "expiresAt": {
                "display_name": "Expires At",
                "short_desc": "Result expiration time",
                "desc": "Timestamp when the crawl results will expire",
                "type": StringOrNothingType,
            },
            "data": {
                "display_name": "Data",
                "short_desc": "Crawled page data",
                "desc": "List of scraped page data, each containing `markdown`, `html`, "
                    "`metadata`, and other requested formats",
                "type": new ListDataType("data", new FirecrawlScrapeDataItemDataType(AutoHashOrNothingType), True),
            },
            "next": {
                "display_name": "Next",
                "short_desc": "Next page URL",
                "desc": "URL for fetching the next page of results, if pagination is required",
                "type": StringOrNothingType,
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace FirecrawlDataProvider
