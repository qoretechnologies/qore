# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore GroqActions class definition

/** GroqActions.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace GroqDataProvider {
#! Contract and routing for one Groq action
public hashdecl GroqActionInfo {
    #! Action display name
    string display_name;
    #! Plain action summary
    string short_desc;
    #! Markdown action description
    string desc;
    #! Action group
    string group;
    #! HTTP method
    string method;
    #! Endpoint path
    string path;
    #! Typed input, or nothing for operations without input
    *AbstractDataProviderType request_type;
    #! Typed output
    AbstractDataProviderType response_type;
    #! Input fields used to build action options
    hash<string, hash<auto>> fields;
}

#! Documented Groq REST operations and action presentation
/** The public API reference and current feature guides define these contracts. The embedded
    OpenAPI description omits current audio and LoRA features and also contains hidden operations.
    See the module documentation for coverage and model-specific restrictions.
*/
public class GroqActions {
    public {
        #! Immutable operation definitions
        const hash<string, hash<GroqActionInfo>> Operations = {
            "get-model": <GroqActionInfo>{
                "display_name": "Get Model",
                "short_desc": "Inspect a model before selecting it for inference",
                "desc": "Inspect a model before selecting it for inference.",
                "group": "Models",
                "method": "GET",
                "path": "models/{model}",
                "request_type": new GroqModelRequestDataType(),
                "response_type": new GroqModelDataType(),
                "fields": GroqModelRequestDataType::Fields,
            },
            "create-response": <GroqActionInfo>{
                "display_name": "Create Response",
                "short_desc": "Generate a response with structured output and tool calls",
                "desc": "Generate a response with structured output and tool calls.\n\nUses the beta Responses API. Select a compatible model. Returns a completed response; streaming and stored response continuation are not supported by this action.",
                "group": "Responses",
                "method": "POST",
                "path": "responses",
                "request_type": new GroqCreateResponseRequestDataType(),
                "response_type": new GroqResponseDataType(),
                "fields": GroqCreateResponseRequestDataType::Fields,
            },
            "transcribe-audio": <GroqActionInfo>{
                "display_name": "Transcribe Audio",
                "short_desc": "Turn a recording into text in its original language",
                "desc": "Turn a recording into text in its original language.\n\nSupply exactly one of `file` or `url`. `json` and `verbose_json` return typed transcript data; `text` returns a string.",
                "group": "Audio",
                "method": "POST",
                "path": "audio/transcriptions",
                "request_type": new GroqTranscriptionRequestDataType(),
                "response_type": new QoreUnionDataType((new GroqAudioResponseDataType(), AbstractDataProviderTypeMap."string",)),
                "fields": GroqTranscriptionRequestDataType::Fields,
            },
            "translate-audio": <GroqActionInfo>{
                "display_name": "Translate Audio",
                "short_desc": "Translate a recording into English text",
                "desc": "Translate a recording into English text.\n\nSupply exactly one of `file` or `url`. `json` and `verbose_json` return typed transcript data; `text` returns a string.",
                "group": "Audio",
                "method": "POST",
                "path": "audio/translations",
                "request_type": new GroqTranslationRequestDataType(),
                "response_type": new QoreUnionDataType((new GroqAudioResponseDataType(), AbstractDataProviderTypeMap."string",)),
                "fields": GroqTranslationRequestDataType::Fields,
            },
            "generate-speech": <GroqActionInfo>{
                "display_name": "Generate Speech",
                "short_desc": "Turn text into spoken audio for customers and audiences",
                "desc": "Turn text into spoken audio for customers and audiences.\n\nReturns raw audio bytes. Select a voice for the chosen model; Orpheus supports `wav` output.",
                "group": "Audio",
                "method": "POST",
                "path": "audio/speech",
                "request_type": new GroqSpeechRequestDataType(),
                "response_type": AbstractDataProviderTypeMap."binary",
                "fields": GroqSpeechRequestDataType::Fields,
            },
            "upload-file": <GroqActionInfo>{
                "display_name": "Upload File",
                "short_desc": "Upload batch requests or a pre-trained LoRA adapter",
                "desc": "Upload batch requests or a pre-trained LoRA adapter.",
                "group": "Files",
                "method": "POST",
                "path": "files",
                "request_type": new GroqUploadFileRequestDataType(),
                "response_type": new GroqFileResponseDataType(),
                "fields": GroqUploadFileRequestDataType::Fields,
            },
            "list-files": <GroqActionInfo>{
                "display_name": "List Files",
                "short_desc": "Find uploaded files and batch result files",
                "desc": "Find uploaded files and batch result files.\n\nReturns the resources made available by the authenticated listing endpoint.",
                "group": "Files",
                "method": "GET",
                "path": "files",
                "request_type": NOTHING,
                "response_type": new GroqListFilesResponseDataType(),
                "fields": {
                },
            },
            "get-file": <GroqActionInfo>{
                "display_name": "Get File",
                "short_desc": "Inspect the size and purpose of an uploaded file",
                "desc": "Inspect the size and purpose of an uploaded file.",
                "group": "Files",
                "method": "GET",
                "path": "files/{file_id}",
                "request_type": new GroqFileRequestDataType(),
                "response_type": new GroqFileResponseDataType(),
                "fields": GroqFileRequestDataType::Fields,
            },
            "download-file": <GroqActionInfo>{
                "display_name": "Download File",
                "short_desc": "Download original input or completed batch results",
                "desc": "Download original input or completed batch results.\n\nReturns the file contents as binary data, including JSONL batch results.",
                "group": "Files",
                "method": "GET",
                "path": "files/{file_id}/content",
                "request_type": new GroqFileRequestDataType(),
                "response_type": AbstractDataProviderTypeMap."binary",
                "fields": GroqFileRequestDataType::Fields,
            },
            "delete-file": <GroqActionInfo>{
                "display_name": "Delete File",
                "short_desc": "Remove a file that is no longer needed",
                "desc": "Remove a file that is no longer needed.",
                "group": "Files",
                "method": "DELETE",
                "path": "files/{file_id}",
                "request_type": new GroqFileRequestDataType(),
                "response_type": new GroqDeleteResponseDataType(),
                "fields": GroqFileRequestDataType::Fields,
            },
            "create-batch": <GroqActionInfo>{
                "display_name": "Create Batch",
                "short_desc": "Submit a file of inference requests for batch processing",
                "desc": "Submit a file of inference requests for batch processing.\n\nReturns the batch immediately. Use `Get Batch` later to inspect progress and download its `output_file_id` with `Download File`. Inference usage is billed by Groq.",
                "group": "Batches",
                "method": "POST",
                "path": "batches",
                "request_type": new GroqCreateBatchRequestDataType(),
                "response_type": new GroqBatchResponseDataType(),
                "fields": GroqCreateBatchRequestDataType::Fields,
            },
            "list-batches": <GroqActionInfo>{
                "display_name": "List Batches",
                "short_desc": "Find batches and inspect their current processing state",
                "desc": "Find batches and inspect their current processing state.\n\nReturns the resources made available by the authenticated listing endpoint.",
                "group": "Batches",
                "method": "GET",
                "path": "batches",
                "request_type": NOTHING,
                "response_type": new GroqListBatchesResponseDataType(),
                "fields": {
                },
            },
            "get-batch": <GroqActionInfo>{
                "display_name": "Get Batch",
                "short_desc": "Inspect progress and locate output files for a batch",
                "desc": "Inspect progress and locate output files for a batch.",
                "group": "Batches",
                "method": "GET",
                "path": "batches/{batch_id}",
                "request_type": new GroqBatchRequestDataType(),
                "response_type": new GroqBatchResponseDataType(),
                "fields": GroqBatchRequestDataType::Fields,
            },
            "cancel-batch": <GroqActionInfo>{
                "display_name": "Cancel Batch",
                "short_desc": "Cancel outstanding requests in a batch",
                "desc": "Cancel outstanding requests in a batch.",
                "group": "Batches",
                "method": "POST",
                "path": "batches/{batch_id}/cancel",
                "request_type": new GroqBatchRequestDataType(),
                "response_type": new GroqBatchResponseDataType(),
                "fields": GroqBatchRequestDataType::Fields,
            },
            "create-fine-tuning": <GroqActionInfo>{
                "display_name": "Register LoRA Adapter",
                "short_desc": "Register a pre-trained adapter for specialized inference",
                "desc": "Register a pre-trained adapter for specialized inference.\n\n**Enterprise access required.** Upload an externally trained LoRA adapter with `purpose` set to `fine_tuning`. Groq hosts adapters and does not train models. See the [LoRA guide](https://console.groq.com/docs/lora).",
                "group": "LoRA Adapters",
                "method": "POST",
                "path": "/v1/fine_tunings",
                "request_type": new GroqCreateFineTuningRequestDataType(),
                "response_type": new GroqFineTuningResponseDataType(),
                "fields": GroqCreateFineTuningRequestDataType::Fields,
            },
            "list-fine-tunings": <GroqActionInfo>{
                "display_name": "List LoRA Adapters",
                "short_desc": "Find adapters available to your organization",
                "desc": "Find adapters available to your organization.\n\n**Enterprise access required.** Upload an externally trained LoRA adapter with `purpose` set to `fine_tuning`. Groq hosts adapters and does not train models. See the [LoRA guide](https://console.groq.com/docs/lora).\n\nReturns the resources made available by the authenticated listing endpoint.",
                "group": "LoRA Adapters",
                "method": "GET",
                "path": "/v1/fine_tunings",
                "request_type": NOTHING,
                "response_type": new GroqListFineTuningsResponseDataType(),
                "fields": {
                },
            },
            "get-fine-tuning": <GroqActionInfo>{
                "display_name": "Get LoRA Adapter",
                "short_desc": "Inspect a registered adapter and its model identifier",
                "desc": "Inspect a registered adapter and its model identifier.\n\n**Enterprise access required.** Upload an externally trained LoRA adapter with `purpose` set to `fine_tuning`. Groq hosts adapters and does not train models. See the [LoRA guide](https://console.groq.com/docs/lora).",
                "group": "LoRA Adapters",
                "method": "GET",
                "path": "/v1/fine_tunings/{id}",
                "request_type": new GroqFineTuningRequestDataType(),
                "response_type": new GroqFineTuningResponseDataType(),
                "fields": GroqFineTuningRequestDataType::Fields,
            },
            "delete-fine-tuning": <GroqActionInfo>{
                "display_name": "Delete LoRA Adapter",
                "short_desc": "Remove a registered adapter from your organization",
                "desc": "Remove a registered adapter from your organization.\n\n**Enterprise access required.** Upload an externally trained LoRA adapter with `purpose` set to `fine_tuning`. Groq hosts adapters and does not train models. See the [LoRA guide](https://console.groq.com/docs/lora).",
                "group": "LoRA Adapters",
                "method": "DELETE",
                "path": "/v1/fine_tunings/{id}",
                "request_type": new GroqFineTuningRequestDataType(),
                "response_type": new GroqDeleteResponseDataType(),
                "fields": GroqFineTuningRequestDataType::Fields,
            },
        };
    }

    #! Registers the additional REST actions
    /** @throw OPTION-ERROR action metadata is invalid
        @throw DATA-PROVIDER-ACTION-ERROR action registration fails
    */
    static registerActions() [dom=PROCESS] {
        foreach hash<auto> entry in (Operations.pairIterator()) {
            hash<GroqActionInfo> op = entry.value;
            string action = entry.key;
            hash<string, hash<ActionOptionInfo>> options =
                DataProviderActionCatalog::getActionOptionFromFields(op.fields);
            # Streaming is intentionally unavailable to completed-response actions.
            remove options.stream;
            if (action == "generate-speech") {
                options.model.has_dependents = True;
                options.model.on_change = ("refetch",);
                options.voice.depends_on = ("model",);
            }
            DataProviderActionCatalog::registerAction(<DataProviderActionInfo>{
                "app": AppName,
                "action": action,
                "path": "/" + action,
                "display_name": op.display_name,
                "short_desc": op.short_desc,
                "desc": op.desc,
                "groups": (op.group,),
                "action_code": DPAT_API,
                "mutates_state": op.method != "GET",
                "options": options,
                "output_type": op.response_type,
            });
        }
    }
}
}
