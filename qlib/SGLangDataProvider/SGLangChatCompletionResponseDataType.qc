# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore SGLangDataProvider module definition

/** SGLangChatCompletionResponseDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Data type for a single choice in a chat completion response
public class SGLangChatChoiceDataType inherits HashDataType {
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
                "type": new SGLangChatMessageDataType(),
            },
            "finish_reason": {
                "display_name": "Finish Reason",
                "desc": "Why generation stopped.  Typical values: `stop` (natural completion or stop "
                    "sequence), `length` (hit `max_tokens`), `tool_calls` (model invoked a tool).",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "logprobs": {
                "display_name": "Log Probabilities",
                "desc": "Optional per-token log probability information (only returned when requested).",
                "type": AbstractDataProviderTypeMap."*hash",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}

#! Data type for SGLang chat completion responses (OpenAI-compatible)
public class SGLangChatCompletionResponseDataType inherits HashDataType {
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
                "desc": "Always `chat.completion` for non-streaming responses.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "created": {
                "display_name": "Created",
                "desc": "Unix timestamp (seconds) when the completion was created.",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "model": {
                "display_name": "Model",
                "desc": "The model that produced this completion.",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "choices": {
                "display_name": "Choices",
                "desc": "List of completion choices.  Normally contains exactly one entry unless the "
                    "request asked for multiple completions.",
                "type": new ListDataType("SGLangChatChoiceList", new SGLangChatChoiceDataType(), True),
            },
            "usage": {
                "display_name": "Usage",
                "desc": "Token usage information for this request.",
                "type": new SGLangUsageDataType(),
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }
}
}
