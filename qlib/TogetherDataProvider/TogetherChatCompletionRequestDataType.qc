# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore TogetherChatCompletionRequestDataType class definition

/** TogetherChatCompletionRequestDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Data type for a single tool call produced by a Together AI assistant message
public class TogetherChatToolCallDataType inherits HashDataType {
    public {
        const Fields = {
            "id": {
                "display_name": "Tool Call ID",
                "short_desc": "Unique identifier for the tool call",
                "desc": "Unique identifier for this tool call.",
                "type": AbstractDataProviderTypeMap."string",
            },
            "type": {
                "display_name": "Tool Type",
                "short_desc": "Type of tool invoked",
                "desc": "Type of tool invoked.  Currently `function` is the only supported type.",
                "type": AbstractDataProviderTypeMap."string",
                "example_value": "function",
            },
            "function": {
                "display_name": "Function Call",
                "short_desc": "Function name and JSON-encoded arguments",
                "desc": "Hash with `name` (string) and `arguments` (JSON-encoded string).",
                "type": AbstractDataProviderTypeMap."hash",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data type for a single chat message
public class TogetherChatMessageDataType inherits HashDataType {
    public {
        const Fields = {
            "role": {
                "display_name": "Role",
                "short_desc": "The role of the message author",
                "desc": "The role of the author of this message: `system`, `user`, `assistant`, "
                    "or `tool`.",
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
                "desc": "The text content of the message.  May be NOTHING on assistant messages "
                    "that only contain `tool_calls`.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "name": {
                "display_name": "Author Name",
                "short_desc": "Optional name of the author",
                "desc": "Optional name identifying the specific author of the message.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "tool_call_id": {
                "display_name": "Tool Call ID",
                "short_desc": "ID of the tool call this message responds to",
                "desc": "Required when `role` is `tool` — identifies which assistant `tool_calls` "
                    "entry this message is returning the result for.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "tool_calls": {
                "display_name": "Tool Calls",
                "short_desc": "Tool invocations emitted by the assistant",
                "desc": "List of tool calls emitted by the assistant.",
                "type": new ListDataType("TogetherChatToolCallList",
                    new TogetherChatToolCallDataType(), True),
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data type for Together AI chat completion requests (OpenAI-compatible)
public class TogetherChatCompletionRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "model": {
                "display_name": "Model",
                "short_desc": "The Together-hosted model to use",
                "desc": "Together model identifier, e.g. "
                    "`meta-llama/Llama-3.3-70B-Instruct-Turbo`, "
                    "`Qwen/Qwen2.5-72B-Instruct-Turbo`, "
                    "`deepseek-ai/DeepSeek-V3`.\n\n"
                    "Consult the [Together AI models list](https://docs.together.ai/docs/inference-models) "
                    "for the currently-served set.",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "meta-llama/Llama-3.3-70B-Instruct-Turbo",
            },
            "messages": {
                "display_name": "Messages",
                "short_desc": "List of chat messages forming the conversation",
                "desc": "The conversation history as a list of message objects.",
                "type": new ListDataType("TogetherChatMessageList",
                    new TogetherChatMessageDataType(), False),
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
                "short_desc": "Maximum tokens to emit in the completion",
                "desc": "Maximum number of tokens to generate.",
                "type": AbstractDataProviderTypeMap."*softint",
                "example_value": 1024,
            },
            "top_p": {
                "display_name": "Top-P",
                "short_desc": "Nucleus sampling cutoff (0-1)",
                "desc": "Nucleus sampling cutoff.  Range `0.0` to `1.0`.",
                "type": AbstractDataProviderTypeMap."*softfloat",
            },
            "top_k": {
                "display_name": "Top-K",
                "short_desc": "Sample only from the K most likely next tokens",
                "desc": "Restrict sampling to the K most likely next tokens.  Together-specific "
                    "extension to the OpenAI chat API.",
                "type": AbstractDataProviderTypeMap."*softint",
            },
            "repetition_penalty": {
                "display_name": "Repetition Penalty",
                "short_desc": "Together's repetition penalty (separate from frequency_penalty)",
                "desc": "Penalize repeated tokens.  Together-specific extension.  Values "
                    "above `1.0` discourage repetition; values below `1.0` encourage it.",
                "type": AbstractDataProviderTypeMap."*softfloat",
            },
            "frequency_penalty": {
                "display_name": "Frequency Penalty",
                "short_desc": "OpenAI-style frequency penalty",
                "desc": "Penalizes tokens by frequency.  Range `-2.0` to `2.0`.",
                "type": AbstractDataProviderTypeMap."*softfloat",
            },
            "presence_penalty": {
                "display_name": "Presence Penalty",
                "short_desc": "OpenAI-style presence penalty",
                "desc": "Penalizes tokens that have already appeared.  Range `-2.0` to `2.0`.",
                "type": AbstractDataProviderTypeMap."*softfloat",
            },
            "stop": {
                "display_name": "Stop Sequences",
                "short_desc": "String or list of strings; generation halts when emitted",
                "desc": "One or more stop sequences.",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "stream": {
                "display_name": "Stream",
                "short_desc": "Stream partial completions as Server-Sent Events",
                "desc": "When `True`, the server streams partial completions as SSE.",
                "type": AbstractDataProviderTypeMap."*softbool",
            },
            "tools": {
                "display_name": "Tools",
                "short_desc": "OpenAI-style tool definitions the model may call",
                "desc": "List of tool definitions the model may invoke.",
                "type": AbstractDataProviderTypeMap."*softlist",
            },
            "tool_choice": {
                "display_name": "Tool Choice",
                "short_desc": "Whether and which tool the model must invoke",
                "desc": "Tool invocation mode: `none`, `auto`, or `required`.",
                "type": AbstractDataProviderTypeMap."*softstring",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "none",
                        "display_name": "None - never call a tool"},
                    <AllowedValueInfo>{"value": "auto",
                        "display_name": "Auto - model decides"},
                    <AllowedValueInfo>{"value": "required",
                        "display_name": "Required - must call a tool"},
                ),
            },
            "response_format": {
                "display_name": "Response Format",
                "short_desc": "Force structured output (e.g. JSON)",
                "desc": "Hash that forces a structured response shape, e.g. "
                    "`{type: \"json_object\"}` or `{type: \"json_schema\", schema: {...}}`.",
                "type": AbstractDataProviderTypeMap."*hash",
                "example_value": {"type": "json_object"},
            },
            "seed": {
                "display_name": "Seed",
                "short_desc": "Deterministic sampling seed",
                "desc": "Integer seed for deterministic sampling.",
                "type": AbstractDataProviderTypeMap."*softint",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data type for Together AI embedding requests (OpenAI-compatible)
public class TogetherEmbeddingRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "model": {
                "display_name": "Model",
                "short_desc": "The Together-hosted embedding model to use",
                "desc": "Together embedding model identifier, e.g. `BAAI/bge-base-en-v1.5`, "
                    "`BAAI/bge-large-en-v1.5`, `togethercomputer/m2-bert-80M-32k-retrieval`.",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "BAAI/bge-base-en-v1.5",
            },
            "input": {
                "display_name": "Input",
                "short_desc": "Text(s) to embed — single string or list of strings",
                "desc": "Text input(s) to convert to embedding vectors.  Can be a single string "
                    "or a list of strings for batch embedding.",
                "type": AbstractDataProviderTypeMap."softlist",
                "required": True,
                "example_value": ("Hello, world!",),
            },
            "encoding_format": {
                "display_name": "Encoding Format",
                "short_desc": "Wire format for the returned vectors",
                "desc": "`float` (default) returns each vector as a JSON list of floats; "
                    "`base64` returns base64-encoded little-endian float32 buffers (smaller "
                    "wire size).",
                "type": AbstractDataProviderTypeMap."*softstring",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "float",
                        "display_name": "Float - JSON list of floats"},
                    <AllowedValueInfo>{"value": "base64",
                        "display_name": "Base64 - encoded float32 buffer"},
                ),
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data type for Together AI image generation requests
public class TogetherImageGenerationRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "model": {
                "display_name": "Model",
                "short_desc": "The Together-hosted image-generation model",
                "desc": "Together image model identifier.  Examples:\n\n"
                    "- `black-forest-labs/FLUX.1-schnell-Free` — fast FLUX.1 (free tier)\n"
                    "- `black-forest-labs/FLUX.1-schnell` — fast FLUX.1\n"
                    "- `black-forest-labs/FLUX.1-dev` — quality FLUX.1\n"
                    "- `stabilityai/stable-diffusion-xl-base-1.0` — SDXL 1.0",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "black-forest-labs/FLUX.1-schnell-Free",
            },
            "prompt": {
                "display_name": "Prompt",
                "short_desc": "Text description of the image to generate",
                "desc": "Free-form text describing the image to generate.  Diffusion models "
                    "are sensitive to wording — be specific about subject, lighting, style, "
                    "and composition for best results.",
                "type": AbstractDataProviderTypeMap."string",
                "required": True,
                "example_value": "A serene mountain lake at sunrise, photorealistic",
            },
            "width": {
                "display_name": "Width",
                "short_desc": "Image width in pixels",
                "desc": "Image width in pixels.  Must match a resolution the chosen model "
                    "supports — typically multiples of 64 in the 256-1792 range.",
                "type": AbstractDataProviderTypeMap."*softint",
                "example_value": 1024,
            },
            "height": {
                "display_name": "Height",
                "short_desc": "Image height in pixels",
                "desc": "Image height in pixels.  See `width` for resolution constraints.",
                "type": AbstractDataProviderTypeMap."*softint",
                "example_value": 1024,
            },
            "steps": {
                "display_name": "Steps",
                "short_desc": "Number of diffusion steps",
                "desc": "Number of denoising steps.  Higher values improve quality at the cost "
                    "of latency.  FLUX schnell is tuned for 1-4 steps; SDXL typically uses "
                    "20-50.",
                "type": AbstractDataProviderTypeMap."*softint",
                "example_value": 4,
            },
            "n": {
                "display_name": "Number of Images",
                "short_desc": "How many images to generate",
                "desc": "Number of images to generate from the prompt.",
                "type": AbstractDataProviderTypeMap."*softint",
                "example_value": 1,
            },
            "response_format": {
                "display_name": "Response Format",
                "short_desc": "Whether to return URLs or base64-encoded image bytes",
                "desc": "`url` (default) returns hosted image URLs.  `b64_json` returns the "
                    "image bytes inline as base64-encoded JSON.",
                "type": AbstractDataProviderTypeMap."*softstring",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "url",
                        "display_name": "URL - hosted image URL"},
                    <AllowedValueInfo>{"value": "b64_json",
                        "display_name": "Base64 - inline image bytes"},
                ),
            },
            "seed": {
                "display_name": "Seed",
                "short_desc": "Deterministic generation seed",
                "desc": "Integer seed.  With a fixed seed, prompt, and model, generation is "
                    "reproducible.",
                "type": AbstractDataProviderTypeMap."*softint",
            },
            "negative_prompt": {
                "display_name": "Negative Prompt",
                "short_desc": "What the image should NOT contain",
                "desc": "Optional negative prompt — features the model should suppress in the "
                    "output (e.g. `\"blurry, low quality, watermark\"`).",
                "type": AbstractDataProviderTypeMap."*softstring",
            },
            "guidance": {
                "display_name": "Guidance Scale",
                "short_desc": "Prompt adherence (CFG scale)",
                "desc": "Classifier-free guidance scale.  Higher values force the output to "
                    "follow the prompt more closely at the cost of diversity.",
                "type": AbstractDataProviderTypeMap."*softfloat",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}
}
