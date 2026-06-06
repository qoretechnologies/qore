# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OpenSearchDataProvider vector index providers

/** OpenSearchVectorIndexDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! The OpenSearch vector index API root data provider
public class OpenSearchVectorIndexDataProvider inherits OpenSearchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "vector-index",
            "desc": "OpenSearch vector index data provider",
            "type": "OpenSearchVectorIndexDataProvider",
            "constructor_options": OpenSearchDataProvider::ConstructorOptions,
            "supports_children": True,
            "children_can_support_apis": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
    }

    private {
        const ChildMap = {
            "create": Class::forName("OpenSearchDataProvider::OpenSearchVectorIndexCreateDataProvider"),
        };
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
        return sprintf("OpenSearch vector index provider for `%s`", rest.getSafeUrl());
    }

    #! Return data provider summary info
    *list<hash<DataProvider::DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map $1.getConstant("ProviderSummaryInfo").getValue(), ChildMap.iterator();
    }

    #! Returns a list of child data provider names, if any
    /** @return a list of child data provider names, if any
    */
    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    #! Returns the given child provider or @ref nothing if the given child is unknown
    /** @return the given child provider or @ref nothing if the given child is unknown

        @see getChildProviderEx()
    */
    private *DataProvider::AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (!cls) {
            return;
        }
        return cls.newObject(rest);
    }

    #! Returns data provider static info
    hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! The OpenSearch vector index create API data provider
public class OpenSearchVectorIndexCreateDataProvider inherits OpenSearchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "OpenSearch vector index create API data provider",
            "type": "OpenSearchVectorIndexCreateDataProvider",
            "constructor_options": OpenSearchDataProvider::ConstructorOptions,
            "supports_request": True,
        };

        #! Provider summary info
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });

        #! Request type
        const RequestType = new OpenSearchVectorIndexCreateRequestDataType();

        #! Response type
        const ResponseType = OpenSearchIndexCreateDataProvider::ResponseType;

        #! Query args
        const QueryArgs = ("wait_for_active_shards", "master_timeout", "timeout");
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
        return sprintf("OpenSearch vector index %s provider for `%s`", ProviderInfo.name, rest.getSafeUrl());
    }

    #! Makes a request and returns the response
    /** @param req the request to serialize and make according to the request type
        @param request_options the request options; will be processed by validateRequestOptions()

        @return the response to the request
    */
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string index = remove req.index;
        string uri = SearchIndexDataProvider::SearchIndexRestUtil::moveQueryOptions(index, \req, QueryArgs);

        hash<auto!> settings = cast<hash<auto!>>(remove req.settings ?? {});
        hash<auto!> index_settings = cast<hash<auto!>>(settings{"index"} ?? {});
        index_settings.knn = True;

        if (exists req{"ef_search"}) {
            index_settings{"knn.algo_param.ef_search"} = remove req.ef_search;
        }
        settings{"index"} = index_settings;

        hash<auto!> mappings = cast<hash<auto!>>(remove req.mappings ?? {});
        hash<auto!> properties = cast<hash<auto!>>(mappings.properties ?? {});

        string vector_field = remove req.vector_field;
        hash<auto!> field_mapping = {
            "type": "knn_vector",
            "dimension": remove req.dimension,
        };

        string space_type = remove req.space_type ?? "l2";
        bool method_requested = exists req{"method_name"} || exists req{"engine"} || exists req{"m"}
            || exists req{"ef_construction"};
        if (method_requested) {
            hash<auto!> method = {
                "name": remove req.method_name ?? "hnsw",
                "engine": remove req.engine ?? "lucene",
                "space_type": space_type,
            };
            hash<auto!> parameters = {};
            OpenSearchSearchActionUtil::moveIfExists(\parameters, \req, "m");
            OpenSearchSearchActionUtil::moveIfExists(\parameters, \req, "ef_construction");
            if (parameters) {
                method.parameters = parameters;
            }
            field_mapping.method = method;
        } else {
            field_mapping.space_type = space_type;
        }

        properties{vector_field} = field_mapping;

        foreach string field in (remove req.text_fields ?? ()) {
            if (!properties{field}) {
                properties{field} = {"type": "text"};
            }
        }

        foreach string field in (remove req.metadata_fields ?? ()) {
            if (!properties{field}) {
                properties{field} = {"type": "keyword"};
            }
        }
        mappings.properties = properties;

        hash<auto!> body = {
            "settings": settings,
            "mappings": mappings,
        };
        return rest.restPut(uri, body).body;
    }

    #! Returns the request type
    /** @return the request type for this provider */
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    /** @return the response type for this response message */
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns data provider static info
    hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

#! OpenSearch vector index create request
public class OpenSearchVectorIndexCreateRequestDataType inherits DataProvider::HashDataType {
    public {
        #! Field descriptions
        const Fields = {
            "index": {
                "type": StringType,
                "display_name": "Index",
                "short_desc": "Index name to create",
                "desc": "The name of the vector index to create",
                "example_value": "my-vector-index",
            },
            "vector_field": {
                "type": StringType,
                "display_name": "Vector Field",
                "short_desc": "Vector field name",
                "desc": "The `knn_vector` field to create in the index mapping",
                "example_value": "embedding",
            },
            "dimension": {
                "type": SoftIntType,
                "display_name": "Dimension",
                "short_desc": "Vector dimensions",
                "desc": "Number of dimensions in the vector field",
                "example_value": 384,
            },
            "space_type": {
                "type": StringOrNothingType,
                "display_name": "Space Type",
                "short_desc": "Vector distance space",
                "desc": "Vector space type, such as `l2`, `cosinesimil`, or `innerproduct`",
                "default_value": "l2",
                "example_value": "cosinesimil",
                "allowed_values_creatable": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "l2", "display_name": "L2"},
                    <AllowedValueInfo>{"value": "cosinesimil", "display_name": "Cosine Similarity"},
                    <AllowedValueInfo>{"value": "innerproduct", "display_name": "Inner Product"},
                    <AllowedValueInfo>{"value": "l1", "display_name": "L1"},
                    <AllowedValueInfo>{"value": "linf", "display_name": "L-Infinity"},
                ),
            },
            "method_name": {
                "type": StringOrNothingType,
                "display_name": "Method Name",
                "short_desc": "ANN method",
                "desc": "Optional approximate nearest-neighbor method name, such as `hnsw`, `ivf`, or `flat`",
                "example_value": "hnsw",
                "allowed_values_creatable": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "hnsw", "display_name": "HNSW"},
                    <AllowedValueInfo>{"value": "ivf", "display_name": "IVF"},
                    <AllowedValueInfo>{"value": "flat", "display_name": "Flat"},
                ),
            },
            "engine": {
                "type": StringOrNothingType,
                "display_name": "Engine",
                "short_desc": "ANN engine",
                "desc": "Optional approximate nearest-neighbor engine, such as `lucene`, `faiss`, or `nmslib`",
                "example_value": "lucene",
                "allowed_values_creatable": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "lucene", "display_name": "Lucene"},
                    <AllowedValueInfo>{"value": "faiss", "display_name": "Faiss"},
                    <AllowedValueInfo>{"value": "nmslib", "display_name": "NMSLIB"},
                ),
            },
            "m": {
                "type": IntOrNothingType,
                "display_name": "M",
                "short_desc": "HNSW graph degree",
                "desc": "Optional method parameter controlling HNSW graph degree",
                "example_value": 16,
            },
            "ef_construction": {
                "type": IntOrNothingType,
                "display_name": "EF Construction",
                "short_desc": "HNSW construction depth",
                "desc": "Optional method parameter controlling HNSW construction search depth",
                "example_value": 128,
            },
            "ef_search": {
                "type": IntOrNothingType,
                "display_name": "EF Search",
                "short_desc": "Default search depth",
                "desc": "Optional default `knn.algo_param.ef_search` index setting",
                "example_value": 100,
            },
            "text_fields": {
                "type": AutoListOrNothingType,
                "display_name": "Text Fields",
                "short_desc": "Text fields to add",
                "desc": "Optional list of text fields to add to the index mapping",
                "example_value": ("message", "summary"),
            },
            "metadata_fields": {
                "type": AutoListOrNothingType,
                "display_name": "Metadata Fields",
                "short_desc": "Keyword metadata fields",
                "desc": "Optional list of keyword metadata fields to add to the index mapping",
                "example_value": ("level", "component"),
            },
            "settings": {
                "type": AutoHashOrNothingType,
                "display_name": "Settings",
                "short_desc": "Additional index settings",
                "desc": "Optional additional index settings; `index.knn` is set automatically",
                "example_value": {"number_of_shards": 1, "number_of_replicas": 0},
            },
            "mappings": {
                "type": AutoHashOrNothingType,
                "display_name": "Mappings",
                "short_desc": "Additional mappings",
                "desc": "Optional additional mappings; the vector field mapping is added automatically",
                "example_value": {"properties": {"created": {"type": "date"}}},
            },
            "wait_for_active_shards": {
                "type": SoftStringOrNothingType,
                "display_name": "Wait For Active Shards",
                "short_desc": "Required active shards",
                "desc": "The number of shard copies that must be active before proceeding with the operation",
                "example_value": "1",
            },
            "master_timeout": {
                "type": StringOrNothingType,
                "display_name": "Master Timeout",
                "short_desc": "Master node timeout",
                "desc": "Period to wait for a connection to the master node",
                "example_value": "30s",
            },
            "timeout": {
                "type": StringOrNothingType,
                "display_name": "Timeout",
                "short_desc": "Response timeout",
                "desc": "Period to wait for a response",
                "example_value": "30s",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}
}
