# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore TogetherChatCompletionResponseDataType class definition

/** TogetherChatCompletionResponseDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace TogetherDataProvider {
#! Data type for the assistant message returned by a Together AI chat completion
public class TogetherChatResponseMessageDataType inherits HashDataType {
    private {
        const Fields = {
            "role": {
                "display_name": "Role",
                "desc": "Always `assistant` for response messages.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "content": {
                "display_name": "Content",
                "desc": "The text content of the assistant's reply.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "tool_calls": {
                "display_name": "Tool Calls",
                "desc": "List of tool invocations emitted by the assistant, if any.",
                "type": new ListDataType("TogetherChatToolCallList",
                    new TogetherChatToolCallDataType(), True),
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data type for a single choice in a chat completion response
public class TogetherChatChoiceDataType inherits HashDataType {
    private {
        const Fields = {
            "index": {
                "display_name": "Index",
                "desc": "Zero-based index of this choice.",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "message": {
                "display_name": "Message",
                "desc": "The assistant message produced by the model.",
                "type": new TogetherChatResponseMessageDataType(),
            },
            "finish_reason": {
                "display_name": "Finish Reason",
                "desc": "Why generation stopped — `stop`, `length`, `tool_calls`, etc.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "logprobs": {
                "display_name": "Log Probabilities",
                "desc": "Per-token log-probability info (when requested).",
                "type": AbstractDataProviderTypeMap."*hash",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data type for Together AI token-usage reporting
public class TogetherUsageDataType inherits HashDataType {
    private {
        const Fields = {
            "prompt_tokens": {
                "display_name": "Prompt Tokens",
                "desc": "Number of tokens consumed by the prompt.",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "completion_tokens": {
                "display_name": "Completion Tokens",
                "desc": "Number of tokens emitted in the completion.",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "total_tokens": {
                "display_name": "Total Tokens",
                "desc": "Total number of tokens processed.",
                "type": AbstractDataProviderTypeMap."*int",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data type for Together AI chat completion responses (OpenAI-compatible)
public class TogetherChatCompletionResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "display_name": "Completion ID",
                "desc": "Unique identifier for this completion.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "object": {
                "display_name": "Object Type",
                "desc": "Object type discriminator (typically 'chat.completion').",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "created": {
                "display_name": "Created",
                "desc": "When the completion was created.  Together emits Unix seconds; the "
                    "data provider converts it to a Qore date before returning.",
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
                "type": new ListDataType("TogetherChatChoiceList",
                    new TogetherChatChoiceDataType(), True),
            },
            "usage": {
                "display_name": "Usage",
                "desc": "Token usage information for this request.",
                "type": new TogetherUsageDataType(),
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data type for a single embedding result
public class TogetherEmbeddingItemDataType inherits HashDataType {
    private {
        const Fields = {
            "index": {
                "display_name": "Index",
                "desc": "Position of this embedding in the input list.",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "object": {
                "display_name": "Object Type",
                "desc": "Object type discriminator (typically 'embedding').",
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

#! Data type for Together AI embedding responses
public class TogetherEmbeddingResponseDataType inherits HashDataType {
    private {
        const Fields = {
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
                "type": new ListDataType("TogetherEmbeddingList",
                    new TogetherEmbeddingItemDataType(), True),
            },
            "usage": {
                "display_name": "Usage",
                "desc": "Token usage information for this embedding request.",
                "type": new TogetherUsageDataType(),
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data type for a single generated image
public class TogetherImageItemDataType inherits HashDataType {
    private {
        const Fields = {
            "url": {
                "display_name": "Image URL",
                "desc": "URL of the generated image (when `response_format` is `url`).",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "b64_json": {
                "display_name": "Image Bytes (Base64)",
                "desc": "Base64-encoded image bytes (when `response_format` is `b64_json`).",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "index": {
                "display_name": "Index",
                "desc": "Position of this image in the output list.",
                "type": AbstractDataProviderTypeMap."*int",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data type for Together AI image-generation responses
public class TogetherImageGenerationResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "id": {
                "display_name": "Generation ID",
                "desc": "Unique identifier for this generation request.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "model": {
                "display_name": "Model",
                "desc": "The image-generation model that produced these images.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "object": {
                "display_name": "Object Type",
                "desc": "Object type discriminator (typically 'list').",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "created": {
                "display_name": "Created",
                "desc": "When the generation was completed.  Together emits Unix seconds; the "
                    "data provider converts it to a Qore date before returning.",
                "type": AbstractDataProviderTypeMap."*date",
            },
            "data": {
                "display_name": "Images",
                "desc": "List of generated images.",
                "type": new ListDataType("TogetherImageList",
                    new TogetherImageItemDataType(), True),
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}
}
