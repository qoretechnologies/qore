# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CdsRestDataProvider module definition

/** CdsEntityDataProvider.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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
#! contains Cds object record information
hashdecl CdsRestRecordInfo {
    #! The record type for the object
    hash<string, AbstractDataField> record_type;

    #! The original field type names
    hash<string, string> field_types;

    #! Hash of date/time fields for this record
    hash<string, bool> date_time_fields;

    #! Key field, if any
    string key;

    #! Entity set name
    string entity_set_name;
}

#! The CdsEntityDataProvider data provider class
public class CdsEntityDataProvider inherits CdsRestDataProviderBase {
    public {
        #! current object name
        string name;

        #! metadata description
        hash<auto> meta;

        #! Static provider info
        const ProviderInfo = CdsRestDataProvider::ProviderInfo - "constructor_options" + <DataProviderInfo>{
            "search_options": {
                "columns": <DataProviderOptionInfo>{
                    "display_name": "Columns",
                    "short_desc": "Columns to include in the output of the query",
                    "type": AbstractDataProviderTypeMap."auto",
                    "desc": "Columns to include in the output of the query; `select` argument for the query expression",
                },
                "limit": <DataProviderOptionInfo>{
                    "display_name": "Limit",
                    "short_desc": "The maximum number of records to return",
                    "type": AbstractDataProviderTypeMap."int",
                    "desc": "The maximum number of records to return; `top` argument for the query expression",
                },
                "groupby": <DataProviderOptionInfo>{
                    "display_name": "Group By",
                    "short_desc": "Field(s) to group results by",
                    "type": new QoreListDataType(new Type("softlist<auto>")),
                    "desc": "Field(s) to group results by in case there are aggregate expressions in the query; "
                        "`groupby` argument for the query expression",
                },
                "orderby": <DataProviderOptionInfo>{
                    "display_name": "Order By",
                    "short_desc": "Fields to order the query result by",
                    "type": new QoreListDataType(new Type("softlist<string>")),
                    "desc": "Fields to order the query result by; `orderby` argument for the query expression",
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
        hash<CdsRestRecordInfo> record_info();
    }

    #! Creates the object from the arguments
    constructor(CdsRestClient rest, string name, hash<auto> meta) : CdsRestDataProviderBase(rest) {
        self.name = name;
        self.meta = meta;
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
        return new CdsRestRecordIterator(rest, name, record_info, where_cond, search_options);
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        hash<DataProviderInfo> rv = ProviderInfo;
        if (meta.Key.PropertyRef."^attributes^".Name) {
            rv += {
                "has_record": True,
                "supports_read": True,
                "supports_native_search": True,
                "supports_update": True,
                "supports_upsert": True,
                "supports_delete": True,
                "supports_create": True,
            };
        }
        return rv;
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
        string url = record_info.entity_set_name;
        hash<auto> info;
        # the id is returned in the "id" field in the response
        hash<auto> resp;
        try {
            resp = rest.post(url, rec, \info);
        } catch (hash<ExceptionInfo> ex) {
            # ensure that any error response body is included in the exception
            hash<auto> ex_arg = info{"request-uri", "response-code", "response-body"};
            # check for duplicate record error
            if (rec{record_info.key} && ex.arg.decoded_body.error."code" == "0x80040237") {
                ex.desc = sprintf("%s: %s", ex.err, ex.desc);
                ex.err = "DUPLICATE-RECORD";
                ex_arg.id = {record_info.key: rec{record_info.key}};
            }
            throw ex.err, ex.desc, ex.arg + ex_arg;
        }
        *string new_key = (info."response-headers"."odata-entityid" =~ x/\(([-0-9a-f]+)\)$/)[0];
        if (!new_key) {
            throw "CDS_ENTITY-CREATE-ERROR", sprintf("failed to find new entity ID in OData-EntityId header: %y",
                info."response-headers"."odata-entityid");
        }
        return rec + {record_info.key: new_key};
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

        string url = sprintf("%s(%s)", record_info.entity_set_name, id_value);
        hash<auto> info;
        hash<auto> resp;
        try {
            resp = rest.patch(url, rec, \info, {"Prefer": "return=representation"});
        } catch (hash<ExceptionInfo> ex) {
            # ensure that any error response body is included in the exception
            hash<auto> ex_arg = info{"request-uri", "response-code", "response-body"};
            throw ex.err, ex.desc, ex.arg + ex_arg;
        }
        # check creation time of record to see if we created or updated
        if ((*string createdon = resp.body.createdon) && date(createdon) < now) {
            return UpsertResultUpdated;
        }
        return UpsertResultInserted;
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

    #! updates a single record
    private updateSingleRecord(string id, hash<auto> set) {
        checkRecord();
        string url = sprintf("%s(%s)", record_info.entity_set_name, id);
        hash<auto> info;
        hash<auto> resp;
        try {
            resp = rest.patch(url, set, \info);
        } catch (hash<ExceptionInfo> ex) {
            # ensure that any error response body is included in the exception
            hash<auto> ex_arg = info{"request-uri", "response-code", "response-body"};
            throw ex.err, ex.desc, ex.arg + ex_arg;
        }
    }

    #! deletes a single record
    private deleteSingleRecord(string id) {
        checkRecord();
        string url = sprintf("%s(%s)", record_info.entity_set_name, id);
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
        *string key = meta.Key.PropertyRef."^attributes^".Name;
        if (!key) {
            throw "CDS-ENTITY-ERROR", sprintf("entity %y has no key property (%y)", name, meta.Key);
        }
        record_info.key = key;
        # get entity set name
        setEntitySetName();
        foreach hash<auto> field in (meta.Property) {
            AbstractDataProviderType type;
            hash<auto> opts += {
                "qore.no_null": True,
            };
            hash<auto> attr = field."^attributes^";
            string cdstype;
            switch (attr.Type) {
                case "Edm.String":
                    type = AbstractDataProviderType::get(SoftStringOrNothingType, opts);
                    break;

                case "Edm.Guid":
                    type = AbstractDataProviderType::get(SoftStringOrNothingType, opts);
                    cdstype = "guid";
                    break;

                case "Edm.Int32":
                case "Edm.Int64":
                    type = AbstractDataProviderType::get(SoftIntOrNothingType, opts);
                    break;

                case "Edm.Double":
                case "Edm.Decimal":
                    type = AbstractDataProviderType::get(SoftFloatOrNothingType, opts);
                    break;

                case "Edm.Boolean":
                    type = AbstractDataProviderType::get(SoftBoolOrNothingType, opts);
                    break;

                case "Edm.Binary":
                    type = AbstractDataProviderType::get(SoftBinaryOrNothingType, opts);
                    break;

                case "Edm.DateTimeOffset":
                    record_info.date_time_fields{attr.Name} = True;
                    type = AbstractDataProviderType::get(SoftDateOrNothingType, opts);
                    break;

                default:
                    type = AbstractDataProviderType::get(AbstractDataProviderType::anyType, opts);
                    break;
            }
            # get description
            *string desc;
            foreach hash<auto> i in (field.Annotation) {
                if (i."^attributes^".Term == "Org.OData.Core.V1.Description") {
                    desc = i."^attributes^".String;
                    break;
                }
            }
            record_info.field_types{attr.Name} = cdstype ?? type.getBaseTypeName();
            record_info.record_type{attr.Name} = new QoreDataField(attr.Name, desc, type);
        }
    }

    #! Determine the entity set name for the entity
    private setEntitySetName() {
        hash<auto> info;
        try {
            string uri = sprintf("EntityDefinitions(LogicalName='%s')?$select=EntitySetName", name);
            hash<auto> resp = rest.get(uri, NOTHING, \info);
            *string entity_set_name;
            if (!(entity_set_name = resp.body.EntitySetName)) {
                throw "CDS-ENTITY-SET-ERROR", sprintf("missing entity set name for %y in %y (query %y)", name, resp.body,
                    uri);
            }
            record_info.entity_set_name = entity_set_name;
            return;
        } catch (hash<ExceptionInfo> ex) {
            if (info."response-headers".status_code != 403) {
                rethrow ex.err, ex.desc, info{"request-uri", "response-uri"};
            }
        }
        # if we get a 403 Unauthorized response, then we determine the entity set name with a heuristic
        record_info.entity_set_name = name;
        if (name[-1] == "y") {
            splice record_info.entity_set_name, -1, 1, "ies";
        } else {
            record_info.entity_set_name += "s";
        }
    }
}
}
