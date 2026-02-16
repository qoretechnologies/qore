# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QdrantSearchRecordIterator class definition

/** QdrantSearchRecordIterator.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the QdrantDataProvider module
public namespace QdrantDataProvider {
#! Record iterator wrapping Qdrant query results
/** Calls `POST /collections/{name}/points/query` and iterates over scored point results
*/
public class QdrantSearchRecordIterator inherits DataProvider::AbstractDataProviderRecordIterator {
    private:internal {
        #! Record type
        const RecordType = new QdrantScoredPointRecordType();

        #! The underlying list iterator
        Qore::ListHashIterator i;
    }

    #! Creates the iterator
    /** @param rest the REST client connection
        @param collection_name the name of the collection to query
        @param where_cond the where clause mapped to Qdrant filter
        @param search_options search options; assumed to have already been processed for validity
    */
    constructor(RestClient::RestClient rest, string collection_name, *hash<auto> where_cond,
            *hash<auto> search_options)
            : AbstractDataProviderRecordIterator(search_options.requires_result) {
        # build request body from search options
        hash<auto> body;
        if (search_options.query) {
            body.query = search_options.query;
        }
        if (search_options."using") {
            body."using" = search_options."using";
        }
        if (search_options.filter) {
            body.filter = search_options.filter;
        }
        if (exists search_options.score_threshold) {
            body.score_threshold = search_options.score_threshold;
        }
        if (search_options.prefetch) {
            body.prefetch = search_options.prefetch;
        }
        if (search_options.params) {
            body.params = search_options.params;
        }
        if (exists search_options.limit) {
            body.limit = search_options.limit;
        }
        if (exists search_options.offset) {
            body.offset = search_options.offset;
        }
        if (exists search_options.with_payload) {
            body.with_payload = boolean(search_options.with_payload);
        } else {
            body.with_payload = True;
        }
        if (exists search_options.with_vector) {
            body.with_vector = boolean(search_options.with_vector);
        }
        if (search_options.lookup_from) {
            body.lookup_from = search_options.lookup_from;
        }

        string uri = sprintf("/collections/%s/points/query", encode_url(collection_name, True));
        *list<auto> points = rest.post(uri, body).body.result.points;
        i = new ListHashIterator(points);
    }

    #! Returns @ref True if the iterator is valid
    bool valid() {
        return i.valid();
    }

    #! Returns a single record if the iterator is valid
    /** @throw INVALID-ITERATOR the iterator is not pointing at a valid element
    */
    hash<auto> getValue() {
        return i.getValue();
    }

    #! Returns the record description, if available
    *hash<string, DataProvider::AbstractDataField> getRecordType() {
        return RecordType.getFields();
    }

    #! Increments the row pointer
    private bool nextImpl() {
        return i.next();
    }
}
}
