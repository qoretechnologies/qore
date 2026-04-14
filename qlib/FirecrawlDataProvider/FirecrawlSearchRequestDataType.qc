# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FirecrawlSearchRequestDataType.qc request type for the Firecrawl search action

/*  FirecrawlSearchRequestDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request type for the Firecrawl search action
public class FirecrawlSearchRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "query": {
                "display_name": "Query",
                "short_desc": "The search query",
                "desc": "The search query string (maximum 500 characters)",
                "type": StringType,
                "required": True,
                "example_value": "firecrawl web scraping",
            },
            "limit": {
                "display_name": "Limit",
                "short_desc": "Maximum number of results",
                "desc": "Maximum number of results to return. Defaults to `5`, maximum `100`",
                "type": SoftIntOrNothingType,
                "attr": {"preselected": True},
            },
            "tbs": {
                "display_name": "Time Filter",
                "short_desc": "Time-based search filter",
                "desc": "Time-based search filter string (e.g., `qdr:d` for past day, "
                    "`qdr:w` for past week, `qdr:m` for past month)",
                "type": StringOrNothingType,
                "example_value": "qdr:w",
            },
            "location": {
                "display_name": "Location",
                "short_desc": "Search location",
                "desc": "Geographic location for the search query (e.g., `United States`)",
                "type": StringOrNothingType,
                "example_value": "United States",
            },
            "country": {
                "display_name": "Country",
                "short_desc": "Country code",
                "desc": "Country code for the search (e.g., `us`, `gb`, `de`)",
                "type": StringOrNothingType,
            },
            "timeout": {
                "display_name": "Timeout",
                "short_desc": "Timeout in milliseconds",
                "desc": "Maximum time in milliseconds to wait for the search to complete",
                "type": SoftIntOrNothingType,
            },
            "ignoreInvalidURLs": {
                "display_name": "Ignore Invalid URLs",
                "short_desc": "Ignore invalid URLs",
                "desc": "If `True`, invalid URLs in results are skipped instead of causing an error",
                "type": SoftBoolOrNothingType,
            },
            "sources": {
                "display_name": "Sources",
                "short_desc": "Filter results by source type",
                "desc": "List of source types to include in search results. "
                    "Defaults to `web` only if not specified",
                "type": SoftListOrNothingType,
                "element_allowed_values": (
                    <AllowedValueInfo>{"value": "web", "display_name": "Web"},
                    <AllowedValueInfo>{"value": "images", "display_name": "Images"},
                    <AllowedValueInfo>{"value": "news", "display_name": "News"},
                ),
            },
            "categories": {
                "display_name": "Categories",
                "short_desc": "Filter results by content category",
                "desc": "List of content categories to filter search results",
                "type": SoftListOrNothingType,
                "element_allowed_values": (
                    <AllowedValueInfo>{"value": "github", "display_name": "GitHub"},
                    <AllowedValueInfo>{"value": "research", "display_name": "Research"},
                    <AllowedValueInfo>{"value": "pdf", "display_name": "PDF"},
                ),
            },
            "scrapeOptions": {
                "display_name": "Scrape Options",
                "short_desc": "Options for scraping search results",
                "desc": "Options applied when scraping each search result page, such as "
                    "`formats`, `onlyMainContent`, `includeTags`, `excludeTags`",
                "type": new FirecrawlScrapeOptionsDataType(AutoHashOrNothingType),
            },
            "enterprise": {
                "display_name": "Enterprise",
                "short_desc": "Enterprise search options",
                "desc": "Enterprise search options for Zero Data Retention (ZDR). Must "
                    "be enabled for your team",
                "type": SoftListOrNothingType,
                "element_allowed_values": (
                    <AllowedValueInfo>{"value": "zdr",
                        "display_name": "End-to-End Zero Data Retention"},
                    <AllowedValueInfo>{"value": "anon",
                        "display_name": "Anonymized Zero Data Retention"},
                ),
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace FirecrawlDataProvider
