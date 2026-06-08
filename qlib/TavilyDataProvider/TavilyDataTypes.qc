# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Tavily data type definitions

/** TavilyDataTypes.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace TavilyDataProvider {
public class TavilySearchRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "query": {
                "display_name": "Query",
                "short_desc": "Natural-language search query",
                "desc": "Natural-language search query — what the agent wants to find on "
                    "the web.",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "Qore 3.0 release date",
            },
            "search_depth": {
                "display_name": "Search Depth",
                "short_desc": "Search depth: 'basic' (fast) or 'advanced' (higher recall)",
                "desc": "Search depth.  `basic` returns faster, `advanced` does more "
                    "iterative refinement at higher cost.",
                "type": AbstractDataProviderTypeMap."*softstring",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "basic", "display_name": "Basic"},
                    <AllowedValueInfo>{"value": "advanced", "display_name": "Advanced"},
                ),
            },
            "max_results": {
                "display_name": "Max Results",
                "short_desc": "Maximum number of search results (1-20)",
                "desc": "Maximum number of results to return.  Range 1-20.  Default 5.",
                "type": AbstractDataProviderTypeMap."*softint",
                "example_value": 5,
            },
            "include_answer": {
                "display_name": "Include Answer",
                "short_desc": "AI-summarize the top results into a single answer",
                "desc": "When `True`, Tavily generates an AI-summarized answer over the "
                    "top results and includes it in the response under the `answer` field.",
                "type": AbstractDataProviderTypeMap."*softbool",
            },
            "include_raw_content": {
                "display_name": "Include Raw Content",
                "short_desc": "Include raw scraped HTML in each result",
                "desc": "When `True`, each result includes `raw_content` with the full "
                    "scraped HTML/text.  Larger response, useful for downstream parsing.",
                "type": AbstractDataProviderTypeMap."*softbool",
            },
            "include_images": {
                "display_name": "Include Images",
                "short_desc": "Return image URLs alongside text results",
                "desc": "When `True`, response includes a list of image URLs.",
                "type": AbstractDataProviderTypeMap."*softbool",
            },
            "include_domains": {
                "display_name": "Include Domains",
                "short_desc": "Restrict search to specific domains",
                "desc": "List of domains to restrict the search to (e.g. "
                    "`(\"docs.qore.org\",)`).",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "exclude_domains": {
                "display_name": "Exclude Domains",
                "short_desc": "Exclude specific domains from results",
                "desc": "List of domains to exclude.",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "topic": {
                "display_name": "Topic",
                "short_desc": "Search topic: 'general' or 'news'",
                "desc": "Topic-aware search.  `news` biases toward recent news sources.",
                "type": AbstractDataProviderTypeMap."*softstring",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "general", "display_name": "General"},
                    <AllowedValueInfo>{"value": "news", "display_name": "News"},
                ),
            },
            "days": {
                "display_name": "Days (news topic)",
                "short_desc": "Restrict 'news' searches to the last N days",
                "desc": "For `topic: news`, restrict results to the last N days.",
                "type": AbstractDataProviderTypeMap."*softint",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public class TavilySearchResultDataType inherits HashDataType {
    private {
        const Fields = {
            "title": {"display_name": "Title",
                "desc": "Page title.",
                "type": AbstractDataProviderTypeMap."*string"},
            "url": {"display_name": "URL",
                "desc": "Result URL.",
                "type": AbstractDataProviderTypeMap."*string"},
            "content": {"display_name": "Content",
                "desc": "Tavily-extracted summary content.",
                "type": AbstractDataProviderTypeMap."*string"},
            "raw_content": {"display_name": "Raw Content",
                "desc": "Full scraped content (only when `include_raw_content` was True).",
                "type": AbstractDataProviderTypeMap."*string"},
            "score": {"display_name": "Score",
                "desc": "Relevance score (0..1).",
                "type": AbstractDataProviderTypeMap."*float"},
            "published_date": {"display_name": "Published Date",
                "desc": "When the page was published (news searches only).",
                "type": AbstractDataProviderTypeMap."*string"},
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public class TavilySearchResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "query": {"display_name": "Query",
                "desc": "Echoed query string.",
                "type": AbstractDataProviderTypeMap."*string"},
            "answer": {"display_name": "Answer",
                "desc": "AI-summarized answer (only when `include_answer` was True).",
                "type": AbstractDataProviderTypeMap."*string"},
            "results": {"display_name": "Results",
                "desc": "Ranked search results.",
                "type": new ListDataType("TavilySearchResultList",
                    new TavilySearchResultDataType(), True)},
            "images": {"display_name": "Images",
                "desc": "Image URLs (only when `include_images` was True).",
                "type": AbstractDataProviderTypeMap."*list"},
            "response_time": {"display_name": "Response Time",
                "desc": "Tavily-reported response time in seconds.",
                "type": AbstractDataProviderTypeMap."*float"},
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public class TavilyExtractRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "urls": {
                "display_name": "URLs",
                "short_desc": "List of URLs to extract content from",
                "desc": "Absolute URLs whose main content should be extracted.",
                "type": AbstractDataProviderTypeMap."softlist",
                "required": True,
                "example_value": ("https://docs.qore.org/",),
            },
            "extract_depth": {
                "display_name": "Extract Depth",
                "short_desc": "Extraction depth: 'basic' or 'advanced'",
                "desc": "Extraction depth.",
                "type": AbstractDataProviderTypeMap."*softstring",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "basic", "display_name": "Basic"},
                    <AllowedValueInfo>{"value": "advanced", "display_name": "Advanced"},
                ),
            },
            "include_images": {
                "display_name": "Include Images",
                "short_desc": "Return image URLs alongside extracted text",
                "desc": "When `True`, each result includes image URLs found on the page.",
                "type": AbstractDataProviderTypeMap."*softbool",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public class TavilyExtractResultDataType inherits HashDataType {
    private {
        const Fields = {
            "url": {"display_name": "URL", "desc": "Extracted URL.",
                "type": AbstractDataProviderTypeMap."*string"},
            "raw_content": {"display_name": "Raw Content",
                "desc": "Extracted page content (markdown/text).",
                "type": AbstractDataProviderTypeMap."*string"},
            "images": {"display_name": "Images",
                "desc": "Image URLs from the page.",
                "type": AbstractDataProviderTypeMap."*list"},
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public class TavilyExtractResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "results": {"display_name": "Results",
                "desc": "Per-URL extraction results.",
                "type": new ListDataType("TavilyExtractResultList",
                    new TavilyExtractResultDataType(), True)},
            "failed_results": {"display_name": "Failed Results",
                "desc": "URLs that could not be extracted, with error reasons.",
                "type": AbstractDataProviderTypeMap."*list"},
            "response_time": {"display_name": "Response Time",
                "desc": "Tavily-reported response time in seconds.",
                "type": AbstractDataProviderTypeMap."*float"},
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public class TavilyCrawlRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "url": {
                "display_name": "Seed URL",
                "short_desc": "URL to start the crawl from",
                "desc": "Seed URL the crawl begins at.",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "max_depth": {
                "display_name": "Max Depth",
                "short_desc": "Maximum crawl depth from the seed",
                "desc": "Maximum link-graph depth from the seed URL.",
                "type": AbstractDataProviderTypeMap."*softint",
            },
            "max_breadth": {
                "display_name": "Max Breadth",
                "short_desc": "Maximum links to follow per page",
                "desc": "Maximum number of outgoing links to follow per page.",
                "type": AbstractDataProviderTypeMap."*softint",
            },
            "limit": {
                "display_name": "Limit",
                "short_desc": "Overall page limit",
                "desc": "Maximum total pages to fetch across the crawl.",
                "type": AbstractDataProviderTypeMap."*softint",
            },
            "instructions": {
                "display_name": "Instructions",
                "short_desc": "Natural-language instructions for the crawl agent",
                "desc": "Optional natural-language hints to guide the crawler (e.g. "
                    "\"focus on API reference pages\").",
                "type": AbstractDataProviderTypeMap."*softstring",
            },
            "select_paths": {
                "display_name": "Select Paths",
                "short_desc": "URL path patterns to include",
                "desc": "List of URL path patterns to include.",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "exclude_paths": {
                "display_name": "Exclude Paths",
                "short_desc": "URL path patterns to exclude",
                "desc": "List of URL path patterns to exclude.",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "extract_depth": {
                "display_name": "Extract Depth",
                "short_desc": "Per-page extraction depth",
                "desc": "Extraction depth applied to each crawled page.",
                "type": AbstractDataProviderTypeMap."*softstring",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "basic", "display_name": "Basic"},
                    <AllowedValueInfo>{"value": "advanced", "display_name": "Advanced"},
                ),
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public class TavilyCrawlResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "base_url": {"display_name": "Base URL",
                "desc": "The seed URL of the crawl.",
                "type": AbstractDataProviderTypeMap."*string"},
            "results": {"display_name": "Results",
                "desc": "Per-page crawl results (url + raw_content).",
                "type": AbstractDataProviderTypeMap."*list"},
            "response_time": {"display_name": "Response Time",
                "desc": "Tavily-reported response time in seconds.",
                "type": AbstractDataProviderTypeMap."*float"},
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public class TavilyMapRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "url": {
                "display_name": "Seed URL",
                "short_desc": "URL to start the URL map from",
                "desc": "Seed URL.",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "max_depth": {
                "display_name": "Max Depth",
                "short_desc": "Maximum link-graph depth",
                "desc": "Maximum link-graph depth.",
                "type": AbstractDataProviderTypeMap."*softint",
            },
            "max_breadth": {
                "display_name": "Max Breadth",
                "short_desc": "Maximum links to follow per page",
                "desc": "Maximum links per page.",
                "type": AbstractDataProviderTypeMap."*softint",
            },
            "limit": {
                "display_name": "Limit",
                "short_desc": "Overall URL limit",
                "desc": "Maximum total URLs to enumerate.",
                "type": AbstractDataProviderTypeMap."*softint",
            },
            "instructions": {
                "display_name": "Instructions",
                "short_desc": "Natural-language instructions for the mapper",
                "desc": "Optional natural-language hints.",
                "type": AbstractDataProviderTypeMap."*softstring",
            },
            "select_paths": {
                "display_name": "Select Paths",
                "short_desc": "URL path patterns to include",
                "desc": "List of URL path patterns to include.",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "exclude_paths": {
                "display_name": "Exclude Paths",
                "short_desc": "URL path patterns to exclude",
                "desc": "List of URL path patterns to exclude.",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public class TavilyMapResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "base_url": {"display_name": "Base URL",
                "desc": "The seed URL of the map.",
                "type": AbstractDataProviderTypeMap."*string"},
            "results": {"display_name": "Results",
                "desc": "Discovered URLs (list of strings).",
                "type": AbstractDataProviderTypeMap."*list"},
            "response_time": {"display_name": "Response Time",
                "desc": "Tavily-reported response time in seconds.",
                "type": AbstractDataProviderTypeMap."*float"},
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}
}
