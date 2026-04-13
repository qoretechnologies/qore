# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SGLangDataProvider module definition

/** SGLangEmbeddingsRequestDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the SGLangDataProvider module
public namespace SGLangDataProvider {
#! Data type for a single embedding item returned by the embeddings API
public class SGLangEmbeddingItemDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "object": {
                "display_name": "Object Type",
                "desc": "Always `embedding`.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "embedding": {
                "display_name": "Embedding",
                "desc": "The embedding vector.  Returned as a list of floats by default, or as a "
                    "base64-encoded string when `encoding_format` is `base64`.",
                "type": AbstractDataProviderTypeMap."softlist",
            },
            "index": {
                "display_name": "Index",
                "desc": "Zero-based index of this embedding in the input list.",
                "type": AbstractDataProviderTypeMap."*int",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data type for SGLang embeddings requests (OpenAI-compatible)
public class SGLangEmbeddingsRequestDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "input": {
                "display_name": "Input Text",
                "short_desc": "The text or list of texts to convert into embeddings",
                "desc": "The text content to embed.  Can be a single string or a list of strings for "
                    "batch processing.\n\n"
                    "**Examples**:\n"
                    "- Single text: `\"How do I reset my password?\"`\n"
                    "- Batch: `[\"Product A description\", \"Product B description\"]`\n\n"
                    "The SGLang server must be running an embedding model (e.g. `bge-m3`, "
                    "`nomic-embed-text-v1.5`).",
                "type": AbstractDataProviderTypeMap."softlist",
                "required": True,
                "example_value": ("The quick brown fox jumps over the lazy dog",),
            },
            "model": {
                "display_name": "Model",
                "short_desc": "Override the model served by the SGLang connection",
                "desc": "Name of the embedding model to use.  Optional; defaults to the model "
                    "configured on the SGLang connection via `getModel()`.\n\n"
                    "**Note**: SGLang serves exactly one model per server, so overriding `model` only "
                    "makes sense when routing to a multi-model gateway.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "encoding_format": {
                "display_name": "Encoding Format",
                "short_desc": "How embedding values are encoded in the response",
                "desc": "Controls how embedding vectors are returned.\n\n"
                    "- `float` — plain JSON number array (default, easiest to consume)\n"
                    "- `base64` — base64-encoded binary blob (smaller on the wire; decode before use)",
                "type": AbstractDataProviderTypeMap."*string",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "float",
                        "display_name": "Float - plain number array (default)"},
                    <AllowedValueInfo>{"value": "base64",
                        "display_name": "Base64 - encoded binary (smaller transfer)"},
                ),
            },
            "user": {
                "display_name": "User ID",
                "short_desc": "Opaque end-user identifier for abuse tracking",
                "desc": "Opaque identifier representing the end user.  Logged server-side for abuse "
                    "tracking and usage attribution; does not affect output.",
                "type": AbstractDataProviderTypeMap."*softstring",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}
}
