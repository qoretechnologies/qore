# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore PerplexityDataProvider module definition

/** PerplexitySearchDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the PerplexityDataProvider module
public namespace PerplexityDataProvider {
#! The Perplexity search data provider class
public class PerplexitySearchDataProvider inherits PerplexityDataProviderCommon {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "search",
            "type": "PerplexitySearchDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = PerplexitySearchRequestType;

        #! Response type
        const ResponseType = PerplexitySearchResponseType;
    }

    #! Creates the object from the arguments
    constructor(*RestClient rest) : PerplexityDataProviderCommon(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Makes a request and returns the response
    /** @param req the request to serialize and make according to the request type
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        # The Perplexity search API expects query as an array of strings
        if (req.query.typeCode() == NT_STRING) {
            req.query = (req.query,);
        }
        return doRestCommand("POST", "search", req).body;
    }

    #! Returns the description of a successful request message, if any
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the description of a response message, if this object represents a response message
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Search result data type
public class PerplexitySearchResultDataType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "title": {
                "display_name": "Title",
                "short_desc": "The title of the search result",
                "desc": "The title of the search result page",
                "type": AbstractDataProviderTypeMap."string",
            },
            "url": {
                "display_name": "URL",
                "short_desc": "The URL of the search result",
                "desc": "The URL of the search result page",
                "type": AbstractDataProviderTypeMap."string",
            },
            "snippet": {
                "display_name": "Snippet",
                "short_desc": "A text snippet from the search result",
                "desc": "A text snippet extracted from the search result page",
                "type": AbstractDataProviderTypeMap."string",
            },
            "date": {
                "display_name": "Date",
                "short_desc": "The publication date of the result",
                "desc": "The publication date of the search result",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "last_updated": {
                "display_name": "Last Updated",
                "short_desc": "When the result was last updated",
                "desc": "The date when the search result page was last updated",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("PerplexitySearchResult") {
        addQoreFields(Fields);
    }
}

#! Constant for the search result data type
public const PerplexitySearchResultDataType = new PerplexitySearchResultDataType();

#! The search request type
public class PerplexitySearchRequestType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "query": {
                "display_name": "Query",
                "short_desc": "The search query",
                "desc": "The search query string; automatically wrapped in an array for the API",
                "type": AbstractDataProviderTypeMap."string",
                "example_value": "latest news about AI",
            },
            "max_results": {
                "display_name": "Max Results",
                "short_desc": "Maximum number of results to return (1-20)",
                "desc": "Maximum number of search results to return. Range: 1-20, default: 10",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "max_tokens": {
                "display_name": "Max Tokens",
                "short_desc": "Maximum tokens per search response",
                "desc": "Maximum tokens per search response. Default: 10,000, maximum: 1,000,000",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "max_tokens_per_page": {
                "display_name": "Max Tokens Per Page",
                "short_desc": "Maximum tokens extracted per page",
                "desc": "Maximum tokens extracted from each page in results. Default: 4,096",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "country": {
                "display_name": "Country",
                "short_desc": "ISO country code for search localization",
                "desc": "ISO country code to localize search results",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "search_domain_filter": {
                "display_name": "Search Domain Filter",
                "short_desc": "Domain allowlist or denylist for search",
                "desc": "A list of domains to include or exclude from search results. Prefix with `-` to exclude",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "search_language_filter": {
                "display_name": "Search Language Filter",
                "short_desc": "ISO 639-1 language codes to filter results",
                "desc": "A list of ISO 639-1 language codes to filter search results",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "search_recency_filter": {
                "display_name": "Search Recency Filter",
                "short_desc": "Filter search results by recency",
                "desc": "Filter search results to only include sources from a specific time period",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "Last Hour",
                        "short_desc": "Sources from the last hour",
                        "desc": "Only include sources from the last hour",
                        "value": "hour",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Last Day",
                        "short_desc": "Sources from the last day",
                        "desc": "Only include sources from the last day",
                        "value": "day",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Last Week",
                        "short_desc": "Sources from the last week",
                        "desc": "Only include sources from the last week",
                        "value": "week",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Last Month",
                        "short_desc": "Sources from the last month",
                        "desc": "Only include sources from the last month",
                        "value": "month",
                    },
                    <AllowedValueInfo>{
                        "display_name": "Last Year",
                        "short_desc": "Sources from the last year",
                        "desc": "Only include sources from the last year",
                        "value": "year",
                    },
                ),
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("PerplexitySearchRequest") {
        addQoreFields(Fields);
    }
}

#! The search request type constant
public const PerplexitySearchRequestType = new PerplexitySearchRequestType();

#! The search response type
public class PerplexitySearchResponseType inherits HashDataType {
    public {
        #! fields
        const Fields = {
            "id": {
                "display_name": "ID",
                "short_desc": "Unique request ID",
                "desc": "Unique identifier for the search request",
                "type": AbstractDataProviderTypeMap."string",
            },
            "results": {
                "display_name": "Results",
                "short_desc": "List of search results",
                "desc": "List of search results with title, URL, and snippet",
                "type": new ListDataType("SearchResults", PerplexitySearchResultDataType),
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("PerplexitySearchResponse") {
        addQoreFields(Fields);
    }
}

#! The search response type constant
public const PerplexitySearchResponseType = new PerplexitySearchResponseType();
}
