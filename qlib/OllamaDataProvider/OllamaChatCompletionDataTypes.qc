# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore OllamaChatCompletion data type definitions

/** OllamaChatCompletionDataTypes.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace OllamaDataProvider {
public class OllamaChatMessageDataType inherits HashDataType {
    public {
        const Fields = {
            "role": {
                "display_name": "Role",
                "short_desc": "The role of the message author",
                "desc": "The role of the author of this message: `system`, `user`, "
                    "`assistant`, or `tool`.",
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
                "short_desc": "The text content of the message",
                "desc": "The text content of the message.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "images": {
                "display_name": "Images",
                "short_desc": "List of base64-encoded images (multimodal models)",
                "desc": "List of base64-encoded image strings.  Used with multimodal Ollama "
                    "models (e.g. `llava`, `bakllava`).",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "tool_calls": {
                "display_name": "Tool Calls",
                "short_desc": "Tool invocations emitted by the assistant",
                "desc": "List of tool calls emitted by the assistant.",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public class OllamaChatCompletionRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "model": {
                "display_name": "Model",
                "short_desc": "Ollama model name (must be installed locally)",
                "desc": "Name of the installed Ollama model, e.g. `llama3.3`, "
                    "`qwen2.5:14b`, `mistral-nemo`.  Use the `list-models` action to see "
                    "what's installed and `ollama pull <model>` on the host to install new ones.",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "llama3.3",
            },
            "messages": {
                "display_name": "Messages",
                "short_desc": "List of chat messages forming the conversation",
                "desc": "The conversation history as a list of message objects.",
                "type": new ListDataType("OllamaChatMessageList",
                    new OllamaChatMessageDataType(), False),
                "required": True,
                "example_value": (
                    {"role": "user", "content": "Hello!"},
                ),
            },
            "temperature": {
                "display_name": "Temperature",
                "short_desc": "Sampling temperature (0-2)",
                "desc": "Controls randomness of the output.  Range `0.0` to `2.0`.",
                "type": AbstractDataProviderTypeMap."*softfloat",
                "example_value": 0.7,
            },
            "max_tokens": {
                "display_name": "Max Tokens",
                "short_desc": "Maximum tokens to emit",
                "desc": "Maximum number of tokens to generate.",
                "type": AbstractDataProviderTypeMap."*softint",
            },
            "top_p": {
                "display_name": "Top-P",
                "short_desc": "Nucleus sampling cutoff (0-1)",
                "desc": "Nucleus sampling cutoff.",
                "type": AbstractDataProviderTypeMap."*softfloat",
            },
            "stop": {
                "display_name": "Stop Sequences",
                "short_desc": "Stop sequences",
                "desc": "String or list of strings; generation halts when emitted.",
                "type": AbstractDataProviderTypeMap."*softlist",
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
                "desc": "List of tool definitions the model may invoke.",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "response_format": {
                "display_name": "Response Format",
                "short_desc": "Force structured output (e.g. JSON)",
                "desc": "Hash that forces a structured response shape, e.g. "
                    "`{type: \"json_object\"}` or `{type: \"json_schema\", schema: {...}}`.",
                "type": AbstractDataProviderTypeMap."*hash",
                "example_value": {"type": "json_object"},
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public class OllamaChatChoiceDataType inherits HashDataType {
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
                "type": AbstractDataProviderTypeMap."*hash",
            },
            "finish_reason": {
                "display_name": "Finish Reason",
                "desc": "Why generation stopped — `stop`, `length`, `tool_calls`.",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public class OllamaUsageDataType inherits HashDataType {
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
                "desc": "Total tokens processed.",
                "type": AbstractDataProviderTypeMap."*int",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public class OllamaChatCompletionResponseDataType inherits HashDataType {
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
                "desc": "When the completion was created.  Ollama emits Unix seconds; "
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
                "type": new ListDataType("OllamaChatChoiceList",
                    new OllamaChatChoiceDataType(), True),
            },
            "usage": {
                "display_name": "Usage",
                "desc": "Token usage information for this request.",
                "type": new OllamaUsageDataType(),
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public class OllamaEmbedRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "model": {
                "display_name": "Model",
                "short_desc": "Ollama embedding model name",
                "desc": "Name of the installed Ollama embedding model, e.g. "
                    "`nomic-embed-text`, `mxbai-embed-large`, `snowflake-arctic-embed`.",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "nomic-embed-text",
            },
            "input": {
                "display_name": "Input",
                "short_desc": "Text(s) to embed",
                "desc": "Text input(s) to convert to embedding vectors.  Can be a single "
                    "string or a list of strings for batch embedding.",
                "type": AbstractDataProviderTypeMap."softlist",
                "required": True,
                "example_value": ("Hello, world!",),
            },
            "truncate": {
                "display_name": "Truncate",
                "short_desc": "Truncate inputs that exceed the model context window",
                "desc": "When `True` (default), inputs longer than the model's context window "
                    "are truncated.  When `False`, the request errors instead.",
                "type": AbstractDataProviderTypeMap."*softbool",
            },
            "options": {
                "display_name": "Model Options",
                "short_desc": "Per-model runtime options (`num_ctx`, etc.)",
                "desc": "Hash of model-specific runtime options.  Ollama-specific extension; "
                    "common keys include `num_ctx`, `num_thread`.",
                "type": AbstractDataProviderTypeMap."*hash",
            },
            "keep_alive": {
                "display_name": "Keep Alive",
                "short_desc": "How long the model stays loaded after the request",
                "desc": "Duration string controlling how long Ollama keeps the model loaded "
                    "in memory after the request completes (e.g. `\"5m\"`, `\"1h\"`, "
                    "`\"0\"` for immediate unload).",
                "type": AbstractDataProviderTypeMap."*softstring",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public class OllamaEmbedResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "model": {
                "display_name": "Model",
                "desc": "The embedding model that produced these vectors.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "embeddings": {
                "display_name": "Embeddings",
                "desc": "List of embedding vectors, one per input text, in request order.  "
                    "Each vector is a list of floats.",
                "type": AbstractDataProviderTypeMap."*list",
            },
            "total_duration": {
                "display_name": "Total Duration (ns)",
                "desc": "Total wall-clock time the request took, in nanoseconds.",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "load_duration": {
                "display_name": "Load Duration (ns)",
                "desc": "Time spent loading the model into memory, in nanoseconds.",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "prompt_eval_count": {
                "display_name": "Prompt Token Count",
                "desc": "Number of prompt tokens processed.",
                "type": AbstractDataProviderTypeMap."*int",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public class OllamaModelInfoDataType inherits HashDataType {
    private {
        const Fields = {
            "name": {
                "display_name": "Name",
                "desc": "Model name (e.g. `llama3.3:latest`).",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "modified_at": {
                "display_name": "Modified At",
                "desc": "When this model was last modified locally.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "size": {
                "display_name": "Size (bytes)",
                "desc": "Disk size of the model in bytes.",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "digest": {
                "display_name": "Digest",
                "desc": "Content-addressed digest of the model.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "details": {
                "display_name": "Details",
                "desc": "Model metadata (family, parameter count, quantization, etc.).",
                "type": AbstractDataProviderTypeMap."*hash",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

public class OllamaListModelsResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "models": {
                "display_name": "Models",
                "desc": "List of locally-installed models.",
                "type": new ListDataType("OllamaModelInfoList",
                    new OllamaModelInfoDataType(), True),
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}
}
