# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FirecrawlBatchScrapeRequestDataType.qc request type for the Firecrawl batch-scrape action

/*  FirecrawlBatchScrapeRequestDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request type for the Firecrawl batch-scrape action
public class FirecrawlBatchScrapeRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "urls": {
                "display_name": "URLs",
                "short_desc": "List of URLs to scrape",
                "desc": "List of URLs to scrape in batch",
                "type": SoftListType,
                "required": True,
                "example_value": ("https://example.com", "https://example.org"),
            },
            "formats": {
                "display_name": "Formats",
                "short_desc": "Output formats to return",
                "desc": "List of output formats to return from each scrape",
                "type": SoftListOrNothingType,
                "attr": {"preselected": True},
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
                "desc": "If `True`, only the main content of each page is returned",
                "type": SoftBoolOrNothingType,
            },
            "includeTags": {
                "display_name": "Include Tags",
                "short_desc": "HTML tags to include",
                "desc": "List of HTML tags to include in the output (e.g., `article`, `main`). "
                    "Only content within these tags is returned",
                "type": SoftListOrNothingType,
            },
            "excludeTags": {
                "display_name": "Exclude Tags",
                "short_desc": "HTML tags to exclude",
                "desc": "List of HTML tags to exclude from the output (e.g., `nav`, `footer`)",
                "type": SoftListOrNothingType,
            },
            "waitFor": {
                "display_name": "Wait For",
                "short_desc": "Wait time in milliseconds",
                "desc": "Time to wait in milliseconds for each page to load before scraping. "
                    "Useful for pages with dynamic content",
                "type": SoftIntOrNothingType,
            },
            "timeout": {
                "display_name": "Timeout",
                "short_desc": "Timeout in milliseconds",
                "desc": "Maximum time in milliseconds to wait for each scrape to complete. "
                    "Range: `1000`-`300000`",
                "type": SoftIntOrNothingType,
            },
            "mobile": {
                "display_name": "Mobile",
                "short_desc": "Use mobile user agent",
                "desc": "If `True`, pages are scraped using a mobile user agent",
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
            "location": {
                "display_name": "Location",
                "short_desc": "Geolocation settings",
                "desc": "Geolocation settings for the scrape requests",
                "type": new FirecrawlLocationDataType(AutoHashOrNothingType),
            },
            "headers": {
                "display_name": "Headers",
                "short_desc": "Custom HTTP headers",
                "desc": "Custom HTTP headers to send with each scrape request",
                "type": AutoHashOrNothingType,
            },
            "actions": {
                "display_name": "Actions",
                "short_desc": "Browser actions to perform",
                "desc": "List of browser actions to perform before scraping each page. "
                    "Each action is a hash with a `type` key and action-specific "
                    "parameters.\n\n"
                    "**Available actions**:\n"
                    "- `wait` — wait for duration (`milliseconds`) or element (`selector`)\n"
                    "- `click` — click an element (`selector`; set `all` for all matches)\n"
                    "- `write` — type text into the focused element (`text`)\n"
                    "- `press` — press a keyboard key (`key`)\n"
                    "- `scroll` — scroll the page (`direction`: `up`/`down`; optional "
                    "`selector`)\n"
                    "- `screenshot` — take a screenshot (`fullPage`, `quality`)\n"
                    "- `scrape` — capture the page content at its current state\n"
                    "- `executeJavascript` — run JavaScript code (`script`)\n"
                    "- `pdf` — generate a PDF (`format`, `landscape`, `scale`)",
                "type": new ListDataType("actions", new FirecrawlActionDataType(AutoHashOrNothingType), True),
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
            "maxAge": {
                "display_name": "Max Age",
                "short_desc": "Cache freshness threshold in milliseconds",
                "desc": "Maximum age in milliseconds for cached content. If a cached version "
                    "of the page exists and is younger than this value, it is returned "
                    "instead of re-scraping. Defaults to `172800000` (48 hours)",
                "type": SoftIntOrNothingType,
            },
            "minAge": {
                "display_name": "Min Age",
                "short_desc": "Minimum cache age in milliseconds",
                "desc": "Minimum age in milliseconds for a cache-only lookup. If set, only "
                    "cached content older than this value is returned; no fresh scrape is "
                    "triggered if no cache entry exists",
                "type": SoftIntOrNothingType,
            },
            "maxConcurrency": {
                "display_name": "Max Concurrency",
                "short_desc": "Maximum concurrent requests",
                "desc": "Maximum number of concurrent scrape requests",
                "type": SoftIntOrNothingType,
            },
            "ignoreInvalidURLs": {
                "display_name": "Ignore Invalid URLs",
                "short_desc": "Ignore invalid URLs",
                "desc": "If `True`, invalid URLs in the list are skipped instead of causing an error",
                "type": SoftBoolOrNothingType,
            },
            "webhook": {
                "display_name": "Webhook",
                "short_desc": "Webhook configuration for notifications",
                "desc": "Webhook configuration to receive notifications during the batch scrape",
                "type": new FirecrawlWebhookDataType(AutoHashOrNothingType),
            },
            "parsers": {
                "display_name": "Parsers",
                "short_desc": "PDF parser configuration",
                "desc": "List of parser configurations for PDF processing. Each entry "
                    "specifies a parser type and processing options",
                "type": new ListDataType("parsers",
                    new FirecrawlParsersDataType(AutoHashOrNothingType), True),
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace FirecrawlDataProvider
