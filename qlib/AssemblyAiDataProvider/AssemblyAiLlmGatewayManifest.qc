# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AssemblyAiDataProvider API action manifest

/** AssemblyAiLlmGatewayManifest.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace AssemblyAiDataProvider {
#! Curated actions and resource selections for the llm-gateway API
public class AssemblyAiLlmGatewayManifest {
    public {
        #! Resource listings used by action dropdowns
        const hash<string, hash<RestSchemaReferenceDataInfo>> ReferenceData = {
            "models": <RestSchemaReferenceDataInfo>{"name": "models", "action": "list-models", "records_path": ("data",), "value_field": "id", "display_field": "name", "desc": "Recent models; values from earlier workflow steps are also accepted."},
        };

        #! Stable action contracts resolved against the vendored API description
        const list<hash<RestSchemaActionInfo>> Manifest = (
            <RestSchemaActionInfo>{
                "action": "create-chat-completion",
                "method": "POST",
                "path": "/chat/completions",
                "operation_id": "createChatCompletion",
                "display_name": "Create Chat Completion",
                "short_desc": "Generate a model response with LLM Gateway",
                "desc": "Generate a completed response from messages or a prompt. Optionally inject a transcript, call tools, or request structured JSON output. Model availability and supported parameters vary by region.",
                "groups": ("Language Models",),
                "fields": {
                    "model": <RestSchemaFieldOverlayInfo>{"ref_data": "models", "supports_custom_values": True},
                    "transcript_id": <RestSchemaFieldOverlayInfo>{"ref_data": "transcripts", "supports_custom_values": True},
                    "stream": <RestSchemaFieldOverlayInfo>{"allowed_values": (<AllowedValueInfo>{"value": False, "display_name": "Completed Response"},), "desc": "This action returns a completed response. Use the native client for server-sent events."},
                },
            },
            <RestSchemaActionInfo>{
                "action": "understand-speech",
                "method": "POST",
                "path": "/understanding",
                "operation_id": "createSpeechUnderstanding",
                "display_name": "Understand Speech",
                "short_desc": "Analyze or transform an existing transcript",
                "desc": "Translate, summarize, identify speakers, format text, or extract action items from a completed transcript. Configure the required tasks in `speech_understanding`.",
                "groups": ("Language Models",),
                "fields": {
                    "transcript_id": <RestSchemaFieldOverlayInfo>{"ref_data": "transcripts", "supports_custom_values": True},
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-models",
                "method": "GET",
                "path": "/models",
                "operation_id": "listAvailableModels",
                "display_name": "List LLM Models",
                "short_desc": "Discover LLM Gateway models and their capabilities",
                "desc": "List available LLM Gateway models, supported parameters, context limits, pricing, and regional availability.",
                "groups": ("Language Models",),
            },
        );
    }
}
}
