# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AbstractODataEntityDataProvider class definition

/** AbstractODataEntityDataProvider.qc Copyright 2023 - 2025 Qore Technologies, s.r.o.

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
#! Abstract base class for OData entity data providers
/** Provides common CRUD operations used by both CDS and Business Central
*/
public class AbstractODataEntityDataProvider inherits CdsRestDataProviderBase {
    public {
        #! Entity name
        string name;

        #! Common search options for OData
        const ODataSearchOptions = GenericRecordSearchOptions{"requires_result", "max_records"} + {
            "columns": <DataProviderOptionInfo>{
                "display_name": "Columns",
                "short_desc": "Columns to include in the output of the query",
                "type": AbstractDataProviderTypeMap."auto",
                "desc": "Columns to include in the output of the query; `$select` argument for the query expression",
            },
            "limit": <DataProviderOptionInfo>{
                "display_name": "Limit",
                "short_desc": "The maximum number of records to return",
                "type": AbstractDataProviderTypeMap."int",
                "desc": "The maximum number of records to return; `$top` argument for the query expression",
            },
            "orderby": <DataProviderOptionInfo>{
                "display_name": "Order By",
                "short_desc": "Fields to order the query result by",
                "type": new QoreListDataType(new Type("softlist<string>")),
                "desc": "Fields to order the query result by; `$orderby` argument for the query expression",
            },
            "orderby_desc": <DataProviderOptionInfo>{
                "display_name": "Order By Descending",
                "short_desc": "Set this flag to use descending sort order for the 'orderby' option",
                "type": AbstractDataProviderTypeMap."bool",
                "desc": "Set this flag to use descending sort order for the `orderby` option",
            },
        };
    }

    private {
        #! Record info for the entity
        hash<ODataRecordInfo> record_info();
    }

    #! Creates the object from the arguments
    constructor(CdsRestClient::CdsRestClient rest, string name) : CdsRestDataProviderBase(rest) {
        self.name = name;
        record_info.entity_name = name;
    }

    #! Creates the object from connection
    constructor(CdsRestClient::CdsRestConnection conn, string name) : CdsRestDataProviderBase(conn) {
        self.name = name;
        record_info.entity_name = name;
    }

    #! Returns the data provider name
    string getName() {
        return name;
    }

    private *hash<string, DataProvider::AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        checkRecordInit();
        return record_info.record_type;
    }

    #! Ensures record info is initialized
    private checkRecordInit() {
        if (!record_info.record_type) {
            initRecordInfo();
        }
    }

    #! Template method: subclasses implement to initialize record info
    abstract private initRecordInfo();

    #! Template method: subclasses implement to get the URI for an entity
    /** @param id optional record ID for single-record operations
        @return the URI path for the entity
    */
    abstract private string getEntityUri(*string id);

    #! Template method: subclasses implement to create a record iterator
    abstract private AbstractDataProviderRecordIterator createRecordIterator(*hash<auto> where_cond, *hash<auto> search_options);

    #! Returns an iterator for zero or more records matching the search options
    private DataProvider::AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        checkRecordInit();
        return createRecordIterator(where_cond, search_options);
    }

    #! Creates the given record to the data provider
    /** @param rec a hash representing a single input record
        @param create_options the create options after processing by validateCreateOptions()

        @return the data written to the data provider with the key field of the new record

        @throw DUPLICATE-RECORD this exception should be thrown if the provider fails due to an attempt to create a
        duplicate record
    */
    private *hash<auto> createRecordImpl(hash<auto> rec, *hash<auto> create_options) {
        checkRecordInit();
        string url = getEntityUri();
        hash<auto> info;
        hash<auto> resp;
        try {
            resp = rest.post(url, rec, \info);
        } catch (hash<ExceptionInfo> ex) {
            hash<auto> ex_arg = info{"request-uri", "response-code", "response-body"};
            # Check for duplicate record error
            if (rec{record_info.key} && isDuplicateError(ex, info)) {
                ex.desc = sprintf("%s: %s", ex.err, ex.desc);
                ex.err = "DUPLICATE-RECORD";
                ex_arg.id = {record_info.key: rec{record_info.key}};
            }
            throw ex.err, ex.desc, ex.arg + ex_arg;
        }
        return processCreateResponse(rec, resp, info);
    }

    #! Template method: check if error is a duplicate record error
    private bool isDuplicateError(hash<ExceptionInfo> ex, hash<auto> info) {
        # Default implementation - subclasses can override
        return False;
    }

    #! Template method: process create response to extract new record ID
    private *hash<auto> processCreateResponse(hash<auto> rec, hash<auto> resp, hash<auto> info) {
        # Default: return response body or original record
        return resp.body ?? rec;
    }

    #! Upserts the given record to the data provider
    /** @param rec a hash representing a single input record
        @param upsert_options the create options after processing by validateUpsertOptions()

        @return see \c db_provider_upsert_results for possible values
    */
    private string upsertRecordImpl(hash<auto> rec, *hash<auto> upsert_options) {
        checkRecordInit();
        *softstring id_value = remove rec{record_info.key};
        if (!id_value.val()) {
            throw "UPSERT-ERROR", sprintf("upsert request requires a value for the %y key field, but none was given",
                record_info.key);
        }
        date now = now_us();

        string url = getEntityUri(id_value);
        hash<auto> info;
        hash<auto> resp;
        try {
            hash<auto> hdr = getUpsertHeaders(id_value);
            resp = rest.patch(url, rec, \info, hdr + {"Prefer": "return=representation"});
        } catch (hash<ExceptionInfo> ex) {
            # If record doesn't exist (404), try to create it
            if (info."response-code" == 404) {
                rec{record_info.key} = id_value;
                createRecordImpl(rec);
                return UpsertResultInserted;
            }
            hash<auto> ex_arg = info{"request-uri", "response-code", "response-body"};
            throw ex.err, ex.desc, ex.arg + ex_arg;
        }

        return checkUpsertResult(resp, now);
    }

    #! Template method: get headers for upsert operation (e.g., ETag for BC)
    private hash<auto> getUpsertHeaders(string id) {
        return {};
    }

    #! Template method: check if upsert resulted in insert or update
    private string checkUpsertResult(hash<auto> resp, date now) {
        # Check creation time of record to see if we created or updated
        # Subclasses can override for API-specific logic
        return UpsertResultUpdated;
    }

    #! Updates zero or more records matching the search options
    /** @param set the hash of field data to set
        @param where_cond a hash for identifying the record(s) to be updated
        @param search_options the update options after processing by validateSearchOptions()

        @return the number of records updated
    */
    private int updateRecordsImpl(hash<auto> set, hash<auto> where_cond, *hash<auto> search_options) {
        checkRecordInit();
        # Check for simple key-based update (not an expression)
        if (!where_cond.exp && where_cond{record_info.key} && where_cond.size() == 1) {
            updateSingleRecord(where_cond{record_info.key}, set);
            return 1;
        }
        AbstractDataProviderRecordIterator i = searchRecords(where_cond, search_options);
        int count = 0;
        map (updateSingleRecord(i.getValue(){record_info.key}, set), ++count), i;
        return count;
    }

    #! Deletes zero or more records
    /** @param where_cond a hash for identifying the record(s) to be deleted
        @param search_options the delete options after processing by validateSearchOptions()

        @return the number of records deleted
    */
    private int deleteRecordsImpl(*hash<auto> where_cond, *hash<auto> search_options) {
        checkRecordInit();
        # Check for simple key-based delete (not an expression)
        if (where_cond && !where_cond.exp && where_cond{record_info.key} && where_cond.size() == 1) {
            deleteSingleRecord(where_cond{record_info.key});
            return 1;
        }
        AbstractDataProviderRecordIterator i = searchRecords(where_cond, search_options);
        int count = 0;
        map (deleteSingleRecord(i.getValue(){record_info.key}), ++count), i;
        return count;
    }

    #! Updates a single record
    private updateSingleRecord(string id, hash<auto> set) {
        checkRecordInit();
        string url = getEntityUri(id);
        hash<auto> info;
        try {
            hash<auto> hdr = getUpdateHeaders(id);
            rest.patch(url, set, \info, hdr);
        } catch (hash<ExceptionInfo> ex) {
            hash<auto> ex_arg = info{"request-uri", "response-code", "response-body"};
            throw ex.err, ex.desc, ex.arg + ex_arg;
        }
    }

    #! Template method: get headers for update operation (e.g., ETag for BC)
    private hash<auto> getUpdateHeaders(string id) {
        return {};
    }

    #! Deletes a single record
    private deleteSingleRecord(string id) {
        checkRecordInit();
        string url = getEntityUri(id);
        hash<auto> info;
        try {
            rest.del(url, NOTHING, \info);
        } catch (hash<ExceptionInfo> ex) {
            hash<auto> ex_arg = info{"request-uri", "response-code", "response-body"};
            throw ex.err, ex.desc, ex.arg + ex_arg;
        }
    }
}
}
