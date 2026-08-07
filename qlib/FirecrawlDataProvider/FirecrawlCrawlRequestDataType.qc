# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FirecrawlCrawlRequestDataType.qc request type for the Firecrawl crawl action

/*  FirecrawlCrawlRequestDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request type for the Firecrawl crawl action
public class FirecrawlCrawlRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "url": {
                "display_name": "URL",
                "short_desc": "The starting URL to crawl",
                "desc": "The starting URL of the website to crawl",
                "type": UriType,
                "required": True,
                "example_value": "https://example.com",
            },
            "prompt": {
                "display_name": "Prompt",
                "short_desc": "AI prompt for filtering results",
                "desc": "An optional AI prompt for filtering or processing crawl results",
                "type": StringOrNothingType,
            },
            "excludePaths": {
                "display_name": "Exclude Paths",
                "short_desc": "URL pathname regex patterns to exclude",
                "desc": "List of URL pathname regex patterns; matching URLs are excluded "
                    "from the crawl (e.g., `^/admin/.*`, `\\.pdf$`)",
                "type": SoftListOrNothingType,
                "example_value": ("^/admin/.*", "\\.pdf$"),
            },
            "includePaths": {
                "display_name": "Include Paths",
                "short_desc": "URL pathname regex patterns to include",
                "desc": "List of URL pathname regex patterns; only matching URLs are "
                    "crawled (e.g., `^/blog/.*`, `^/docs/.*`)",
                "type": SoftListOrNothingType,
                "example_value": ("^/blog/.*",),
            },
            "maxDiscoveryDepth": {
                "display_name": "Max Discovery Depth",
                "short_desc": "Maximum link depth to follow",
                "desc": "Maximum depth of links to follow from the starting URL",
                "type": SoftIntOrNothingType,
            },
            "limit": {
                "display_name": "Limit",
                "short_desc": "Maximum number of pages to crawl",
                "desc": "Maximum number of pages to crawl. Defaults to `10000`",
                "type": SoftIntOrNothingType,
            },
            "sitemap": {
                "display_name": "Sitemap",
                "short_desc": "Sitemap handling mode",
                "desc": "How to handle the sitemap when crawling",
                "type": StringOrNothingType,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "skip", "display_name": "Skip"},
                    <AllowedValueInfo>{"value": "include", "display_name": "Include"},
                    <AllowedValueInfo>{"value": "only", "display_name": "Only"},
                ),
            },
            "ignoreQueryParameters": {
                "display_name": "Ignore Query Parameters",
                "short_desc": "Ignore URL query parameters",
                "desc": "If `True`, URLs with different query parameters are treated as the same page",
                "type": SoftBoolOrNothingType,
            },
            "deduplicateSimilarURLs": {
                "display_name": "Deduplicate Similar URLs",
                "short_desc": "Treat URL variants as duplicates",
                "desc": "If `True`, URL variants (`www.`/non-`www.`, `http`/`https`, trailing "
                    "slashes, `index.html`) are normalized and treated as duplicates. "
                    "Defaults to `True`",
                "type": SoftBoolOrNothingType,
            },
            "crawlEntireDomain": {
                "display_name": "Crawl Entire Domain",
                "short_desc": "Crawl the entire domain",
                "desc": "If `True`, the entire domain is crawled, including all subdomains",
                "type": SoftBoolOrNothingType,
            },
            "allowExternalLinks": {
                "display_name": "Allow External Links",
                "short_desc": "Follow external links",
                "desc": "If `True`, links to external domains are followed",
                "type": SoftBoolOrNothingType,
            },
            "allowSubdomains": {
                "display_name": "Allow Subdomains",
                "short_desc": "Allow subdomain crawling",
                "desc": "If `True`, subdomains of the starting URL are included in crawling",
                "type": SoftBoolOrNothingType,
            },
            "delay": {
                "display_name": "Delay",
                "short_desc": "Delay between requests in seconds",
                "desc": "Delay in seconds between each request during crawling",
                "type": SoftIntOrNothingType,
            },
            "maxConcurrency": {
                "display_name": "Max Concurrency",
                "short_desc": "Maximum concurrent requests",
                "desc": "Maximum number of concurrent requests during crawling",
                "type": SoftIntOrNothingType,
            },
            "webhook": {
                "display_name": "Webhook",
                "short_desc": "Webhook configuration for notifications",
                "desc": "Webhook configuration to receive notifications during the crawl",
                "type": new FirecrawlWebhookDataType(AutoHashOrNothingType),
            },
            "regexOnFullURL": {
                "display_name": "Regex on Full URL",
                "short_desc": "Match patterns against full URL",
                "desc": "If `True`, `includePaths` and `excludePaths` patterns are matched "
                    "against the full URL including query parameters. If `False` (default), "
                    "patterns are matched against the pathname only",
                "type": SoftBoolOrNothingType,
            },
            "scrapeOptions": {
                "display_name": "Scrape Options",
                "short_desc": "Options for scraping each page",
                "desc": "Options applied when scraping each crawled page, such as `formats`, "
                    "`onlyMainContent`, `includeTags`, `excludeTags`",
                "type": new FirecrawlScrapeOptionsDataType(AutoHashOrNothingType),
            },
            "ignoreRobotsTxt": {
                "display_name": "Ignore robots.txt",
                "short_desc": "Bypass robots.txt directives",
                "desc": "If `True`, the crawl ignores `robots.txt` directives. Enterprise "
                    "feature — contact Firecrawl support to enable",
                "type": SoftBoolOrNothingType,
            },
            "robotsUserAgent": {
                "display_name": "robots.txt User Agent",
                "short_desc": "User-agent for robots.txt evaluation",
                "desc": "User-agent string used when evaluating `robots.txt` rules. "
                    "Enterprise feature — contact Firecrawl support to enable",
                "type": StringOrNothingType,
            },
            "zeroDataRetention": {
                "display_name": "Zero Data Retention",
                "short_desc": "Disable Firecrawl-side data retention",
                "desc": "If `True`, Firecrawl does not retain any crawl data on its "
                    "servers after returning the response. Enterprise feature — contact "
                    "Firecrawl support to enable",
                "type": SoftBoolOrNothingType,
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace FirecrawlDataProvider
