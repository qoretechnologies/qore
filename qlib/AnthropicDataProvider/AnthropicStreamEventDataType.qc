# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AnthropicDataProvider module definition

/** AnthropicStreamEventDataType.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! contains all public definitions in the AnthropicDataProvider module
public namespace AnthropicDataProvider {
#! Stream event constant
public const EVENT_ANTHROPIC_MESSAGE_STREAM = "anthropic-message-stream-event";

#! Delta data type for Anthropic streaming content_block_delta and message_delta events
/** Covers all delta variants:
    - `text_delta`: incremental text content
    - `input_json_delta`: incremental tool input JSON
    - `thinking_delta`: incremental thinking content
    - `signature_delta`: thinking block integrity signature
    - `citations_delta`: citation to append
    - message-level delta with `stop_reason` and `stop_sequence`
*/
public class AnthropicDeltaDataType inherits HashDataType {
    public {
        const Fields = {
            "type": {
                "display_name": "Delta Type",
                "short_desc": "The type of delta content",
                "desc": "The type of delta: `text_delta`, `input_json_delta`, `thinking_delta`, "
                    "`signature_delta`, or `citations_delta`",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "text": {
                "display_name": "Text",
                "short_desc": "Incremental text content",
                "desc": "Incremental text content. Present when `type` is `text_delta`",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "partial_json": {
                "display_name": "Partial JSON",
                "short_desc": "Incremental tool input JSON",
                "desc": "Partial JSON string for tool input. Present when `type` is `input_json_delta`",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "thinking": {
                "display_name": "Thinking",
                "short_desc": "Incremental thinking content",
                "desc": "Incremental thinking content. Present when `type` is `thinking_delta`",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "signature": {
                "display_name": "Signature",
                "short_desc": "Thinking block integrity signature",
                "desc": "Integrity signature for a thinking block. Present when `type` is `signature_delta`",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "stop_reason": {
                "display_name": "Stop Reason",
                "short_desc": "Why the model stopped generating",
                "desc": "The reason the model stopped. Present in `message_delta` events",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "stop_sequence": {
                "display_name": "Stop Sequence",
                "short_desc": "Which stop sequence matched",
                "desc": "Which custom stop sequence was generated. Present in `message_delta` events when "
                    "`stop_reason` is `stop_sequence`",
                "type": AbstractDataProviderTypeMap."*string",
            },
        };
    }

    #! Creates the object
    constructor() {
        addQoreFields(Fields);
    }

    #! Creates the object with the given base type
    constructor(Type base_type) : HashDataType(base_type) {
        addQoreFields(Fields);
    }
}

#! Stream event data type for Anthropic SSE messages
public class AnthropicStreamEventDataType inherits HashDataType {
    public {
        const Fields = {
            "type": {
                "display_name": "Event Type",
                "short_desc": "The stream event type",
                "desc": "The stream event type from the Anthropic API: `message_start`, `content_block_start`, "
                    "`content_block_delta`, `content_block_stop`, `message_delta`, `message_stop`, `ping`, or "
                    "`error`",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "index": {
                "display_name": "Content Block Index",
                "short_desc": "Index of the content block",
                "desc": "Index of the content block (present in `content_block_start`, `content_block_delta`, "
                    "and `content_block_stop` events)",
                "type": AbstractDataProviderTypeMap."*int",
            },
            "message": {
                "display_name": "Message",
                "short_desc": "Full message object (on message_start)",
                "desc": "Full message object, present on `message_start` events. Contains `id`, `type`, "
                    "`role`, `content`, `model`, `stop_reason`, `stop_sequence`, and `usage`",
                "type": new AnthropicMessageResponseDataType(AutoHashOrNothingType),
            },
            "content_block": {
                "display_name": "Content Block",
                "short_desc": "Content block info (on content_block_start)",
                "desc": "Content block information, present on `content_block_start` events. Contains `type` "
                    "and type-specific fields (e.g. `text` for text blocks, `id`/`name`/`input` for tool_use)",
                "type": new AnthropicContentBlockDataType(AutoHashOrNothingType),
            },
            "delta": {
                "display_name": "Delta",
                "short_desc": "Incremental data (on delta events)",
                "desc": "Incremental data, present on `content_block_delta` and `message_delta` events. "
                    "For text deltas: `{\"type\": \"text_delta\", \"text\": \"...\"}`. "
                    "For tool input deltas: `{\"type\": \"input_json_delta\", \"partial_json\": \"...\"}`",
                "type": new AnthropicDeltaDataType(AutoHashOrNothingType),
            },
            "usage": {
                "display_name": "Usage",
                "short_desc": "Token usage (on message_delta)",
                "desc": "Token usage information, present on `message_delta` events",
                "type": new AnthropicUsageDataType(AutoHashOrNothingType),
            },
            "event": {
                "display_name": "Event Name",
                "short_desc": "SSE event name",
                "desc": "SSE event name from the server",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "id": {
                "display_name": "Event ID",
                "short_desc": "SSE event ID",
                "desc": "SSE event ID from the server",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "raw_data": {
                "display_name": "Raw Data",
                "short_desc": "Raw event data",
                "desc": "Raw event data when JSON parsing is not possible",
                "type": AbstractDataProviderTypeMap."*string",
            },
            "error": {
                "display_name": "Error",
                "short_desc": "Error payload for stream failures",
                "desc": "Exception info when a stream error occurs",
                "type": AbstractDataProviderTypeMap."*hash",
            },
        };
    }

    #! Creates the object from the arguments
    constructor() {
        addQoreFields(Fields);
        setDefaultOtherFieldType(AbstractDataProviderTypeMap."any");
    }
}

#! Singleton instance
public const AnthropicStreamEventDataType = new AnthropicStreamEventDataType();
}
