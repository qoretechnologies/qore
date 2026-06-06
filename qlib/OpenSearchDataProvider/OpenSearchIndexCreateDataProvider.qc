# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OpenSearchDataProvider module definition

/** OpenSearchIndexCreateDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the OpenSearchDataProvider module
public namespace OpenSearchDataProvider {
#! The OpenSearch index create API data provider
/** This API allows the caller to create a new OpenSearch index from the arguments describing the new index.
*/
public class OpenSearchIndexCreateDataProvider inherits OpenSearchDataProviderBase {
    public {
        #! Provider info
        const hash<DataProviderInfo> ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "OpenSearch index create API data provider",
            "type": "OpenSearchIndexCreateDataProvider",
            "constructor_options": OpenSearchDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const hash<DataProviderSummaryInfo> ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const AbstractDataProviderType RequestType = new OpenSearchIndexCreateRequestDataType();

        #! Response type
        const AbstractDataProviderType ResponseType = new OpenSearchIndexCreateResponseDataType();

        #! Query args
        const list<string> QueryArgs = ("wait_for_active_shards", "master_timeout", "timeout");
    }

    #! Creates the object from constructor options
    constructor(*hash<auto> options) {
        checkOptions("CONSTRUCTOR-ERROR", OpenSearchDataProvider::ConstructorOptions, options);
        rest = OpenSearchDataProvider::getRestConnection(options);
    }

    #! Creates the object from a REST connection
    constructor(RestClient::AbstractRestClient rest) : OpenSearchDataProviderBase(rest) {
    }

    #! Returns the data provider name
    string getName() {
        return ProviderInfo.name;
    }

    #! Returns the data provider description
    *string getDesc() {
        return sprintf("OpenSearch data index %s provider for `%s`", ProviderInfo.name, rest.getSafeUrl());
    }

    #! Makes a request and returns the response
    /** @param req the request to serialize and make according to the request type
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string uri_path = remove req.index;
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
        return rest.restPut(uri_path, req).body;
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

#! Aliases data type
public class OpenSearchIndexAliasesDataType inherits DataProvider::HashDataType {
    constructor() : HashDataType(AutoHashOrNothingType) {
        setDefaultOtherFieldType(new OpenSearchIndexDataTypeBase());
    }
}

#! Create index request data type
public class OpenSearchIndexCreateRequestDataType inherits OpenSearchIndexDataTypeBase {
    public {
        #! Field descriptions
        const hash<string, AbstractDataField> Fields = {
            # query parameters
            "index": {
                "type": StringType,
                "display_name": "Index",
                "short_desc": "Index name to create",
                "desc": "The name of the index to create",
                "example_value": "my-index",
            },
            "wait_for_active_shards": {
                "type": SoftStringOrNothingType,
                "display_name": "Wait For Active Shards",
                "short_desc": "Required active shards",
                "desc": "The number of shard copies that must be active before proceeding with the operation. "
                    "Set to `all` or any positive integer up to the total number of shards in the "
                    "index (number_of_replicas+1). Default: `1`, the primary shard.",
                "example_value": "1",
            },
            "master_timeout": {
                "type": StringOrNothingType,
                "display_name": "Master Timeout",
                "short_desc": "Master node timeout",
                "desc": "Period to wait for a connection to the master node (default `30s`). If no response is "
                    "received before the timeout expires, the request fails and returns an error",
                "example_value": "30s",
            },
            "timeout": {
                "type": StringOrNothingType,
                "display_name": "Timeout",
                "short_desc": "Response timeout",
                "desc": "Period to wait for a response (default `30s`). If no response is received before the "
                    "timeout expires, the request fails and returns an error",
                "example_value": "30s",
            },

            # body parameters
            "aliases": {
                "type": new OpenSearchIndexAliasesDataType(),
                "display_name": "Aliases",
                "short_desc": "Index aliases",
                "desc": "The key is the alias name. Index alias names support date math",
            },
            "mappings": {
                "type": AutoHashOrNothingType,
                "display_name": "Mappings",
                "short_desc": "Field mappings",
                "desc": "Mapping for fields in the index. If specified, this mapping can include:\n"
                    "- Field names\n"
                    "- Field data types\n"
                    "- Mapping parameters",
                "example_value": {"properties": {"title": {"type": "text"}}},
            },
            "settings": {
                "type": AutoHashOrNothingType,
                "display_name": "Settings",
                "short_desc": "Index configuration",
                "desc": "Configuration options for the index",
                "example_value": {"number_of_shards": 1, "number_of_replicas": 1},
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Index create API response
public class OpenSearchIndexCreateResponseDataType inherits OpenSearchAcknowledgedDataType {
    private {
        #! Field descriptions
        const hash<string, AbstractDataField> Fields = {
            "shards_acknowledged": {
                "type": BoolType,
                "display_name": "Shards Acknowledged?",
                "short_desc": "Shards started in time",
                "desc": "Indicates whether the requisite number of shard copies were started for each shard in the "
                    "index before timing out",
                "example_value": True,
            },
            "index": {
                "type": StringType,
                "desc": "The name of the index created",
            },
        };
    }

    #! Creates the object
    constructor() {
        map addField(new QoreDataField($1.key, $1.value.desc, $1.value.type, $1.value.default_value)),
            Fields.pairIterator();
    }
}
}
