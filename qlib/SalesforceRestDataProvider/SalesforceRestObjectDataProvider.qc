# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SalesforceRestDataProvider module definition

/** SalesforceRestObjectDataProvider.qc Copyright 2019 - 2024 Qore Technologies, s.r.o.

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

#! contains all public definitions in the SalesforceRestDataProvider module
public namespace SalesforceRestDataProvider {
#! contains Salesforce object record information
hashdecl SalesforceRestRecordInfo {
    #! The record type for the object
    hash<string, AbstractDataField> record_type;

    #! The original field type names
    hash<string, string> field_types;

    #! Hash of date/time fields for this record
    hash<string, bool> date_time_fields;

    #! Hash of date fields for this record
    hash<string, bool> date_fields;

    #! Hash of time fields for this record
    hash<string, bool> time_fields;

    #! Fields that cannot be included when creating records
    hash<string, bool> no_create;

    #! Fields that cannot be included when updating records
    hash<string, bool> no_update;

    #! Fields that get a default value on create
    hash<string, bool> defaulted_on_create;
}

#! The SalesforceRestObjectDataProvider data provider class
public class SalesforceRestObjectDataProvider inherits SalesforceRestDataProviderBase {
    public {
        #! current object name
        string name;

        #! sobject description
        hash<auto> sobject;
    }

    private {
        #! The description of this object type
        hash<auto> desc;

        #! Record info for this object
        hash<SalesforceRestRecordInfo> record_info();
    }

    #! Creates the object from the arguments
    constructor(SalesforceRestClient rest, string name, hash<auto> sobject) : SalesforceRestDataProviderBase(rest) {
        self.name = name;
        self.sobject = sobject;
        getRecordInfoIntern();
    }

    #! Returns the data provider name
    string getName() {
        return name;
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("Salesforce data provider for URL `%s` and sobject `%s`; supports a table-like record-based "
            "interface", rest.getURL(), name);
    }

    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return record_info.record_type;
    }

    private *hash<string, AbstractDataField> getUpdateRecordTypeImpl(*hash<auto> search_options) {
        return record_info.record_type - keys record_info.no_update;
    }

    private *hash<string, AbstractDataField> getCreateRecordTypeImpl(*hash<auto> create_options) {
        *hash<string, QoreDataField> rv = record_info.record_type - keys record_info.no_create;
        map rv{$1}.updateFieldType(rv{$1}.getType().getOrNothingType()), keys record_info.defaulted_on_create,
            rv{$1} && rv{$1}.getType().isMandatory();
        return rv;
    }

    #! Returns an iterator for zero or more records matching the search options
    /** @param where_cond the search criteria
        @param search_options the search options after processing by validateSearchOptions()

        @see requestSearchRecordsImpl()
    */
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond, *hash<auto> search_options) {
        where_cond = fixSalesforceRecord(where_cond);
        return new SalesforceRestRecordIterator(rest, name, record_info, where_cond, search_options);
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        hash<DataProviderInfo> rv = SalesforceRestDataProvider::ProviderInfo - "constructor_options" + <DataProviderInfo>{
            "search_options": {
                "columns": <DataProviderOptionInfo>{
                    "display_name": "Columns",
                    "short_desc": "Columns to include in output",
                    "type": AbstractDataProviderTypeMap."any",
                    "desc": "column argument for the SOQL select expression",
                },
                "scope": <DataProviderOptionInfo>{
                    "display_name": "Scope",
                    "short_desc": "Query scope",
                    "type": AbstractDataProviderTypeMap."string",
                    "desc": "The scope to use for the query",
                },
                "limit": <DataProviderOptionInfo>{
                    "display_name": "Limit",
                    "short_desc": "The maximum number of records to return",
                    "type": AbstractDataProviderTypeMap."int",
                    "desc": "the maximum number of records to return",
                },
                "offset": <DataProviderOptionInfo>{
                    "display_name": "Offset",
                    "short_desc": "The offset in the record list for records to return",
                    "type": AbstractDataProviderTypeMap."int",
                    "desc": "The offset in the record list for records to return",
                },
                "groupby": <DataProviderOptionInfo>{
                    "display_name": "Group By",
                    "short_desc": "Field(s) to group results by",
                    "type": new QoreListDataType(new Type("softlist<auto>")),
                    "desc": "Field(s) to group results by in case there are aggregate expressions in the query; group "
                        "by argument for the SOQL select expression",
                },
                "having": <DataProviderOptionInfo>{
                    "display_name": "Having Clause",
                    "short_desc": "A 'having' clause for the select expression",
                    "type": AbstractDataProviderTypeMap."hash",
                    "desc": "A `having` clause for the SOQL select expression",
                },
                "orderby": <DataProviderOptionInfo>{
                    "display_name": "Order By",
                    "short_desc": "Fields to order the query result by",
                    "type": new QoreListDataType(new Type("softlist<string>")),
                    "desc": "Sets the order by argument for the SOQL select expression",
                },
                "orderby_desc": <DataProviderOptionInfo>{
                    "display_name": "Order By Descending",
                    "short_desc": "Sort in descending order",
                    "type": AbstractDataProviderTypeMap."bool",
                    "desc": "Sort in descending order",
                },
                "forview": <DataProviderOptionInfo>{
                    "display_name": "Use For View",
                    "short_desc": "Uses FOR VIEW with the query",
                    "type": AbstractDataProviderTypeMap."softbool",
                    "desc": "Uses `FOR VIEW` with the query to update the view date",
                },
                "forreference": <DataProviderOptionInfo>{
                    "display_name": "Use For Reference",
                    "short_desc": "Uses FOR REFERENCE with the query",
                    "type": AbstractDataProviderTypeMap."softbool",
                    "desc": "Uses `FOR REFERENCE` with the query to update the reference date",
                },
            },
            "expressions": AbstractDataProvider::GenericExpressions{
                DP_SEARCH_OP_EQ,
                DP_SEARCH_OP_LT,
                DP_SEARCH_OP_LE,
                DP_SEARCH_OP_GT,
                DP_SEARCH_OP_GE,
                DP_SEARCH_OP_IN,
                DP_SEARCH_OP_NOT,
            } + {
                SOQL_OP_LIKE: <DataProviderExpressionInfo>{
                    "type": DET_Operator,
                    "display_name": "like",
                    "name": SOQL_OP_LIKE,
                    "symbol": "like",
                    "desc": "The value to bind as the 'like' value (ex: '%some string%')",
                    "args": (DataProviderSignatureStringType, DataProviderSignatureStringValueType),
                    "return_type": AbstractDataProviderTypeMap."bool",
                },
            },
            "upsert_options": {
                "extidfield": <DataProviderOptionInfo>{
                    "display_name": "External ID Field",
                    "short_desc": "Sets the name of the external ID field",
                    "type": AbstractDataProviderTypeMap."string",
                    "desc": "Sets the name of the external ID field in the new record data",
                    "required": True,
                },
            },
        };
        if (record_info.record_type) {
            if (sobject.queryable) {
                rv.supports_read = True;
                rv.supports_native_search = True;
                rv.has_record = True;
            }
            if (sobject.createable) {
                rv.supports_create = True;
                rv.has_record = True;
            }
            if (sobject.mergeable) {
                rv.supports_upsert = True;
                rv.has_record = True;
            }
            if (sobject.deletable) {
                rv.supports_delete = True;
                rv.has_record = True;
            }
            if (sobject.updateable) {
                rv.supports_update = True;
                rv.has_record = True;
            }
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
        rec = fixSalesforceRecord(rec);
        string url = sprintf("sobjects/%s", name);
        hash<auto> info;
        # the id is returned in the "id" field in the response
        hash<auto> resp;
        try {
            resp = rest.post(url, rec, \info);
        } catch (hash<ExceptionInfo> ex) {
            # ensure that any error response body is included in the exception
            hash<auto> ex_arg = info{"request-uri", "response-code", "response-body"};
            # check for duplicate record error
            if (info."response-body"[0].errorCode == "DUPLICATE_VALUE"
                && (*string id = (info."response-body"[0].message =~ x/record with id: ([0-9a-zA-Z]+)/)[0])) {
                ex.desc = sprintf("%s: %s", ex.err, ex.desc);
                ex.err = "DUPLICATE-RECORD";
                ex_arg.id = id;
            }
            rethrow ex.err, ex.desc, ex_arg;
        }
        if (!resp.body.success) {
            throw "UPSERT-ERROR", sprintf("error creating record: %y", resp.errors);
        }
        return rec + {"id": resp.body.id};
    }

    #! Upserts the given record to the data provider
    /** @param rec a hash representing a single input record
        @param upsert_options the create options after processing by validateUpsertOptions()

        @return see @ref db_provider_upsert_results for possible values
    */
    private string upsertRecordImpl(hash<auto> rec, *hash<auto> upsert_options) {
        string extid = upsert_options.extidfield;
        # the external ID info must be removed from the request body
        *softstring extid_value = remove rec{extid};
        if (!extid_value.val()) {
            throw "UPSERT-ERROR", sprintf("upsert request requires a value for the %y field, but none was given",
            extid);
        }
        rec = fixSalesforceRecord(rec);
        string url = sprintf("sobjects/%s/%s/%s", name, extid, extid_value);
        hash<auto> info;
        # the id is returned in the "id" field in the response
        hash<auto> resp;
        try {
            resp = rest.patch(url, rec, \info);
        } catch (hash<ExceptionInfo> ex) {
            # ensure that any error response body is included in the exception
            hash<auto> ex_arg = info{"request-uri", "response-code", "response-body"};
            throw ex.err, ex.desc, ex_arg;
        }
        if (!resp.body.success) {
            throw "UPSERT-ERROR", sprintf("error upserting record with external ID %y = %y: %y", extid, rec{extid},
                resp.errors);
        }
        # status code 201 = inserted, 204 = updated
        return info."response-headers".status_code == 201
            ? UpsertResultInserted
            : UpsertResultUpdated;
    }

    #! Updates zero or more records matching the search options
    /** @param set the hash of field data to set
        @param where_cond a hash for identifying the record(s) to be updated
        @param search_options the update options after processing by validateSearchOptions()

        @return the number of records updated
    */
    private int updateRecordsImpl(hash<auto> set, hash<auto> where_cond, *hash<auto> search_options) {
        if (where_cond.Id) {
            updateSingleRecord(where_cond.Id, set);
            return 1;
        }
        where_cond = fixSalesforceRecord(where_cond);
        AbstractDataProviderRecordIterator i = searchRecords(where_cond, search_options);
        int count = 0;
        map (updateSingleRecord(i.getValue().Id, set), ++count), i;
        return count;
    }

    #! Deletes zero or more records
    /**
        @param where_cond a hash for identifying the record(s) to be deleted
        @param search_options the delete options after processing by validateSearchOptions()

        @return the number of records deleted
    */
    private int deleteRecordsImpl(*hash<auto> where_cond, *hash<auto> search_options) {
        if (where_cond.Id) {
            deleteSingleRecord(where_cond.Id);
            return 1;
        }
        where_cond = fixSalesforceRecord(where_cond);
        AbstractDataProviderRecordIterator i = searchRecords(where_cond, search_options);
        int count = 0;
        map (deleteSingleRecord(i.getValue().Id), ++count), i;
        return count;
    }

    #! updates a single record
    private updateSingleRecord(string id, hash<auto> set) {
        string url = sprintf("sobjects/%s/%s", name, id);
        hash<auto> info;
        # the id is returned in the "id" field in the response
        hash<auto> resp;
        try {
            resp = rest.patch(url, set, \info);
        } catch (hash<ExceptionInfo> ex) {
            # ensure that any error response body is included in the exception
            hash<auto> ex_arg = info{"request-uri", "response-code", "response-body"};
            throw ex.err, ex.desc, ex_arg;
        }
    }

    #! deletes a single record
    private deleteSingleRecord(string id) {
        string url = sprintf("sobjects/%s/%s", name, id);
        hash<auto> info;
        # the id is returned in the "id" field in the response
        hash<auto> resp;
        try {
            resp = rest.del(url, NOTHING, \info);
        } catch (hash<ExceptionInfo> ex) {
            # ensure that any error response body is included in the exception
            hash<auto> ex_arg = info{"request-uri", "response-code", "response-body"};
            throw ex.err, ex.desc, ex_arg;
        }
    }

    #! Retrieves the record type
    private getRecordInfoIntern() {
        desc = rest.get("sobjects/" + name + "/describe").body;
        foreach hash<auto> field in (desc.fields) {
            hash<auto> info += {
                "name": field.name,
                "display_name": field.label,
                "short_desc": field.label,
            };
            if (field.hasKey("createable") && !field.createable) {
                info.attr.createable = False;
                record_info.no_create{field.name} = True;
            }
            if (field.hasKey("updateable") && !field.updateable) {
                info.attr.updateable = False;
                record_info.no_update{field.name} = True;
            }
            if (field.defaultedOnCreate) {
                record_info.defaulted_on_create{field.name} = True;
            }
            hash<auto> opts += {
                "qore.no_null": True,
            };
            record_info.field_types{field.name} = field.type;
            switch (field.type) {
                case "string":
                case "id":
                case "textarea":
                case "email":
                case "reference":
                case "phone":
                case "url": {
                    if (field.length) {
                        opts."string.max_size_chars" = field.length;
                    }
                    info.type = AbstractDataProviderType::get(
                        field.nillable ? SoftStringOrNothingType : SoftStringType, opts
                    );
                    break;
                }

                case "picklist":
                    string typename = field.picklistValues[0].value.type();
                    if (field.nillable) {
                        typename = "*" + typename;
                    }
                    info.type = AbstractDataProviderType::get(typename, opts);
                    info.allowed_values = ();
                    foreach hash<auto> h in (field.picklistValues) {
                        if (!h.active) {
                            continue;
                        }
                        if (h.defaultValue) {
                            info.default_value = h.value;
                        }
                        info.allowed_values += <AllowedValueInfo>{
                            "display_name": h.label,
                            "value": h.value,
                        };
                    }
                    break;

                case "datetime":
                    info.type = AbstractDataProviderType::get(field.nillable ? SoftDateOrNothingType : SoftDateType,
                        opts);
                    record_info.date_time_fields{field.name} = True;
                    break;

                case "date":
                    info.type = AbstractDataProviderType::get(field.nillable ? SoftDateOrNothingType : SoftDateType,
                        opts);
                    record_info.date_fields{field.name} = True;
                    break;

                case "time":
                    info.type = AbstractDataProviderType::get(
                        field.nillable ? SoftStringOrNothingType : SoftStringType, opts
                    );
                    record_info.time_fields{field.name} = True;
                    break;

                case "int":
                    info.type = AbstractDataProviderType::get(field.nillable ? SoftIntOrNothingType : SoftIntType,
                        opts);
                    break;

                case "percent":
                case "currency":
                case "double":
                    info.type = AbstractDataProviderType::get(field.nillable ? SoftFloatOrNothingType : SoftFloatType,
                        opts);
                    break;

                case "boolean":
                    info.type = AbstractDataProviderType::get(field.nillable ? SoftBoolOrNothingType : SoftBoolType,
                        opts);
                    break;

                case "address":
                    info.type = new HashDataType(field.nillable ? AutoHashOrNothingType : AutoHashType, field.type,
                        opts);
                    break;

                default:
                    info.type = AbstractDataProviderType::get(AbstractDataProviderType::anyType, opts);
                    break;
            }
            record_info.record_type{field.name} = new QoreDataField(info);
        }
    }

    #! Fix salesforce records for serialization
    /** Ensure that:
        - \c DATE fields are serialized as YYYY-MM-DD strings
        - \c TIME fields are serialized as HH:mm:SS.xx strings
    */
    private *hash<auto> fixSalesforceRecord(*hash<auto> rec) {
        hash<auto> rv;
        foreach hash<auto> i in (rec.pairIterator()) {
            if (i.value.typeCode() == NT_DATE) {
                if (record_info.field_types{i.key} == "date") {
                    rv{i.key} = i.value.format("YYYY-MM-DD");
                } else if (record_info.field_types{i.key} == "time") {
                    rv{i.key} = i.value.format("HH:mm:SS.xxZ");
                }
            } else {
                rv{i.key} = i.value;
            }
        }
        return rv;
    }
}
}
