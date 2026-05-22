# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ChromaDataProvider root class definition

/** ChromaDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace ChromaDataProvider {
public class ChromaDataProvider inherits ChromaDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "chroma",
            "type": "ChromaDataProvider",
            "supports_children": True,
            "constructor_options": ConstructorOptions,
            "children_can_support_apis": True,
        };
    }

    private {
        static hash<string, Reflection::Class> childMap = DefaultChildMap;

        const DefaultChildMap = {
            "query":
                Class::forName("ChromaDataProvider::ChromaQueryDataProvider"),
        };
    }

    constructor(RestClientIo::RestClientIo rest) : ChromaDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : ChromaDataProviderCommon(options) {}

    string getName() { return ProviderInfo.name; }

    *list<hash<DataProviderSummaryInfo>> getChildProviderSummaryInfo() {
        return map $1.getConstant("ProviderSummaryInfo").getValue(), childMap.iterator();
    }

    private *list<string> getChildProviderNamesImpl() { return keys childMap; }

    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = childMap{name};
        return cls ? cls.newObject(rest) : NOTHING;
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

public class ChromaDataProviderFactory inherits AbstractDataProviderFactory {
    private {
        static Class cls = new Class("ChromaDataProvider");
        const FactoryInfo = <DataProviderFactoryInfo>{
            "name": "chroma",
            "desc": "Chroma vector store data provider factory",
            "children_can_support_apis": True,
        };
    }
    private hash<DataProviderFactoryInfo> getInfoImpl() { return FactoryInfo; }
    private hash<DataProviderInfo> getProviderInfoImpl() { return ChromaDataProvider::ProviderInfo; }
    private Class getClassImpl() { return cls; }
}

public class ChromaQueryRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "collection_id": {
                "display_name": "Collection ID",
                "short_desc": "Chroma collection ID (UUID)",
                "desc": "The Chroma collection ID (UUID, not the collection name).",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
            },
            "query_embeddings": {
                "display_name": "Query Embeddings",
                "short_desc": "List of query embedding vectors",
                "desc": "List of query embedding vectors.  Typically one vector per "
                    "request; Chroma accepts a list to batch independent queries.",
                "type": AbstractDataProviderTypeMap."softlist",
                "required": True,
            },
            "n_results": {
                "display_name": "Number of Results",
                "short_desc": "How many top-K results to return per query",
                "desc": "Number of nearest-neighbor results to return per query vector.",
                "type": AbstractDataProviderTypeMap."*softint",
                "example_value": 10,
            },
            "where": {
                "display_name": "Where (metadata filter)",
                "short_desc": "Chroma metadata filter expression",
                "desc": "Chroma metadata filter — a hash that follows Chroma's filter "
                    "DSL.  See [Chroma docs](https://docs.trychroma.com/usage-guide#using-where-filters).",
                "type": AbstractDataProviderTypeMap."*hash",
            },
            "where_document": {
                "display_name": "Where Document (full-text filter)",
                "short_desc": "Chroma full-text filter expression",
                "desc": "Chroma full-text filter — operates on document content.",
                "type": AbstractDataProviderTypeMap."*hash",
            },
            "include": {
                "display_name": "Include",
                "short_desc": "Which fields to include in the response",
                "desc": "List of fields to include: `metadatas`, `documents`, `embeddings`, "
                    "`distances`.",
                "type": AbstractDataProviderTypeMap."*softlist",
                "example_value": ("metadatas", "distances"),
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class ChromaQueryResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "ids": {
                "display_name": "IDs",
                "desc": "Nested list of result IDs, one inner list per query embedding.",
                "type": AbstractDataProviderTypeMap."*list",
            },
            "distances": {
                "display_name": "Distances",
                "desc": "Nested list of distances (smaller = closer).",
                "type": AbstractDataProviderTypeMap."*list",
            },
            "metadatas": {
                "display_name": "Metadatas",
                "desc": "Nested list of metadata hashes.",
                "type": AbstractDataProviderTypeMap."*list",
            },
            "documents": {
                "display_name": "Documents",
                "desc": "Nested list of stored document texts (when `documents` is in `include`).",
                "type": AbstractDataProviderTypeMap."*list",
            },
            "embeddings": {
                "display_name": "Embeddings",
                "desc": "Nested list of stored embeddings (when `embeddings` is in `include`).",
                "type": AbstractDataProviderTypeMap."*list",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class ChromaQueryDataProvider inherits ChromaDataProviderCommon {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "query",
            "type": "ChromaQueryDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_request": True,
        };
        const ProviderSummaryInfo = cast<hash<DataProviderSummaryInfo>>(ProviderInfo{
            AbstractDataProvider::DataProviderSummaryInfoKeys
        });
        static ChromaQueryRequestDataType RequestType = new ChromaQueryRequestDataType();
        static ChromaQueryResponseDataType ResponseType = new ChromaQueryResponseDataType();
    }

    constructor(RestClientIo::RestClientIo rest) : ChromaDataProviderCommon(rest) {}
    constructor(*hash<auto> options) : ChromaDataProviderCommon(options) {}

    string getName() { return ProviderInfo.name; }
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() { return RequestType; }
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() { return ResponseType; }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string cid = remove req.collection_id;
        return rest.restPost(collectionPath(cid, "/query"), req).body;
    }
}
}
