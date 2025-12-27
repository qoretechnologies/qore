# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ODataRecordIterator class definition

/** ODataRecordIterator.qc Copyright 2023 - 2025 Qore Technologies, s.r.o.

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

#! contains all public definitions in the CdsRestDataProvider module
public namespace CdsRestDataProvider {
#! Defines the unified record iterator class for OData entity iterators
/** Supports both Dynamics 365 CDS and Business Central with optional company scoping and pagination
*/
public class ODataRecordIterator inherits AbstractODataRecordIterator {
    #! creates the iterator
    /** @param rest the REST client connection
        @param entity_name the name of the entity (for CDS) or entity path (includes company for BC)
        @param record_info record information describing the entity
        @param where_cond the where clause for the query, if any
        @param search_options search options; assumed to have already been processed for validity before this call
    */
    constructor(CdsRestClient::CdsRestClient rest, string entity_name, hash<ODataRecordInfo> record_info,
            *hash<auto> where_cond, *hash<auto> search_options)
            : AbstractODataRecordIterator(rest, record_info, search_options) {
        # Make query
        *string query = makeQuery(where_cond, search_options);
        hash<auto> info;
        try {
            string uri = entity_name;
            if (query) {
                uri += sprintf("?%s", query);
            }
            hash<auto> resp = rest.get(uri, NOTHING, \info);
            i = new ListHashIterator(resp.body.value ?? ());
            # Save next link for pagination (used by BC, ignored by CDS)
            nextLink = resp.body."@odata.nextLink";
        } catch (hash<ExceptionInfo> ex) {
            # ensure that any error response body is included in the exception
            hash<auto> ex_arg = {
                "query": query,
            } + info{"response-code", "response-body"};
            throw ex.err, ex.desc, ex_arg;
        }
    }

    #! Increments the row pointer when retrieving rows from a select statement
    /** Implements pagination support via @odata.nextLink
        @return @ref True if there is a row to retrieve, @ref False if not
    */
    private bool nextImpl() {
        if (i.next()) {
            return True;
        }

        # Check for next page (pagination support)
        if (nextLink) {
            hash<auto> info;
            try {
                hash<auto> resp = rest.get(nextLink, NOTHING, \info);
                i = new ListHashIterator(resp.body.value ?? ());
                nextLink = resp.body."@odata.nextLink";
                return i.next();
            } catch (hash<ExceptionInfo> ex) {
                # Clear pagination state and rethrow - caller should be aware of the error
                nextLink = NOTHING;
                rethrow;
            }
        }

        return False;
    }
}
}
