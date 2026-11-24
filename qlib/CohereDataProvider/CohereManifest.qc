# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore CohereManifest class definition

/** CohereManifest.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace CohereDataProvider {
#! Reviewed API operations and their action presentation
/** The original chat, embed, rerank, and classify action IDs are preserved. Deprecated generation,
    summarization, connectors and fine-tuning administration are not added; Chat covers the generation
    use cases and Classify remains available for existing workflows. Streaming is omitted from the
    completed-response Chat action. The generic REST action remains available for specialized requests.

    Model dropdowns use the documented v1 listing, filtered by task where supported. Parse and audio
    are not in the listing's endpoint enum, so those fields use the complete model list and permit
    manually supplied model names. Listings fetch one page; custom IDs remain accepted.

    @see https://github.com/cohere-ai/cohere-developer-experience/blob/f19a6485639746f919d2b7fc2b63b476e2f39490/cohere-openapi.yaml
*/
public class CohereManifest {
    public {
        #! Dropdowns backed by exported listing actions
        const hash<string, hash<RestSchemaReferenceDataInfo>> ReferenceData = {
            "chat-models": <RestSchemaReferenceDataInfo>{
                "name": "chat-models",
                "action": "list-models",
                "options": {
                    "page_size": 1000,
                    "endpoint": "chat",
                },
                "records_path": (
                    "models",
                ),
                "value_field": "name",
                "display_field": "name",
                "desc": "Available chat models; enter a name directly if listing is unavailable.",
            },
            "embed-models": <RestSchemaReferenceDataInfo>{
                "name": "embed-models",
                "action": "list-models",
                "options": {
                    "page_size": 1000,
                    "endpoint": "embed",
                },
                "records_path": (
                    "models",
                ),
                "value_field": "name",
                "display_field": "name",
                "desc": "Available embed models; enter a name directly if listing is unavailable.",
            },
            "rerank-models": <RestSchemaReferenceDataInfo>{
                "name": "rerank-models",
                "action": "list-models",
                "options": {
                    "page_size": 1000,
                    "endpoint": "rerank",
                },
                "records_path": (
                    "models",
                ),
                "value_field": "name",
                "display_field": "name",
                "desc": "Available rerank models; enter a name directly if listing is unavailable.",
            },
            "classify-models": <RestSchemaReferenceDataInfo>{
                "name": "classify-models",
                "action": "list-models",
                "options": {
                    "page_size": 1000,
                    "endpoint": "classify",
                },
                "records_path": (
                    "models",
                ),
                "value_field": "name",
                "display_field": "name",
                "desc": "Available classify models; enter a name directly if listing is unavailable.",
            },
            "all-models": <RestSchemaReferenceDataInfo>{
                "name": "all-models",
                "action": "list-models",
                "options": {
                    "page_size": 1000,
                },
                "records_path": (
                    "models",
                ),
                "value_field": "name",
                "display_field": "name",
                "desc": "Available models; enter a name directly if listing is unavailable.",
            },
            "datasets": <RestSchemaReferenceDataInfo>{
                "name": "datasets",
                "action": "list-datasets",
                "records_path": (
                    "datasets",
                ),
                "value_field": "id",
                "display_field": "name",
                "desc": "Existing datasets in this account.",
            },
            "embed-jobs": <RestSchemaReferenceDataInfo>{
                "name": "embed-jobs",
                "action": "list-embed-jobs",
                "records_path": (
                    "embed_jobs",
                ),
                "value_field": "job_id",
                "display_field": "name",
                "desc": "Existing embedding jobs in this account.",
            },
            "batches": <RestSchemaReferenceDataInfo>{
                "name": "batches",
                "action": "list-batches",
                "records_path": (
                    "batches",
                ),
                "value_field": "id",
                "display_field": "name",
                "desc": "Existing batches in this account.",
            },
        };

        #! Stable action IDs and schema-checked presentation overrides
        const list<hash<RestSchemaActionInfo>> Manifest = (
            <RestSchemaActionInfo>{
                "action": "chat",
                "method": "POST",
                "mutates_state": False,
                "path": "/v2/chat",
                "operation_id": "chatv2",
                "display_name": "Chat",
                "short_desc": "Generate a response with tools, grounding, and structured output",
                "desc": "Generate a response from conversation messages. Add `tools` for function calls, `documents` for grounded answers, or `response_format` for structured JSON. This action returns the completed response.",
                "groups": (
                    "Chat",
                ),
                "fields": {
                    "model": <RestSchemaFieldOverlayInfo>{
                        "default_value": "command-a-03-2025",
                        "preselected": True,
                        "ref_data": "chat-models",
                        "supports_custom_values": True,
                        "short_desc": "Cohere chat model",
                    },
                    "stream": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "messages": <RestSchemaFieldOverlayInfo>{
                        "example_value": (
                            {
                                "role": "user",
                                "content": "Summarize the customer request.",
                            },
                        ),
                        "display_name": "Messages",
                        "short_desc": "List of chat messages forming the conversation",
                        "desc": "The conversation history as a list of message objects.",
                    },
                    "temperature": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                        "short_desc": "Control response randomness",
                        "desc": "Adjust sampling randomness. Lower values give more focused responses; higher values produce more varied wording.",
                    },
                    "max_tokens": <RestSchemaFieldOverlayInfo>{
                        "example_value": 256,
                        "preselected": True,
                        "display_name": "Max Tokens",
                        "short_desc": "Maximum tokens to emit",
                        "desc": "Maximum number of tokens to generate.",
                    },
                    "p": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Top-P",
                        "short_desc": "Nucleus sampling cutoff (0-1)",
                        "desc": "Nucleus sampling cutoff.",
                    },
                    "k": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Top-K",
                        "short_desc": "Sample only from the K most likely next tokens",
                        "desc": "Restrict sampling to the K most likely next tokens.",
                    },
                    "stop_sequences": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Stop Sequences",
                        "short_desc": "List of stop sequences",
                        "desc": "List of stop sequences; generation halts when any is emitted.",
                    },
                    "tools": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Tools",
                        "short_desc": "Tool definitions the model may call",
                        "desc": "List of tool definitions.  Each entry has `type: function` and a `function` hash with `name`, `description`, and JSON-schema `parameters`.",
                    },
                    "response_format": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Response Format",
                        "short_desc": "Force structured output (e.g. JSON schema)",
                        "desc": "Hash that forces a structured response shape, e.g. `{type: \"json_object\"}` or `{type: \"json_object\", schema: {...}}`.",
                    },
                    "safety_mode": <RestSchemaFieldOverlayInfo>{
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "CONTEXTUAL",
                                "display_name": "Contextual",
                            },
                            <AllowedValueInfo>{
                                "value": "STRICT",
                                "display_name": "Strict",
                            },
                            <AllowedValueInfo>{
                                "value": "OFF",
                                "display_name": "Off",
                            },
                        ),
                    },
                    "tool_choice": <RestSchemaFieldOverlayInfo>{
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "REQUIRED",
                                "display_name": "Required",
                            },
                            <AllowedValueInfo>{
                                "value": "NONE",
                                "display_name": "None",
                            },
                        ),
                        "short_desc": "Control whether the model calls a tool",
                        "desc": "Use `REQUIRED` to request a tool call or `NONE` to prevent one. Omit this option to let the model choose.",
                    },
                    "seed": <RestSchemaFieldOverlayInfo>{
                        "short_desc": "Sampling seed for repeatable requests",
                    },
                },
                "flatten_body": True,
            },
            <RestSchemaActionInfo>{
                "action": "embed",
                "method": "POST",
                "mutates_state": False,
                "path": "/v2/embed",
                "operation_id": "embedv2",
                "display_name": "Create Embeddings",
                "short_desc": "Embed text, images, or mixed content for search and retrieval",
                "desc": "Create vectors for semantic search, classification, or clustering. Supply one of `texts`, `images`, or `inputs`, choose the matching `input_type`, and select the numeric `embedding_types` needed by your vector store. Use an Embed v4 model for mixed-content `inputs`.",
                "groups": (
                    "Embeddings",
                ),
                "fields": {
                    "model": <RestSchemaFieldOverlayInfo>{
                        "default_value": "embed-multilingual-v3.0",
                        "preselected": True,
                        "ref_data": "embed-models",
                        "supports_custom_values": True,
                        "short_desc": "Cohere embedding model",
                    },
                    "input_type": <RestSchemaFieldOverlayInfo>{
                        "default_value": "search_document",
                        "preselected": True,
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "search_document",
                                "display_name": "Search Document - text to be indexed",
                            },
                            <AllowedValueInfo>{
                                "value": "search_query",
                                "display_name": "Search Query - user query",
                            },
                            <AllowedValueInfo>{
                                "value": "classification",
                                "display_name": "Classification",
                            },
                            <AllowedValueInfo>{
                                "value": "clustering",
                                "display_name": "Clustering",
                            },
                            <AllowedValueInfo>{
                                "value": "image",
                                "display_name": "Image",
                            },
                        ),
                        "short_desc": "Choose how the embeddings will be used",
                    },
                    "embedding_types": <RestSchemaFieldOverlayInfo>{
                        "default_value": (
                            "float",
                        ),
                        "element_allowed_values": (
                            <AllowedValueInfo>{
                                "value": "float",
                                "display_name": "Float",
                            },
                            <AllowedValueInfo>{
                                "value": "int8",
                                "display_name": "Signed 8-Bit Integer",
                            },
                            <AllowedValueInfo>{
                                "value": "uint8",
                                "display_name": "Unsigned 8-Bit Integer",
                            },
                            <AllowedValueInfo>{
                                "value": "binary",
                                "display_name": "Binary",
                            },
                            <AllowedValueInfo>{
                                "value": "ubinary",
                                "display_name": "Unsigned Binary",
                            },
                        ),
                    },
                    "texts": <RestSchemaFieldOverlayInfo>{
                        "example_value": (
                            "A customer asked to reschedule delivery.",
                        ),
                        "preselected": True,
                        "required_groups": (
                            "input",
                        ),
                        "exclusive_with": (
                            "images",
                            "inputs",
                        ),
                        "display_name": "Texts",
                        "short_desc": "List of texts to embed",
                        "desc": "List of input texts to convert to embedding vectors.",
                    },
                    "images": <RestSchemaFieldOverlayInfo>{
                        "required_groups": (
                            "input",
                        ),
                        "exclusive_with": (
                            "texts",
                            "inputs",
                        ),
                    },
                    "inputs": <RestSchemaFieldOverlayInfo>{
                        "required_groups": (
                            "input",
                        ),
                        "exclusive_with": (
                            "texts",
                            "images",
                        ),
                    },
                    "truncate": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Truncate",
                        "short_desc": "How to truncate inputs that exceed context",
                        "desc": "How to handle inputs that exceed the embedding model's context window.  Default `END`.",
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "NONE",
                                "display_name": "None - error on overflow",
                            },
                            <AllowedValueInfo>{
                                "value": "START",
                                "display_name": "Start - truncate beginning",
                            },
                            <AllowedValueInfo>{
                                "value": "END",
                                "display_name": "End - truncate end (default)",
                            },
                        ),
                    },
                },
                "flatten_body": True,
            },
            <RestSchemaActionInfo>{
                "action": "rerank",
                "method": "POST",
                "mutates_state": False,
                "path": "/v2/rerank",
                "operation_id": "rerankv2",
                "display_name": "Rerank Documents",
                "short_desc": "Rank candidate documents by relevance to a query",
                "desc": "Improve search results by scoring candidate documents against a query. Use `top_n` to return only the best matches; each result identifies the original document by its `index`.",
                "groups": (
                    "Rerank",
                ),
                "fields": {
                    "model": <RestSchemaFieldOverlayInfo>{
                        "default_value": "rerank-v3.5",
                        "preselected": True,
                        "ref_data": "rerank-models",
                        "supports_custom_values": True,
                        "short_desc": "Cohere reranker model",
                    },
                    "top_n": <RestSchemaFieldOverlayInfo>{
                        "example_value": 5,
                        "preselected": True,
                        "display_name": "Top N",
                        "short_desc": "How many top-scoring results to return",
                        "desc": "Number of top-ranked documents to return.",
                    },
                    "query": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Query",
                        "short_desc": "The user query to rerank documents against",
                        "desc": "Free-form user query.",
                    },
                    "documents": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Documents",
                        "short_desc": "Candidate documents to rerank",
                        "desc": "List of candidate documents to rerank against the query.",
                    },
                },
                "flatten_body": True,
            },
            <RestSchemaActionInfo>{
                "action": "classify",
                "method": "POST",
                "mutates_state": False,
                "path": "/v1/classify",
                "operation_id": "classify",
                "display_name": "Classify Text",
                "short_desc": "Classify text with an existing classification model",
                "desc": "Legacy classification endpoint, retained for compatibility. **Cohere has retired classification fine-tuning and previously fine-tuned models.** Use Chat with structured output for new classification workflows.",
                "groups": (
                    "Classification",
                ),
                "fields": {
                    "model": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                        "ref_data": "classify-models",
                        "supports_custom_values": True,
                        "short_desc": "Existing fine-tuned classification model",
                    },
                    "truncate": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Truncate",
                        "short_desc": "How to truncate inputs that exceed context",
                        "desc": "Truncation strategy: `NONE`, `START`, or `END` (default).",
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "NONE",
                                "display_name": "None",
                            },
                            <AllowedValueInfo>{
                                "value": "START",
                                "display_name": "Start",
                            },
                            <AllowedValueInfo>{
                                "value": "END",
                                "display_name": "End",
                            },
                        ),
                    },
                },
                "flatten_body": True,
            },
            <RestSchemaActionInfo>{
                "action": "list-models",
                "method": "GET",
                "path": "/v1/models",
                "operation_id": "list-models",
                "display_name": "List Models",
                "short_desc": "Find available models and their supported tasks",
                "desc": "List models available to this account. Filter by `endpoint` to choose a compatible model. Follow `next_page_token` with `page_token` to retrieve subsequent pages.",
                "groups": (
                    "Models",
                ),
                "flatten_body": True,
                "fields": {
                    "endpoint": <RestSchemaFieldOverlayInfo>{
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "chat",
                                "display_name": "Chat",
                            },
                            <AllowedValueInfo>{
                                "value": "embed",
                                "display_name": "Embed",
                            },
                            <AllowedValueInfo>{
                                "value": "classify",
                                "display_name": "Classify",
                            },
                            <AllowedValueInfo>{
                                "value": "summarize",
                                "display_name": "Summarize",
                            },
                            <AllowedValueInfo>{
                                "value": "rerank",
                                "display_name": "Rerank",
                            },
                            <AllowedValueInfo>{
                                "value": "rate",
                                "display_name": "Rate",
                            },
                            <AllowedValueInfo>{
                                "value": "generate",
                                "display_name": "Generate",
                            },
                        ),
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-model",
                "method": "GET",
                "path": "/v1/models/{model}",
                "operation_id": "get-model",
                "display_name": "Get Model",
                "short_desc": "Inspect a model and its supported capabilities",
                "desc": "Retrieve the capabilities, context size, tokenizer URL, and defaults for a selected model.",
                "groups": (
                    "Models",
                ),
                "fields": {
                    "model": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                        "ref_data": "all-models",
                        "supports_custom_values": True,
                    },
                },
                "flatten_body": True,
            },
            <RestSchemaActionInfo>{
                "action": "tokenize",
                "method": "POST",
                "mutates_state": False,
                "path": "/v1/tokenize",
                "operation_id": "tokenize",
                "display_name": "Tokenize Text",
                "short_desc": "Convert text to model-specific token IDs",
                "desc": "Count and inspect tokens before constructing prompts. Token IDs depend on the selected model; use the same model when detokenizing.",
                "groups": (
                    "Tokens",
                ),
                "fields": {
                    "model": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                        "ref_data": "all-models",
                        "supports_custom_values": True,
                    },
                },
                "flatten_body": True,
            },
            <RestSchemaActionInfo>{
                "action": "detokenize",
                "method": "POST",
                "mutates_state": False,
                "path": "/v1/detokenize",
                "operation_id": "detokenize",
                "display_name": "Detokenize Tokens",
                "short_desc": "Convert model-specific token IDs back into text",
                "desc": "Decode token IDs produced for the selected model into text.",
                "groups": (
                    "Tokens",
                ),
                "fields": {
                    "model": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                        "ref_data": "all-models",
                        "supports_custom_values": True,
                    },
                },
                "flatten_body": True,
            },
            <RestSchemaActionInfo>{
                "action": "parse",
                "method": "POST",
                "mutates_state": False,
                "path": "/v2/parse",
                "operation_id": "parsev2",
                "display_name": "Parse Document Image",
                "short_desc": "Extract structured blocks or Markdown from a document image",
                "desc": "Extract text, tables, and image regions from a document image. Supply an image URL or data URI in `document.image_url`. Cohere currently accepts images, not PDF files. Choose `output_format` for Markdown or structured blocks.",
                "groups": (
                    "Documents",
                ),
                "fields": {
                    "model": <RestSchemaFieldOverlayInfo>{
                        "default_value": "parse-v5.0",
                        "preselected": True,
                        "ref_data": "all-models",
                        "supports_custom_values": True,
                    },
                    "document": <RestSchemaFieldOverlayInfo>{
                        "example_value": {
                            "type": "image_url",
                            "image_url": "https://cohere.com/favicon-32x32.png",
                        },
                    },
                    "output_format": <RestSchemaFieldOverlayInfo>{
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "blocks",
                                "display_name": "Structured Blocks",
                            },
                            <AllowedValueInfo>{
                                "value": "markdown",
                                "display_name": "Markdown",
                            },
                        ),
                    },
                },
                "flatten_body": True,
            },
            <RestSchemaActionInfo>{
                "action": "transcribe",
                "method": "POST",
                "mutates_state": False,
                "path": "/v2/audio/transcriptions",
                "operation_id": "create-transcription",
                "display_name": "Transcribe Audio",
                "short_desc": "Turn recorded speech into text",
                "desc": "Transcribe audio from a FLAC, MP3, MPEG, MPGA, OGG, or WAV file. Specify the spoken `language` and an audio transcription model.",
                "groups": (
                    "Audio",
                ),
                "fields": {
                    "model": <RestSchemaFieldOverlayInfo>{
                        "default_value": "cohere-transcribe-03-2026",
                        "preselected": True,
                        "ref_data": "all-models",
                        "supports_custom_values": True,
                    },
                    "language": <RestSchemaFieldOverlayInfo>{
                        "example_value": "en",
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "en",
                                "display_name": "English",
                            },
                            <AllowedValueInfo>{
                                "value": "de",
                                "display_name": "German",
                            },
                            <AllowedValueInfo>{
                                "value": "fr",
                                "display_name": "French",
                            },
                            <AllowedValueInfo>{
                                "value": "it",
                                "display_name": "Italian",
                            },
                            <AllowedValueInfo>{
                                "value": "es",
                                "display_name": "Spanish",
                            },
                            <AllowedValueInfo>{
                                "value": "pt",
                                "display_name": "Portuguese",
                            },
                            <AllowedValueInfo>{
                                "value": "el",
                                "display_name": "Greek",
                            },
                            <AllowedValueInfo>{
                                "value": "nl",
                                "display_name": "Dutch",
                            },
                            <AllowedValueInfo>{
                                "value": "pl",
                                "display_name": "Polish",
                            },
                            <AllowedValueInfo>{
                                "value": "vi",
                                "display_name": "Vietnamese",
                            },
                            <AllowedValueInfo>{
                                "value": "zh",
                                "display_name": "Chinese",
                            },
                            <AllowedValueInfo>{
                                "value": "ar",
                                "display_name": "Arabic",
                            },
                            <AllowedValueInfo>{
                                "value": "ja",
                                "display_name": "Japanese",
                            },
                            <AllowedValueInfo>{
                                "value": "ko",
                                "display_name": "Korean",
                            },
                        ),
                        "validation_regex": "^[a-z]{2}$",
                    },
                },
                "flatten_body": True,
            },
            <RestSchemaActionInfo>{
                "action": "create-dataset",
                "method": "POST",
                "path": "/v1/datasets",
                "operation_id": "create-dataset",
                "display_name": "Create Dataset",
                "short_desc": "Upload data for embedding, batch inference, or fine-tuning",
                "desc": "Upload a JSONL or CSV dataset for the selected task. `data` contains the file bytes; `eval_data` optionally supplies evaluation data. Check dataset validation before starting a job.",
                "groups": (
                    "Datasets",
                ),
                "flatten_body": True,
                "fields": {
                    "type": <RestSchemaFieldOverlayInfo>{
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "embed-input",
                                "display_name": "Embed Input",
                            },
                            <AllowedValueInfo>{
                                "value": "embed-result",
                                "display_name": "Embed Result",
                            },
                            <AllowedValueInfo>{
                                "value": "cluster-result",
                                "display_name": "Cluster Result",
                            },
                            <AllowedValueInfo>{
                                "value": "cluster-outliers",
                                "display_name": "Cluster Outliers",
                            },
                            <AllowedValueInfo>{
                                "value": "reranker-finetune-input",
                                "display_name": "Reranker Finetune Input",
                            },
                            <AllowedValueInfo>{
                                "value": "single-label-classification-finetune-input",
                                "display_name": "Single Label Classification Finetune Input",
                            },
                            <AllowedValueInfo>{
                                "value": "chat-finetune-input",
                                "display_name": "Chat Finetune Input",
                            },
                            <AllowedValueInfo>{
                                "value": "multi-label-classification-finetune-input",
                                "display_name": "Multi Label Classification Finetune Input",
                            },
                            <AllowedValueInfo>{
                                "value": "batch-chat-input",
                                "display_name": "Batch Chat Input",
                            },
                            <AllowedValueInfo>{
                                "value": "batch-openai-chat-input",
                                "display_name": "Batch OpenAI Chat Input",
                            },
                            <AllowedValueInfo>{
                                "value": "batch-embed-v2-input",
                                "display_name": "Batch Embed V2 Input",
                            },
                            <AllowedValueInfo>{
                                "value": "batch-chat-v2-input",
                                "display_name": "Batch Chat V2 Input",
                            },
                        ),
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-datasets",
                "method": "GET",
                "path": "/v1/datasets",
                "operation_id": "list-datasets",
                "display_name": "List Datasets",
                "short_desc": "Find datasets by task, validation status, and creation time",
                "desc": "Find datasets to process or review. Filter by type and validation status, and use `limit` and `offset` for pagination.",
                "groups": (
                    "Datasets",
                ),
                "fields": {
                    "datasetType": <RestSchemaFieldOverlayInfo>{
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "embed-input",
                                "display_name": "Embed Input",
                            },
                            <AllowedValueInfo>{
                                "value": "embed-result",
                                "display_name": "Embed Result",
                            },
                            <AllowedValueInfo>{
                                "value": "cluster-result",
                                "display_name": "Cluster Result",
                            },
                            <AllowedValueInfo>{
                                "value": "cluster-outliers",
                                "display_name": "Cluster Outliers",
                            },
                            <AllowedValueInfo>{
                                "value": "reranker-finetune-input",
                                "display_name": "Reranker Finetune Input",
                            },
                            <AllowedValueInfo>{
                                "value": "single-label-classification-finetune-input",
                                "display_name": "Single Label Classification Finetune Input",
                            },
                            <AllowedValueInfo>{
                                "value": "chat-finetune-input",
                                "display_name": "Chat Finetune Input",
                            },
                            <AllowedValueInfo>{
                                "value": "multi-label-classification-finetune-input",
                                "display_name": "Multi Label Classification Finetune Input",
                            },
                            <AllowedValueInfo>{
                                "value": "batch-chat-input",
                                "display_name": "Batch Chat Input",
                            },
                            <AllowedValueInfo>{
                                "value": "batch-openai-chat-input",
                                "display_name": "Batch OpenAI Chat Input",
                            },
                            <AllowedValueInfo>{
                                "value": "batch-embed-v2-input",
                                "display_name": "Batch Embed V2 Input",
                            },
                            <AllowedValueInfo>{
                                "value": "batch-chat-v2-input",
                                "display_name": "Batch Chat V2 Input",
                            },
                        ),
                    },
                    "validationStatus": <RestSchemaFieldOverlayInfo>{
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "unknown",
                                "display_name": "Unknown",
                            },
                            <AllowedValueInfo>{
                                "value": "queued",
                                "display_name": "Queued",
                            },
                            <AllowedValueInfo>{
                                "value": "processing",
                                "display_name": "Processing",
                            },
                            <AllowedValueInfo>{
                                "value": "failed",
                                "display_name": "Failed",
                            },
                            <AllowedValueInfo>{
                                "value": "validated",
                                "display_name": "Validated",
                            },
                            <AllowedValueInfo>{
                                "value": "skipped",
                                "display_name": "Skipped",
                            },
                        ),
                    },
                },
                "flatten_body": True,
            },
            <RestSchemaActionInfo>{
                "action": "get-dataset",
                "method": "GET",
                "path": "/v1/datasets/{id}",
                "operation_id": "get-dataset",
                "display_name": "Get Dataset",
                "short_desc": "Inspect dataset validation and download information",
                "desc": "Retrieve dataset metadata, validation errors, and available download URLs.",
                "groups": (
                    "Datasets",
                ),
                "fields": {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                        "ref_data": "datasets",
                        "supports_custom_values": True,
                    },
                },
                "flatten_body": True,
            },
            <RestSchemaActionInfo>{
                "action": "delete-dataset",
                "method": "DELETE",
                "path": "/v1/datasets/{id}",
                "operation_id": "delete-dataset",
                "display_name": "Delete Dataset",
                "short_desc": "Remove a dataset from the account",
                "desc": "Permanently delete the selected dataset. Download any needed input or output data before deleting it.",
                "groups": (
                    "Datasets",
                ),
                "fields": {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                        "ref_data": "datasets",
                        "supports_custom_values": True,
                    },
                },
                "flatten_body": True,
            },
            <RestSchemaActionInfo>{
                "action": "get-dataset-usage",
                "method": "GET",
                "path": "/v1/datasets/usage",
                "operation_id": "get-dataset-usage",
                "display_name": "Get Dataset Usage",
                "short_desc": "Inspect dataset storage usage",
                "desc": "Review dataset storage usage before uploading additional data.",
                "groups": (
                    "Datasets",
                ),
                "flatten_body": True,
            },
            <RestSchemaActionInfo>{
                "action": "create-embed-job",
                "method": "POST",
                "path": "/v1/embed-jobs",
                "operation_id": "create-embed-job",
                "display_name": "Create Embedding Job",
                "short_desc": "Generate embeddings for an uploaded dataset",
                "desc": "Submit an `embed-input` dataset for batch embedding. Select an embedding model and the intended `input_type`. The response contains the `job_id` used to inspect or cancel the job.",
                "groups": (
                    "Embedding Jobs",
                ),
                "fields": {
                    "model": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                        "ref_data": "embed-models",
                        "supports_custom_values": True,
                    },
                    "dataset_id": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                        "ref_data": "datasets",
                        "supports_custom_values": True,
                    },
                    "input_type": <RestSchemaFieldOverlayInfo>{
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "search_document",
                                "display_name": "Search Document - text to be indexed",
                            },
                            <AllowedValueInfo>{
                                "value": "search_query",
                                "display_name": "Search Query - user query",
                            },
                            <AllowedValueInfo>{
                                "value": "classification",
                                "display_name": "Classification",
                            },
                            <AllowedValueInfo>{
                                "value": "clustering",
                                "display_name": "Clustering",
                            },
                            <AllowedValueInfo>{
                                "value": "image",
                                "display_name": "Image",
                            },
                        ),
                    },
                    "truncate": <RestSchemaFieldOverlayInfo>{
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "START",
                                "display_name": "Start",
                            },
                            <AllowedValueInfo>{
                                "value": "END",
                                "display_name": "End",
                            },
                        ),
                    },
                },
                "flatten_body": True,
            },
            <RestSchemaActionInfo>{
                "action": "list-embed-jobs",
                "method": "GET",
                "path": "/v1/embed-jobs",
                "operation_id": "list-embed-jobs",
                "display_name": "List Embedding Jobs",
                "short_desc": "Find submitted embedding jobs",
                "desc": "List embedding jobs and inspect their current status.",
                "groups": (
                    "Embedding Jobs",
                ),
                "flatten_body": True,
            },
            <RestSchemaActionInfo>{
                "action": "get-embed-job",
                "method": "GET",
                "path": "/v1/embed-jobs/{id}",
                "operation_id": "get-embed-job",
                "display_name": "Get Embedding Job",
                "short_desc": "Inspect an embedding job and its output dataset",
                "desc": "Retrieve the status of an embedding job and the dataset containing its results.",
                "groups": (
                    "Embedding Jobs",
                ),
                "fields": {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                        "ref_data": "embed-jobs",
                        "supports_custom_values": True,
                    },
                },
                "flatten_body": True,
            },
            <RestSchemaActionInfo>{
                "action": "cancel-embed-job",
                "method": "POST",
                "path": "/v1/embed-jobs/{id}/cancel",
                "operation_id": "cancel-embed-job",
                "display_name": "Cancel Embedding Job",
                "short_desc": "Cancel an unfinished embedding job",
                "desc": "Request cancellation of an embedding job that is still running.",
                "groups": (
                    "Embedding Jobs",
                ),
                "fields": {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                        "ref_data": "embed-jobs",
                        "supports_custom_values": True,
                    },
                },
                "flatten_body": True,
            },
            <RestSchemaActionInfo>{
                "action": "create-batch",
                "method": "POST",
                "path": "/v2/batches",
                "operation_id": "CreateBatch",
                "display_name": "Create Batch",
                "short_desc": "Process a dataset with batch inference",
                "desc": "Submit a dataset for batch inference with the selected model. Give the batch a recognizable `name` so it can be found later.",
                "groups": (
                    "Batches",
                ),
                "fields": {
                    "model": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                        "ref_data": "all-models",
                        "supports_custom_values": True,
                    },
                    "input_dataset_id": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                        "ref_data": "datasets",
                        "supports_custom_values": True,
                    },
                },
                "flatten_body": True,
            },
            <RestSchemaActionInfo>{
                "action": "list-batches",
                "method": "GET",
                "path": "/v2/batches",
                "operation_id": "ListBatches",
                "display_name": "List Batches",
                "short_desc": "Find batch inference jobs",
                "desc": "List batch inference jobs. Follow `next_page_token` using `page_token` to retrieve subsequent pages.",
                "groups": (
                    "Batches",
                ),
                "flatten_body": True,
            },
            <RestSchemaActionInfo>{
                "action": "get-batch",
                "method": "GET",
                "path": "/v2/batches/{id}",
                "operation_id": "GetBatch",
                "display_name": "Get Batch",
                "short_desc": "Inspect batch progress and output data",
                "desc": "Retrieve batch status, record counts, token usage, and the output dataset identifier.",
                "groups": (
                    "Batches",
                ),
                "fields": {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                        "ref_data": "batches",
                        "supports_custom_values": True,
                    },
                },
                "flatten_body": True,
            },
            <RestSchemaActionInfo>{
                "action": "cancel-batch",
                "method": "POST",
                "path": "/v2/batches/{id}:cancel",
                "operation_id": "CancelBatch",
                "display_name": "Cancel Batch",
                "short_desc": "Cancel an unfinished batch inference job",
                "desc": "Request cancellation of a batch that is still running.",
                "groups": (
                    "Batches",
                ),
                "fields": {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                        "ref_data": "batches",
                        "supports_custom_values": True,
                    },
                },
                "flatten_body": True,
            },
            <RestSchemaActionInfo>{
                "action": "check-api-key",
                "method": "POST",
                "mutates_state": False,
                "path": "/v1/check-api-key",
                "operation_id": "checkAPIKey",
                "display_name": "Check API Key",
                "short_desc": "Check whether the connection API key is valid",
                "desc": "Check the connection's API key without generating content. Inspect the boolean `valid` result; HTTP success alone does not mean that a key is valid. This does not check remaining trial quota.",
                "groups": (
                    "Connection",
                ),
                "flatten_body": True,
            },
        );
    }
}
}
