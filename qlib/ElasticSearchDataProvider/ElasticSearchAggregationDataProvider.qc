# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ElasticSearchDataProvider module definition

/** ElasticSearchAggregationDataProvider.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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
        bool q;
        foreach string arg in (QueryArgs) {
            if (!exists req{arg}) {
                continue;
            }
            if (!q) {
                uri += "?";
                q = True;
            } else {
                uri += "&";
            }
            uri += sprintf("%s=%s", arg, remove req{arg});
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
                "desc": "The name of the index to search (can include wildcards or comma-separated list)",
            },
            "query": {
                "type": AutoHashOrNothingType,
                "desc": "Optional query to filter documents before aggregation (ElasticSearch Query DSL)",
            },
            "aggs": {
                "type": AutoHashOrNothingType,
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
            },
            "aggregations": {
                "type": AutoHashOrNothingType,
                "desc": "Alias for 'aggs' - the aggregations to compute",
            },
            "size": {
                "type": IntOrNothingType,
                "desc": "Number of hits to return. Default: 0 (aggregation-only, no documents returned)",
                "default_value": 0,
            },
            "routing": {
                "type": StringOrNothingType,
                "desc": "Custom routing value to limit search to specific shards",
            },
            "preference": {
                "type": StringOrNothingType,
                "desc": "Which shard replicas to execute the search on",
            },
            "timeout": {
                "type": StringOrNothingType,
                "desc": "Search timeout (e.g., '10s' for 10 seconds)",
            },
            "request_cache": {
                "type": SoftBoolStringOrNothingType,
                "desc": "Whether to cache the request",
            },
            "ignore_unavailable": {
                "type": SoftBoolStringOrNothingType,
                "desc": "Whether to ignore unavailable indices",
            },
            "allow_no_indices": {
                "type": SoftBoolStringOrNothingType,
                "desc": "Whether to allow wildcards that match no indices",
            },
            "expand_wildcards": {
                "type": StringOrNothingType,
                "desc": "How to expand wildcards: 'open', 'closed', 'hidden', 'none', 'all'",
            },
        };
    }

    #! Creates the object
    constructor() {
        map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type, $1.value.default_value)),
            Fields.pairIterator();
    }
}

#! Aggregation shards response data type
public class ElasticSearchAggregationShardsDataType inherits DataProvider::HashDataType {
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

#! ElasticSearch aggregation response data type
public class ElasticSearchAggregationResponseDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "took": {
                "type": IntType,
                "desc": "How long the request took in milliseconds",
            },
            "timed_out": {
                "type": BoolType,
                "desc": "Whether the request timed out",
            },
            "_shards": {
                "type": new ElasticSearchAggregationShardsDataType(),
                "desc": "Shard information",
            },
            "hits": {
                "type": AutoHashOrNothingType,
                "desc": "Search hits (if size > 0)",
            },
            "aggregations": {
                "type": AutoHashOrNothingType,
                "desc": "The aggregation results. Structure depends on the aggregations requested. "
                    "Each aggregation name maps to its result.",
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
