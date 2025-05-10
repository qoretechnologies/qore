# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ServiceNowRestDataProvider module definition

/** ServiceNowTableDataProvider.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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

#! contains all public definitions in the ServiceNowRestDataProvider module
public namespace ServiceNowRestDataProvider {
#! contains ServiceNow object record information
hashdecl ServiceNowRestRecordInfo {
    #! The record type for the object
    hash<string, AbstractDataField> record_type;

    #! The original field type names
    hash<string, string> field_types;

    #! Hash of date/time fields for this record
    hash<string, bool> date_time_fields;

    #! Hash of boolean fields for this record
    hash<string, bool> bool_fields;

    #! Hash of integer fields for this record
    hash<string, bool> int_fields;
}

#! The ServiceNowTableDataProvider data provider class
public class ServiceNowTableDataProvider inherits ServiceNowRestDataProviderBase {
    public {
        #! Current object name
        string name;

        #! Access description
        hash<auto> access;

        #! Metadata description
        hash<auto> meta;

        #! Provider info
        const ProviderInfo = ServiceNowRestDataProvider::ProviderInfo - "constructor_options"
                + <DataProviderInfo>{
            "supports_children": False,
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
                    "desc": "The maximum number of records to return",
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
                    "desc": "Field(s) to group results by in case there are aggregate expressions in the query; the "
                        "`GROUPBY` argument for the query expression",
                },
                "orderby": <DataProviderOptionInfo>{
                    "display_name": "Order By",
                    "short_desc": "Fields to order the query result by",
                    "type": new QoreListDataType(new Type("softlist<string>")),
                    "desc": "Fields to order the query result by; the `ORDERBY` argument for the query expression",
                },
                "orderbydesc": <DataProviderOptionInfo>{
                    "display_name": "Order By Descending",
                    "short_desc": "Fields to order the query result in descending order by",
                    "type": new QoreListDataType(new Type("softlist<string>")),
                    "desc": "Fields to order the query result in descending order by; the `ORDERBYDESC` argument for "
                        "the query expression",
                },
            },
        };

        #! System columns
        const SysColumns = {
            "sys_created_by": {
                "internal_type": {
                    "value": "string",
                }
            },
            "sys_created_on": {
                "internal_type": {
                    "value": "glide_date_time",
                }
            },
            "sys_updated_by": {
                "internal_type": {
                    "value": "string",
                }
            },
            "sys_updated_on": {
                "internal_type": {
                    "value": "glide_date_time",
                }
            },
        };

        #! Supported expressions
        const Expressions = {
            DP_OP_AND: {
                "exp": AbstractDataProvider::GenericExpressions{DP_OP_AND},
                "impl": string sub (*string ignored, auto arg) {
                    throw "WHERE-ERROR", "ServiceNow does not support nested boolean logic in queries";
                },
            },
            QUERY_OP_EQ: {
                "exp": sym(AbstractDataProvider::GenericExpressions{DP_SEARCH_OP_EQ}, QUERY_OP_EQ),
                "impl": string sub (string cn, auto arg) {
                    return sprintf("%s=%s", cn, arg);
                },
            },
            QUERY_OP_NE: {
                "exp": sym(AbstractDataProvider::GenericExpressions{DP_SEARCH_OP_NE}, QUERY_OP_NE),
                "impl": string sub (string cn, auto arg) {
                    return sprintf("%s!=%s", cn, arg);
                },
            },
            QUERY_OP_LT: {
                "exp": sym(AbstractDataProvider::GenericExpressions{DP_SEARCH_OP_LT}, QUERY_OP_LT),
                "impl": string sub (string cn, auto arg) {
                    return sprintf("%s<%s", cn, arg);
                },
            },
            QUERY_OP_LE: {
                "exp": sym(AbstractDataProvider::GenericExpressions{DP_SEARCH_OP_LE}, QUERY_OP_LE),
                "impl": string sub (string cn, auto arg) {
                    return sprintf("%s<=%s", cn, arg);
                },
            },
            QUERY_OP_GT: {
                "exp": sym(AbstractDataProvider::GenericExpressions{DP_SEARCH_OP_GT}, QUERY_OP_GT),
                "impl": string sub (string cn, auto arg) {
                    return sprintf("%s>%s", cn, arg);
                },
            },
            QUERY_OP_GE: {
                "exp": sym(AbstractDataProvider::GenericExpressions{DP_SEARCH_OP_GE}, QUERY_OP_GE),
                "impl": string sub (string cn, auto arg) {
                    return sprintf("%s>=%s", cn, arg);
                },
            },
            QUERY_OP_IN: {
                "exp": sym(AbstractDataProvider::GenericExpressions{DP_SEARCH_OP_IN}, QUERY_OP_IN),
                "impl": string sub (string cn, auto arg) {
                    return sprintf("%sIN%s", cn, (foldl $1 + "," + $2, arg));
                },
            },
            QUERY_OP_NOTIN: {
                "exp": <DataProviderExpressionInfo>{
                    "type": DET_Operator,
                    "name": QUERY_OP_NOTIN,
                    "display_name": "notin",
                    "short_desc": "An expression with the ServiceNow 'notin' operator",
                    "desc": "An expression with the ServiceNow 'notin' operator",
                    "symbol": "NOTIN",
                    "args": (DataProviderSignatureFieldType, DataProviderSignatureStringValueType),
                    "return_type": AbstractDataProviderTypeMap."bool",
                },
                "impl": string sub (string cn, auto arg) {
                    return sprintf("%sNOTIN%s", cn, (foldl $1 + "," + $2, arg));
                },
            },
            QUERY_OP_LIKE: {
                "exp": <DataProviderExpressionInfo>{
                    "type": DET_Operator,
                    "name": QUERY_OP_LIKE,
                    "display_name": "like",
                    "short_desc": "An expression with the ServiceNow 'like' operator",
                    "desc": "An expression with the ServiceNow 'like' operator",
                    "symbol": "LIKE",
                    "args": (DataProviderSignatureFieldType, DataProviderSignatureStringValueType),
                    "return_type": AbstractDataProviderTypeMap."bool",
                },
                "impl": string sub (string cn, auto arg) {
                    return sprintf("%sLIKE%s", cn, arg);
                },
            },
            QUERY_OP_NOTLIKE: {
                "exp": <DataProviderExpressionInfo>{
                    "type": DET_Operator,
                    "name": QUERY_OP_NOTLIKE,
                    "display_name": "notlike",
                    "short_desc": "An expression with the ServiceNow 'notlike' operator",
                    "desc": "An expression with the ServiceNow 'notlike' operator",
                    "symbol": "NOTLIKE",
                    "args": (DataProviderSignatureFieldType, DataProviderSignatureStringValueType),
                    "return_type": AbstractDataProviderTypeMap."bool",
                },
                "impl": string sub (string cn, auto arg) {
                    return sprintf("%sNOTLIKE%s", cn, arg);
                },
            },
            QUERY_OP_STARTSWITH: {
                "exp": <DataProviderExpressionInfo>{
                    "type": DET_Operator,
                    "name": QUERY_OP_STARTSWITH,
                    "display_name": "like",
                    "short_desc": "An expression with the ServiceNow 'startswith' operator",
                    "desc": "An expression with the ServiceNow 'startswith' operator",
                    "symbol": "STARTSWITH",
                    "args": (DataProviderSignatureFieldType, DataProviderSignatureStringValueType),
                    "return_type": AbstractDataProviderTypeMap."bool",
                },
                "impl": string sub (string cn, auto arg) {
                    return sprintf("%sSTARTSWITH%s", cn, arg);
                },
            },
            QUERY_OP_ENDSWITH: {
                "exp": <DataProviderExpressionInfo>{
                    "type": DET_Operator,
                    "name": QUERY_OP_ENDSWITH,
                    "display_name": "like",
                    "short_desc": "An expression with the ServiceNow 'endswith' operator",
                    "desc": "An expression with the ServiceNow 'endswith' operator",
                    "symbol": "ENDSWITH",
                    "args": (DataProviderSignatureFieldType, DataProviderSignatureStringValueType),
                    "return_type": AbstractDataProviderTypeMap."bool",
                },
                "impl": string sub (string cn, auto arg) {
                    return sprintf("%sISEMPTY", cn);
                },
            },
            QUERY_OP_ISNOTEMPTY: {
                "exp": <DataProviderExpressionInfo>{
                    "type": DET_Operator,
                    "name": QUERY_OP_ISNOTEMPTY,
                    "display_name": "notlike",
                    "short_desc": "An expression with the ServiceNow 'isnotempty' operator",
                    "desc": "An expression with the ServiceNow 'isnotempty' operator",
                    "symbol": "ISNOTEMPTY",
                    "args": (DataProviderSignatureFieldType,),
                    "return_type": AbstractDataProviderTypeMap."bool",
                },
                "impl": string sub (string cn, auto arg) {
                    return sprintf("%sISNOTEMPTY", cn);
                },
            },
        };
    }

    private {
        #! Record info for the table
        hash<ServiceNowRestRecordInfo> record_info();

        #! URI path prefix
        string uri_path = "table/";
    }

    #! Creates the object from the arguments
    constructor(ServiceNowRestClient rest, string name, hash<auto> access, hash<auto> meta)
            : ServiceNowRestDataProviderBase(rest){
        self.name = name;
        self.access = access;
        self.meta = SysColumns + meta;
        uri_path += name;
        getRecordInfoIntern();
    }

    #! Returns the data provider name
    string getName() {
        return name;
    }

    #! Returns the data provider description
    string getDesc() {
        return sprintf("ServiceNow table %y", name);
    }

    private *hash<string, DataProvider::AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return record_info.record_type;
    }

    #! Returns an iterator for zero or more records matching the search options
    /** @param where_cond the search criteria
        @param search_options the search options after processing by validateSearchOptions()

        @see requestSearchRecordsImpl()
    */
    private DataProvider::AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
       return new ServiceNowRestRecordIterator(rest, name, record_info, where_cond, search_options);
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        hash<DataProviderInfo> rv = ProviderInfo;
        if (record_info.record_type) {
            rv += {
                "has_record": True,
                "supports_read": access.read_access,
                "supports_native_search": True,
                "supports_update": access.update_access,
                # NOTE delete_access seems to always be False, while records can still be deleted
                "supports_delete": access.create_access,
                "supports_create": access.create_access,
                "supports_search_expressions": True,
                "expressions": (map {$1.key: $1.value.exp}, Expressions.pairIterator()),
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
        hash<auto> info;
        # the id is returned in the "id" field in the response
        hash<auto> resp;
        try {
            resp = rest.post(uri_path, rec, \info);
        } catch (hash<ExceptionInfo> ex) {
            # ensure that any error response body is included in the exception
            hash<auto> ex_arg = info{"request-uri", "response-code", "response-body"};
            throw ex.err, ex.desc, ex.arg + ex_arg;
        }

        *string new_key = resp.body.response.result.sys_id;
        if (!new_key) {
            throw "SERVICENOW-RECORD-CREATE-ERROR", sprintf("failed to find new row ID in response: %y",
                resp.body.response.result);
        }
        return resp.body.response.result;
    }

    #! Updates zero or more records matching the search options
    /** @param set the hash of field data to set
        @param where_cond a hash for identifying the record(s) to be updated
        @param search_options the update options after processing by validateSearchOptions()

        @return the number of records updated
    */
    private int updateRecordsImpl(hash<auto> set, hash<DataProviderExpression> where_cond,
            *hash<auto> search_options) {
        /*
        if (where_cond.sys_id && where_cond.size() == 1) {
            updateSingleRecord(where_cond.sys_id, set);
            return 1;
        }
        */

        AbstractDataProviderRecordIterator i = searchRecords(where_cond, search_options + {"columns": "sys_id"});
        int count = 0;
        map (updateSingleRecord(i.getValue().sys_id, set), ++count), i;
        return count;
    }

    #! Deletes zero or more records
    /**
        @param where_cond a hash for identifying the record(s) to be deleted
        @param search_options the delete options after processing by validateSearchOptions()

        @return the number of records deleted
    */
    private int deleteRecordsImpl(*hash<DataProviderExpression> where_cond, *hash<auto> search_options) {
        /*
        if (where_cond.sys_id && where_cond.size() == 1) {
            deleteSingleRecord(where_cond.sys_id);
            return 1;
        }
        */

        AbstractDataProviderRecordIterator i = searchRecords(where_cond, search_options + {"columns": "sys_id"});
        int count = 0;
        map (deleteSingleRecord(i.getValue().sys_id), ++count), i;
        return count;
    }

    #! updates a single record
    private updateSingleRecord(string id, hash<auto> set) {
        string url = sprintf("%s/%s", uri_path, id);
        hash<auto> info;
        hash<auto> resp;
        try {
            resp = rest.patch(url, set, \info);
            # the actual values set on the server (which might be different from those requested):
            #     resp.body.response.result{keys set};
            # current cannot be used, so we ignore the response
        } catch (hash<ExceptionInfo> ex) {
            # ensure that any error response body is included in the exception
            hash<auto> ex_arg = info{"request-uri", "response-code", "response-body"};
            throw ex.err, ex.desc, ex.arg + ex_arg;
        }
    }

    #! deletes a single record
    private deleteSingleRecord(string id) {
        string url = sprintf("%s/%s", uri_path, id);
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
        foreach hash<auto> i in (meta.pairIterator()) {
            AbstractDataProviderType type;
            hash<auto> opts += {
                "qore.no_null": True,
            };
            switch (i.value.internal_type.value) {
                case "integer":
                    record_info.int_fields{i.key} = True;
                    type = i.value.mandatory == "true"
                        ? AbstractDataProviderType::get(SoftIntType, opts)
                        : AbstractDataProviderType::get(SoftIntOrNothingType, opts);
                    break;

                case "string":
                case "GUID":
                    if (i.value.max_length) {
                        opts."string.max_size_chars" = i.value.max_length.toInt();
                    }
                    type = i.value.mandatory == "true"
                        ? AbstractDataProviderType::get(SoftStringType, opts)
                        : AbstractDataProviderType::get(SoftStringOrNothingType, opts);
                    break;

                case "glide_date":
                case "glide_date_time":
                    record_info.date_time_fields{i.key} = True;
                    type = i.value.mandatory == "true"
                        ? AbstractDataProviderType::get(SoftDateType, opts)
                        : AbstractDataProviderType::get(SoftDateOrNothingType, opts);
                    break;

                case "boolean":
                    record_info.bool_fields{i.key} = True;
                    type = i.value.mandatory == "true"
                        ? AbstractDataProviderType::get(SoftBoolType, opts)
                        : AbstractDataProviderType::get(SoftBoolOrNothingType, opts);
                    break;

                default:
                    type = AbstractDataProviderType::get(AbstractDataProviderType::anyType, opts);
                    break;
            }
            # get description
            *string desc = i.value.column_label;
            record_info.field_types{i.key} = type.getBaseTypeName();
            record_info.record_type{i.key} = new QoreDataField(i.key, desc, type);
        }
    }

    #! Returns an equality comparison expression
    private hash<DataProviderExpression> getEqualityComparisonExpression(int role, hash<DataProviderInfo> info, string key,
            auto value) {
        return <DataProviderExpression>{
            "exp": info.expressions.eq.name,
            "args": (<DataProviderFieldReference>{"field": key},) + value,
        };
    }
}
}

namespace Priv {
hash<DataProviderExpressionInfo> sub sym(hash<DataProviderExpressionInfo> exp, string name) {
    exp.name = name;
    if (exp.args.size() > 1 && exp.args[0] == DataProviderSignatureAnyType) {
        exp.args[0] = DataProviderSignatureFieldType;
    }
    return exp;
}
}