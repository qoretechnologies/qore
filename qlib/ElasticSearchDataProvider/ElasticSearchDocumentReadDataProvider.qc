# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ElasticSearchDataProvider module definition

/** ElasticSearchDocumentReadDataProvider.qc Copyright 2023 - 2026 Qore Technologies, s.r.o.

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
#! The ElasticSearch document read API data provider
/** This API allows the caller to read an ElasticSearch document
*/
public class ElasticSearchDocumentReadDataProvider inherits ElasticSearchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "read",
            "desc": "ElasticSearch document read API data provider",
            "type": "ElasticSearchDocumentReadDataProvider",
            "constructor_options": ElasticSearchDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new ElasticSearchDocumentReadRequestDataType();

        #! Response type
        const ResponseType = new ElasticSearchDocumentReadResponseDataType();

        #! Query args
        const QueryArgs = ("preference", "realtime", "refresh", "routing", "stored_fields", "_source",
            "_source_excludes", "_source_includes", "version", "version_type");
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        checkOptions("CONSTRUCTOR-ERROR", ElasticSearchDataProvider::ConstructorOptions, options);
        rest = ElasticSearchDataProvider::getRestConnection(options);
    }

    #! Creates the object from a REST connection
    constructor(RestClient::AbstractRestClient rest) : ElasticSearchDataProviderBase(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("ElasticSearch data document %s provider for `%s`", ProviderInfo.name, rest.getSafeUrl());
    }

    #! Makes a request and returns the response
    /** @param req the request to serialize and make according to the request type
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string uri_path = sprintf("%s/_doc/%s", remove req.index, remove req.id);
        bool q;
        foreach string arg in (QueryArgs) {
            if (!exists req{arg}) {
                continue;
            }
            if (!q) {
                uri_path += "?";
                q = True;
            } else {
                uri_path += "&";
            }
            uri_path += sprintf("%s=%s", arg, remove req{arg});
        }
        return rest.restGet(uri_path).body;
    }

    #! Returns the description of a successful request message, if any
    /** @return the request type for this provider
    */
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the description of a response message, if this object represents a response message
    /** @return the response type for this response message
    */
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns data provider static info
    hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! Document read API request data type
class ElasticSearchDocumentReadRequestDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            # query parameters
            "index": {
                "type": StringType,
                "display_name": "Index",
                "short_desc": "Index name",
                "desc": "The name of the index holding the document to read",
                "example_value": "my-index",
            },
            "id": {
                "type": StringType,
                "display_name": "Document ID",
                "short_desc": "Document identifier",
                "desc": "The ID of the document to read",
                "example_value": "doc-123",
            },
            "preference": {
                "type": StringOrNothingType,
                "display_name": "Preference",
                "short_desc": "Node or shard preference",
                "desc": "Specifies the node or shard the operation should be performed on; random by default",
                "example_value": "_local",
            },
            "realtime": {
                "type": SoftBoolStringType,
                "display_name": "Realtime?",
                "short_desc": "Real-time read",
                "desc": "If `true` (the default), the request is real-time as opposed to near-real-time",
                "example_value": True,
            },
            "refresh": {
                "type": SoftBoolStringType,
                "display_name": "Refresh?",
                "short_desc": "Refresh before read",
                "desc": "If `true` (default `false`), the request refreshes the relevant shard before retrieving the "
                    "document",
                "example_value": False,
            },
            "routing": {
                "type": StringOrNothingType,
                "display_name": "Routing",
                "short_desc": "Custom routing value",
                "desc": "Custom value used to route operations to a specific shard",
                "example_value": "user123",
            },
            "stored_fields": {
                "type": SoftBoolStringType,
                "display_name": "Stored Fields?",
                "short_desc": "Retrieve stored fields",
                "desc": "If `true` (default `false`), retrieves the document fields stored in the index rather than "
                    "the document `_source`",
                "example_value": False,
            },
            "_source": {
                "type": StringOrNothingType,
                "display_name": "Source",
                "short_desc": "Source field control",
                "desc": "`true` or `false` to return the `_source` field or not, or a list of fields to return",
                "example_value": "true",
            },
            "_source_excludes": {
                "type": StringOrNothingType,
                "display_name": "Source Excludes",
                "short_desc": "Fields to exclude",
                "desc": "A comma-separated list of source fields to exclude from the response.\n\n"
                    "You can also use this parameter to exclude fields from the subset specified in the "
                    "`_source_includes` query parameter.\n\n"
                    "If the `_source` parameter is `false`, this parameter is ignored",
                "example_value": "meta.*",
            },
            "_source_includes": {
                "type": StringOrNothingType,
                "display_name": "Source Includes",
                "short_desc": "Fields to include",
                "desc": "A comma-separated list of source fields to include in the response.\n\n"
                    "If this parameter is specified, only these source fields are returned. You can exclude fields "
                    "from this subset using the `_source_excludes` query parameter.\n\n"
                    "If the `_source` parameter is `false`, this parameter is ignored",
                "example_value": "title,content",
            },
            "version": {
                "type": IntOrNothingType,
                "display_name": "Version",
                "short_desc": "Required version",
                "desc": "Explicit version number for concurrency control. The specified version must match the "
                    "current version of the document for the request to succeed",
                "example_value": 1,
            },
            "version_type": {
                "type": StringOrNothingType,
                "display_name": "Version Type",
                "short_desc": "Version type",
                "desc": "Specific version type: `external`, `external_gte`",
                "example_value": "external",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Document read API response
public class ElasticSearchDocumentReadResponseDataType inherits DataProvider::HashDataType {
    private {
        #! Field descriptions
        const Fields = {
            "_index": {
                "type": StringType,
                "display_name": "Index",
                "short_desc": "Index name",
                "desc": "The name of the index the document was added to",
                "example_value": "my-index",
            },
            "_id": {
                "type": StringType,
                "display_name": "Document ID",
                "short_desc": "Document identifier",
                "desc": "The unique identifier for the added document",
                "example_value": "doc-123",
            },
            "_version": {
                "type": IntType,
                "display_name": "Version",
                "short_desc": "Document version",
                "desc": "The document version. Incremented each time the document is updated",
                "example_value": 1,
            },
            "found": {
                "type": BoolType,
                "display_name": "Found?",
                "short_desc": "Document exists",
                "desc": "Indicates whether the document exists: `true` or `false`",
                "example_value": True,
            },
            "_routing": {
                "type": StringOrNothingType,
                "display_name": "Routing",
                "short_desc": "Document routing",
                "desc": "The explicit routing, if set",
                "example_value": "user123",
            },
            "_source": {
                "type": StringOrNothingType,
                "display_name": "Source",
                "short_desc": "Document data",
                "desc": "If `found` is `true`, contains the document data formatted in JSON. Excluded if the "
                    "`_source` parameter is set to `false` or the `stored_fields` parameter is set to `true`",
            },
            "_fields": {
                "type": StringOrNothingType,
                "display_name": "Fields",
                "short_desc": "Stored fields",
                "desc": "If the `stored_fields` parameter is set to `true` and `found` is `true`, this will contain "
                    "the document fields stored in the index",
            },
            "_seq_no": {
                "type": IntType,
                "display_name": "Sequence Number",
                "short_desc": "Sequence number",
                "desc": "The sequence number assigned to the document for the indexing operation. Sequence numbers "
                    "are used to ensure an older version of a document doesn't overwrite a newer version",
                "example_value": 1,
            },
            "_primary_term": {
                "type": IntType,
                "display_name": "Primary Term",
                "short_desc": "Primary term",
                "desc": "The primary term assigned to the document for the indexing operation",
                "example_value": 1,
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}
}
