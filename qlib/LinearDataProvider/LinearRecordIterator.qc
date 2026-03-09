# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore LinearDataProvider module definition

/*  LinearRecordIterator.qc Copyright 2025 - 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the LinearDataProvider module
public namespace LinearDataProvider {

#! Record iterator for Linear data with cursor-based pagination
/** This iterator handles Linear's Relay-style cursor-based pagination,
    fetching pages on-demand as the iterator progresses.
*/
public class LinearRecordIterator inherits DataProvider::AbstractDataProviderRecordIterator {
    private {
        #! The REST client for API calls
        LinearRestClient::LinearRestClient rest;

        #! The GraphQL query template
        string query;

        #! The data path in the response (e.g., "issues", "teams")
        string dataPath;

        #! Optional nested data path for queries like issue.comments
        *string nestedDataPath;

        #! Filter/variables for the query
        *hash<auto> variables;

        #! Page size for pagination
        int pageSize;

        #! Current cursor for pagination
        *string cursor;

        #! Whether there are more pages
        bool hasNextPage = True;

        #! Current page data iterator
        ListHashIterator i();

        #! Record type for the iterator
        *hash<string, AbstractDataField> recordType;
    }

    #! Creates the iterator
    /** @param rest the REST client for API calls
        @param query the GraphQL query template
        @param dataPath the path to the data in the response (e.g., "issues")
        @param variables optional variables for the query
        @param pageSize the number of records per page (default: 50)
        @param recordType optional record type definition
    */
    constructor(LinearRestClient::LinearRestClient rest, string query, string dataPath,
            *hash<auto> variables, int pageSize = DefaultPageSize,
            *hash<string, AbstractDataField> recordType) {
        @assert(rest, "REST client cannot be null");
        @assert(query.val(), "GraphQL query cannot be empty");
        @assert(dataPath.val(), "data path cannot be empty");
        @assert(pageSize > 0, "page size must be positive");
        self.rest = rest;
        self.query = query;
        self.dataPath = dataPath;
        self.variables = variables;
        self.pageSize = pageSize;
        self.recordType = recordType;
    }

    #! Creates the iterator with a nested data path
    /** @param rest the REST client for API calls
        @param query the GraphQL query template
        @param dataPath the path to the data in the response (e.g., "issue")
        @param nestedDataPath the nested path to the connection (e.g., "comments")
        @param variables optional variables for the query
        @param pageSize the number of records per page (default: 50)
        @param recordType optional record type definition
    */
    constructor(LinearRestClient::LinearRestClient rest, string query, string dataPath,
            string nestedDataPath, *hash<auto> variables, int pageSize = DefaultPageSize,
            *hash<string, AbstractDataField> recordType) {
        @assert(rest, "REST client cannot be null");
        @assert(query.val(), "GraphQL query cannot be empty");
        @assert(dataPath.val(), "data path cannot be empty");
        @assert(nestedDataPath.val(), "nested data path cannot be empty");
        @assert(pageSize > 0, "page size must be positive");
        self.rest = rest;
        self.query = query;
        self.dataPath = dataPath;
        self.nestedDataPath = nestedDataPath;
        self.variables = variables;
        self.pageSize = pageSize;
        self.recordType = recordType;
    }

    #! Returns True if the iterator is valid
    bool valid() {
        return i.valid();
    }

    #! Returns the current record value
    /** @return the current record as a hash

        @throw INVALID-ITERATOR the iterator is not valid
    */
    hash<auto> getValue() {
        return LinearDataProviderBase::flattenConnections(i.getValue());
    }

    #! Returns the value of a field in the current record
    /** @param key the field name

        @return the field value

        @throw INVALID-ITERATOR the iterator is not valid
    */
    auto memberGate(string key) {
        return getValue(){key};
    }

    #! Returns the record type, if available
    *hash<string, AbstractDataField> getRecordType() {
        return recordType;
    }

    #! Increments the row pointer when retrieving rows
    /** Implements pagination support via cursor-based pagination
        @return @ref True if there is a row to retrieve, @ref False if not
    */
    private bool nextImpl() {
        if (i.next()) {
            return True;
        }

        # Check for next page (pagination support)
        if (hasNextPage) {
            return fetchNextPage();
        }

        return False;
    }

    #! Fetches the next page of results
    /** @return True if records were fetched, False if no more data
    */
    private bool fetchNextPage() {
        @assert(rest, "REST client must be initialized");
        @assert(pageSize > 0, "page size must be positive");
        hash<auto> vars = {
            "first": pageSize,
        } + (variables ?? {});
        if (cursor) {
            vars.after = cursor;
        }

        hash<auto> response = rest.graphql(query, vars);
        @assert(response.body, "GraphQL response body cannot be null");

        if (response.body.errors) {
            throw "GRAPHQL-ERROR", response.body.errors[0].message ?? "Unknown error",
                response.body.errors;
        }

        # Navigate to the connection data
        *hash<auto> connection;
        if (nestedDataPath) {
            # For nested queries like issue.comments
            connection = response.body.data{dataPath}{nestedDataPath};
        } else {
            connection = response.body.data{dataPath};
        }

        if (!connection) {
            hasNextPage = False;
            return False;
        }

        # Update pagination state
        cursor = connection.pageInfo.endCursor;
        hasNextPage = connection.pageInfo.hasNextPage ?? False;

        # Check for empty results
        if (!connection.nodes || !connection.nodes.size()) {
            hasNextPage = False;
            return False;
        }

        # Create iterator for current page
        i = new ListHashIterator(connection.nodes);
        return i.next();
    }
}

} # namespace LinearDataProvider
