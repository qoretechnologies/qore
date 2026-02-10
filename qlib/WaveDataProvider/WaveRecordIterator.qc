# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WaveDataProvider module definition

/*  WaveRecordIterator.qc Copyright 2025 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the WaveDataProvider module
public namespace WaveDataProvider {

#! Record iterator for Wave data with page-number-based pagination
/** This iterator handles Wave's page-number-based pagination,
    fetching pages on-demand as the iterator progresses.
*/
public class WaveRecordIterator inherits DataProvider::AbstractDataProviderRecordIterator {
    private {
        #! The REST client for API calls
        WaveRestClient::WaveRestClient rest;

        #! The GraphQL query template
        string query;

        #! The root path in the response (e.g., "business")
        *string rootPath;

        #! The data path in the response (e.g., "customers")
        string dataPath;

        #! Filter/variables for the query
        *hash<auto> variables;

        #! Page size for pagination
        int pageSize;

        #! Current page number (1-based)
        int currentPage = 1;

        #! Total number of pages
        int totalPages = 1;

        #! Whether the first page has been fetched
        bool initialized = False;

        #! Current page data iterator
        ListHashIterator i();

        #! Record type for the iterator
        *hash<string, AbstractDataField> recordType;
    }

    #! Creates the iterator with a root path and data path
    /** @param rest the REST client for API calls
        @param query the GraphQL query template
        @param rootPath the root path in the response (e.g., "business")
        @param dataPath the data path under the root (e.g., "customers")
        @param variables optional variables for the query
        @param pageSize the number of records per page (default: 50)
        @param recordType optional record type definition
    */
    constructor(WaveRestClient::WaveRestClient rest, string query, string rootPath,
            string dataPath, *hash<auto> variables, int pageSize = DefaultPageSize,
            *hash<string, AbstractDataField> recordType) {
        @assert(rest, "REST client cannot be null");
        @assert(query.val(), "GraphQL query cannot be empty");
        @assert(rootPath.val(), "root path cannot be empty");
        @assert(dataPath.val(), "data path cannot be empty");
        @assert(pageSize > 0, "page size must be positive");
        self.rest = rest;
        self.query = query;
        self.rootPath = rootPath;
        self.dataPath = dataPath;
        self.variables = variables;
        self.pageSize = pageSize;
        self.recordType = recordType;
    }

    #! Creates the iterator without a root path for top-level queries
    /** @param rest the REST client for API calls
        @param query the GraphQL query template
        @param dataPath the data path in the response (e.g., "businesses")
        @param variables optional variables for the query
        @param pageSize the number of records per page (default: 50)
        @param recordType optional record type definition
    */
    constructor(WaveRestClient::WaveRestClient rest, string query, string dataPath,
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

    #! Returns True if the iterator is valid
    bool valid() {
        return i.valid();
    }

    #! Returns the current record value
    /** @return the current record as a hash

        @throw INVALID-ITERATOR the iterator is not valid
    */
    hash<auto> getValue() {
        return WaveDataProviderBase::flattenEdges(i.getValue());
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
    /** Implements pagination support via page-number-based pagination
        @return @ref True if there is a row to retrieve, @ref False if not
    */
    private bool nextImpl() {
        if (i.next()) {
            return True;
        }

        # Check for next page (pagination support)
        if (!initialized || currentPage <= totalPages) {
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
        hash<auto> vars = (variables ?? {}) + {
            "page": currentPage,
            "pageSize": pageSize,
        };

        hash<auto> response = rest.graphql(query, vars);
        @assert(response.body, "GraphQL response body cannot be null");

        if (response.body.errors) {
            throw "GRAPHQL-ERROR", response.body.errors[0].message ?? "Unknown error",
                response.body.errors;
        }

        # Navigate to the connection data
        *hash<auto> connection;
        if (rootPath) {
            connection = response.body.data{rootPath}{dataPath};
        } else {
            connection = response.body.data{dataPath};
        }

        if (!connection) {
            initialized = True;
            return False;
        }

        # Extract nodes from edges
        *list<auto> nodes;
        if (connection.hasKey("edges") && connection.edges.typeCode() == NT_LIST) {
            nodes = map $1.node ?? $1, connection.edges;
        }

        # Update pagination state from pageInfo
        if (connection.pageInfo) {
            totalPages = connection.pageInfo.totalPages ?? 1;
        }

        initialized = True;
        currentPage++;

        # Check for empty results
        if (!nodes || !nodes.size()) {
            return False;
        }

        # Create iterator for current page
        i = new ListHashIterator(nodes);
        return i.next();
    }
}

} # namespace WaveDataProvider
