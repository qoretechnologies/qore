# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore BcEntityDataProvider class definition

/** BcEntityDataProvider.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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
#! The Business Central entity data provider class
/** Provides full CRUD operations for a Business Central entity
*/
public class BcEntityDataProvider inherits BcRestDataProviderBase {
    public {
        #! Current entity name
        string name;

        #! Company ID
        string companyId;

        #! Static provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "BcEntityDataProvider",
            "supports_children": False,
            "has_record": True,
            "supports_read": True,
            "supports_native_search": True,
            "supports_update": True,
            "supports_upsert": True,
            "supports_delete": True,
            "supports_create": True,
            "search_options": GenericRecordSearchOptions{"requires_result", "max_records"} + {
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
            },
        };
    }

    private {
        #! Record info for the entity
        hash<BcRecordInfo> record_info();
    }

    #! Creates the object from the arguments
    constructor(CdsRestClient::BusinessCentralRestClient rest, string companyId, string name)
            : BcRestDataProviderBase(rest) {
        self.companyId = companyId;
        self.name = name;
        record_info.entity_name = name;
    }

    #! Returns the data provider name
    string getName() {
        return name;
    }

    private *hash<string, DataProvider::AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        checkRecord();
        return record_info.record_type;
    }

    private checkRecord() {
        if (!record_info.record_type) {
            getRecordInfoIntern();
        }
    }

    #! Returns an iterator for zero or more records matching the search options
    /** @param where_cond the search criteria
        @param search_options the search options after processing by validateSearchOptions()

        @see requestSearchRecordsImpl()
    */
    private DataProvider::AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        checkRecord();
        return new BcRecordIterator(rest, companyId, name, record_info, where_cond, search_options);
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Creates the given record to the data provider
    /** @param rec a hash representing a single input record
        @param create_options the create options after processing by validateCreateOptions()

        @return the data written to the data provider with the \c "id" field of the new record

        @throw DUPLICATE-RECORD this exception should be thrown if the provider fails due to an attempt to create a
        duplicate record
    */
    private *hash<auto> createRecordImpl(hash<auto> rec, *hash<auto> create_options) {
        checkRecord();
        string url = sprintf("companies(%s)/%s", companyId, name);
        hash<auto> info;
        hash<auto> resp;
        try {
            resp = rest.post(url, rec, \info);
        } catch (hash<ExceptionInfo> ex) {
            # ensure that any error response body is included in the exception
            hash<auto> ex_arg = info{"request-uri", "response-code", "response-body"};
            # check for duplicate record error (Business Central uses different error codes)
            if (rec{record_info.key} && (ex.err == "BadRequest" || ex.err =~ /duplicate/i)) {
                ex.desc = sprintf("%s: %s", ex.err, ex.desc);
                ex.err = "DUPLICATE-RECORD";
                ex_arg.id = {record_info.key: rec{record_info.key}};
            }
            throw ex.err, ex.desc, ex.arg + ex_arg;
        }
        # Return the created record with its ID
        return resp.body ?? rec;
    }

    #! Upserts the given record to the data provider
    /** @param rec a hash representing a single input record
        @param upsert_options the create options after processing by validateUpsertOptions()

        @return see @ref db_provider_upsert_results for possible values
    */
    private string upsertRecordImpl(hash<auto> rec, *hash<auto> upsert_options) {
        checkRecord();
        *softstring id_value = remove rec{record_info.key};
        if (!id_value.val()) {
            throw "UPSERT-ERROR", sprintf("upsert request requires a value for the %y key field, but none was given",
                record_info.key);
        }
        date now = now_us();

        string url = sprintf("companies(%s)/%s(%s)", companyId, name, id_value);
        hash<auto> info;
        hash<auto> resp;
        try {
            # First try to get the existing record to get its ETag
            *string etag = getRecordEtag(id_value);
            hash<auto> hdr;
            if (etag) {
                hdr."If-Match" = etag;
            }
            resp = rest.patch(url, rec, \info, hdr + {"Prefer": "return=representation"});
        } catch (hash<ExceptionInfo> ex) {
            # If record doesn't exist (404), try to create it
            if (info."response-code" == 404) {
                rec{record_info.key} = id_value;
                createRecordImpl(rec);
                return UpsertResultInserted;
            }
            # ensure that any error response body is included in the exception
            hash<auto> ex_arg = info{"request-uri", "response-code", "response-body"};
            throw ex.err, ex.desc, ex.arg + ex_arg;
        }

        # Check if we updated or inserted based on response
        if (resp.body.systemCreatedAt) {
            date created = date(resp.body.systemCreatedAt);
            if (created < now) {
                return UpsertResultUpdated;
            }
        }
        return UpsertResultUpdated;
    }

    #! Updates zero or more records matching the search options
    /** @param set the hash of field data to set
        @param where_cond a hash for identifying the record(s) to be updated
        @param search_options the update options after processing by validateSearchOptions()

        @return the number of records updated
    */
    private int updateRecordsImpl(hash<auto> set, hash<auto> where_cond, *hash<auto> search_options) {
        checkRecord();
        if (where_cond{record_info.key} && where_cond.size() == 1) {
            updateSingleRecord(where_cond{record_info.key}, set);
            return 1;
        }
        AbstractDataProviderRecordIterator i = searchRecords(where_cond, search_options);
        int count = 0;
        map (updateSingleRecord(i.getValue(){record_info.key}, set), ++count), i;
        return count;
    }

    #! Deletes zero or more records
    /**
        @param where_cond a hash for identifying the record(s) to be deleted
        @param search_options the delete options after processing by validateSearchOptions()

        @return the number of records deleted
    */
    private int deleteRecordsImpl(*hash<auto> where_cond, *hash<auto> search_options) {
        checkRecord();
        if (where_cond{record_info.key} && where_cond.size() == 1) {
            deleteSingleRecord(where_cond{record_info.key});
            return 1;
        }
        AbstractDataProviderRecordIterator i = searchRecords(where_cond, search_options);
        int count = 0;
        map (deleteSingleRecord(i.getValue(){record_info.key}), ++count), i;
        return count;
    }

    #! Gets the ETag for a record
    private *string getRecordEtag(string id) {
        string url = sprintf("companies(%s)/%s(%s)", companyId, name, id);
        hash<auto> info;
        try {
            hash<auto> resp = rest.get(url, NOTHING, \info);
            return resp.body."@odata.etag";
        } catch (hash<ExceptionInfo> ex) {
            # If record doesn't exist, return nothing
            if (info."response-code" == 404) {
                return NOTHING;
            }
            rethrow;
        }
    }

    #! Updates a single record
    private updateSingleRecord(string id, hash<auto> set) {
        checkRecord();
        string url = sprintf("companies(%s)/%s(%s)", companyId, name, id);
        hash<auto> info;
        hash<auto> resp;
        try {
            # Get ETag for optimistic concurrency
            *string etag = getRecordEtag(id);
            hash<auto> hdr;
            if (etag) {
                hdr."If-Match" = etag;
            }
            resp = rest.patch(url, set, \info, hdr);
        } catch (hash<ExceptionInfo> ex) {
            # ensure that any error response body is included in the exception
            hash<auto> ex_arg = info{"request-uri", "response-code", "response-body"};
            throw ex.err, ex.desc, ex.arg + ex_arg;
        }
    }

    #! Deletes a single record
    private deleteSingleRecord(string id) {
        checkRecord();
        string url = sprintf("companies(%s)/%s(%s)", companyId, name, id);
        hash<auto> info;
        hash<auto> resp;
        try {
            resp = rest.del(url, NOTHING, \info);
        } catch (hash<ExceptionInfo> ex) {
            # ensure that any error response body is included in the exception
            hash<auto> ex_arg = info{"request-uri", "response-code", "response-body"};
            throw ex.err, ex.desc, ex.arg + ex_arg;
        }
    }

    #! Retrieves the record type
    private getRecordInfoIntern() {
        # Business Central uses "id" as the primary key for most entities
        record_info.key = "id";

        # Try to get schema from a sample record or $metadata
        hash<auto> info;
        try {
            # Try to get a single record to infer schema
            string uri = sprintf("companies(%s)/%s?$top=1", companyId, name);
            hash<auto> resp = rest.get(uri, NOTHING, \info);
            if (resp.body.value && resp.body.value.size() > 0) {
                inferRecordTypeFromSample(resp.body.value[0]);
                return;
            }
        } catch (hash<ExceptionInfo> ex) {
            # If we can't get schema, use a basic type
        }

        # Fallback: use generic auto type
        record_info.record_type = {
            "id": new QoreDataField("id", "Record ID", AbstractDataProviderType::get(SoftStringOrNothingType)),
        };
        record_info.field_types.id = "guid";
    }

    #! Infers record type from a sample record
    private inferRecordTypeFromSample(hash<auto> sample) {
        foreach hash<auto> i in (sample.pairIterator()) {
            # Skip OData metadata fields
            if (i.key =~ /^@/) {
                continue;
            }

            AbstractDataProviderType type;
            hash<auto> opts = {"qore.no_null": True};
            string fieldType;

            switch (i.value.typeCode()) {
                case NT_STRING:
                    # Check if it looks like a GUID
                    if (i.value =~ /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i) {
                        type = AbstractDataProviderType::get(SoftStringOrNothingType, opts);
                        fieldType = "guid";
                    } else if (i.value =~ /^\d{4}-\d{2}-\d{2}T/) {
                        # ISO date/time format
                        type = AbstractDataProviderType::get(SoftDateOrNothingType, opts);
                        fieldType = "datetime";
                        record_info.date_time_fields{i.key} = True;
                    } else {
                        type = AbstractDataProviderType::get(SoftStringOrNothingType, opts);
                        fieldType = "string";
                    }
                    break;

                case NT_INT:
                    type = AbstractDataProviderType::get(SoftIntOrNothingType, opts);
                    fieldType = "int";
                    break;

                case NT_FLOAT:
                    type = AbstractDataProviderType::get(SoftFloatOrNothingType, opts);
                    fieldType = "double";
                    break;

                case NT_NUMBER:
                    type = AbstractDataProviderType::get(SoftFloatOrNothingType, opts);
                    fieldType = "decimal";
                    break;

                case NT_BOOLEAN:
                    type = AbstractDataProviderType::get(SoftBoolOrNothingType, opts);
                    fieldType = "bool";
                    break;

                case NT_DATE:
                    type = AbstractDataProviderType::get(SoftDateOrNothingType, opts);
                    fieldType = "datetime";
                    record_info.date_time_fields{i.key} = True;
                    break;

                case NT_NULL:
                case NT_NOTHING:
                    type = AbstractDataProviderType::get(AbstractDataProviderType::anyType, opts);
                    fieldType = "auto";
                    break;

                default:
                    type = AbstractDataProviderType::get(AbstractDataProviderType::anyType, opts);
                    fieldType = "auto";
                    break;
            }

            record_info.field_types{i.key} = fieldType;
            record_info.record_type{i.key} = new QoreDataField(i.key, NOTHING, type);
        }
    }
}
}
