# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ElasticSearchDataProvider module definition

/** ElasticSearchIndexesDataProvider.qc Copyright 2023 - 2024 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ElasticSearchDataProvider module
public namespace ElasticSearchDataProvider {
#! This class exposes ElasticSearch indices as record-based data providers
/** The record type is made up of the document attributes plus the following read-only fields:
    - \c _id: the document ID which functions as the data provider record ID
    - \c _score: the score returned from searches

    Note that "where hashes" for updates and deletes accept a single field, \c _id, to identify the single document
    (data provider record) to be updated or deleted.

    Searches are performed with \c match logic; for more flexible searching, use the index search API data provider
    found at \c index/search
*/
public class ElasticSearchIndexTableDataProvider inherits ElasticSearchDataProviderBase {
    public {
        #! Current index name
        string name;

        #! Index hash
        hash<auto> index;

        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "desc": "ElasticSearch index table data provider; provdes table-like access to an ElasticSearch index; "
                "fields returned in searches are document properties, plus the special `_id` (document ID) and "
                "`_score` (query match score) values, which are read-only.\n\n"
                "Updating and deleting can be performed, but the 'where clause hash` must contain only a single "
                "`_id` key to identify the document by its ID.\n\n"
                "Searches are performed with `match`; for more flexible searching, use the index search API data "
                "provider, `index/search`",
            "type": "ElasticSearchIndexTableDataProvider",
            "constructor_options": ElasticSearchDataProvider::ConstructorOptions,
            "has_record": True,
            "supports_read": True,
            "supports_native_search": True,
            "supports_update": True,
            "supports_delete": True,
            "supports_create": True,
            "supports_bulk_create": True,
            "supports_bulk_upsert": True,
            "create_options": {
                "if_seq_no": <DataProviderOptionInfo>{
                    "display_name": "Only If Sequence",
                    "short_desc": "Only perform the operation if the document has this sequence number",
                    "type": AbstractDataProviderTypeMap."int",
                    "desc": "Only perform the operation if the document has this sequence number",
                },
                "if_primary_term": <DataProviderOptionInfo>{
                    "display_name": "Only If Primary Term",
                    "short_desc": "Only perform the operation if the document has this primary term",
                    "type": AbstractDataProviderTypeMap."int",
                    "desc": "Only perform the operation if the document has this primary term",
                },
                "op_type": <DataProviderOptionInfo>{
                    "display_name": "Create Operation Type",
                    "short_desc": "Create or index the document",
                    "type": AbstractDataProviderTypeMap."string",
                    "desc": "Set to create or only index the document if it does not already exist (put if absent). If a "
                        "document with the specified `_id` already exists, the indexing operation will fail. Same as "
                        "using the `<index>/_create` endpoint. Valid values: `index`, `create`. If document id is "
                        "specified, it defaults to `index`. Otherwise, it defaults to `create`",
                    "allowed_values": (
                        <AllowedValueInfo>{
                            "value": "index",
                            "desc": "Index the document",
                        },
                        <AllowedValueInfo>{
                            "value": "create",
                            "desc": "Create the document",
                        },
                    ),
                },
                "pipeline": <DataProviderOptionInfo>{
                    "display_name": "Pipeline",
                    "short_desc": "ID of the pipeline to use to preprocess incoming documents",
                    "type": AbstractDataProviderTypeMap."string",
                    "desc": "ID of the pipeline to use to preprocess incoming documents. If the index has a default "
                        "ingest pipeline specified, then setting the value to _none disables the default ingest pipeline "
                        "for this request. If a final pipeline is configured it will always run, regardless of the value "
                        "of this parameter",
                },
                "refresh": <DataProviderOptionInfo>{
                    "display_name": "Refresh Shards",
                    "short_desc": "Indicates if shards should be refreshed after this operation",
                    "type": AbstractDataProviderTypeMap."string",
                    "desc": "If `true`, Elasticsearch refreshes the affected shards to make this operation visible to "
                        "search, if `wait_for` then wait for a refresh to make this operation visible to search, if "
                        "`false` do nothing with refreshes. Valid values: `true`, `false`, `wait_for`",
                    "allowed_values": (
                        <AllowedValueInfo>{
                            "value": "true",
                            "desc": "Refresh shards to make this operation visible to searches",
                        },
                        <AllowedValueInfo>{
                            "value": "false",
                            "desc": "Do nothing with refreshes",
                        },
                        <AllowedValueInfo>{
                            "value": "wait_for",
                            "desc": "Wait for a refresh to make this operation visible to searches",
                        },
                    ),
                },
                "routing": <DataProviderOptionInfo>{
                    "display_name": "Shard Routing",
                    "short_desc": "Custom value used to route operations to a specific shard",
                    "type": AbstractDataProviderTypeMap."string",
                    "desc": "Custom value used to route operations to a specific shard",
                },
                "timeout": <DataProviderOptionInfo>{
                    "display_name": "Response Timeout",
                    "short_desc": "Period to wait for a response",
                    "type": AbstractDataProviderTypeMap."string",
                    "desc": "Period to wait for a response (ex: `1m`). If no response is received before the "
                        "timeout expires, the request fails and returns an error",
                },
                "version": <DataProviderOptionInfo>{
                    "display_name": "Version Control",
                    "short_desc": "Explicit version number for concurrency control",
                    "type": AbstractDataProviderTypeMap."int",
                    "desc": "Explicit version number for concurrency control. The specified version must match the "
                        "current version of the document for the request to succeed",
                },
                "version_type": <DataProviderOptionInfo>{
                    "display_name": "Version Type",
                    "short_desc": "Specific version type",
                    "type": AbstractDataProviderTypeMap."string",
                    "desc": "Specific version type: `external`, `external_gte`",
                    "allowed_values": (
                        <AllowedValueInfo>{
                            "value": "external",
                            "desc": "external",
                        },
                        <AllowedValueInfo>{
                            "value": "external_gte",
                            "desc": "external_gte",
                        },
                    ),
                },
                "wait_for_active_shards": <DataProviderOptionInfo>{
                    "display_name": "Wait For Active Shards",
                    "short_desc": "The number of shard copies that must be active before proceeding",
                    "type": AbstractDataProviderTypeMap."string",
                    "desc": "The number of shard copies that must be active before proceeding with the operation. Set to "
                        "`all` or any positive integer up to the total number of shards in the index (number_of_replicas+1)",
                },
                "require_alias": <DataProviderOptionInfo>{
                    "display_name": "Require Alias",
                    "short_desc": "If true, the destination must be an index alias",
                    "type": SoftBoolDataProviderStringType,
                    "desc": "If `true`, the destination must be an index alias",
                },
            },
            "search_options": GenericRecordSearchOptions{"requires_result", "max_records"} + {
                "if_seq_no": <DataProviderOptionInfo>{
                    "display_name": "Only If Sequence",
                    "short_desc": "Only perform the operation if the document has this sequence number",
                    "type": AbstractDataProviderTypeMap."int",
                    "desc": "Only perform the operation if the document has this sequence number\n"
                        "(update only)",
                },
                "if_primary_term": <DataProviderOptionInfo>{
                    "display_name": "Only If Primary Term",
                    "short_desc": "Only perform the operation if the document has this primary term",
                    "type": AbstractDataProviderTypeMap."int",
                    "desc": "Only perform the operation if the document has this primary term\n"
                        "(update only)",
                },
                "lang": <DataProviderOptionInfo>{
                    "display_name": "Script Language",
                    "short_desc": "The script language",
                    "type": AbstractDataProviderTypeMap."string",
                    "desc": "The script language\n"
                        "(update only)",
                },
                "operator": <DataProviderOptionInfo>{
                    "display_name": "Boolean Operator",
                    "short_desc": "And or Or logic for searches",
                    "type": AbstractDataProviderTypeMap."string",
                    "desc": "Boolean logic used to interpret text in the query value. Valid values are:\n"
                        "- `or` (Default): For example, a query value of `capital of Hungary` is interpreted as "
                            "`capital OR of OR Hungary`\n"
                        "- `and`: For example, a query value of `capital of Hungary` is interpreted as "
                            "`capital AND of AND Hungary`\n"
                        "(search only)",
                    "allowed_values": (
                        <AllowedValueInfo>{
                            "value": "or",
                            "desc": "Or logic - any terms match (default)",
                        },
                        <AllowedValueInfo>{
                            "value": "and",
                            "desc": "And logic - all terms must match",
                        }
                    ),
                },
                "require_alias": <DataProviderOptionInfo>{
                    "display_name": "Require Alias",
                    "short_desc": "If true the destination must be an index alias",
                    "type": SoftBoolDataProviderStringType,
                    "desc": "If `true`, the destination must be an index alias\n"
                        "(update only)",
                },
                "refresh": <DataProviderOptionInfo>{
                    "display_name": "Refresh Shards",
                    "short_desc": "Indicates if shards should be refreshed after this operation",
                    "type": AbstractDataProviderTypeMap."string",
                    "desc": "If `true`, Elasticsearch refreshes the affected shards to make this operation visible to "
                        "search, if `wait_for` then wait for a refresh to make this operation visible to search, if "
                        "`false` do nothing with refreshes. Valid values: `true`, `false`, `wait_for`",
                    "allowed_values": (
                        <AllowedValueInfo>{
                            "value": "true",
                            "desc": "Refresh shards to make this operation visible to searches",
                        },
                        <AllowedValueInfo>{
                            "value": "false",
                            "desc": "Do nothing with refreshes",
                        },
                        <AllowedValueInfo>{
                            "value": "wait_for",
                            "desc": "Wait for a refresh to make this operation visible to searches",
                        },
                    ),
                },
                "retry_on_conflict": <DataProviderOptionInfo>{
                    "display_name": "Conflict Retries",
                    "short_desc": "How many retries should be made after a conflict?",
                    "type": AbstractDataProviderTypeMap."int",
                    "desc": "Specify how many times should the operation be retried when a conflict occurs\n"
                        "(update only)",
                },
                "routing": <DataProviderOptionInfo>{
                    "display_name": "Shard Routing",
                    "short_desc": "Custom value used to route operations to a specific shard",
                    "type": AbstractDataProviderTypeMap."string",
                    "desc": "Custom value used to route operations to a specific shard\n"
                        "(update only)",
                },
                "_source": <DataProviderOptionInfo>{
                    "display_name": "Source Retrieval",
                    "short_desc": "Specify how or if source retrieval should be performed",
                    "type": AbstractDataProviderTypeMap."string",
                    "desc": "Set to `false` to disable source retrieval (default: `true`). You can also specify a "
                        "comma-separated list of the fields you want to retrieve\n"
                        "(update only)",
                },
                "_source_excludes": <DataProviderOptionInfo>{
                    "display_name": "Source excludes",
                    "short_desc": "The source fields to exclude",
                    "type": AbstractDataProviderTypeMap."string",
                    "desc": "Specify the source fields you want to exclude\n"
                        "(update only)",
                },
                "_source_includes": <DataProviderOptionInfo>{
                    "display_name": "Source Includes",
                    "short_desc": "The source fields to include",
                    "type": AbstractDataProviderTypeMap."string",
                    "desc": "Specify the source fields you want to include\n"
                        "(update only)",
                },
                "timeout": <DataProviderOptionInfo>{
                    "display_name": "Response Timeout",
                    "short_desc": "Period to wait for a response",
                    "type": AbstractDataProviderTypeMap."string",
                    "desc": "Period to wait for a response (ex: `1m` = 1 minute). If no response is received before "
                        "the timeout expires, the request fails and returns an error",
                    "example_value": "1m",
                },
                "version": <DataProviderOptionInfo>{
                    "display_name": "Version Number",
                    "short_desc": "Explicit version number for concurrency control",
                    "type": AbstractDataProviderTypeMap."string",
                    "desc": "Explicit version number for concurrency control",
                },
                "wait_for_active_shards": <DataProviderOptionInfo>{
                    "display_name": "Wait For Active Shards",
                    "short_desc": "The number of shard copies that must be active before proceeding",
                    "type": AbstractDataProviderTypeMap."string",
                    "desc": "The number of shard copies that must be active before proceeding with the operation. "
                        "Set to `all` or any positive integer up to the total number of shards in the index "
                        "(number_of_replicas+1)\n"
                        "(update only)",
                },
            },
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! ElasticSearch property types to data types
        const TypeMap = {
            "binary": AbstractDataProviderTypeMap."base64binary",

            "boolean": AbstractDataProviderTypeMap."boolean",

            "keyword": AbstractDataProviderTypeMap."string",
            "constant_keyword": AbstractDataProviderTypeMap."string",
            "wildcard": AbstractDataProviderTypeMap."string",

            "ip": AbstractDataProviderTypeMap."string",
            "version": AbstractDataProviderTypeMap."string",
            "mumur3": AbstractDataProviderTypeMap."string",

            "text": AbstractDataProviderTypeMap."string",
            "match_only_text": AbstractDataProviderTypeMap."string",

            "byte": AbstractDataProviderTypeMap."int",
            "short": AbstractDataProviderTypeMap."int",
            "integer": AbstractDataProviderTypeMap."int",
            "long": AbstractDataProviderTypeMap."int",

            "unsigned_long": AbstractDataProviderTypeMap."number",

            "date": AbstractDataProviderTypeMap."date",
            # NOTE: lossy conversion here; Qore only supports microsecond resolution
            "date_nanos": AbstractDataProviderTypeMap."date",

            "double": AbstractDataProviderTypeMap."float",
            "float": AbstractDataProviderTypeMap."float",
            "half_float": AbstractDataProviderTypeMap."float",
            "scaled_float": AbstractDataProviderTypeMap."float",

            "object": AbstractDataProviderTypeMap."hash",
            "flattened": AbstractDataProviderTypeMap."hash",

            "nested": AbstractDataProviderTypeMap."list",
        };

        #! Search query options
        const SearchQueryOptions = ("timeout",);
    }

    private {
        #! The record type for the object
        hash<string, AbstractDataField> record_type = {
            "_id": new QoreDataField("_id", "the document ID", AbstractDataProviderTypeMap."softstring"),
            "_score": new QoreDataField("_score", "the score for the match (read-only)",
                AbstractDataProviderTypeMap."float"),
        };
    }

    #! Creates the object from a REST connection
    constructor(RestClient::RestClient rest, string name, hash<auto> index) : ElasticSearchDataProviderBase (rest) {
        self.name = name;
        self.index = index;
        getRecordInfoIntern();
    }

    #! Returns the data provider name
    string getName() {
        return name;
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("ElasticSearch data provider for index `%s` for `%s`", name, rest.getSafeURL());
    }

    #! Returns an iterator for zero or more records matching the search options
    /** @param where_cond the search criteria
        @param search_options the search options after processing by validateSearchOptions()

        @see requestSearchRecordsImpl()
    */
    private DataProvider::AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        return new ElasticSearchRecordIterator(rest, name, index, record_type, where_cond, search_options);
    }

    #! Updates a single document in the index
    /** @param set the hash of field data to set
        @param where_cond a hash for identifying the record(s) to be updated
        @param search_options the search options (see @ref SearchOptions) after processing by validateSearchOptions()

        @return the number of records updated

        @note this API will commit the row to the database unless the datasource was already in a transaction before
        this call
    */
    private int updateRecordsImpl(hash<auto> set, *hash<auto> where_cond, *hash<auto> search_options) {
        # NOTE: this call will throw an exception if the update operation fails
        rest.post(getDocUriPath("_update", where_cond, "update", search_options,
            ElasticSearchDocumentUpdateDataProvider::QueryArgs), {"doc": set});
        return 1;
    }

    /**
        @param where_cond a hash for identifying the record(s) to be deleted; can only contain the \c "_id" field
        @param search_options the search options (see @ref SearchOptions) after processing by validateSearchOptions();
        ignored for this API call

        @return the number of records deleted

        @throw INVALID-OPERATION the data provider does not support record updating
    */
    private int deleteRecordsImpl(*hash<auto> where_cond, *hash<auto> search_options) {
        # NOTE: this call will throw an exception if the delete operation fails
        rest.del(getDocUriPath("_doc", where_cond, "delete", search_options,
            ElasticSearchDocumentDeleteDataProvider::QueryArgs));
        return 1;
    }

    #! Writes the given record to the data provider
    /** @param rec a hash representing a single input record
        @param create_options the create options (see @ref CreateOptions) after processing by validateCreateOptions()

        @return the data written to the data provider plus any output (returning) variables

        @note this API will commit the row to the database unless the datasource was already in a transaction before
        this call
    */
    private *hash<auto> createRecordImpl(hash<auto> rec, *hash<auto> create_options) {
        string uri = sprintf("%s/_doc", name);
        if (create_options) {
            uri = getQueryOptions(uri, create_options, ElasticSearchDocumentCreateDataProvider::QueryArgs);
        }

        # NOTE: this call will throw an exception if the create operation fails
        string id = rest.post(uri, rec).body._id;
        return {"_id": id} + rec;
    }

    #! Returns a bulk insert operation object for this data provider
    /** @param block_size the number of records in a block; the bulk insert operation must be flushed before this
        number of records is processed

        @return a bulk insert operation object for this data provider
    */
    DataProvider::AbstractDataProviderBulkOperation getBulkInserter(*int block_size) {
        return new ElasticSearchBulkOperation(self, rest, name, "index", block_size);
    }

    #! Returns a bulk upsert operation object for this data provider
    /** @param block_size the number of records in a block; the bulk upsert operation must be flushed before this
        number of records is processed

        @return a bulk upsert operation object for this data provider
    */
    DataProvider::AbstractDataProviderBulkOperation getBulkUpserter(*int block_size) {
        return new ElasticSearchBulkOperation(self, rest, name, "index", block_size);
    }

    #! Returns data provider static info
    hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns a data type for a property
    DataProvider::AbstractDataProviderType getTypeForProperty(string name, hash<auto> prop) {
        if (prop.properties) {
            HashDataType type();
            map type.addField(new QoreDataField($1.key, NOTHING,
                ElasticSearchIndexTableDataProvider::getTypeForProperty($1.key, $1.value))),
                prop.properties.pairIterator();
            return type;
        }

        return TypeMap{prop.type} ?? AbstractDataProviderTypeMap."any";
    }

    #! Returns a URI path with query options
    static string getQueryOptions(string uri, hash<auto> search_options, list<string> query_args) {
        bool q;
        foreach string arg in (query_args) {
            if (!exists search_options{arg}) {
                continue;
            }
            if (!q) {
                uri += "?";
                q = True;
            } else {
                uri += "&";
            }
            uri += sprintf("%s=%s", arg, search_options{arg});
        }
        return uri;
    }

    #! Makes sure that the where_cond is only the _id field
    private string getDocUriPath(string uri_str, *hash<auto> where_cond, string action, *hash<auto> search_options,
            *list<string> query_args) {
        if (!where_cond._id.val()) {
            throw "ELASTICSEARCH-" + action.upr() + "-ERROR", sprintf("missing the '_id' field to identify the "
                "document to %s in index %y", action, name);
        }
        if (where_cond._id.typeCode() != NT_STRING) {
            throw "ELASTICSEARCH-" + action.upr() + "-ERROR", sprintf("the '_id' field to identify the document to "
                "%s in index %y must be a string; got type %y instead", action, name, where_cond._id.type());
        }
        if (where_cond.size() > 1) {
            throw "ELASTICSEARCH-" + action.upr() + "-ERROR", sprintf("only one '_id' field can be used to identify "
                "the document to %s in index %y; got %d fields (%y)", action, name, where_cond.size(),
                keys where_cond);
        }
        string rv = sprintf("%s/%s/%s", name, uri_str, where_cond._id);
        return search_options && query_args ? getQueryOptions(rv, search_options, query_args) : rv;
    }

    #! Retrieves the record type
    private getRecordInfoIntern() {
        foreach hash<auto> i in (index.mappings.properties.pairIterator()) {
            # skip aliases
            if (i.value.type == "alias") {
                continue;
            }
            record_type{i.key} = new QoreDataField(i.key, NOTHING, getTypeForProperty(i.key, i.value));
        }
    }

    private *hash<string, DataProvider::AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return record_type;
    }
}
}
