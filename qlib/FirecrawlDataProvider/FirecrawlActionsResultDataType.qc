# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FirecrawlActionsResultDataType.qc browser actions result type for Firecrawl scrape responses

/*  FirecrawlActionsResultDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Browser actions result type for Firecrawl scrape responses
/** Contains results from browser actions executed during scraping, including
    screenshots, scrape captures, JavaScript return values, and generated PDFs.
*/
public class FirecrawlActionsResultDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "screenshots": {
                "display_name": "Screenshots",
                "short_desc": "Screenshot URLs",
                "desc": "List of URLs to captured screenshot images. URLs expire after "
                    "24 hours. Order matches the `screenshot` actions in the request",
                "type": SoftListOrNothingType,
            },
            "scrapes": {
                "display_name": "Scrapes",
                "short_desc": "Page scrape captures",
                "desc": "List of scrape results captured during browser actions. Each "
                    "entry contains the page content at the time the `scrape` action "
                    "was executed",
                "type": SoftListOrNothingType,
            },
            "javascriptReturns": {
                "display_name": "JavaScript Returns",
                "short_desc": "JavaScript execution results",
                "desc": "List of return values from `executeJavascript` actions",
                "type": SoftListOrNothingType,
            },
            "pdfs": {
                "display_name": "PDFs",
                "short_desc": "Generated PDF URLs",
                "desc": "List of signed URLs to generated PDF files. Order matches "
                    "the `pdf` actions in the request",
                "type": SoftListOrNothingType,
            },
        };
    }

    #! Creates the type
    constructor(*Type base_type) : HashDataType(base_type ?? AutoHashType) {
        addQoreFields(Fields);
    }
}

} # namespace FirecrawlDataProvider
