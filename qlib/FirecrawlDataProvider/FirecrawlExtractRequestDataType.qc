# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FirecrawlExtractRequestDataType.qc request type for the Firecrawl extract action

/*  FirecrawlExtractRequestDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request type for the Firecrawl extract action
public class FirecrawlExtractRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "urls": {
                "display_name": "URLs",
                "short_desc": "URLs to extract from",
                "desc": "List of URLs to extract data from (supports glob patterns like "
                    "`https://example.com/blog/*`)",
                "type": SoftListType,
                "required": True,
                "example_value": ("https://example.com/*",),
            },
            "prompt": {
                "display_name": "Prompt",
                "short_desc": "Extraction prompt",
                "desc": "Natural language prompt describing what data to extract from the pages",
                "type": StringOrNothingType,
                "attr": {"preselected": True},
                "example_value": "Extract the company name, pricing plans, and features",
            },
            "schema": {
                "display_name": "Schema",
                "short_desc": "JSON Schema for extraction",
                "desc": "A JSON Schema object defining the structure of the data to extract. "
                    "Used to guide the extraction and validate the output format.\n\n"
                    "**Example**: `{\"type\": \"object\", \"properties\": "
                    "{\"name\": {\"type\": \"string\"}, \"price\": {\"type\": \"number\"}}}`",
                "type": AutoHashOrNothingType,
            },
            "enableWebSearch": {
                "display_name": "Enable Web Search",
                "short_desc": "Enable web search",
                "desc": "If `True`, web search is enabled to supplement extraction with "
                    "additional data from search results",
                "type": SoftBoolOrNothingType,
            },
            "ignoreSitemap": {
                "display_name": "Ignore Sitemap",
                "short_desc": "Ignore the sitemap",
                "desc": "If `True`, the sitemap is ignored when discovering pages to extract from",
                "type": SoftBoolOrNothingType,
            },
            "includeSubdomains": {
                "display_name": "Include Subdomains",
                "short_desc": "Include subdomain URLs",
                "desc": "If `True`, subdomain URLs are included in the extraction",
                "type": SoftBoolOrNothingType,
            },
            "showSources": {
                "display_name": "Show Sources",
                "short_desc": "Include source URLs",
                "desc": "If `True`, the source URLs used for extraction are included in the response",
                "type": SoftBoolOrNothingType,
            },
            "ignoreInvalidURLs": {
                "display_name": "Ignore Invalid URLs",
                "short_desc": "Ignore invalid URLs",
                "desc": "If `True`, invalid URLs in the list are skipped instead of causing an error",
                "type": SoftBoolOrNothingType,
            },
            "scrapeOptions": {
                "display_name": "Scrape Options",
                "short_desc": "Options for scraping pages",
                "desc": "Options applied when scraping each page during extraction, such as "
                    "`formats`, `onlyMainContent`, `includeTags`, `excludeTags`",
                "type": new FirecrawlScrapeOptionsDataType(AutoHashOrNothingType),
            },
        };
    }

    #! Creates the type
    constructor() {
        addQoreFields(Fields);
    }
}

} # namespace FirecrawlDataProvider
