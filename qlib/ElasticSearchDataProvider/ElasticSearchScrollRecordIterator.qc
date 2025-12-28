# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ElasticSearchScrollRecordIterator class definition

/** ElasticSearchScrollRecordIterator.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! contains all public definitions in the ElasticSearchDataProvider module
public namespace ElasticSearchDataProvider {
#! Defines the record iterator class using the Scroll API for large result sets
/** This iterator uses the ElasticSearch Scroll API for efficient iteration over large result sets.
    The Scroll API is designed for processing large numbers of results and maintains a consistent
    view of the index (point-in-time snapshot).

    @note The scroll context is automatically cleaned up when the iterator is destroyed or when
    all results have been consumed.

    @see https://www.elastic.co/guide/en/elasticsearch/reference/current/scroll-api.html
*/
public class ElasticSearchScrollRecordIterator inherits DataProvider::AbstractDataProviderRecordIterator {
    private:internal {
        #! The REST client object for API calls
        RestClient::RestClient rest;

        #! Current index name
        string name;

        #! Index hash
        hash<auto> index;

        #! The record type for the object
        hash<string, DataProvider::AbstractDataField> record_type;

        #! Current scroll ID
        *string scroll_id;

        #! Scroll timeout (how long to keep the scroll context alive)
        string scroll_timeout;

        #! Current batch of records
        list<hash<auto>> current_batch = ();

        #! Current index within the batch
        int batch_index = -1;

        #! Whether we have exhausted all results
        bool exhausted = False;

        #! Batch size for scroll requests
        int batch_size;

        #! Query match options
        const MatchOptions = ("operator",);
    }

    #! Creates the scroll iterator
    /** @param rest the REST client connection
        @param name the name of the index
        @param index the index definition hash
        @param record_type record type information
        @param where_cond the where clause for the query, if any
        @param search_options search options; assumed to have already been processed for validity before this call
        @param scroll_timeout the scroll timeout (default: "1m" = 1 minute)
        @param batch_size the batch size for scroll requests (default: 1000)
    */
    constructor(RestClient::RestClient rest, string name, hash<auto> index,
            hash<string, DataProvider::AbstractDataField> record_type, *hash<auto> where_cond,
            *hash<auto> search_options, string scroll_timeout = "1m", int batch_size = 1000)
            : AbstractDataProviderRecordIterator(search_options.requires_result, search_options.max_records) {
        self.rest = rest;
        self.name = name;
        self.index = index;
        self.record_type = record_type;
        self.scroll_timeout = scroll_timeout;
        self.batch_size = batch_size;

        # Make initial scroll request
        string uri = sprintf("%s/_search?scroll=%s&size=%d", name, scroll_timeout, batch_size);

        if (search_options) {
            uri = ElasticSearchIndexTableDataProvider::getQueryOptions(uri, search_options,
                ElasticSearchIndexTableDataProvider::SearchQueryOptions);
        }

        # Make query
        *hash<auto> query_body = makeQuery(where_cond, search_options);
        hash<auto> info;
        try {
            hash<auto> response = rest.post(uri, query_body, \info).body;
            scroll_id = response._scroll_id;
            processHits(response.hits.hits);
        } catch (hash<ExceptionInfo> ex) {
            # Ensure that any error response body is included in the exception
            hash<auto> ex_arg = {
                "query": query_body,
            } + info{"response-code", "response-body"};
            rethrow ex.err, ex.desc, ex_arg;
        }
    }

    #! Destructor - cleans up the scroll context
    destructor() {
        clearScroll();
    }

    #! Clears the scroll context
    private clearScroll() {
        if (scroll_id) {
            try {
                rest.del("_search/scroll", {"scroll_id": scroll_id});
            } catch () {
                # Ignore errors when clearing scroll - it may have already expired
            }
            delete scroll_id;
        }
    }

    #! Returns @ref True if the iterator is valid
    bool valid() {
        return batch_index >= 0 && batch_index < current_batch.size();
    }

    #! Returns a single record if the iterator is valid
    /** @throw INVALID-ITERATOR the iterator is not pointing at a valid element
    */
    hash<auto> getValue() {
        if (!valid()) {
            throw "INVALID-ITERATOR", "the iterator is not pointing at a valid element";
        }
        return current_batch[batch_index];
    }

    #! Returns the value of the given field in the current row, if the iterator is valid
    /** @param key the name of the field

        @return the value of the given field in the current row, if the iterator is valid

        @throw FIELD-ERROR invalid or unknown field name
    */
    auto memberGate(string key) {
        hash<auto> row = getValue();
        auto rv = row{key};
        if (!exists rv && !row.hasKey(key)) {
            throw "FIELD-ERROR", sprintf("the current record does not have field %y; valid fields: %y", key,
                keys row);
        }
        return rv;
    }

    #! Returns the query body
    *hash<auto> makeQuery(*hash<auto> where_cond, *hash<auto> search_options) {
        if (!where_cond) {
            return;
        }
        return {
            "query": {
                "match": map {$1.key: {
                    "query": $1.value
                } + search_options{MatchOptions}}, where_cond.pairIterator(),
            },
        };
    }

    #! Returns the record description, if available
    *hash<string, DataProvider::AbstractDataField> getRecordType() {
        return record_type;
    }

    #! Increments the row pointer when retrieving rows
    /** @return @ref True if there is a row to retrieve, @ref False if not (no more rows to be retrieved)
    */
    private bool nextImpl() {
        ++batch_index;

        # Check if we need to fetch the next batch
        if (batch_index >= current_batch.size()) {
            if (exhausted) {
                return False;
            }
            fetchNextBatch();
            if (exhausted || current_batch.empty()) {
                return False;
            }
            batch_index = 0;
        }

        return True;
    }

    #! Fetches the next batch of records using the scroll API
    private fetchNextBatch() {
        if (!scroll_id) {
            exhausted = True;
            return;
        }

        hash<auto> info;
        try {
            hash<auto> response = rest.post("_search/scroll", {
                "scroll": scroll_timeout,
                "scroll_id": scroll_id,
            }, \info).body;

            # Update scroll_id (it may change between requests)
            scroll_id = response._scroll_id;

            processHits(response.hits.hits);

            if (current_batch.empty()) {
                # No more results
                exhausted = True;
                clearScroll();
            }
        } catch (hash<ExceptionInfo> ex) {
            exhausted = True;
            clearScroll();
            hash<auto> ex_arg = info{"response-code", "response-body"};
            rethrow ex.err, ex.desc, ex_arg;
        }
    }

    #! Processes the hits from a scroll response
    private processHits(*list<hash<auto>> hits) {
        if (!hits) {
            current_batch = ();
            return;
        }
        current_batch = map $1{"_id", "_score"} + $1._source, hits;
    }
}
}
