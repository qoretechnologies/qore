# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FirecrawlScrapeOptionsDataType.qc scrape options sub-type for Firecrawl requests

/*  FirecrawlScrapeOptionsDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Scrape options sub-type used within crawl, search, and extract requests
public class FirecrawlScrapeOptionsDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "formats": {
                "display_name": "Formats",
                "short_desc": "Output formats to return",
                "desc": "List of output formats to return from each scraped page",
                "type": SoftListOrNothingType,
                "element_allowed_values": (
                    <AllowedValueInfo>{"value": "markdown", "display_name": "Markdown"},
                    <AllowedValueInfo>{"value": "html", "display_name": "HTML"},
                    <AllowedValueInfo>{"value": "rawHtml", "display_name": "Raw HTML"},
                    <AllowedValueInfo>{"value": "links", "display_name": "Links"},
                    <AllowedValueInfo>{"value": "images", "display_name": "Images"},
                    <AllowedValueInfo>{"value": "screenshot", "display_name": "Screenshot"},
                    <AllowedValueInfo>{"value": "json", "display_name": "JSON"},
                    <AllowedValueInfo>{"value": "summary", "display_name": "Summary"},
                    <AllowedValueInfo>{"value": "audio", "display_name": "Audio"},
                    <AllowedValueInfo>{"value": "changeTracking",
                        "display_name": "Change Tracking"},
                    <AllowedValueInfo>{"value": "branding", "display_name": "Branding"},
                ),
            },
            "onlyMainContent": {
                "display_name": "Only Main Content",
                "short_desc": "Extract only the main content",
                "desc": "If `True`, only the main content of each page is returned, "
                    "excluding headers, navigation, footers, etc. Defaults to `True`",
                "type": SoftBoolOrNothingType,
            },
            "onlyCleanContent": {
                "display_name": "Only Clean Content",
                "short_desc": "Apply LLM-based content cleanup",
                "desc": "If `True`, an LLM-based cleanup pass is applied to each page to "
                    "remove navigation, ads, and boilerplate. Defaults to `False`",
                "type": SoftBoolOrNothingType,
            },
            "includeTags": {
                "display_name": "Include Tags",
                "short_desc": "HTML tags to include",
                "desc": "List of HTML tags to include in the output (e.g., `article`, `main`)",
                "type": SoftListOrNothingType,
            },
            "excludeTags": {
                "display_name": "Exclude Tags",
                "short_desc": "HTML tags to exclude",
                "desc": "List of HTML tags to exclude from the output (e.g., `nav`, `footer`)",
                "type": SoftListOrNothingType,
            },
            "headers": {
                "display_name": "Headers",
                "short_desc": "Custom HTTP headers",
                "desc": "Custom HTTP headers to send with each scrape request",
                "type": AutoHashOrNothingType,
            },
            "waitFor": {
                "display_name": "Wait For",
                "short_desc": "Wait time in milliseconds",
                "desc": "Time to wait in milliseconds for each page to load before scraping",
                "type": SoftIntOrNothingType,
            },
            "mobile": {
                "display_name": "Mobile",
                "short_desc": "Use mobile user agent",
                "desc": "If `True`, pages are scraped using a mobile user agent",
                "type": SoftBoolOrNothingType,
            },
            "timeout": {
                "display_name": "Timeout",
                "short_desc": "Timeout in milliseconds",
                "desc": "Maximum time in milliseconds to wait for each scrape to complete. "
                    "Range: `1000`-`300000`",
                "type": SoftIntOrNothingType,
            },
            "removeBase64Images": {
                "display_name": "Remove Base64 Images",
                "short_desc": "Remove base64-encoded images",
                "desc": "If `True`, base64-encoded images are removed from the output",
                "type": SoftBoolOrNothingType,
            },
            "skipTlsVerification": {
                "display_name": "Skip TLS Verification",
                "short_desc": "Skip TLS certificate verification",
                "desc": "If `True`, TLS certificate verification is skipped",
                "type": SoftBoolOrNothingType,
            },
            "blockAds": {
                "display_name": "Block Ads",
                "short_desc": "Block advertisements",
                "desc": "If `True`, advertisements are blocked during scraping",
                "type": SoftBoolOrNothingType,
            },
            "proxy": {
                "display_name": "Proxy",
                "short_desc": "Proxy configuration",
                "desc": "The proxy configuration to use for scrape requests",
                "type": StringOrNothingType,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "basic", "display_name": "Basic"},
                    <AllowedValueInfo>{"value": "enhanced", "display_name": "Enhanced"},
                    <AllowedValueInfo>{"value": "auto", "display_name": "Auto"},
                ),
            },
            "maxAge": {
                "display_name": "Max Age",
                "short_desc": "Cache freshness threshold in milliseconds",
                "desc": "Maximum age in milliseconds for cached content. Defaults to "
                    "`172800000` (48 hours)",
                "type": SoftIntOrNothingType,
            },
            "minAge": {
                "display_name": "Min Age",
                "short_desc": "Minimum cache age in milliseconds",
                "desc": "Minimum age in milliseconds for a cache-only lookup",
                "type": SoftIntOrNothingType,
            },
            "parsers": {
                "display_name": "Parsers",
                "short_desc": "PDF parser configuration",
                "desc": "List of parser configurations for PDF processing. Each entry "
                    "specifies a parser type and processing options",
                "type": new ListDataType("parsers",
                    new FirecrawlParsersDataType(AutoHashOrNothingType), True),
            },
            "storeInCache": {
                "display_name": "Store In Cache",
                "short_desc": "Cache the scraped content",
                "desc": "If `True`, the scraped content is stored in the Firecrawl cache so "
                    "that subsequent scrapes within `maxAge` can reuse it. Defaults to `True`",
                "type": SoftBoolOrNothingType,
            },
            "zeroDataRetention": {
                "display_name": "Zero Data Retention",
                "short_desc": "Disable Firecrawl-side data retention",
                "desc": "If `True`, Firecrawl does not retain any scraped data on its "
                    "servers after returning the response. Enterprise feature — contact "
                    "Firecrawl support to enable",
                "type": SoftBoolOrNothingType,
            },
            "profile": {
                "display_name": "Profile",
                "short_desc": "Persistent browser profile",
                "desc": "Named browser profile to reuse session state (cookies, local "
                    "storage) across scrape requests",
                "type": new FirecrawlProfileDataType(AutoHashOrNothingType),
            },
        };
    }

    #! Creates the type
    constructor(*Type base_type) : HashDataType(base_type ?? AutoHashType) {
        addQoreFields(Fields);
    }
}

} # namespace FirecrawlDataProvider
