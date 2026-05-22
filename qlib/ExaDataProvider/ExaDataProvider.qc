# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ExaDataProvider class definitions

/** ExaDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace ExaDataProvider {
#! Common base for Exa data providers
public class ExaDataProviderCommon inherits DataProvider::AbstractDataProvider {
    public {
        const DefaultExaUrl = "https://api.exa.ai";

        const ConstructorOptions = {
            "apikey": <DataProviderOptionInfo>{
                "display_name": "Exa API Key",
                "short_desc": "Exa API key (x-api-key header)",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "The Exa API key — sent as the `x-api-key` header.",
                "required": True,
                "sensitive": True,
            },
            "url": <DataProviderOptionInfo>{
                "display_name": "Server URL",
                "short_desc": "Exa server URL",
                "type": AbstractDataProviderTypeMap."string",
                "desc": "URL of the Exa REST API.",
                "default_value": DefaultExaUrl,
            },
            "restclient": <DataProviderOptionInfo>{
                "display_name": "REST Client",
                "short_desc": "The REST client object to use",
                "type": AbstractDataProviderType::get(
                    new Type("RestClientIo::RestClientIo"), NOTHING, {
                        DTT_ClientOnly: True,
                    }),
                "desc": "The REST client object to use",
            },
        };
    }

    private {
        *RestClientIo::RestClientIo rest;
    }

    constructor(RestClientIo::RestClientIo rest) {
        self.rest = rest;
    }

    constructor(*hash<auto> options) {
        *hash<auto> copts = checkOptions("CONSTRUCTOR-ERROR", ConstructorOptions, options);
        if (copts.restclient) {
            rest = copts.restclient;
        } else {
            if (!copts.apikey.val()) {
                throw "CONSTRUCTOR-ERROR",
                    "missing required 'apikey' argument for Exa API communication";
            }
            rest = new ExaRestClient::ExaRestClientIo({
                "url": copts.url,
                "apikey": copts.apikey,
            });
        }
    }
}

public class ExaDataProvider inherits ExaDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "exa",
            "type": "ExaDataProvider",
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "children_can_support_apis": True,
        };
    }

    private {
        static hash<string, Reflection::Class> childMap = {
            "search":
                Class::forName("ExaDataProvider::ExaSearchDataProvider"),
            "find-similar":
                Class::forName("ExaDataProvider::ExaFindSimilarDataProvider"),
            "contents":
                Class::forName("ExaDataProvider::ExaContentsDataProvider"),
        };
    }

    constructor(RestClientIo::RestClientIo rest) : ExaDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : ExaDataProviderCommon(options) {}

    string getName() { return ProviderInfo.name; }

    *list<hash<DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map $1.getConstant("ProviderSummaryInfo").getValue(), childMap.iterator();
    }
    private *list<string> getChildProviderNamesImpl() { return keys childMap; }
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = childMap{name};
        return cls ? cls.newObject(rest) : NOTHING;
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

public class ExaDataProviderFactory inherits AbstractDataProviderFactory {
    private {
        static Class cls = new Class("ExaDataProvider");
        const FactoryInfo = <DataProviderFactoryInfo>{
            "name": "exa",
            "desc": "Exa neural-search data provider factory",
            "children_can_support_apis": True,
        };
    }
    private hash<DataProviderFactoryInfo> getInfoImpl() { return FactoryInfo; }
    private hash<DataProviderInfo> getProviderInfoImpl() { return ExaDataProvider::ProviderInfo; }
    private Class getClassImpl() { return cls; }
}

public class ExaSearchRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "query": {
                "display_name": "Query",
                "short_desc": "Search query (natural language)",
                "desc": "The search query in natural language.",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "papers about Mamba state-space models",
            },
            "type": {
                "display_name": "Search Type",
                "short_desc": "neural / keyword / auto",
                "desc": "Search type.  `neural` uses embeddings; `keyword` is "
                    "traditional; `auto` routes between them.",
                "type": AbstractDataProviderTypeMap."*softstring",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "neural", "display_name": "Neural"},
                    <AllowedValueInfo>{"value": "keyword", "display_name": "Keyword"},
                    <AllowedValueInfo>{"value": "auto", "display_name": "Auto"},
                ),
            },
            "category": {
                "display_name": "Category",
                "short_desc": "Result category filter (company, paper, etc.)",
                "desc": "Filter results by entity category.",
                "type": AbstractDataProviderTypeMap."*softstring",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "company", "display_name": "Company"},
                    <AllowedValueInfo>{"value": "research paper",
                        "display_name": "Research Paper"},
                    <AllowedValueInfo>{"value": "news", "display_name": "News"},
                    <AllowedValueInfo>{"value": "github", "display_name": "GitHub"},
                    <AllowedValueInfo>{"value": "tweet", "display_name": "Tweet"},
                    <AllowedValueInfo>{"value": "personal site",
                        "display_name": "Personal Site"},
                    <AllowedValueInfo>{"value": "pdf", "display_name": "PDF"},
                ),
            },
            "num_results": {
                "display_name": "Number of Results",
                "short_desc": "Number of results to return",
                "desc": "Number of results.  Default 10.",
                "type": AbstractDataProviderTypeMap."*softint",
                "example_value": 10,
            },
            "include_domains": {
                "display_name": "Include Domains",
                "short_desc": "Restrict results to these domains",
                "desc": "List of domains to restrict results to.",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "exclude_domains": {
                "display_name": "Exclude Domains",
                "short_desc": "Exclude these domains",
                "desc": "List of domains to exclude from results.",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "start_published_date": {
                "display_name": "Start Published Date",
                "short_desc": "Only pages published on/after this ISO date",
                "desc": "Restrict results to pages published on or after this ISO date.",
                "type": AbstractDataProviderTypeMap."*softstring",
            },
            "end_published_date": {
                "display_name": "End Published Date",
                "short_desc": "Only pages published on/before this ISO date",
                "desc": "Restrict results to pages published on or before this ISO date.",
                "type": AbstractDataProviderTypeMap."*softstring",
            },
            "contents": {
                "display_name": "Contents Config",
                "short_desc": "Include text/highlights/summary in results",
                "desc": "Hash configuring content fetching alongside the search: "
                    "`{text: True, highlights: {...}, summary: {...}}`.  Saves a "
                    "follow-up call to `/contents`.",
                "type": AbstractDataProviderTypeMap."*hash",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class ExaFindSimilarRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "url": {
                "display_name": "Seed URL",
                "short_desc": "URL to find similar pages for",
                "desc": "Seed URL.",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "num_results": {
                "display_name": "Number of Results",
                "short_desc": "Number of similar results",
                "desc": "Number of similar pages to return.  Default 10.",
                "type": AbstractDataProviderTypeMap."*softint",
            },
            "exclude_source_domain": {
                "display_name": "Exclude Source Domain",
                "short_desc": "Skip pages from the seed URL's own domain",
                "desc": "When `True`, results don't include the seed URL's own domain.",
                "type": AbstractDataProviderTypeMap."*softbool",
            },
            "include_domains": {
                "display_name": "Include Domains",
                "short_desc": "Restrict results to these domains",
                "desc": "List of domains to restrict results to.",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "exclude_domains": {
                "display_name": "Exclude Domains",
                "short_desc": "Exclude these domains",
                "desc": "List of domains to exclude.",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "contents": {
                "display_name": "Contents Config",
                "short_desc": "Include text/highlights/summary in results",
                "desc": "Same as on `/search`; saves a follow-up call to `/contents`.",
                "type": AbstractDataProviderTypeMap."*hash",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class ExaContentsRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "ids": {
                "display_name": "IDs",
                "short_desc": "List of Exa result IDs (or URLs)",
                "desc": "Result IDs from a prior search (or absolute URLs).",
                "type": AbstractDataProviderTypeMap."softlist",
                "required": True,
            },
            "text": {
                "display_name": "Text",
                "short_desc": "Include full text content",
                "desc": "When `True`, results include the full text content of each "
                    "page.",
                "type": AbstractDataProviderTypeMap."*softbool",
            },
            "highlights": {
                "display_name": "Highlights",
                "short_desc": "Highlight configuration hash",
                "desc": "Highlight options: `{num_sentences, highlights_per_url, "
                    "query}`.",
                "type": AbstractDataProviderTypeMap."*hash",
            },
            "summary": {
                "display_name": "Summary",
                "short_desc": "Summary configuration hash",
                "desc": "Summary options: `{query}` to drive a guided summary.",
                "type": AbstractDataProviderTypeMap."*hash",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class ExaSearchResultDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {"display_name": "ID", "desc": "Exa result ID.",
                "type": AbstractDataProviderTypeMap."*string"},
            "url": {"display_name": "URL", "desc": "Result URL.",
                "type": AbstractDataProviderTypeMap."*string"},
            "title": {"display_name": "Title", "desc": "Page title.",
                "type": AbstractDataProviderTypeMap."*string"},
            "score": {"display_name": "Score",
                "desc": "Relevance / similarity score.",
                "type": AbstractDataProviderTypeMap."*float"},
            "published_date": {"display_name": "Published Date",
                "desc": "ISO publication date when available.",
                "type": AbstractDataProviderTypeMap."*string"},
            "author": {"display_name": "Author", "desc": "Author when available.",
                "type": AbstractDataProviderTypeMap."*string"},
            "text": {"display_name": "Text",
                "desc": "Full text content (when contents config requested it).",
                "type": AbstractDataProviderTypeMap."*string"},
            "highlights": {"display_name": "Highlights",
                "desc": "Highlight sentences.",
                "type": AbstractDataProviderTypeMap."*list"},
            "summary": {"display_name": "Summary",
                "desc": "AI-summarized snippet.",
                "type": AbstractDataProviderTypeMap."*string"},
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class ExaSearchResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "results": {"display_name": "Results",
                "desc": "Ranked search results.",
                "type": new ListDataType("ExaSearchResultList",
                    new ExaSearchResultDataType(), True)},
            "autopromptString": {"display_name": "Autoprompt String",
                "desc": "Exa-rewritten query when autoprompt was used.",
                "type": AbstractDataProviderTypeMap."*string"},
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class ExaContentsResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "results": {"display_name": "Results",
                "desc": "Content results, one per requested ID.",
                "type": new ListDataType("ExaSearchResultList2",
                    new ExaSearchResultDataType(), True)},
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class ExaSearchDataProvider inherits ExaDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "search",
            "type": "ExaSearchDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        static ExaSearchRequestDataType RequestType = new ExaSearchRequestDataType();
        static ExaSearchResponseDataType ResponseType = new ExaSearchResponseDataType();
    }

    constructor(RestClientIo::RestClientIo rest) : ExaDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : ExaDataProviderCommon(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return rest.restPost("/search", req).body;
    }
}

public class ExaFindSimilarDataProvider inherits ExaDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "find-similar",
            "type": "ExaFindSimilarDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        static ExaFindSimilarRequestDataType RequestType =
            new ExaFindSimilarRequestDataType();
        static ExaSearchResponseDataType ResponseType = new ExaSearchResponseDataType();
    }

    constructor(RestClientIo::RestClientIo rest) : ExaDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : ExaDataProviderCommon(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return rest.restPost("/findSimilar", req).body;
    }
}

public class ExaContentsDataProvider inherits ExaDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "contents",
            "type": "ExaContentsDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        static ExaContentsRequestDataType RequestType = new ExaContentsRequestDataType();
        static ExaContentsResponseDataType ResponseType = new ExaContentsResponseDataType();
    }

    constructor(RestClientIo::RestClientIo rest) : ExaDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : ExaDataProviderCommon(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return rest.restPost("/contents", req).body;
    }
}
}
