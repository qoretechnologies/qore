# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore JinaManifest class definition

/** JinaManifest.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace JinaDataProvider {
#! Stable action identities and presentation for the pinned Jina API schema
public class JinaManifest {
    public {
        #! Model subsets accepted by the pinned classification, training, and batch schemas
        const TrainingModels = select JinaRestClientBase::EmbeddingModels,
            $1.value =~ /^jina-(embeddings-v(2-base-.*|3|4|5-text-(nano|small))|clip-v[12])$/;
        const ClassificationModels = TrainingModels + (select JinaRestClientBase::EmbeddingModels,
            $1.value =~ /^jina-code-embeddings-/);
        const BatchModels = select JinaRestClientBase::EmbeddingModels,
            $1.value =~ /^jina-embeddings-v5-text-(nano|small)$/;

        #! Listing operations that supply action dropdowns
        const hash<string, hash<RestSchemaReferenceDataInfo>> ReferenceData = {
            "embedding-models": <RestSchemaReferenceDataInfo>{"name": "embedding-models", "action": "list-models", "records_path": ("data",), "value_field": "id", "display_field": "name", "desc": "Available Jina models compatible with this operation."},
            "rerank-models": <RestSchemaReferenceDataInfo>{"name": "rerank-models", "action": "list-models", "records_path": ("data",), "value_field": "id", "display_field": "name", "desc": "Available Jina models compatible with this operation."},
            "classification-models": <RestSchemaReferenceDataInfo>{"name": "classification-models", "action": "list-models", "records_path": ("data",), "value_field": "id", "display_field": "name", "desc": "Available Jina models compatible with this operation."},
            "training-models": <RestSchemaReferenceDataInfo>{"name": "training-models", "action": "list-models", "records_path": ("data",), "value_field": "id", "display_field": "name", "desc": "Available Jina models compatible with this operation."},
            "batch-models": <RestSchemaReferenceDataInfo>{"name": "batch-models", "action": "list-models", "records_path": ("data",), "value_field": "id", "display_field": "name", "desc": "Available Jina models compatible with this operation."},
            "all-models": <RestSchemaReferenceDataInfo>{"name": "all-models", "action": "list-models", "records_path": ("data",), "value_field": "id", "display_field": "name", "desc": "Available Jina models compatible with this operation."},
            "classifiers": <RestSchemaReferenceDataInfo>{"name": "classifiers", "action": "list-classifiers", "value_field": "classifier_id", "desc": "Saved classifiers in this account; custom IDs are accepted."},
            "batches": <RestSchemaReferenceDataInfo>{"name": "batches", "action": "list-batches", "options": {"limit": 100}, "value_field": "batch_id", "desc": "Recent embedding batches; custom IDs are accepted."},
        };

        #! Batch timestamps use ISO strings without format annotations in the upstream schema
        const BatchTimestamps = {
            "created_at": <RestSchemaFieldOverlayInfo>{"codec": RestSchemaActionCodecs::Iso8601Utc},
            "completed_at": <RestSchemaFieldOverlayInfo>{"codec": RestSchemaActionCodecs::Iso8601Utc},
            "expires_at": <RestSchemaFieldOverlayInfo>{"codec": RestSchemaActionCodecs::Iso8601Utc},
        };

        #! API operations available as application actions
        const list<hash<RestSchemaActionInfo>> Manifest = (
            <RestSchemaActionInfo>{
                "action": "create-embeddings",
                "field_order": ("model", "input", "task", "dimensions", "embedding_type"),
                "method": "POST",
                "mutates_state": False,
                "path": "/v1/embeddings",
                "display_name": "Create Embeddings",
                "short_desc": "Create text, image, audio, video, PDF, and sparse search vectors",
                "desc": "Convert content into vectors for semantic search. Choose a `model` suited to your input and set `task` for query or document retrieval. Model-specific options are validated by the API schema.",
                "groups": ("Embeddings",),
                "operation_id": "embeddings_v1_embeddings_post",
                "fields": {
                    "model": <RestSchemaFieldOverlayInfo>{"default_value": "jina-embeddings-v3", "ref_data": "embedding-models", "supports_custom_values": True, "allowed_values": JinaRestClientBase::EmbeddingModels},
                    "input": <RestSchemaFieldOverlayInfo>{"example_value": ("A customer asks about their delivery.",), "desc": "Content to embed: text, a structured multimodal item, or a list. Supported image, audio, video, and PDF fields depend on `model`."},
                    "task": <RestSchemaFieldOverlayInfo>{"preselected": True, "desc": "Optimize for retrieval, similarity, classification, clustering, or code search. The selected `model` determines which tasks are accepted.", "allowed_values": (<AllowedValueInfo>{"value": "retrieval.query", "display_name": "Retrieval Query"}, <AllowedValueInfo>{"value": "retrieval.passage", "display_name": "Retrieval Passage"}, <AllowedValueInfo>{"value": "text-matching", "display_name": "Text Matching"}, <AllowedValueInfo>{"value": "classification", "display_name": "Classification"}, <AllowedValueInfo>{"value": "separation", "display_name": "Separation"}, <AllowedValueInfo>{"value": "clustering", "display_name": "Clustering"}, <AllowedValueInfo>{"value": "code.query", "display_name": "Code Query"}, <AllowedValueInfo>{"value": "code.passage", "display_name": "Code Passage"}, <AllowedValueInfo>{"value": "nl2code.query", "display_name": "Nl2Code Query"}, <AllowedValueInfo>{"value": "nl2code.passage", "display_name": "Nl2Code Passage"}, <AllowedValueInfo>{"value": "qa.query", "display_name": "Qa Query"}, <AllowedValueInfo>{"value": "qa.passage", "display_name": "Qa Passage"}, <AllowedValueInfo>{"value": "code2code.query", "display_name": "Code2Code Query"}, <AllowedValueInfo>{"value": "code2code.passage", "display_name": "Code2Code Passage"}, <AllowedValueInfo>{"value": "code2nl.query", "display_name": "Code2Nl Query"}, <AllowedValueInfo>{"value": "code2nl.passage", "display_name": "Code2Nl Passage"}, <AllowedValueInfo>{"value": "code2completion.query", "display_name": "Code2Completion Query"}, <AllowedValueInfo>{"value": "code2completion.passage", "display_name": "Code2Completion Passage"})},
                    "dimensions": <RestSchemaFieldOverlayInfo>{"example_value": 32, "preselected": True, "desc": "Output vector size. The selected `model` determines the supported range."},
                    "normalized": <RestSchemaFieldOverlayInfo>{"desc": "If `true` (default), embeddings are L2-normalized to unit length."},
                    "truncate": <RestSchemaFieldOverlayInfo>{"desc": "If `true`, truncates input exceeding the model's max token limit instead of returning an error."},
                    "return_multivector": <RestSchemaFieldOverlayInfo>{"desc": "If `true`, returns one embedding per token. Cannot be used with `dimensions`."},
                    "return_tokenized_input": <RestSchemaFieldOverlayInfo>{"desc": "If `true`, returns tokens alongside multi-vector embeddings. Requires `return_multivector=true`."},
                    "late_chunking": <RestSchemaFieldOverlayInfo>{"desc": "Use `True` to process inputs as one sequence before splitting, preserving context across chunks."},
                    "embedding_type": <RestSchemaFieldOverlayInfo>{"preselected": True, "element_allowed_values": (<AllowedValueInfo>{"value": "float", "display_name": "Float"}, <AllowedValueInfo>{"value": "base64", "display_name": "Base64"}, <AllowedValueInfo>{"value": "binary", "display_name": "Binary"}, <AllowedValueInfo>{"value": "ubinary", "display_name": "Ubinary"})},
                },
            },
            <RestSchemaActionInfo>{
                "action": "rerank",
                "field_order": ("model", "query", "documents", "top_n", "return_documents"),
                "method": "POST",
                "mutates_state": False,
                "path": "/v1/rerank",
                "display_name": "Rerank Documents",
                "short_desc": "Rank text or image documents by relevance to a query",
                "desc": "Improve search quality by ranking candidate `documents` against a `query`. Use `top_n` to keep the strongest matches and `return_documents` to control response size.",
                "groups": ("Reranking",),
                "operation_id": "rerank_v1_rerank_post",
                "fields": {
                    "model": <RestSchemaFieldOverlayInfo>{"default_value": "jina-reranker-v2-base-multilingual", "ref_data": "rerank-models", "supports_custom_values": True, "allowed_values": JinaRestClientBase::RerankModels},
                    "query": <RestSchemaFieldOverlayInfo>{"example_value": "Where is my order?"},
                    "documents": <RestSchemaFieldOverlayInfo>{"example_value": ("Track a delivery", "Change a password"), "desc": "Candidate text or image documents. Supported input shapes depend on `model`."},
                    "return_embeddings": <RestSchemaFieldOverlayInfo>{"desc": "If `true`, returns the document embedding alongside the relevance score."},
                    "return_documents": <RestSchemaFieldOverlayInfo>{"desc": "Use `True` (the default) to include document content in each result."},
                    "top_n": <RestSchemaFieldOverlayInfo>{"example_value": 3, "preselected": True},
                },
            },
            <RestSchemaActionInfo>{
                "action": "classify",
                "field_order": ("model", "classifier_id", "input", "labels"),
                "method": "POST",
                "mutates_state": False,
                "path": "/v1/classify",
                "display_name": "Classify Content",
                "short_desc": "Assign labels to text or images without training a classifier",
                "desc": "Route feedback, support requests, or images into `labels` with a selected `model`. For a saved classifier, provide `classifier_id` instead of `model` and `labels`.",
                "groups": ("Classification",),
                "operation_id": "classify_v1_classify_post",
                "fields": {
                    "model": <RestSchemaFieldOverlayInfo>{"ref_data": "classification-models", "supports_custom_values": True, "allowed_values": ClassificationModels, "required_groups": ("classifier",)},
                    "labels": <RestSchemaFieldOverlayInfo>{"example_value": ("billing", "delivery"), "preselected": True},
                    "classifier_id": <RestSchemaFieldOverlayInfo>{"ref_data": "classifiers", "supports_custom_values": True, "required_groups": ("classifier",), "exclusive_with": ("model", "labels")},
                },
            },
            <RestSchemaActionInfo>{
                "action": "train-classifier",
                "field_order": ("model", "classifier_id", "input", "access", "num_iters"),
                "method": "POST",
                "path": "/v1/train",
                "display_name": "Train or Update Classifier",
                "short_desc": "Create a reusable classifier or add labeled training examples",
                "desc": "Train a classifier from labeled `input` examples and a `model`. Set `access` to control visibility. To update an existing classifier, provide `classifier_id` instead. **Service availability:** Jina returned HTTP 500 for training during verification on September 7, 2026.",
                "groups": ("Classification",),
                "operation_id": "train_v1_train_post",
                "fields": {
                    "model": <RestSchemaFieldOverlayInfo>{"ref_data": "training-models", "supports_custom_values": True, "allowed_values": TrainingModels, "required_groups": ("classifier",)},
                    "classifier_id": <RestSchemaFieldOverlayInfo>{"ref_data": "classifiers", "supports_custom_values": True, "required_groups": ("classifier",), "exclusive_with": ("model", "access")},
                    "access": <RestSchemaFieldOverlayInfo>{"allowed_values": ({"value": "private", "display_name": "Private"}, {"value": "public", "display_name": "Public"}), "preselected": True, "desc": "Visibility of the new classifier. Choose `private` for business training data; Jina defaults to `public` when omitted."},
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-classifiers",
                "method": "GET",
                "path": "/v1/classifiers",
                "display_name": "List Classifiers",
                "short_desc": "List reusable classifiers belonging to your account",
                "desc": "Find saved classifiers to reuse or delete. **Service availability:** Jina returned HTTP 500 for this endpoint during verification on September 7, 2026; classifier IDs can still be supplied directly.",
                "groups": ("Classification",),
            },
            <RestSchemaActionInfo>{
                "action": "delete-classifier",
                "method": "DELETE",
                "path": "/v1/classifiers/{classifier_id}",
                "display_name": "Delete Classifier",
                "short_desc": "Remove a saved classifier from your account",
                "desc": "Delete the selected `classifier_id` when its trained labels are no longer needed. Workflows using that classifier must select a replacement.",
                "groups": ("Classification",),
                "operation_id": "delete_classifier_v1_classifiers__classifier_id__delete",
                "fields": {
                    "classifier_id": <RestSchemaFieldOverlayInfo>{"ref_data": "classifiers", "supports_custom_values": True, "desc": "The saved classifier to remove."},
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-models",
                "response_fields": {"data": <RestSchemaFieldOverlayInfo>{"fields": {
                    "created": <RestSchemaFieldOverlayInfo>{"codec": RestSchemaActionCodecs::UnixSeconds},
                }}},
                "method": "GET",
                "path": "/v1/models",
                "display_name": "List Models",
                "short_desc": "Discover available models, modalities, context limits, and prices",
                "desc": "Compare model capabilities before choosing an embedding or reranking model. The catalog is public; use the connection ping to verify your API key.",
                "groups": ("Models",),
                "operation_id": "list_models_v1_models_get",
            },
            <RestSchemaActionInfo>{
                "action": "get-model",
                "response_fields": {"created": <RestSchemaFieldOverlayInfo>{"codec": RestSchemaActionCodecs::UnixSeconds}},
                "method": "GET",
                "path": "/v1/models/{model_id}",
                "display_name": "Get Model",
                "short_desc": "Inspect capabilities and pricing for a selected model",
                "desc": "Return metadata for `model_id`, including supported inputs, context length, and pricing. Both short IDs and names prefixed with `jina-ai/` are accepted.",
                "groups": ("Models",),
                "operation_id": "get_model_v1_models__model_id__get",
                "fields": {
                    "model_id": <RestSchemaFieldOverlayInfo>{"ref_data": "all-models", "supports_custom_values": True, "example_value": "jina-embeddings-v3", "desc": "The model whose capabilities and pricing to retrieve."},
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-batch-embeddings",
                "field_order": ("input", "input_url", "model", "task", "dimensions", "webhook_url"),
                "response_fields": BatchTimestamps,
                "method": "POST",
                "path": "/v1/batch/embeddings",
                "display_name": "Create Batch Embeddings",
                "short_desc": "Submit a large collection of documents for asynchronous embedding",
                "desc": "Submit inline JSONL records in `input` or an `input_url` for bulk embedding. Each record contains `custom_id` and `body.input`. Use `webhook_url` for completion notifications, then retrieve status and output with the batch actions.",
                "groups": ("Batch Embeddings",),
                "operation_id": "create_batch_job_v1_batch_embeddings_post",
                "fields": {
                    "model": <RestSchemaFieldOverlayInfo>{"ref_data": "batch-models", "supports_custom_values": True, "allowed_values": BatchModels, "default_value": "jina-embeddings-v5-text-nano"},
                    "input": <RestSchemaFieldOverlayInfo>{"desc": "Inline records containing `custom_id` and `body.input`. Provide either `input` or `input_url`.", "type": new ListDataType("JinaBatchInputRecords", JinaBatchInputRecordDataType), "required_groups": ("input",), "exclusive_with": ("input_url",), "example_value": ({"custom_id": "ticket-1", "body": {"input": "A customer asks about their delivery."}},), "preselected": True},
                    "input_url": <RestSchemaFieldOverlayInfo>{"desc": "URL of a JSONL input file in GCS, S3, or HTTP storage. Provide either `input_url` or `input`.", "required_groups": ("input",), "exclusive_with": ("input",), "preselected": True},
                    "task": <RestSchemaFieldOverlayInfo>{"allowed_values": ({"value": "retrieval.query", "display_name": "Retrieval Query"}, {"value": "retrieval.passage", "display_name": "Retrieval Passage"}, {"value": "text-matching", "display_name": "Text Matching"}, {"value": "clustering", "display_name": "Clustering"}, {"value": "classification", "display_name": "Classification"})},
                    "normalized": <RestSchemaFieldOverlayInfo>{"desc": "If `true` (default), embeddings are L2-normalized to unit length."},
                    "webhook_url": <RestSchemaFieldOverlayInfo>{"desc": "An HTTPS URL to notify when processing completes. Verify completion using `get-batch` before trusting a notification."},
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-batch",
                "response_fields": BatchTimestamps,
                "method": "GET",
                "path": "/v1/batch/{batch_id}",
                "display_name": "Get Batch Status",
                "short_desc": "Inspect a batch embedding job and its result download URLs",
                "desc": "Check processing progress and token statistics for `batch_id`. Completed jobs include output URLs; downloads expire after 24 hours.",
                "groups": ("Batch Embeddings",),
                "operation_id": "get_batch_status_v1_batch__batch_id__get",
                "fields": {
                    "batch_id": <RestSchemaFieldOverlayInfo>{"ref_data": "batches", "supports_custom_values": True, "desc": "The embedding batch to inspect or cancel."},
                },
            },
            <RestSchemaActionInfo>{
                "action": "cancel-batch",
                "response_fields": BatchTimestamps,
                "method": "DELETE",
                "path": "/v1/batch/{batch_id}",
                "display_name": "Cancel Batch",
                "short_desc": "Cancel a pending or processing embedding batch",
                "desc": "Stop further processing of `batch_id` when its embeddings are no longer needed. Already completed work may still be charged by Jina.",
                "groups": ("Batch Embeddings",),
                "operation_id": "cancel_batch_job_v1_batch__batch_id__delete",
                "fields": {
                    "batch_id": <RestSchemaFieldOverlayInfo>{"ref_data": "batches", "supports_custom_values": True, "desc": "The embedding batch to inspect or cancel."},
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-batches",
                "response_fields": BatchTimestamps,
                "method": "GET",
                "path": "/v1/batches",
                "display_name": "List Batches",
                "short_desc": "List recent embedding batches for your account",
                "desc": "Find recently submitted embedding jobs and their statuses. Set `limit` to control how many are returned; custom batch IDs can also be entered directly.",
                "groups": ("Batch Embeddings",),
                "operation_id": "list_batch_jobs_v1_batches_get",
                "fields": {
                    "limit": <RestSchemaFieldOverlayInfo>{"example_value": 10, "preselected": True, "desc": "Maximum number of recent batches to return."},
                },
            },
        );
    }
}
}
