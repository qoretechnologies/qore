# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore JotformDataProvider module definition

/*  JotformRecordIterator.qc Copyright 2025 - 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the JotformDataProvider module
public namespace JotformDataProvider {

#! Record iterator for JotForm data with offset/limit pagination
/** This iterator handles JotForm's offset-based pagination,
    fetching pages on-demand as the iterator progresses.
*/
public class JotformRecordIterator inherits DataProvider::AbstractDataProviderRecordIterator {
    private {
        #! The REST client for API calls
        JotformRestClient::JotformRestClient rest;

        #! The API endpoint path
        string endpoint;

        #! Query parameters for the request
        *hash<auto> queryParams;

        #! Page size for pagination
        int pageSize;

        #! Current offset for pagination
        int offset = 0;

        #! Whether there are more pages
        bool hasMore = True;

        #! Current page data iterator
        ListHashIterator i();

        #! Record type for the iterator
        *hash<string, AbstractDataField> recordType;

        #! Total limit for results (0 = unlimited)
        int totalLimit = 0;

        #! Count of records returned so far
        int recordCount = 0;
    }

    #! Creates the iterator
    /** @param rest the REST client for API calls
        @param endpoint the API endpoint path (e.g., "/user/forms")
        @param queryParams optional query parameters for the request
        @param pageSize the number of records per page (default: 100)
        @param recordType optional record type definition
        @param totalLimit optional total limit on number of records (0 = unlimited)
    */
    constructor(JotformRestClient::JotformRestClient rest, string endpoint,
            *hash<auto> queryParams, int pageSize = DefaultPageSize,
            *hash<string, AbstractDataField> recordType, int totalLimit = 0) {
        @assert(rest, "REST client cannot be null");
        @assert(endpoint.val(), "endpoint cannot be empty");
        @assert(pageSize > 0, "page size must be positive");
        self.rest = rest;
        self.endpoint = endpoint;
        self.queryParams = queryParams;
        self.pageSize = pageSize;
        self.recordType = recordType;
        self.totalLimit = totalLimit;
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
        return i.getValue();
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
    /** Implements pagination support via offset-based pagination
        @return @ref True if there is a row to retrieve, @ref False if not
    */
    private bool nextImpl() {
        # Check if we've hit the total limit
        if (totalLimit > 0 && recordCount >= totalLimit) {
            return False;
        }

        if (i.next()) {
            recordCount++;
            return True;
        }

        # Check for next page (pagination support)
        if (hasMore) {
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

        # Build query parameters with pagination
        hash<auto> params = queryParams ?? {};
        params.offset = offset;
        params.limit = pageSize;

        # Adjust limit if we have a total limit
        if (totalLimit > 0) {
            int remaining = totalLimit - recordCount;
            if (remaining <= 0) {
                hasMore = False;
                return False;
            }
            params.limit = min(pageSize, remaining);
        }

        string url = endpoint;
        if (params) {
            url += "?" + buildQueryString(params);
        }
        hash<auto> response = rest.get(url);

        # JotForm returns content in the response body
        if (!response.body || !response.body.content) {
            hasMore = False;
            return False;
        }

        # Check for API errors
        if (response.body.responseCode.val() && response.body.responseCode != 200) {
            throw "JOTFORM-API-ERROR", response.body.message ?? "Unknown error", response.body;
        }

        list<auto> records = response.body.content;

        # Check for empty results
        if (!records || !records.size()) {
            hasMore = False;
            return False;
        }

        # Update pagination state
        offset += records.size();

        # If we got fewer than requested, there are no more pages
        if (records.size() < params.limit) {
            hasMore = False;
        }

        # Create iterator for current page
        i = new ListHashIterator(records);
        if (i.next()) {
            recordCount++;
            return True;
        }
        return False;
    }

    #! Builds a query string from a hash of parameters
    private string buildQueryString(hash<auto> params) {
        list<string> parts;
        foreach string key in (keys params) {
            auto val = params{key};
            if (exists val) {
                parts += sprintf("%s=%s", key, encode_url(string(val), True));
            }
        }
        return parts.join("&");
    }
}

} # namespace JotformDataProvider
