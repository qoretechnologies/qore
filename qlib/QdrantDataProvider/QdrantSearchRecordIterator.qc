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
        @param where_cond the where clause mapped to Qdrant filter; supports both DataProvider expression trees
        (with \c exp key) and plain hash equality conditions
        @param search_options search options; assumed to have already been processed for validity
    */
    constructor(RestClientIo::RestClientIo rest, string collection_name, *hash<auto> where_cond,
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

        # Process where_cond into a Qdrant filter
        if (where_cond) {
            *hash<auto> where_filter = processWhereCondition(where_cond);
            if (where_filter) {
                # Ensure the filter is a valid top-level Qdrant filter (must/should/must_not)
                where_filter = ensureTopLevelFilter(where_filter);
                if (body.filter) {
                    # Merge where_cond filter with search_options filter using AND semantics
                    body.filter = {"must": (where_filter, body.filter)};
                } else {
                    body.filter = where_filter;
                }
            }
        }

        string uri = sprintf("/collections/%s/points/query", encode_url(collection_name, True));
        *list<auto> points = rest.restPost(uri, body).body.result.points;
        i = new ListHashIterator(points);
    }

    #! Processes a DataProvider expression argument and returns the Qdrant filter hash
    /** @param exp the expression to process
        @return the Qdrant filter hash for the expression
    */
    hash<auto> processExpressionArg(hash<DataProviderExpression> exp) {
        *hash<auto> expinfo = QdrantExpressions{exp.exp};
        if (!expinfo) {
            throw "WHERE-ERROR", sprintf("expression references unknown operator %y; known operators: %y",
                exp.exp, keys QdrantExpressions);
        }

        # Handle logical operators (AND, OR, NOT) - pass all args to impl
        if (exp.exp == DP_OP_AND || exp.exp == DP_OP_OR || exp.exp == DP_SEARCH_OP_NOT) {
            return call_function_args(expinfo.impl, (self, exp.args));
        }

        # For comparison operators, first argument must be a field reference
        if (exp.args.size() < 1 || !(exp.args[0] instanceof hash<DataProviderFieldReference>)) {
            throw "WHERE-ERROR", sprintf("the first argument to expression %y must be a field reference",
                exp.exp);
        }
        string field = cast<hash<DataProviderFieldReference>>(exp.args[0]).field;

        # For IN operator, collect all values into a single list
        if (exp.exp == DP_SEARCH_OP_IN) {
            list<auto> values = ();
            for (int j = 1; j < exp.args.size(); ++j) {
                if (exp.args[j].typeCode() == NT_LIST) {
                    values += exp.args[j];
                } else {
                    push values, exp.args[j];
                }
            }
            return call_function_args(expinfo.impl, (field, values));
        }

        # For BETWEEN, pass field + lo + hi
        if (exp.exp == DP_SEARCH_OP_BETWEEN) {
            if (exp.args.size() < 3) {
                throw "WHERE-ERROR", sprintf("expression %y requires 3 arguments (field reference + lo + hi), "
                    "but got %d", exp.exp, exp.args.size());
            }
            return call_function_args(expinfo.impl, (field, exp.args[1], exp.args[2]));
        }

        # Standard comparison operators: field + value
        if (exp.args.size() < 2) {
            throw "WHERE-ERROR", sprintf("expression %y requires at least 2 arguments (field reference + value), "
                "but got %d", exp.exp, exp.args.size());
        }
        return call_function_args(expinfo.impl, (field, exp.args[1]));
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

    #! Ensures a filter hash is a valid top-level Qdrant filter
    /** Qdrant requires filter objects to have \c must, \c should, or \c must_not at the top level.
        Comparison conditions (with \c key field) need to be wrapped in a \c must array.
        @param filter the filter hash to check
        @return a valid top-level Qdrant filter
    */
    private hash<auto> ensureTopLevelFilter(hash<auto> filter) {
        if (filter.hasKey("must") || filter.hasKey("should") || filter.hasKey("must_not")) {
            return filter;
        }
        return {"must": (filter,)};
    }

    #! Processes a where_cond hash into a Qdrant filter hash
    /** @param where_cond the where condition; either a DataProvider expression tree (with \c exp key)
        or a plain hash of field: value equality conditions
        @return the Qdrant filter hash
    */
    private *hash<auto> processWhereCondition(hash<auto> where_cond) {
        # Check if this is a DataProvider expression
        if (where_cond.exp) {
            return processExpressionArg(cast<hash<DataProviderExpression>>(where_cond));
        }

        # Plain hash: convert each key: value pair to equality conditions
        list<auto> conditions;
        foreach hash<auto> pair in (where_cond.pairIterator()) {
            push conditions, {"key": pair.key, "match": {"value": pair.value}};
        }
        if (conditions.size() == 1) {
            return conditions[0];
        }
        return {"must": conditions};
    }
}
}
