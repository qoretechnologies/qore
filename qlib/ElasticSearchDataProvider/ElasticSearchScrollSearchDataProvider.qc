# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ElasticSearchDataProvider module definition

/** ElasticSearchScrollSearchDataProvider.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ElasticSearchDataProvider module
public namespace ElasticSearchDataProvider {
#! The ElasticSearch scroll search API data provider
/** This class implements the Scroll API for efficient iteration over large result sets.

    The Scroll API provides a point-in-time snapshot of the index and is designed for
    processing large numbers of results. It should not be used for real-time user requests.

    Usage:
    1. Make an initial request with `scroll` parameter (e.g., "1m" for 1 minute)
    2. Use the returned `_scroll_id` for subsequent requests
    3. When done, optionally clear the scroll context with `clear: true`

    @see https://www.elastic.co/guide/en/elasticsearch/reference/current/scroll-api.html
*/
public class ElasticSearchScrollSearchDataProvider inherits ElasticSearchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "scroll-search",
            "desc": "ElasticSearch scroll search API data provider for efficient iteration over large result sets",
            "type": "ElasticSearchScrollSearchDataProvider",
            "constructor_options": ElasticSearchDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new ElasticSearchScrollSearchRequestDataType();

        #! Response type
        const ResponseType = new ElasticSearchScrollSearchResponseDataType();

        #! Query args for initial search
        const InitialQueryArgs = ("scroll", "size", "routing", "preference", "request_cache");
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        checkOptions("CONSTRUCTOR-ERROR", ElasticSearchDataProvider::ConstructorOptions, options);
        rest = ElasticSearchDataProvider::getRestConnection(options);
    }

    #! Creates the object from a REST connection
    constructor(RestClient::RestClient rest) : ElasticSearchDataProviderBase(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("ElasticSearch scroll search API provider for `%s`", rest.getSafeURL());
    }

    #! Makes a request and returns the response
    /** @param req the request to serialize and make according to the request type
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        # Handle clear scroll request
        if (req.clear && req.scroll_id) {
            return clearScroll(req.scroll_id);
        }

        # Handle continue scroll request (using existing scroll_id)
        if (req.scroll_id) {
            return continueScroll(req.scroll_id, req.scroll ?? "1m");
        }

        # Handle initial scroll request
        return initialScroll(req);
    }

    #! Makes the initial scroll request
    private hash<auto> initialScroll(hash<auto> req) {
        string index = remove req.index;
        string scroll = remove req.scroll ?? "1m";
        int size = remove req.size ?? 1000;

        string uri = sprintf("%s/_search?scroll=%s&size=%d", index, scroll, size);

        # Add other query args
        foreach string arg in (InitialQueryArgs) {
            if (arg == "scroll" || arg == "size") {
                continue;  # Already handled
            }
            if (exists req{arg}) {
                uri += sprintf("&%s=%s", arg, remove req{arg});
            }
        }

        # Remove non-body fields
        remove req.scroll_id;
        remove req.clear;

        hash<auto> response = rest.post(uri, req.size() ? req : NOTHING).body;
        return {
            "_scroll_id": response._scroll_id,
            "took": response.took,
            "timed_out": response.timed_out,
            "_shards": response._shards,
            "hits": response.hits,
        };
    }

    #! Continues scrolling with an existing scroll_id
    private hash<auto> continueScroll(string scroll_id, string scroll) {
        hash<auto> response = rest.post("_search/scroll", {
            "scroll": scroll,
            "scroll_id": scroll_id,
        }).body;
        return {
            "_scroll_id": response._scroll_id,
            "took": response.took,
            "timed_out": response.timed_out,
            "_shards": response._shards,
            "hits": response.hits,
        };
    }

    #! Clears a scroll context
    private hash<auto> clearScroll(string scroll_id) {
        try {
            rest.del("_search/scroll", {"scroll_id": scroll_id});
            return {"succeeded": True};
        } catch (hash<ExceptionInfo> ex) {
            return {"succeeded": False, "error": ex.desc};
        }
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
    hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! ElasticSearch scroll search request data type
public class ElasticSearchScrollSearchRequestDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "index": {
                "type": StringOrNothingType,
                "desc": "The name of the index to search (required for initial scroll, not needed for continue/clear)",
            },
            "scroll": {
                "type": StringOrNothingType,
                "desc": "How long to keep the scroll context alive (e.g., '1m' for 1 minute, '5m' for 5 minutes). "
                    "Default: '1m'",
                "default_value": "1m",
            },
            "scroll_id": {
                "type": StringOrNothingType,
                "desc": "The scroll ID from a previous scroll response. If provided, continues the scroll. "
                    "If not provided, starts a new scroll.",
            },
            "size": {
                "type": IntOrNothingType,
                "desc": "The number of hits to return per scroll page. Default: 1000",
                "default_value": 1000,
            },
            "clear": {
                "type": SoftBoolStringType,
                "desc": "If true, clears the scroll context (requires scroll_id). Use this when done scrolling "
                    "to free up resources.",
            },
            "query": {
                "type": AutoHashOrNothingType,
                "desc": "The query to execute using ElasticSearch Query DSL (for initial scroll only)",
            },
            "routing": {
                "type": StringOrNothingType,
                "desc": "Custom routing value (for initial scroll only)",
            },
            "preference": {
                "type": StringOrNothingType,
                "desc": "Specifies which shard replicas to execute the search request on (for initial scroll only)",
            },
            "request_cache": {
                "type": SoftBoolStringType,
                "desc": "Whether to cache the request (for initial scroll only)",
            },
        };
    }

    #! Creates the object
    constructor() {
        map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type, $1.value.default_value)),
            Fields.pairIterator();
    }
}

#! Scroll search shards response data type
public class ElasticSearchScrollSearchShardsDataType inherits DataProvider::HashDataType {
    private {
        const Fields = {
            "total": {
                "type": IntType,
                "desc": "Total number of shards",
            },
            "successful": {
                "type": IntType,
                "desc": "Number of successful shards",
            },
            "skipped": {
                "type": IntOrNothingType,
                "desc": "Number of skipped shards",
            },
            "failed": {
                "type": IntType,
                "desc": "Number of failed shards",
            },
        };
    }

    constructor() {
        map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type, $1.value.default_value)),
            Fields.pairIterator();
    }
}

#! ElasticSearch scroll search response data type
public class ElasticSearchScrollSearchResponseDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "_scroll_id": {
                "type": StringOrNothingType,
                "desc": "The scroll ID for subsequent requests. This may change between requests.",
            },
            "took": {
                "type": IntOrNothingType,
                "desc": "How long the request took in milliseconds",
            },
            "timed_out": {
                "type": BoolOrNothingType,
                "desc": "Whether the request timed out",
            },
            "_shards": {
                "type": new ElasticSearchScrollSearchShardsDataType(),
                "desc": "Shard information",
            },
            "hits": {
                "type": AutoHashOrNothingType,
                "desc": "The search hits containing total count and the actual hit documents",
            },
            "succeeded": {
                "type": BoolOrNothingType,
                "desc": "For clear operations: whether the scroll context was successfully cleared",
            },
            "error": {
                "type": StringOrNothingType,
                "desc": "For clear operations: error message if clearing failed",
            },
        };
    }

    #! Creates the object
    constructor() {
        map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type, $1.value.default_value)),
            Fields.pairIterator();
    }
}
}
