# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore DeepSeekChatCompletionResponseDataType class definition

/** DeepSeekChatCompletionResponseDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the DeepSeekDataProvider module
public namespace DeepSeekDataProvider {
#! Data type for the assistant message returned by a DeepSeek chat completion
public class DeepSeekChatResponseMessageDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "role": {
                "display_name": "Role",
                "desc": "Always `assistant` for response messages.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "content": {
                "display_name": "Content",
                "desc": "The text content of the assistant's reply.  May be NOTHING if the response "
                    "is a tool call only.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "reasoning_content": {
                "display_name": "Reasoning Content",
                "desc": "Chain-of-thought reasoning emitted by DeepSeek reasoner models "
                    "(e.g. `deepseek-reasoner`).  Empty / not present for non-reasoner models.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "tool_calls": {
                "display_name": "Tool Calls",
                "desc": "List of tool invocations emitted by the assistant, if any.",
                "type": new ListDataType("DeepSeekChatToolCallList",
                    new DeepSeekChatToolCallDataType(), True),
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data type for a single choice in a chat completion response
public class DeepSeekChatChoiceDataType inherits HashDataType {
    private {
        #! Field definitions
        const Fields = {
            "index": {
                "display_name": "Index",
                "desc": "Zero-based index of this choice in the `choices` list.",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "message": {
                "display_name": "Message",
                "desc": "The assistant message produced by the model.",
                "type": new DeepSeekChatResponseMessageDataType(),
            },
            "finish_reason": {
                "display_name": "Finish Reason",
                "desc": "Why generation stopped.  Typical values: `stop` (natural completion or "
                    "stop sequence), `length` (hit `max_tokens`), `tool_calls` (model invoked a "
                    "tool), `content_filter` (output was blocked).",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "logprobs": {
                "display_name": "Log Probabilities",
                "desc": "Per-token log-probability information (only returned when requested).",
                "type": AbstractDataProviderTypeMap."*hash",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data type for DeepSeek token-usage reporting
public class DeepSeekUsageDataType inherits HashDataType {
    private {
        #! Field definitions
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
                "desc": "Total number of tokens processed (prompt + completion).",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "prompt_cache_hit_tokens": {
                "display_name": "Prompt Cache Hit Tokens",
                "desc": "Number of prompt tokens served from DeepSeek's prompt cache (billed at "
                    "a discount).",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "prompt_cache_miss_tokens": {
                "display_name": "Prompt Cache Miss Tokens",
                "desc": "Number of prompt tokens that missed the cache (billed at the full rate).",
                "type": AbstractDataProviderTypeMap."*int",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data type for DeepSeek chat completion responses (OpenAI-compatible)
public class DeepSeekChatCompletionResponseDataType inherits HashDataType {
    private {
        #! Field definitions
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
                "desc": "When the completion was created.  DeepSeek emits a Unix-seconds integer "
                    "on the wire; the data provider converts it to a Qore date before returning.",
                "type": AbstractDataProviderTypeMap."*date",
            },
            "model": {
                "display_name": "Model",
                "desc": "The model that produced this completion.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "system_fingerprint": {
                "display_name": "System Fingerprint",
                "desc": "An opaque identifier of the backend model+config combination that served "
                    "this request — useful when correlating outputs to a specific server version.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "choices": {
                "display_name": "Choices",
                "desc": "List of completion choices.  Normally contains exactly one entry unless "
                    "the request asked for multiple completions.",
                "type": new ListDataType("DeepSeekChatChoiceList",
                    new DeepSeekChatChoiceDataType(), True),
            },
            "usage": {
                "display_name": "Usage",
                "desc": "Token usage information for this request, including DeepSeek's "
                    "prompt-cache hit/miss breakdown.",
                "type": new DeepSeekUsageDataType(),
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}
}
