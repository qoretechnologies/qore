# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Mistral data type definitions

/** MistralDataTypes.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace MistralDataProvider {
public class MistralChatMessageDataType inherits HashDataType {
    public {
        const Fields = {
            "role": {
                "display_name": "Role",
                "short_desc": "Message role",
                "desc": "Role: `system`, `user`, `assistant`, or `tool`.",
                "type": AbstractDataProviderTypeMap."string",
                "example_value": "user",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "system", "display_name": "System"},
                    <AllowedValueInfo>{"value": "user", "display_name": "User"},
                    <AllowedValueInfo>{"value": "assistant", "display_name": "Assistant"},
                    <AllowedValueInfo>{"value": "tool", "display_name": "Tool"},
                ),
            },
            "content": {
                "display_name": "Content",
                "short_desc": "Message text content",
                "desc": "Message text content.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "name": {
                "display_name": "Author Name",
                "short_desc": "Optional name of the author",
                "desc": "Optional author identifier.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "tool_call_id": {
                "display_name": "Tool Call ID",
                "short_desc": "ID of the tool call this message responds to",
                "desc": "Required when `role` is `tool`.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "tool_calls": {
                "display_name": "Tool Calls",
                "short_desc": "Tool invocations emitted by the assistant",
                "desc": "List of tool calls emitted by the assistant.",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class MistralChatCompletionRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "model": {
                "display_name": "Model",
                "short_desc": "Mistral model name",
                "desc": "Mistral model identifier, e.g. `mistral-large-latest`, "
                    "`mistral-small-latest`, `codestral-latest`, `mistral-nemo`.",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "mistral-large-latest",
            },
            "messages": {
                "display_name": "Messages",
                "short_desc": "List of chat messages forming the conversation",
                "desc": "The conversation history.",
                "type": new ListDataType("MistralChatMessageList",
                    new MistralChatMessageDataType(), False),
                "required": True,
                "example_value": (
                    {"role": "user", "content": "Hello!"},
                ),
            },
            "temperature": {
                "display_name": "Temperature",
                "short_desc": "Sampling temperature (0-1.5)",
                "desc": "Controls randomness.  Mistral recommends `0.3` as a default.",
                "type": AbstractDataProviderTypeMap."*softfloat",
                "example_value": 0.3,
            },
            "max_tokens": {
                "display_name": "Max Tokens",
                "short_desc": "Maximum tokens to emit",
                "desc": "Maximum number of tokens to generate.",
                "type": AbstractDataProviderTypeMap."*softint",
            },
            "top_p": {
                "display_name": "Top-P",
                "short_desc": "Nucleus sampling cutoff",
                "desc": "Nucleus sampling cutoff (0-1).",
                "type": AbstractDataProviderTypeMap."*softfloat",
            },
            "stream": {
                "display_name": "Stream",
                "short_desc": "Stream partial completions as SSE",
                "desc": "When `True`, the server streams partial completions as SSE.",
                "type": AbstractDataProviderTypeMap."*softbool",
            },
            "tools": {
                "display_name": "Tools",
                "short_desc": "OpenAI-style tool definitions the model may call",
                "desc": "List of tool definitions.",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "tool_choice": {
                "display_name": "Tool Choice",
                "short_desc": "Tool invocation mode",
                "desc": "Tool invocation mode: `none`, `auto`, or `any` (Mistral's "
                    "equivalent of `required`).",
                "type": AbstractDataProviderTypeMap."*softstring",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "none",
                        "display_name": "None - never call a tool"},
                    <AllowedValueInfo>{"value": "auto",
                        "display_name": "Auto - model decides"},
                    <AllowedValueInfo>{"value": "any",
                        "display_name": "Any - must call a tool"},
                ),
            },
            "response_format": {
                "display_name": "Response Format",
                "short_desc": "Force structured output (e.g. JSON)",
                "desc": "Hash that forces a structured response shape, e.g. "
                    "`{type: \"json_object\"}`.",
                "type": AbstractDataProviderTypeMap."*hash",
                "example_value": {"type": "json_object"},
            },
            "safe_prompt": {
                "display_name": "Safe Prompt",
                "short_desc": "Inject Mistral's safety preamble",
                "desc": "When `True`, Mistral injects its safety preamble.",
                "type": AbstractDataProviderTypeMap."*softbool",
            },
            "random_seed": {
                "display_name": "Random Seed",
                "short_desc": "Deterministic sampling seed",
                "desc": "Integer seed for deterministic sampling.",
                "type": AbstractDataProviderTypeMap."*softint",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class MistralChatChoiceDataType inherits HashDataType {
    private {
        const Fields = {
            "index": {
                "display_name": "Index",
                "desc": "Zero-based index of this choice.",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "message": {
                "display_name": "Message",
                "desc": "The assistant message.",
                "type": AbstractDataProviderTypeMap."*hash",
            },
            "finish_reason": {
                "display_name": "Finish Reason",
                "desc": "Why generation stopped.",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class MistralUsageDataType inherits HashDataType {
    private {
        const Fields = {
            "prompt_tokens": {
                "display_name": "Prompt Tokens",
                "desc": "Prompt tokens consumed.",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "completion_tokens": {
                "display_name": "Completion Tokens",
                "desc": "Completion tokens emitted.",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "total_tokens": {
                "display_name": "Total Tokens",
                "desc": "Total tokens processed.",
                "type": AbstractDataProviderTypeMap."*int",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class MistralChatCompletionResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "display_name": "Completion ID",
                "desc": "Unique identifier for this completion.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "object": {
                "display_name": "Object Type",
                "desc": "Object type discriminator.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "created": {
                "display_name": "Created",
                "desc": "When the completion was created.  Mistral emits Unix seconds; "
                    "the data provider converts it to a Qore date before returning.",
                "type": AbstractDataProviderTypeMap."*date",
            },
            "model": {
                "display_name": "Model",
                "desc": "The model that produced this completion.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "choices": {
                "display_name": "Choices",
                "desc": "List of completion choices.",
                "type": new ListDataType("MistralChatChoiceList",
                    new MistralChatChoiceDataType(), True),
            },
            "usage": {
                "display_name": "Usage",
                "desc": "Token usage information.",
                "type": new MistralUsageDataType(),
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class MistralEmbeddingRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "model": {
                "display_name": "Model",
                "short_desc": "Mistral embedding model",
                "desc": "Mistral embedding model identifier, currently `mistral-embed`.",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "mistral-embed",
            },
            "input": {
                "display_name": "Input",
                "short_desc": "Text(s) to embed",
                "desc": "Text input(s) to convert to embedding vectors.",
                "type": AbstractDataProviderTypeMap."softlist",
                "required": True,
                "example_value": ("Hello, world!",),
            },
            "encoding_format": {
                "display_name": "Encoding Format",
                "short_desc": "Wire format for vectors",
                "desc": "`float` (default) or `base64`.",
                "type": AbstractDataProviderTypeMap."*softstring",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class MistralEmbeddingItemDataType inherits HashDataType {
    private {
        const Fields = {
            "index": {
                "display_name": "Index",
                "desc": "Position of this embedding in the input list.",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "object": {
                "display_name": "Object Type",
                "desc": "Object type discriminator.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "embedding": {
                "display_name": "Embedding Vector",
                "desc": "The embedding vector as a list of floats.",
                "type": AbstractDataProviderTypeMap."*list",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class MistralEmbeddingResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "display_name": "Response ID",
                "desc": "Mistral-assigned response ID.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "object": {
                "display_name": "Object Type",
                "desc": "Object type discriminator (typically 'list').",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "model": {
                "display_name": "Model",
                "desc": "The embedding model that produced these vectors.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "data": {
                "display_name": "Embeddings",
                "desc": "List of embedding results, one per input text, in request order.",
                "type": new ListDataType("MistralEmbeddingList",
                    new MistralEmbeddingItemDataType(), True),
            },
            "usage": {
                "display_name": "Usage",
                "desc": "Token usage information.",
                "type": new MistralUsageDataType(),
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class MistralOcrRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "model": {
                "display_name": "Model",
                "short_desc": "Mistral OCR model",
                "desc": "Mistral OCR model identifier, e.g. `mistral-ocr-latest`.",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "mistral-ocr-latest",
            },
            "document": {
                "display_name": "Document",
                "short_desc": "Document to OCR (URL or base64 data URI)",
                "desc": "Hash describing the document.  One of:\n\n"
                    "- `{type: \"document_url\", document_url: \"https://...\"}` — "
                    "remote PDF URL (preferred for large documents)\n"
                    "- `{type: \"image_url\", image_url: \"data:image/png;base64,...\"}` "
                    "— inline base64-encoded image (small inputs only)",
                "type": AbstractDataProviderTypeMap."hash",
                "required": True,
            },
            "pages": {
                "display_name": "Pages",
                "short_desc": "Specific pages to OCR (0-indexed list)",
                "desc": "Optional list of 0-indexed page numbers to OCR.  Defaults to all "
                    "pages.",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "include_image_base64": {
                "display_name": "Include Image Base64",
                "short_desc": "Embed images inline as base64 in the response",
                "desc": "When `True`, embedded images are returned as base64 in the "
                    "response.  When `False` (default), images are referenced by URL.",
                "type": AbstractDataProviderTypeMap."*softbool",
            },
            "image_limit": {
                "display_name": "Image Limit",
                "short_desc": "Maximum number of images to extract",
                "desc": "Cap on the number of images extracted from the document.",
                "type": AbstractDataProviderTypeMap."*softint",
            },
            "image_min_size": {
                "display_name": "Image Min Size",
                "short_desc": "Minimum image size (pixels) to extract",
                "desc": "Skip images smaller than this size in pixels.",
                "type": AbstractDataProviderTypeMap."*softint",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class MistralOcrPageDataType inherits HashDataType {
    private {
        const Fields = {
            "index": {
                "display_name": "Page Index",
                "desc": "0-indexed page number.",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "markdown": {
                "display_name": "Markdown",
                "desc": "Page content as structured markdown (preserves headers, tables, "
                    "lists, inline image references).",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "images": {
                "display_name": "Images",
                "desc": "List of images extracted from this page.",
                "type": AbstractDataProviderTypeMap."*list",
            },
            "dimensions": {
                "display_name": "Dimensions",
                "desc": "Page dimensions in pixels (`{dpi, height, width}`).",
                "type": AbstractDataProviderTypeMap."*hash",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}

public class MistralOcrResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "model": {
                "display_name": "Model",
                "desc": "The OCR model that processed this document.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "pages": {
                "display_name": "Pages",
                "desc": "List of OCR'd pages with structured markdown.",
                "type": new ListDataType("MistralOcrPageList",
                    new MistralOcrPageDataType(), True),
            },
            "usage_info": {
                "display_name": "Usage Info",
                "desc": "Mistral OCR usage hash (`pages_processed`, `doc_size_bytes`).",
                "type": AbstractDataProviderTypeMap."*hash",
            },
        };
    }
    constructor() { addQoreFields(Fields); }
}
}
