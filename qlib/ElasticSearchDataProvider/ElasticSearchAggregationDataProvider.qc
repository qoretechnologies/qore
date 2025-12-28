# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ElasticSearchDataProvider module definition

/** ElasticSearchAggregationDataProvider.qc Copyright 2025 Qore Technologies, s.r.o.

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
#! The ElasticSearch aggregation API data provider
/** This class implements the Aggregations API for computing analytics over search results.

    ElasticSearch provides a rich set of aggregation types:

    @b Bucket @b Aggregations - partition documents into groups (buckets):
    - @c terms: Group by field values
    - @c histogram: Group numbers into intervals
    - @c date_histogram: Group dates into intervals
    - @c range: Group into custom ranges
    - @c filter: Single bucket matching a query
    - @c filters: Multiple named buckets from queries

    @b Metric @b Aggregations - compute statistics over documents:
    - @c avg: Average value
    - @c sum: Sum of values
    - @c min: Minimum value
    - @c max: Maximum value
    - @c stats: Count, min, max, avg, sum
    - @c extended_stats: Adds variance, std deviation
    - @c cardinality: Approximate distinct count
    - @c percentiles: Percentile values

    Aggregations can be nested to create complex analytics.

    @see https://www.elastic.co/guide/en/elasticsearch/reference/current/search-aggregations.html
*/
public class ElasticSearchAggregationDataProvider inherits ElasticSearchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "aggregate",
            "desc": "ElasticSearch aggregation API data provider for computing analytics over search results",
            "type": "ElasticSearchAggregationDataProvider",
            "constructor_options": ElasticSearchDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new ElasticSearchAggregationRequestDataType();

        #! Response type
        const ResponseType = new ElasticSearchAggregationResponseDataType();

        #! Query args
        const QueryArgs = ("routing", "preference", "timeout", "request_cache", "ignore_unavailable",
            "allow_no_indices", "expand_wildcards");
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
        return sprintf("ElasticSearch aggregation API provider for `%s`", rest.getSafeURL());
    }

    #! Makes a request and returns the response
    /** @param req the request to serialize and make according to the request type
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string index = remove req.index;
        string uri = sprintf("%s/_search", index);

        # Build query parameters
        bool has_query_params;
        foreach string arg in (QueryArgs) {
            if (!exists req{arg}) {
                continue;
            }
            if (!has_query_params) {
                uri += "?";
                has_query_params = True;
            } else {
                uri += "&";
            }
            uri += sprintf("%s=%s", arg, encode_url(string(remove req{arg}), True));
        }

        # Build request body
        hash<auto> body = {};

        # Set size to 0 by default for aggregation-only queries
        body.size = req.size ?? 0;
        remove req.size;

        # Add query if provided
        if (req.query) {
            body.query = remove req.query;
        }

        # Add aggregations
        if (req.aggs) {
            body.aggs = remove req.aggs;
        } else if (req.aggregations) {
            body.aggs = remove req.aggregations;
        }

        hash<auto> response = rest.post(uri, body).body;
        return {
            "took": response.took,
            "timed_out": response.timed_out,
            "_shards": response._shards,
            "hits": response.hits,
            "aggregations": response.aggregations,
        };
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

#! ElasticSearch aggregation request data type
public class ElasticSearchAggregationRequestDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "index": {
                "type": StringType,
                "display_name": "Index Name",
                "short_desc": "Target index for aggregation",
                "desc": "The name of the index to search (can include wildcards or comma-separated list)",
                "example_value": "logs-*",
            },
            "query": {
                "type": AutoHashOrNothingType,
                "display_name": "Query Filter",
                "short_desc": "Filter documents before aggregation",
                "desc": "Optional query to filter documents before aggregation (ElasticSearch Query DSL)",
                "example_value": {"match": {"status": "error"}},
            },
            "aggs": {
                "type": AutoHashOrNothingType,
                "display_name": "Aggregations",
                "short_desc": "Aggregation definitions",
                "desc": "The aggregations to compute. Each key is the aggregation name, value is the "
                    "aggregation definition. Example:\n"
                    "```\n"
                    "{\n"
                    "  \"avg_price\": {\"avg\": {\"field\": \"price\"}},\n"
                    "  \"by_category\": {\n"
                    "    \"terms\": {\"field\": \"category\"},\n"
                    "    \"aggs\": {\n"
                    "      \"category_avg\": {\"avg\": {\"field\": \"price\"}}\n"
                    "    }\n"
                    "  }\n"
                    "}\n"
                    "```",
                "example_value": {"avg_price": {"avg": {"field": "price"}}},
            },
            "aggregations": {
                "type": AutoHashOrNothingType,
                "display_name": "Aggregations (Alias)",
                "short_desc": "Alias for aggs field",
                "desc": "Alias for 'aggs' - the aggregations to compute",
            },
            "size": {
                "type": IntOrNothingType,
                "display_name": "Result Size",
                "short_desc": "Number of hits to return",
                "desc": "Number of hits to return. Default: 0 (aggregation-only, no documents returned)",
                "default_value": 0,
                "example_value": 0,
            },
            "routing": {
                "type": StringOrNothingType,
                "display_name": "Routing",
                "short_desc": "Custom routing value",
                "desc": "Custom routing value to limit search to specific shards",
                "example_value": "user123",
            },
            "preference": {
                "type": StringOrNothingType,
                "display_name": "Preference",
                "short_desc": "Shard replica preference",
                "desc": "Which shard replicas to execute the search on",
                "example_value": "_local",
            },
            "timeout": {
                "type": StringOrNothingType,
                "display_name": "Timeout",
                "short_desc": "Search timeout",
                "desc": "Search timeout (e.g., '10s' for 10 seconds)",
                "example_value": "10s",
            },
            "request_cache": {
                "type": SoftBoolStringType,
                "display_name": "Request Cache?",
                "short_desc": "Cache the request",
                "desc": "Whether to cache the request",
                "example_value": True,
            },
            "ignore_unavailable": {
                "type": SoftBoolStringType,
                "display_name": "Ignore Unavailable?",
                "short_desc": "Ignore unavailable indices",
                "desc": "Whether to ignore unavailable indices",
                "example_value": True,
            },
            "allow_no_indices": {
                "type": SoftBoolStringType,
                "display_name": "Allow No Indices?",
                "short_desc": "Allow no matching indices",
                "desc": "Whether to allow wildcards that match no indices",
                "example_value": True,
            },
            "expand_wildcards": {
                "type": StringOrNothingType,
                "display_name": "Expand Wildcards",
                "short_desc": "Wildcard expansion mode",
                "desc": "How to expand wildcards: 'open', 'closed', 'hidden', 'none', 'all'",
                "example_value": "open",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Aggregation shards response data type
public class ElasticSearchAggregationShardsDataType inherits DataProvider::HashDataType {
    private {
        const Fields = {
            "total": {
                "type": IntType,
                "display_name": "Total Shards",
                "short_desc": "Total number of shards",
                "desc": "Total number of shards",
                "example_value": 5,
            },
            "successful": {
                "type": IntType,
                "display_name": "Successful Shards",
                "short_desc": "Shards that succeeded",
                "desc": "Number of successful shards",
                "example_value": 5,
            },
            "skipped": {
                "type": IntOrNothingType,
                "display_name": "Skipped Shards",
                "short_desc": "Shards that were skipped",
                "desc": "Number of skipped shards",
                "example_value": 0,
            },
            "failed": {
                "type": IntType,
                "display_name": "Failed Shards",
                "short_desc": "Shards that failed",
                "desc": "Number of failed shards",
                "example_value": 0,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! ElasticSearch aggregation response data type
public class ElasticSearchAggregationResponseDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "took": {
                "type": IntType,
                "display_name": "Time Taken",
                "short_desc": "Request duration in ms",
                "desc": "How long the request took in milliseconds",
                "example_value": 15,
            },
            "timed_out": {
                "type": BoolType,
                "display_name": "Timed Out?",
                "short_desc": "Whether request timed out",
                "desc": "Whether the request timed out",
                "example_value": False,
            },
            "_shards": {
                "type": new ElasticSearchAggregationShardsDataType(),
                "display_name": "Shards",
                "short_desc": "Shard execution info",
                "desc": "Shard information",
            },
            "hits": {
                "type": AutoHashOrNothingType,
                "display_name": "Hits",
                "short_desc": "Search hits if size > 0",
                "desc": "Search hits (if size > 0)",
            },
            "aggregations": {
                "type": AutoHashOrNothingType,
                "display_name": "Aggregations",
                "short_desc": "Aggregation results",
                "desc": "The aggregation results. Structure depends on the aggregations requested. "
                    "Each aggregation name maps to its result.",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}
}
