# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Jina data type definitions

/** JinaDataTypes.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace JinaDataProvider {
#! Compatibility type backed by the pinned Jina request schema
public class JinaEmbeddingRequestDataType inherits HashDataType {
    #! Returns the fields of the type, from the pinned schema
    /** The schema is parsed when the fields are first needed, not when the module is loaded
    */
    static *hash<string, AbstractDataField> schemaFields() {
        return JinaSchema::getActionSet().getOperationEx("create-embeddings").getRequestType().getFields();
    }

    #! Creates the schema-derived data type
    constructor() : HashDataType("JinaEmbeddingRequest") {
        foreach AbstractDataField field in (schemaFields().values()) {
            addField(field);
        }
    }
}

public class JinaEmbeddingItemDataType inherits HashDataType {
    private {
        const Fields = {
            "index": {
                "display_name": "Index",
                "desc": "Position of this embedding in the input list.",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "object": {
                "display_name": "Object Type",
                "desc": "Object type discriminator (typically `embedding`).",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "embedding": {
                "display_name": "Embedding Vector",
                "desc": "The embedding vector — a list of floats.",
                "type": AbstractDataProviderTypeMap."*list",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public class JinaUsageDataType inherits HashDataType {
    private {
        const Fields = {
            "total_tokens": {
                "display_name": "Total Tokens",
                "desc": "Total tokens consumed.",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "prompt_tokens": {
                "display_name": "Prompt Tokens",
                "desc": "Prompt tokens consumed.",
                "type": AbstractDataProviderTypeMap."*int",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Compatibility type backed by the pinned Jina response schema
public class JinaEmbeddingResponseDataType inherits HashDataType {
    #! Returns the fields of the type, from the pinned schema
    /** The schema is parsed when the fields are first needed, not when the module is loaded
    */
    static *hash<string, AbstractDataField> schemaFields() {
        return JinaSchema::getActionSet().getOperationEx("create-embeddings").getResponseType().getFields();
    }

    #! Creates the schema-derived data type
    constructor() : HashDataType("JinaEmbeddingResponse") {
        foreach AbstractDataField field in (schemaFields().values()) {
            addField(field);
        }
    }
}

#! Compatibility type backed by the pinned Jina request schema
public class JinaRerankRequestDataType inherits HashDataType {
    #! Returns the fields of the type, from the pinned schema
    /** The schema is parsed when the fields are first needed, not when the module is loaded
    */
    static *hash<string, AbstractDataField> schemaFields() {
        return JinaSchema::getActionSet().getOperationEx("rerank").getRequestType().getFields();
    }

    #! Creates the schema-derived data type
    constructor() : HashDataType("JinaRerankRequest") {
        foreach AbstractDataField field in (schemaFields().values()) {
            addField(field);
        }
    }
}

public class JinaRerankResultDataType inherits HashDataType {
    private {
        const Fields = {
            "index": {
                "display_name": "Index",
                "desc": "Original index of this document in the input `documents` list.",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "relevance_score": {
                "display_name": "Relevance Score",
                "desc": "Relevance score (higher = more relevant).",
                "type": AbstractDataProviderTypeMap."*float",
            },
            "document": {
                "display_name": "Document",
                "desc": "Echoed document text or hash (only when `return_documents` is `True`).",
                "type": AbstractDataProviderTypeMap."*hash",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Compatibility type backed by the pinned Jina response schema
public class JinaRerankResponseDataType inherits HashDataType {
    #! Returns the fields of the type, from the pinned schema
    /** The schema is parsed when the fields are first needed, not when the module is loaded
    */
    static *hash<string, AbstractDataField> schemaFields() {
        return JinaSchema::getActionSet().getOperationEx("rerank").getResponseType().getFields();
    }

    #! Creates the schema-derived data type
    constructor() : HashDataType("JinaRerankResponse") {
        foreach AbstractDataField field in (schemaFields().values()) {
            addField(field);
        }
    }
}

public const JinaEmbeddingItemDataType = new JinaEmbeddingItemDataType();
public const JinaUsageDataType = new JinaUsageDataType();
public const JinaRerankResultDataType = new JinaRerankResultDataType();

}
