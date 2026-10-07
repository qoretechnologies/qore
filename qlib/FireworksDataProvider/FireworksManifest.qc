# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore Fireworks AI action catalog

/** FireworksManifest.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace FireworksDataProvider {
#! Reviewed Fireworks operations, forms, and account-scoped dropdowns
/** The original <tt>chat-completion</tt> and <tt>create-embeddings</tt> action identities remain stable.
    Five vendor schemas retain their own API base paths. Streaming is exposed by the LLM adapter;
    app inference actions return complete responses. Dropdowns fetch one page and accept custom IDs.

    Session-token exchange, internal training-session coordination, enterprise identity administration,
    secret/key administration, developer passes, and deprecated evaluator creation are intentionally
    excluded. These are authentication/platform administration surfaces rather than model workflows.
    The generic REST action remains available for specialized requests.
*/
public class FireworksManifest {
    public {
        const hash<string, hash<RestSchemaReferenceDataInfo>> ReferenceData = {
            "accounts": <RestSchemaReferenceDataInfo>{
                "name": "accounts",
                "action": "list-accounts",
                "options": {
                    "pageSize": 200,
                },
                "records_path": (
                    "accounts",
                ),
                "value_field": "name",
                "display_field": "displayName",
                "desc": "Accounts accessible with this API key.",
            },
            "public-models": <RestSchemaReferenceDataInfo>{
                "name": "public-models",
                "action": "list-models",
                "options": {
                    "account_id": "fireworks",
                    "pageSize": 200,
                },
                "records_path": (
                    "models",
                ),
                "value_field": "name",
                "display_field": "displayName",
                "desc": "Public model catalog. Enter another model resource name if it is not on this page.",
            },
            "batch-inference-jobs": <RestSchemaReferenceDataInfo>{
                "name": "batch-inference-jobs",
                "action": "list-batch-inference-jobs",
                "options": {
                    "pageSize": 200,
                },
                "options_from": {
                    "account_id": "account_id",
                },
                "records_path": (
                    "batchInferenceJobs",
                ),
                "value_field": "name",
                "display_field": "displayName",
                "desc": "Resources in the selected account. Enter a resource name directly if it is not on this page.",
            },
            "datasets": <RestSchemaReferenceDataInfo>{
                "name": "datasets",
                "action": "list-datasets",
                "options": {
                    "pageSize": 200,
                },
                "options_from": {
                    "account_id": "account_id",
                },
                "records_path": (
                    "datasets",
                ),
                "value_field": "name",
                "display_field": "displayName",
                "desc": "Resources in the selected account. Enter a resource name directly if it is not on this page.",
            },
            "deployed-models": <RestSchemaReferenceDataInfo>{
                "name": "deployed-models",
                "action": "list-deployed-models",
                "options": {
                    "pageSize": 200,
                },
                "options_from": {
                    "account_id": "account_id",
                },
                "records_path": (
                    "deployedModels",
                ),
                "value_field": "name",
                "display_field": "displayName",
                "desc": "Resources in the selected account. Enter a resource name directly if it is not on this page.",
            },
            "deployment-shapes": <RestSchemaReferenceDataInfo>{
                "name": "deployment-shapes",
                "action": "list-deployment-shapes",
                "options": {
                    "pageSize": 200,
                },
                "options_from": {
                    "account_id": "account_id",
                },
                "records_path": (
                    "deploymentShapes",
                ),
                "value_field": "name",
                "display_field": "displayName",
                "desc": "Resources in the selected account. Enter a resource name directly if it is not on this page.",
            },
            "deployments": <RestSchemaReferenceDataInfo>{
                "name": "deployments",
                "action": "list-deployments",
                "options": {
                    "pageSize": 200,
                },
                "options_from": {
                    "account_id": "account_id",
                },
                "records_path": (
                    "deployments",
                ),
                "value_field": "name",
                "display_field": "displayName",
                "desc": "Resources in the selected account. Enter a resource name directly if it is not on this page.",
            },
            "dpo-jobs": <RestSchemaReferenceDataInfo>{
                "name": "dpo-jobs",
                "action": "list-dpo-jobs",
                "options": {
                    "pageSize": 200,
                },
                "options_from": {
                    "account_id": "account_id",
                },
                "records_path": (
                    "dpoJobs",
                ),
                "value_field": "name",
                "display_field": "displayName",
                "desc": "Resources in the selected account. Enter a resource name directly if it is not on this page.",
            },
            "evaluation-jobs": <RestSchemaReferenceDataInfo>{
                "name": "evaluation-jobs",
                "action": "list-evaluation-jobs",
                "options": {
                    "pageSize": 200,
                },
                "options_from": {
                    "account_id": "account_id",
                },
                "records_path": (
                    "evaluationJobs",
                ),
                "value_field": "name",
                "display_field": "displayName",
                "desc": "Resources in the selected account. Enter a resource name directly if it is not on this page.",
            },
            "evaluations": <RestSchemaReferenceDataInfo>{
                "name": "evaluations",
                "action": "list-evaluations",
                "options": {
                    "pageSize": 200,
                },
                "options_from": {
                    "account_id": "account_id",
                },
                "records_path": (
                    "evaluations",
                ),
                "value_field": "name",
                "display_field": "name",
                "desc": "Resources in the selected account. Enter a resource name directly if it is not on this page.",
            },
            "evaluators": <RestSchemaReferenceDataInfo>{
                "name": "evaluators",
                "action": "list-evaluators",
                "options": {
                    "pageSize": 200,
                },
                "options_from": {
                    "account_id": "account_id",
                },
                "records_path": (
                    "evaluators",
                ),
                "value_field": "name",
                "display_field": "displayName",
                "desc": "Resources in the selected account. Enter a resource name directly if it is not on this page.",
            },
            "models": <RestSchemaReferenceDataInfo>{
                "name": "models",
                "action": "list-models",
                "options": {
                    "pageSize": 200,
                },
                "options_from": {
                    "account_id": "account_id",
                },
                "records_path": (
                    "models",
                ),
                "value_field": "name",
                "display_field": "displayName",
                "desc": "Resources in the selected account. Enter a resource name directly if it is not on this page.",
            },
            "pricing-plans": <RestSchemaReferenceDataInfo>{
                "name": "pricing-plans",
                "action": "list-pricing-plans",
                "options": {
                    "pageSize": 200,
                },
                "options_from": {
                    "account_id": "account_id",
                },
                "records_path": (
                    "pricingPlans",
                ),
                "value_field": "name",
                "display_field": "name",
                "desc": "Resources in the selected account. Enter a resource name directly if it is not on this page.",
            },
            "quotas": <RestSchemaReferenceDataInfo>{
                "name": "quotas",
                "action": "list-quotas",
                "options": {
                    "pageSize": 200,
                },
                "options_from": {
                    "account_id": "account_id",
                },
                "records_path": (
                    "quotas",
                ),
                "value_field": "name",
                "display_field": "name",
                "desc": "Resources in the selected account. Enter a resource name directly if it is not on this page.",
            },
            "reinforcement-fine-tuning-jobs": <RestSchemaReferenceDataInfo>{
                "name": "reinforcement-fine-tuning-jobs",
                "action": "list-reinforcement-fine-tuning-jobs",
                "options": {
                    "pageSize": 200,
                },
                "options_from": {
                    "account_id": "account_id",
                },
                "records_path": (
                    "reinforcementFineTuningJobs",
                ),
                "value_field": "name",
                "display_field": "displayName",
                "desc": "Resources in the selected account. Enter a resource name directly if it is not on this page.",
            },
            "routers": <RestSchemaReferenceDataInfo>{
                "name": "routers",
                "action": "list-routers",
                "options": {
                    "pageSize": 200,
                },
                "options_from": {
                    "account_id": "account_id",
                },
                "records_path": (
                    "routers",
                ),
                "value_field": "name",
                "display_field": "displayName",
                "desc": "Resources in the selected account. Enter a resource name directly if it is not on this page.",
            },
            "serverless-models": <RestSchemaReferenceDataInfo>{
                "name": "serverless-models",
                "action": "list-serverless-models",
                "options": {
                    "pageSize": 200,
                },
                "options_from": {
                    "account_id": "account_id",
                },
                "records_path": (
                    "models",
                ),
                "value_field": "name",
                "display_field": "displayName",
                "desc": "Resources in the selected account. Enter a resource name directly if it is not on this page.",
            },
            "supervised-fine-tuning-jobs": <RestSchemaReferenceDataInfo>{
                "name": "supervised-fine-tuning-jobs",
                "action": "list-supervised-fine-tuning-jobs",
                "options": {
                    "pageSize": 200,
                },
                "options_from": {
                    "account_id": "account_id",
                },
                "records_path": (
                    "supervisedFineTuningJobs",
                ),
                "value_field": "name",
                "display_field": "displayName",
                "desc": "Resources in the selected account. Enter a resource name directly if it is not on this page.",
            },
            "training-shapes": <RestSchemaReferenceDataInfo>{
                "name": "training-shapes",
                "action": "list-training-shapes",
                "options": {
                    "pageSize": 200,
                },
                "options_from": {
                    "account_id": "account_id",
                },
                "records_path": (
                    "trainingShapes",
                ),
                "value_field": "name",
                "display_field": "displayName",
                "desc": "Resources in the selected account. Enter a resource name directly if it is not on this page.",
            },
            "user-usage-limits": <RestSchemaReferenceDataInfo>{
                "name": "user-usage-limits",
                "action": "list-user-usage-limits",
                "options": {
                    "pageSize": 200,
                },
                "options_from": {
                    "account_id": "account_id",
                },
                "records_path": (
                    "userUsageLimits",
                ),
                "value_field": "name",
                "display_field": "name",
                "desc": "Resources in the selected account. Enter a resource name directly if it is not on this page.",
            },
            "inference-models": <RestSchemaReferenceDataInfo>{
                "name": "inference-models",
                "action": "list-serverless-models",
                "options": {
                    "pageSize": 200,
                },
                "options_reference": "accounts",
                "reference_option": "account_id",
                "records_path": (
                    "models",
                ),
                "value_field": "name",
                "display_field": "displayName",
                "desc": "Serverless models available to your accounts. Enter a dedicated model or router resource name directly.",
            },
            "deployment-shape-versions": <RestSchemaReferenceDataInfo>{
                "name": "deployment-shape-versions",
                "action": "list-deployment-shape-versions",
                "options": {
                    "pageSize": 200,
                },
                "options_from": {
                    "account_id": "account_id",
                    "deployment_shape_id": "deployment_shape_id",
                },
                "records_path": (
                    "deploymentShapeVersions",
                ),
                "value_field": "name",
                "display_field": "name",
            },
            "evaluator-versions": <RestSchemaReferenceDataInfo>{
                "name": "evaluator-versions",
                "action": "list-evaluator-versions",
                "options": {
                    "pageSize": 200,
                },
                "options_from": {
                    "account_id": "account_id",
                    "evaluator_id": "evaluator_id",
                },
                "records_path": (
                    "evaluatorVersions",
                ),
                "value_field": "name",
                "display_field": "name",
            },
            "training-shape-versions": <RestSchemaReferenceDataInfo>{
                "name": "training-shape-versions",
                "action": "list-training-shape-versions",
                "options": {
                    "pageSize": 200,
                },
                "options_from": {
                    "account_id": "account_id",
                    "training_shape_id": "training_shape_id",
                },
                "records_path": (
                    "trainingShapeVersions",
                ),
                "value_field": "name",
                "display_field": "name",
            },
        };

        const hash<string, hash<RestSchemaReferenceDataInfo>> ResponseReferenceData = {
            "responses": <RestSchemaReferenceDataInfo>{
                "name": "responses",
                "action": "list-responses",
                "records_path": (
                    "data",
                ),
                "value_field": "id",
                "display_field": "id",
                "desc": "Stored responses visible to this API key.",
            },
        };

        const list<hash<RestSchemaActionInfo>> ControlManifest = (
            <RestSchemaActionInfo>{
                "action": "list-accounts",
                "method": "GET",
                "path": "/v1/accounts",
                "operation_id": "Gateway_ListAccounts",
                "display_name": "List Accounts",
                "short_desc": "List Accounts in Fireworks AI",
                "desc": "List Accounts. Choose the account to use in subsequent Fireworks operations. Use `pageToken` with the returned `nextPageToken` to retrieve further pages.",
                "groups": (
                    "Accounts",
                ),
                "flatten_body": True,
                "fields": {
                    "pageSize": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "preselected": True,
                        "example_value": 20,
                    },
                    "pageToken": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Token",
                        "desc": "Use `nextPageToken` from the previous result to fetch another page.",
                    },
                    "filter": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Filter",
                        "desc": "Filter the results using the [Fireworks filter syntax](https://docs.fireworks.ai/api-reference/list-models).",
                        "preselected": True,
                    },
                    "orderBy": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Order By",
                        "desc": "Order results by supported resource fields, for example `name`.",
                    },
                    "readMask": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Read Mask",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-account",
                "method": "GET",
                "path": "/v1/accounts/{account_id}",
                "operation_id": "Gateway_GetAccount",
                "display_name": "Get Account",
                "short_desc": "Get Account in Fireworks AI",
                "desc": "Get Account. Choose the account to use in subsequent Fireworks operations.",
                "groups": (
                    "Accounts",
                ),
                "flatten_body": True,
                "fields": {
                    "readMask": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Read Mask",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-account-usage-filter-options",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/accountUsageFilterOptions",
                "operation_id": "Gateway_GetAccountUsageFilterOptions",
                "display_name": "Get Account Usage Filter Options",
                "short_desc": "Get Account Usage Filter Options in Fireworks AI",
                "desc": "Get Account Usage Filter Options. Monitor usage, limits, and costs for your account.",
                "groups": (
                    "Usage",
                ),
                "flatten_body": True,
                "fields": {
                    "startTime": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Start Time",
                        "codec": "iso-8601-utc",
                    },
                    "endTime": <RestSchemaFieldOverlayInfo>{
                        "display_name": "End Time",
                        "codec": "iso-8601-utc",
                    },
                    "usageType": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Usage Type",
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "USAGE_TYPE_UNSPECIFIED",
                                "display_name": "Unspecified",
                            },
                            <AllowedValueInfo>{
                                "value": "SERVERLESS",
                                "display_name": "Serverless",
                            },
                            <AllowedValueInfo>{
                                "value": "DEDICATED_DEPLOYMENT",
                                "display_name": "Dedicated Deployment",
                            },
                            <AllowedValueInfo>{
                                "value": "TRAINING",
                                "display_name": "Training",
                            },
                        ),
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-audit-logs",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/auditLogs",
                "operation_id": "Gateway_ListAuditLogs",
                "display_name": "List Audit Logs",
                "short_desc": "List Audit Logs in Fireworks AI",
                "desc": "List Audit Logs. Review account activity for operational investigations. Use `pageToken` with the returned `nextPageToken` to retrieve further pages.",
                "groups": (
                    "Audit Logs",
                ),
                "flatten_body": True,
                "fields": {
                    "startTime": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Start Time",
                        "codec": "iso-8601-utc",
                    },
                    "endTime": <RestSchemaFieldOverlayInfo>{
                        "display_name": "End Time",
                        "codec": "iso-8601-utc",
                    },
                    "email": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Email",
                    },
                    "pageSize": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "preselected": True,
                        "example_value": 20,
                    },
                    "pageToken": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Token",
                        "desc": "Use `nextPageToken` from the previous result to fetch another page.",
                    },
                    "filter": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Filter",
                        "desc": "Filter the results using the [Fireworks filter syntax](https://docs.fireworks.ai/api-reference/list-models).",
                        "preselected": True,
                    },
                    "orderBy": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Order By",
                        "desc": "Order results by supported resource fields, for example `name`.",
                    },
                    "readMask": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Read Mask",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-batch-inference-jobs",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/batchInferenceJobs",
                "operation_id": "Gateway_ListBatchInferenceJobs",
                "display_name": "List Batch Inference Jobs",
                "short_desc": "List Batch Inference Jobs in Fireworks AI",
                "desc": "List Batch Inference Jobs. Run large inference workloads asynchronously and inspect their results. **Creating jobs incurs inference charges.** Use `pageToken` with the returned `nextPageToken` to retrieve further pages.",
                "groups": (
                    "Batch Inference",
                ),
                "flatten_body": True,
                "fields": {
                    "pageSize": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "preselected": True,
                        "example_value": 20,
                    },
                    "pageToken": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Token",
                        "desc": "Use `nextPageToken` from the previous result to fetch another page.",
                    },
                    "filter": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Filter",
                        "desc": "Filter the results using the [Fireworks filter syntax](https://docs.fireworks.ai/api-reference/list-models).",
                        "preselected": True,
                    },
                    "orderBy": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Order By",
                        "desc": "Order results by supported resource fields, for example `name`.",
                    },
                    "readMask": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Read Mask",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-batch-inference-job",
                "method": "POST",
                "path": "/v1/accounts/{account_id}/batchInferenceJobs",
                "operation_id": "Gateway_CreateBatchInferenceJob",
                "display_name": "Create Batch Inference Job",
                "short_desc": "Create Batch Inference Job in Fireworks AI",
                "desc": "Create Batch Inference Job. Run large inference workloads asynchronously and inspect their results. **Creating jobs incurs inference charges.**",
                "groups": (
                    "Batch Inference",
                ),
                "flatten_body": True,
                "fields": {
                    "batchInferenceJobId": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Batch Inference Job ID",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "createTime": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "expireTime": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "createdBy": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "state": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "status": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "model": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "public-models",
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                    "updateTime": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "precision": <RestSchemaFieldOverlayInfo>{
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "PRECISION_UNSPECIFIED",
                                "display_name": "Unspecified",
                            },
                            <AllowedValueInfo>{
                                "value": "FP16",
                                "display_name": "FP16",
                            },
                            <AllowedValueInfo>{
                                "value": "FP8",
                                "display_name": "FP8",
                            },
                            <AllowedValueInfo>{
                                "value": "FP8_MM",
                                "display_name": "FP8 MM",
                            },
                            <AllowedValueInfo>{
                                "value": "FP8_AR",
                                "display_name": "FP8 AR",
                            },
                            <AllowedValueInfo>{
                                "value": "FP8_MM_KV_ATTN",
                                "display_name": "FP8 MM KV ATTN",
                            },
                            <AllowedValueInfo>{
                                "value": "FP8_KV",
                                "display_name": "FP8 KV",
                            },
                            <AllowedValueInfo>{
                                "value": "FP8_MM_V2",
                                "display_name": "FP8 MM V2",
                            },
                            <AllowedValueInfo>{
                                "value": "FP8_V2",
                                "display_name": "FP8 V2",
                            },
                            <AllowedValueInfo>{
                                "value": "FP8_MM_KV_ATTN_V2",
                                "display_name": "FP8 MM KV ATTN V2",
                            },
                            <AllowedValueInfo>{
                                "value": "NF4",
                                "display_name": "NF4",
                            },
                            <AllowedValueInfo>{
                                "value": "FP4",
                                "display_name": "FP4",
                            },
                            <AllowedValueInfo>{
                                "value": "BF16",
                                "display_name": "BF16",
                            },
                            <AllowedValueInfo>{
                                "value": "FP4_BLOCKSCALED_MM",
                                "display_name": "FP4 Blockscaled MM",
                            },
                            <AllowedValueInfo>{
                                "value": "FP4_MX_MOE",
                                "display_name": "FP4 Mx MOE",
                            },
                        ),
                    },
                    "jobProgress": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "placement": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "region": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (
                                    <AllowedValueInfo>{
                                        "value": "REGION_UNSPECIFIED",
                                        "display_name": "Unspecified",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_IOWA_1",
                                        "display_name": "US: Iowa 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_VIRGINIA_1",
                                        "display_name": "US: Virginia 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_VIRGINIA_2",
                                        "display_name": "US: Virginia 2",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_ILLINOIS_1",
                                        "display_name": "US: Illinois 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "AP_TOKYO_1",
                                        "display_name": "Asia Pacific: Tokyo 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_ARIZONA_1",
                                        "display_name": "US: Arizona 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_TEXAS_1",
                                        "display_name": "US: Texas 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_ILLINOIS_2",
                                        "display_name": "US: Illinois 2",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "EU_FRANKFURT_1",
                                        "display_name": "Europe: Frankfurt 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_TEXAS_2",
                                        "display_name": "US: Texas 2",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "EU_ICELAND_1",
                                        "display_name": "Europe: Iceland 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "EU_ICELAND_2",
                                        "display_name": "Europe: Iceland 2",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_WASHINGTON_1",
                                        "display_name": "US: Washington 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_WASHINGTON_2",
                                        "display_name": "US: Washington 2",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_WASHINGTON_3",
                                        "display_name": "US: Washington 3",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "AP_TOKYO_2",
                                        "display_name": "Asia Pacific: Tokyo 2",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_CALIFORNIA_1",
                                        "display_name": "US: California 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_UTAH_1",
                                        "display_name": "US: Utah 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_ARIZONA_3",
                                        "display_name": "US: Arizona 3",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_GEORGIA_1",
                                        "display_name": "US: Georgia 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_GEORGIA_2",
                                        "display_name": "US: Georgia 2",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_WASHINGTON_4",
                                        "display_name": "US: Washington 4",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_GEORGIA_3",
                                        "display_name": "US: Georgia 3",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "NA_BRITISHCOLUMBIA_1",
                                        "display_name": "North America: British Columbia 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_GEORGIA_4",
                                        "display_name": "US: Georgia 4",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_OHIO_1",
                                        "display_name": "US: Ohio 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_NEWYORK_1",
                                        "display_name": "US: New York 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "EU_NETHERLANDS_1",
                                        "display_name": "Europe: Netherlands 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_WASHINGTON_5",
                                        "display_name": "US: Washington 5",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_MINNESOTA_1",
                                        "display_name": "US: Minnesota 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_CALIFORNIA_2",
                                        "display_name": "US: California 2",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "NA_BRITISHCOLUMBIA_2",
                                        "display_name": "North America: British Columbia 2",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "AP_MALAYSIA_2",
                                        "display_name": "Asia Pacific: Malaysia 2",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_OREGON_1",
                                        "display_name": "US: Oregon 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "NA_BRITISHCOLUMBIA_3",
                                        "display_name": "North America: British Columbia 3",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "AP_NEWSOUTHWALES_1",
                                        "display_name": "Asia Pacific: New South Wales 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "AP_TAIWAN_1",
                                        "display_name": "Asia Pacific: Taiwan 1",
                                    },
                                ),
                            },
                            "multiRegion": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (
                                    <AllowedValueInfo>{
                                        "value": "MULTI_REGION_UNSPECIFIED",
                                        "display_name": "Unspecified",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "GLOBAL",
                                        "display_name": "Global",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US",
                                        "display_name": "Us",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "EUROPE",
                                        "display_name": "Europe",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "APAC",
                                        "display_name": "Apac",
                                    },
                                ),
                            },
                        },
                    },
                    "lifecycle": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "waitingOnCapacity": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "displayName": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-batch-inference-job",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/batchInferenceJobs/{batch_inference_job_id}",
                "operation_id": "Gateway_GetBatchInferenceJob",
                "display_name": "Get Batch Inference Job",
                "short_desc": "Get Batch Inference Job in Fireworks AI",
                "desc": "Get Batch Inference Job. Run large inference workloads asynchronously and inspect their results. **Creating jobs incurs inference charges.**",
                "groups": (
                    "Batch Inference",
                ),
                "flatten_body": True,
                "fields": {
                    "readMask": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Read Mask",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "batch_inference_job_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Batch Inference Job ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-batch-inference-job",
                        "ref_data": "batch-inference-jobs",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-batch-inference-job",
                "method": "DELETE",
                "path": "/v1/accounts/{account_id}/batchInferenceJobs/{batch_inference_job_id}",
                "operation_id": "Gateway_DeleteBatchInferenceJob",
                "display_name": "Delete Batch Inference Job",
                "short_desc": "Delete Batch Inference Job in Fireworks AI",
                "desc": "Delete Batch Inference Job. Run large inference workloads asynchronously and inspect their results. **Creating jobs incurs inference charges.** **Deletion is permanent.**",
                "groups": (
                    "Batch Inference",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "batch_inference_job_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Batch Inference Job ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-batch-inference-job",
                        "ref_data": "batch-inference-jobs",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-billing-summary",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/billing/summary",
                "operation_id": "Gateway_GetBillingSummary",
                "display_name": "Get Billing Summary",
                "short_desc": "Get Billing Summary in Fireworks AI",
                "desc": "Get Billing Summary. Inspect billing information to track spending.",
                "groups": (
                    "Billing",
                ),
                "flatten_body": True,
                "fields": {
                    "startTime": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Start Time",
                        "codec": "iso-8601-utc",
                    },
                    "endTime": <RestSchemaFieldOverlayInfo>{
                        "display_name": "End Time",
                        "codec": "iso-8601-utc",
                    },
                    "granularity": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Granularity",
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "GRANULARITY_UNSPECIFIED",
                                "display_name": "Unspecified",
                            },
                            <AllowedValueInfo>{
                                "value": "DAILY",
                                "display_name": "Daily",
                            },
                        ),
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "query-usage-costs",
                "method": "POST",
                "path": "/v1/accounts/{account_id}/usageCosts:query",
                "operation_id": "Gateway_QueryUsageCosts",
                "display_name": "Query Usage Costs",
                "short_desc": "Query Usage Costs in Fireworks AI",
                "desc": "Query Usage Costs. Monitor usage, limits, and costs for your account.",
                "groups": (
                    "Usage",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "startTime": <RestSchemaFieldOverlayInfo>{
                        "codec": "iso-8601-utc",
                    },
                    "endTime": <RestSchemaFieldOverlayInfo>{
                        "codec": "iso-8601-utc",
                    },
                    "scope": <RestSchemaFieldOverlayInfo>{
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "SCOPE_UNSPECIFIED",
                                "display_name": "Unspecified",
                            },
                            <AllowedValueInfo>{
                                "value": "ACCOUNT",
                                "display_name": "Account",
                            },
                            <AllowedValueInfo>{
                                "value": "SELF",
                                "display_name": "Self",
                            },
                        ),
                    },
                    "filter": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "model": <RestSchemaFieldOverlayInfo>{
                                "ref_data": "public-models",
                                "supports_custom_values": True,
                            },
                            "apiKeyId": <RestSchemaFieldOverlayInfo>{
                                "sensitive": True,
                            },
                        },
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-billing-settings",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/billingSettings",
                "operation_id": "Gateway_GetBillingSettings",
                "display_name": "Get Billing Settings",
                "short_desc": "Get Billing Settings in Fireworks AI",
                "desc": "Get Billing Settings. Inspect billing information to track spending.",
                "groups": (
                    "Billing",
                ),
                "flatten_body": True,
                "fields": {
                    "readMask": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Read Mask",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-account-usage",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/billingUsage",
                "operation_id": "Gateway_GetAccountUsage",
                "display_name": "Get Account Usage",
                "short_desc": "Get Account Usage in Fireworks AI",
                "desc": "Get Account Usage. Monitor usage, limits, and costs for your account.",
                "groups": (
                    "Usage",
                ),
                "flatten_body": True,
                "fields": {
                    "startTime": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Start Time",
                        "codec": "iso-8601-utc",
                    },
                    "endTime": <RestSchemaFieldOverlayInfo>{
                        "display_name": "End Time",
                        "codec": "iso-8601-utc",
                    },
                    "usageType": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Usage Type",
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "USAGE_TYPE_UNSPECIFIED",
                                "display_name": "Unspecified",
                            },
                            <AllowedValueInfo>{
                                "value": "SERVERLESS",
                                "display_name": "Serverless",
                            },
                            <AllowedValueInfo>{
                                "value": "DEDICATED_DEPLOYMENT",
                                "display_name": "Dedicated Deployment",
                            },
                            <AllowedValueInfo>{
                                "value": "TRAINING",
                                "display_name": "Training",
                            },
                        ),
                    },
                    "timezone": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Timezone",
                    },
                    "groupBy": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Group By",
                    },
                    "filter": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Filter",
                        "desc": "Filter the results using the [Fireworks filter syntax](https://docs.fireworks.ai/api-reference/list-models).",
                        "preselected": True,
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-datasets",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/datasets",
                "operation_id": "Gateway_ListDatasets",
                "display_name": "List Datasets",
                "short_desc": "List Datasets in Fireworks AI",
                "desc": "List Datasets. Prepare and manage the data used by batch inference and fine-tuning. Use `pageToken` with the returned `nextPageToken` to retrieve further pages.",
                "groups": (
                    "Datasets",
                ),
                "flatten_body": True,
                "fields": {
                    "pageSize": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "preselected": True,
                        "example_value": 20,
                    },
                    "pageToken": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Token",
                        "desc": "Use `nextPageToken` from the previous result to fetch another page.",
                    },
                    "filter": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Filter",
                        "desc": "Filter the results using the [Fireworks filter syntax](https://docs.fireworks.ai/api-reference/list-models).",
                        "preselected": True,
                    },
                    "orderBy": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Order By",
                        "desc": "Order results by supported resource fields, for example `name`.",
                    },
                    "readMask": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Read Mask",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-dataset",
                "method": "POST",
                "path": "/v1/accounts/{account_id}/datasets",
                "operation_id": "Gateway_CreateDataset",
                "display_name": "Create Dataset",
                "short_desc": "Create Dataset in Fireworks AI",
                "desc": "Create Dataset. Prepare and manage the data used by batch inference and fine-tuning.",
                "groups": (
                    "Datasets",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "dataset": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "name": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                            "createTime": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                            "state": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                            "status": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                            "transformed": <RestSchemaFieldOverlayInfo>{
                                "fields": {
                                    "originalFormat": <RestSchemaFieldOverlayInfo>{
                                        "allowed_values": (
                                            <AllowedValueInfo>{
                                                "value": "FORMAT_UNSPECIFIED",
                                                "display_name": "Unspecified",
                                            },
                                            <AllowedValueInfo>{
                                                "value": "CHAT",
                                                "display_name": "Chat",
                                            },
                                            <AllowedValueInfo>{
                                                "value": "COMPLETION",
                                                "display_name": "Completion",
                                            },
                                            <AllowedValueInfo>{
                                                "value": "RL",
                                                "display_name": "Reinforcement Learning",
                                            },
                                        ),
                                    },
                                },
                            },
                            "format": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (
                                    <AllowedValueInfo>{
                                        "value": "FORMAT_UNSPECIFIED",
                                        "display_name": "Unspecified",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "CHAT",
                                        "display_name": "Chat",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "COMPLETION",
                                        "display_name": "Completion",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "RL",
                                        "display_name": "Reinforcement Learning",
                                    },
                                ),
                            },
                            "createdBy": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                            "updateTime": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                            "estimatedTokenCount": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                            "averageTurnCount": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                            "encryptionState": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                        },
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-dataset",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/datasets/{dataset_id}",
                "operation_id": "Gateway_GetDataset",
                "display_name": "Get Dataset",
                "short_desc": "Get Dataset in Fireworks AI",
                "desc": "Get Dataset. Prepare and manage the data used by batch inference and fine-tuning.",
                "groups": (
                    "Datasets",
                ),
                "flatten_body": True,
                "fields": {
                    "readMask": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Read Mask",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "dataset_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Dataset ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-dataset",
                        "ref_data": "datasets",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-dataset",
                "method": "PATCH",
                "path": "/v1/accounts/{account_id}/datasets/{dataset_id}",
                "operation_id": "Gateway_UpdateDataset",
                "display_name": "Update Dataset",
                "short_desc": "Update Dataset in Fireworks AI",
                "desc": "Update Dataset. Prepare and manage the data used by batch inference and fine-tuning.",
                "groups": (
                    "Datasets",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "dataset_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Dataset ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-dataset",
                        "ref_data": "datasets",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                    "createTime": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "state": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "status": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "transformed": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "originalFormat": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (
                                    <AllowedValueInfo>{
                                        "value": "FORMAT_UNSPECIFIED",
                                        "display_name": "Unspecified",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "CHAT",
                                        "display_name": "Chat",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "COMPLETION",
                                        "display_name": "Completion",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "RL",
                                        "display_name": "Reinforcement Learning",
                                    },
                                ),
                            },
                        },
                    },
                    "format": <RestSchemaFieldOverlayInfo>{
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "FORMAT_UNSPECIFIED",
                                "display_name": "Unspecified",
                            },
                            <AllowedValueInfo>{
                                "value": "CHAT",
                                "display_name": "Chat",
                            },
                            <AllowedValueInfo>{
                                "value": "COMPLETION",
                                "display_name": "Completion",
                            },
                            <AllowedValueInfo>{
                                "value": "RL",
                                "display_name": "Reinforcement Learning",
                            },
                        ),
                    },
                    "createdBy": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "updateTime": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "estimatedTokenCount": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "averageTurnCount": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "encryptionState": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "displayName": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-dataset",
                "method": "DELETE",
                "path": "/v1/accounts/{account_id}/datasets/{dataset_id}",
                "operation_id": "Gateway_DeleteDataset",
                "display_name": "Delete Dataset",
                "short_desc": "Delete Dataset in Fireworks AI",
                "desc": "Delete Dataset. Prepare and manage the data used by batch inference and fine-tuning. **Deletion is permanent.**",
                "groups": (
                    "Datasets",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "dataset_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Dataset ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-dataset",
                        "ref_data": "datasets",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-dataset-download-endpoint",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/datasets/{dataset_id}:getDownloadEndpoint",
                "operation_id": "Gateway_GetDatasetDownloadEndpoint",
                "display_name": "Get Dataset Download Endpoint",
                "short_desc": "Get Dataset Download Endpoint in Fireworks AI",
                "desc": "Get Dataset Download Endpoint. Prepare and manage the data used by batch inference and fine-tuning.",
                "groups": (
                    "Datasets",
                ),
                "flatten_body": True,
                "fields": {
                    "readMask": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Read Mask",
                    },
                    "downloadLineage": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Download Lineage",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "dataset_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Dataset ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-dataset",
                        "ref_data": "datasets",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-dataset-upload-endpoint",
                "method": "POST",
                "path": "/v1/accounts/{account_id}/datasets/{dataset_id}:getUploadEndpoint",
                "operation_id": "Gateway_GetDatasetUploadEndpoint",
                "display_name": "Get Dataset Upload Endpoint",
                "short_desc": "Get Dataset Upload Endpoint in Fireworks AI",
                "desc": "Get Dataset Upload Endpoint. Prepare and manage the data used by batch inference and fine-tuning.",
                "groups": (
                    "Datasets",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "dataset_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Dataset ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-dataset",
                        "ref_data": "datasets",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "split-dataset",
                "method": "POST",
                "path": "/v1/accounts/{account_id}/datasets/{dataset_id}:splitDataset",
                "operation_id": "Gateway_SplitDataset",
                "display_name": "Split Dataset",
                "short_desc": "Split Dataset in Fireworks AI",
                "desc": "Split Dataset. Prepare and manage the data used by batch inference and fine-tuning.",
                "groups": (
                    "Datasets",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "dataset_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Dataset ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-dataset",
                        "ref_data": "datasets",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "validate-dataset-upload",
                "method": "POST",
                "path": "/v1/accounts/{account_id}/datasets/{dataset_id}:validateUpload",
                "operation_id": "Gateway_ValidateDatasetUpload",
                "display_name": "Validate Dataset Upload",
                "short_desc": "Validate Dataset Upload in Fireworks AI",
                "desc": "Validate Dataset Upload. Prepare and manage the data used by batch inference and fine-tuning.",
                "groups": (
                    "Datasets",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "dataset_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Dataset ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-dataset",
                        "ref_data": "datasets",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-deployed-models",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/deployedModels",
                "operation_id": "Gateway_ListDeployedModels",
                "display_name": "List Deployed Models",
                "short_desc": "List Deployed Models in Fireworks AI",
                "desc": "List Deployed Models. Manage the capacity serving your models. **Creating, scaling, or restoring capacity can incur charges.** Use `pageToken` with the returned `nextPageToken` to retrieve further pages.",
                "groups": (
                    "Deployments",
                ),
                "flatten_body": True,
                "fields": {
                    "pageSize": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "preselected": True,
                        "example_value": 20,
                    },
                    "pageToken": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Token",
                        "desc": "Use `nextPageToken` from the previous result to fetch another page.",
                    },
                    "filter": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Filter",
                        "desc": "Filter the results using the [Fireworks filter syntax](https://docs.fireworks.ai/api-reference/list-models).",
                        "preselected": True,
                    },
                    "orderBy": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Order By",
                        "desc": "Order results by supported resource fields, for example `name`.",
                    },
                    "readMask": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Read Mask",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-deployed-model",
                "method": "POST",
                "path": "/v1/accounts/{account_id}/deployedModels",
                "operation_id": "Gateway_CreateDeployedModel",
                "display_name": "Create Deployed Model",
                "short_desc": "Create Deployed Model in Fireworks AI",
                "desc": "Create Deployed Model. Manage the capacity serving your models. **Creating, scaling, or restoring capacity can incur charges.**",
                "groups": (
                    "Deployments",
                ),
                "flatten_body": True,
                "fields": {
                    "replaceMergedAddon": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Replace Merged Addon",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "createTime": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "model": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "public-models",
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                    "deployment": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "deployments",
                        "supports_custom_values": True,
                        "depends_on": (
                            "account_id",
                        ),
                    },
                    "state": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "status": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "updateTime": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "displayName": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-deployed-model",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/deployedModels/{deployed_model_id}",
                "operation_id": "Gateway_GetDeployedModel",
                "display_name": "Get Deployed Model",
                "short_desc": "Get Deployed Model in Fireworks AI",
                "desc": "Get Deployed Model. Manage the capacity serving your models. **Creating, scaling, or restoring capacity can incur charges.**",
                "groups": (
                    "Deployments",
                ),
                "flatten_body": True,
                "fields": {
                    "readMask": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Read Mask",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "deployed_model_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Deployed Model ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-deployed-model",
                        "ref_data": "deployed-models",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-deployed-model",
                "method": "PATCH",
                "path": "/v1/accounts/{account_id}/deployedModels/{deployed_model_id}",
                "operation_id": "Gateway_UpdateDeployedModel",
                "display_name": "Update Deployed Model",
                "short_desc": "Update Deployed Model in Fireworks AI",
                "desc": "Update Deployed Model. Manage the capacity serving your models. **Creating, scaling, or restoring capacity can incur charges.**",
                "groups": (
                    "Deployments",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "deployed_model_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Deployed Model ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-deployed-model",
                        "ref_data": "deployed-models",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                    "createTime": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "model": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "public-models",
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                    "deployment": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "deployments",
                        "supports_custom_values": True,
                        "depends_on": (
                            "account_id",
                        ),
                    },
                    "state": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "status": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "updateTime": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "displayName": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-deployed-model",
                "method": "DELETE",
                "path": "/v1/accounts/{account_id}/deployedModels/{deployed_model_id}",
                "operation_id": "Gateway_DeleteDeployedModel",
                "display_name": "Delete Deployed Model",
                "short_desc": "Delete Deployed Model in Fireworks AI",
                "desc": "Delete Deployed Model. Manage the capacity serving your models. **Creating, scaling, or restoring capacity can incur charges.** **Deletion is permanent.**",
                "groups": (
                    "Deployments",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "deployed_model_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Deployed Model ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-deployed-model",
                        "ref_data": "deployed-models",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-deployment-shapes",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/deploymentShapes",
                "operation_id": "Gateway_ListDeploymentShapes",
                "display_name": "List Deployment Shapes",
                "short_desc": "List Deployment Shapes in Fireworks AI",
                "desc": "List Deployment Shapes. Inspect supported deployment configurations before provisioning capacity. Use `pageToken` with the returned `nextPageToken` to retrieve further pages.",
                "groups": (
                    "Deployment Shapes",
                ),
                "flatten_body": True,
                "fields": {
                    "pageSize": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "preselected": True,
                        "example_value": 20,
                    },
                    "pageToken": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Token",
                        "desc": "Use `nextPageToken` from the previous result to fetch another page.",
                    },
                    "filter": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Filter",
                        "desc": "Filter the results using the [Fireworks filter syntax](https://docs.fireworks.ai/api-reference/list-models).",
                        "preselected": True,
                    },
                    "orderBy": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Order By",
                        "desc": "Order results by supported resource fields, for example `name`.",
                    },
                    "readMask": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Read Mask",
                    },
                    "targetModel": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Target Model",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-deployment-shape",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/deploymentShapes/{deployment_shape_id}",
                "operation_id": "Gateway_GetDeploymentShape",
                "display_name": "Get Deployment Shape",
                "short_desc": "Get Deployment Shape in Fireworks AI",
                "desc": "Get Deployment Shape. Inspect supported deployment configurations before provisioning capacity.",
                "groups": (
                    "Deployment Shapes",
                ),
                "flatten_body": True,
                "fields": {
                    "readMask": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Read Mask",
                    },
                    "skipShapeValidation": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Skip Shape Validation",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "deployment_shape_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Deployment Shape ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-deployment-shape",
                        "ref_data": "deployment-shapes",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-deployment-shape-versions",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/deploymentShapes/{deployment_shape_id}/versions",
                "operation_id": "Gateway_ListDeploymentShapeVersions",
                "display_name": "List Deployment Shape Versions",
                "short_desc": "List Deployment Shape Versions in Fireworks AI",
                "desc": "List Deployment Shape Versions. Inspect supported deployment configurations before provisioning capacity. Use `pageToken` with the returned `nextPageToken` to retrieve further pages.",
                "groups": (
                    "Deployment Shapes",
                ),
                "flatten_body": True,
                "fields": {
                    "pageSize": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "preselected": True,
                        "example_value": 20,
                    },
                    "pageToken": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Token",
                        "desc": "Use `nextPageToken` from the previous result to fetch another page.",
                    },
                    "filter": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Filter",
                        "desc": "Filter the results using the [Fireworks filter syntax](https://docs.fireworks.ai/api-reference/list-models).",
                        "preselected": True,
                    },
                    "orderBy": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Order By",
                        "desc": "Order results by supported resource fields, for example `name`.",
                    },
                    "readMask": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Read Mask",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "deployment_shape_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Deployment Shape ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-deployment-shape",
                        "ref_data": "deployment-shapes",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-deployment-shape-version",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/deploymentShapes/{deployment_shape_id}/versions/{version_id}",
                "operation_id": "Gateway_GetDeploymentShapeVersion",
                "display_name": "Get Deployment Shape Version",
                "short_desc": "Get Deployment Shape Version in Fireworks AI",
                "desc": "Get Deployment Shape Version. Inspect supported deployment configurations before provisioning capacity.",
                "groups": (
                    "Deployment Shapes",
                ),
                "flatten_body": True,
                "fields": {
                    "readMask": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Read Mask",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "deployment_shape_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Deployment Shape ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-deployment-shape",
                        "ref_data": "deployment-shapes",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                    "version_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Version ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-version",
                        "ref_data": "deployment-shape-versions",
                        "depends_on": (
                            "account_id",
                            "deployment_shape_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-deployments",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/deployments",
                "operation_id": "Gateway_ListDeployments",
                "display_name": "List Deployments",
                "short_desc": "List Deployments in Fireworks AI",
                "desc": "List Deployments. Manage the capacity serving your models. **Creating, scaling, or restoring capacity can incur charges.** Use `pageToken` with the returned `nextPageToken` to retrieve further pages.",
                "groups": (
                    "Deployments",
                ),
                "flatten_body": True,
                "fields": {
                    "pageSize": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "preselected": True,
                        "example_value": 20,
                    },
                    "pageToken": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Token",
                        "desc": "Use `nextPageToken` from the previous result to fetch another page.",
                    },
                    "filter": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Filter",
                        "desc": "Filter the results using the [Fireworks filter syntax](https://docs.fireworks.ai/api-reference/list-models).",
                        "preselected": True,
                    },
                    "orderBy": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Order By",
                        "desc": "Order results by supported resource fields, for example `name`.",
                    },
                    "showDeleted": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Show Deleted",
                    },
                    "readMask": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Read Mask",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-deployment",
                "method": "POST",
                "path": "/v1/accounts/{account_id}/deployments",
                "operation_id": "Gateway_CreateDeployment",
                "display_name": "Create Deployment",
                "short_desc": "Create Deployment in Fireworks AI",
                "desc": "Create Deployment. Manage the capacity serving your models. **Creating, scaling, or restoring capacity can incur charges.**",
                "groups": (
                    "Deployments",
                ),
                "flatten_body": True,
                "fields": {
                    "disableAutoDeploy": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Disable Auto Deploy",
                    },
                    "disableSpeculativeDecoding": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Disable Speculative Decoding",
                    },
                    "deploymentId": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Deployment ID",
                    },
                    "validateOnly": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Validate Only",
                    },
                    "skipShapeValidation": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Skip Shape Validation",
                    },
                    "skipImageTagValidation": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Skip Image Tag Validation",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "createTime": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "expireTime": <RestSchemaFieldOverlayInfo>{
                        "codec": "iso-8601-utc",
                    },
                    "purgeTime": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "deleteTime": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "state": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "status": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "desiredReplicaCount": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "replicaCount": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "baseModel": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "public-models",
                        "supports_custom_values": True,
                    },
                    "acceleratorType": <RestSchemaFieldOverlayInfo>{
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "ACCELERATOR_TYPE_UNSPECIFIED",
                                "display_name": "Unspecified",
                            },
                            <AllowedValueInfo>{
                                "value": "NVIDIA_A100_80GB",
                                "display_name": "NVIDIA A100 80GB",
                            },
                            <AllowedValueInfo>{
                                "value": "NVIDIA_H100_80GB",
                                "display_name": "NVIDIA H100 80GB",
                            },
                            <AllowedValueInfo>{
                                "value": "AMD_MI300X_192GB",
                                "display_name": "AMD MI300X 192GB",
                            },
                            <AllowedValueInfo>{
                                "value": "NVIDIA_A10G_24GB",
                                "display_name": "NVIDIA A10G 24GB",
                            },
                            <AllowedValueInfo>{
                                "value": "NVIDIA_A100_40GB",
                                "display_name": "NVIDIA A100 40GB",
                            },
                            <AllowedValueInfo>{
                                "value": "NVIDIA_L4_24GB",
                                "display_name": "NVIDIA L4 24GB",
                            },
                            <AllowedValueInfo>{
                                "value": "NVIDIA_H200_141GB",
                                "display_name": "NVIDIA H200 141GB",
                            },
                            <AllowedValueInfo>{
                                "value": "NVIDIA_B200_180GB",
                                "display_name": "NVIDIA B200 180GB",
                            },
                            <AllowedValueInfo>{
                                "value": "AMD_MI325X_256GB",
                                "display_name": "AMD MI325X 256GB",
                            },
                            <AllowedValueInfo>{
                                "value": "AMD_MI350X_288GB",
                                "display_name": "AMD MI350X 288GB",
                            },
                            <AllowedValueInfo>{
                                "value": "NVIDIA_B300_288GB",
                                "display_name": "NVIDIA B300 288GB",
                            },
                            <AllowedValueInfo>{
                                "value": "NVIDIA_GB200",
                                "display_name": "NVIDIA GB200",
                            },
                            <AllowedValueInfo>{
                                "value": "NVIDIA_GB300",
                                "display_name": "NVIDIA GB300",
                            },
                        ),
                    },
                    "precision": <RestSchemaFieldOverlayInfo>{
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "PRECISION_UNSPECIFIED",
                                "display_name": "Unspecified",
                            },
                            <AllowedValueInfo>{
                                "value": "FP16",
                                "display_name": "FP16",
                            },
                            <AllowedValueInfo>{
                                "value": "FP8",
                                "display_name": "FP8",
                            },
                            <AllowedValueInfo>{
                                "value": "FP8_MM",
                                "display_name": "FP8 MM",
                            },
                            <AllowedValueInfo>{
                                "value": "FP8_AR",
                                "display_name": "FP8 AR",
                            },
                            <AllowedValueInfo>{
                                "value": "FP8_MM_KV_ATTN",
                                "display_name": "FP8 MM KV ATTN",
                            },
                            <AllowedValueInfo>{
                                "value": "FP8_KV",
                                "display_name": "FP8 KV",
                            },
                            <AllowedValueInfo>{
                                "value": "FP8_MM_V2",
                                "display_name": "FP8 MM V2",
                            },
                            <AllowedValueInfo>{
                                "value": "FP8_V2",
                                "display_name": "FP8 V2",
                            },
                            <AllowedValueInfo>{
                                "value": "FP8_MM_KV_ATTN_V2",
                                "display_name": "FP8 MM KV ATTN V2",
                            },
                            <AllowedValueInfo>{
                                "value": "NF4",
                                "display_name": "NF4",
                            },
                            <AllowedValueInfo>{
                                "value": "FP4",
                                "display_name": "FP4",
                            },
                            <AllowedValueInfo>{
                                "value": "BF16",
                                "display_name": "BF16",
                            },
                            <AllowedValueInfo>{
                                "value": "FP4_BLOCKSCALED_MM",
                                "display_name": "FP4 Blockscaled MM",
                            },
                            <AllowedValueInfo>{
                                "value": "FP4_MX_MOE",
                                "display_name": "FP4 Mx MOE",
                            },
                        ),
                    },
                    "cluster": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "draftModel": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "public-models",
                        "supports_custom_values": True,
                    },
                    "directRouteApiKeys": <RestSchemaFieldOverlayInfo>{
                        "sensitive": True,
                    },
                    "directRouteType": <RestSchemaFieldOverlayInfo>{
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "DIRECT_ROUTE_TYPE_UNSPECIFIED",
                                "display_name": "Unspecified",
                            },
                            <AllowedValueInfo>{
                                "value": "INTERNET",
                                "display_name": "Internet",
                            },
                            <AllowedValueInfo>{
                                "value": "GCP_PRIVATE_SERVICE_CONNECT",
                                "display_name": "GCP Private Service Connect",
                            },
                            <AllowedValueInfo>{
                                "value": "AWS_PRIVATELINK",
                                "display_name": "AWS PrivateLink",
                            },
                        ),
                    },
                    "directRouteHandle": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "placement": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "region": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (
                                    <AllowedValueInfo>{
                                        "value": "REGION_UNSPECIFIED",
                                        "display_name": "Unspecified",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_IOWA_1",
                                        "display_name": "US: Iowa 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_VIRGINIA_1",
                                        "display_name": "US: Virginia 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_VIRGINIA_2",
                                        "display_name": "US: Virginia 2",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_ILLINOIS_1",
                                        "display_name": "US: Illinois 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "AP_TOKYO_1",
                                        "display_name": "Asia Pacific: Tokyo 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_ARIZONA_1",
                                        "display_name": "US: Arizona 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_TEXAS_1",
                                        "display_name": "US: Texas 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_ILLINOIS_2",
                                        "display_name": "US: Illinois 2",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "EU_FRANKFURT_1",
                                        "display_name": "Europe: Frankfurt 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_TEXAS_2",
                                        "display_name": "US: Texas 2",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "EU_ICELAND_1",
                                        "display_name": "Europe: Iceland 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "EU_ICELAND_2",
                                        "display_name": "Europe: Iceland 2",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_WASHINGTON_1",
                                        "display_name": "US: Washington 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_WASHINGTON_2",
                                        "display_name": "US: Washington 2",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_WASHINGTON_3",
                                        "display_name": "US: Washington 3",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "AP_TOKYO_2",
                                        "display_name": "Asia Pacific: Tokyo 2",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_CALIFORNIA_1",
                                        "display_name": "US: California 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_UTAH_1",
                                        "display_name": "US: Utah 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_ARIZONA_3",
                                        "display_name": "US: Arizona 3",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_GEORGIA_1",
                                        "display_name": "US: Georgia 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_GEORGIA_2",
                                        "display_name": "US: Georgia 2",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_WASHINGTON_4",
                                        "display_name": "US: Washington 4",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_GEORGIA_3",
                                        "display_name": "US: Georgia 3",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "NA_BRITISHCOLUMBIA_1",
                                        "display_name": "North America: British Columbia 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_GEORGIA_4",
                                        "display_name": "US: Georgia 4",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_OHIO_1",
                                        "display_name": "US: Ohio 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_NEWYORK_1",
                                        "display_name": "US: New York 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "EU_NETHERLANDS_1",
                                        "display_name": "Europe: Netherlands 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_WASHINGTON_5",
                                        "display_name": "US: Washington 5",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_MINNESOTA_1",
                                        "display_name": "US: Minnesota 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_CALIFORNIA_2",
                                        "display_name": "US: California 2",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "NA_BRITISHCOLUMBIA_2",
                                        "display_name": "North America: British Columbia 2",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "AP_MALAYSIA_2",
                                        "display_name": "Asia Pacific: Malaysia 2",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_OREGON_1",
                                        "display_name": "US: Oregon 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "NA_BRITISHCOLUMBIA_3",
                                        "display_name": "North America: British Columbia 3",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "AP_NEWSOUTHWALES_1",
                                        "display_name": "Asia Pacific: New South Wales 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "AP_TAIWAN_1",
                                        "display_name": "Asia Pacific: Taiwan 1",
                                    },
                                ),
                            },
                            "multiRegion": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (
                                    <AllowedValueInfo>{
                                        "value": "MULTI_REGION_UNSPECIFIED",
                                        "display_name": "Unspecified",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "GLOBAL",
                                        "display_name": "Global",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US",
                                        "display_name": "Us",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "EUROPE",
                                        "display_name": "Europe",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "APAC",
                                        "display_name": "Apac",
                                    },
                                ),
                            },
                        },
                    },
                    "region": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "updateTime": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "hotLoadBucketType": <RestSchemaFieldOverlayInfo>{
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "BUCKET_TYPE_UNSPECIFIED",
                                "display_name": "Unspecified",
                            },
                            <AllowedValueInfo>{
                                "value": "MINIO",
                                "display_name": "Minio",
                            },
                            <AllowedValueInfo>{
                                "value": "S3",
                                "display_name": "S3",
                            },
                            <AllowedValueInfo>{
                                "value": "NEBIUS",
                                "display_name": "Nebius",
                            },
                            <AllowedValueInfo>{
                                "value": "FW_HOSTED",
                                "display_name": "Fireworks Hosted",
                            },
                        ),
                    },
                    "deploymentShape": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "deployment-shapes",
                        "supports_custom_values": True,
                        "depends_on": (
                            "account_id",
                        ),
                    },
                    "replicaStats": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "hotLoadTransitionType": <RestSchemaFieldOverlayInfo>{
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "HOT_LOAD_TRANSITION_TYPE_UNSPECIFIED",
                                "display_name": "Unspecified",
                            },
                            <AllowedValueInfo>{
                                "value": "ASYNC",
                                "display_name": "Asynchronous",
                            },
                            <AllowedValueInfo>{
                                "value": "SYNC",
                                "display_name": "Synchronous",
                            },
                        ),
                    },
                    "displayName": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-deployment",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/deployments/{deployment_id}",
                "operation_id": "Gateway_GetDeployment",
                "display_name": "Get Deployment",
                "short_desc": "Get Deployment in Fireworks AI",
                "desc": "Get Deployment. Manage the capacity serving your models. **Creating, scaling, or restoring capacity can incur charges.**",
                "groups": (
                    "Deployments",
                ),
                "flatten_body": True,
                "fields": {
                    "readMask": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Read Mask",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "deployment_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Deployment ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-deployment",
                        "ref_data": "deployments",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-deployment",
                "method": "PATCH",
                "path": "/v1/accounts/{account_id}/deployments/{deployment_id}",
                "operation_id": "Gateway_UpdateDeployment",
                "display_name": "Update Deployment",
                "short_desc": "Update Deployment in Fireworks AI",
                "desc": "Update Deployment. Manage the capacity serving your models. **Creating, scaling, or restoring capacity can incur charges.**",
                "groups": (
                    "Deployments",
                ),
                "flatten_body": True,
                "fields": {
                    "skipShapeValidation": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Skip Shape Validation",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "deployment_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Deployment ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-deployment",
                        "ref_data": "deployments",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                    "createTime": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "expireTime": <RestSchemaFieldOverlayInfo>{
                        "codec": "iso-8601-utc",
                    },
                    "purgeTime": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "deleteTime": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "state": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "status": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "desiredReplicaCount": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "replicaCount": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "baseModel": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "public-models",
                        "supports_custom_values": True,
                    },
                    "acceleratorType": <RestSchemaFieldOverlayInfo>{
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "ACCELERATOR_TYPE_UNSPECIFIED",
                                "display_name": "Unspecified",
                            },
                            <AllowedValueInfo>{
                                "value": "NVIDIA_A100_80GB",
                                "display_name": "NVIDIA A100 80GB",
                            },
                            <AllowedValueInfo>{
                                "value": "NVIDIA_H100_80GB",
                                "display_name": "NVIDIA H100 80GB",
                            },
                            <AllowedValueInfo>{
                                "value": "AMD_MI300X_192GB",
                                "display_name": "AMD MI300X 192GB",
                            },
                            <AllowedValueInfo>{
                                "value": "NVIDIA_A10G_24GB",
                                "display_name": "NVIDIA A10G 24GB",
                            },
                            <AllowedValueInfo>{
                                "value": "NVIDIA_A100_40GB",
                                "display_name": "NVIDIA A100 40GB",
                            },
                            <AllowedValueInfo>{
                                "value": "NVIDIA_L4_24GB",
                                "display_name": "NVIDIA L4 24GB",
                            },
                            <AllowedValueInfo>{
                                "value": "NVIDIA_H200_141GB",
                                "display_name": "NVIDIA H200 141GB",
                            },
                            <AllowedValueInfo>{
                                "value": "NVIDIA_B200_180GB",
                                "display_name": "NVIDIA B200 180GB",
                            },
                            <AllowedValueInfo>{
                                "value": "AMD_MI325X_256GB",
                                "display_name": "AMD MI325X 256GB",
                            },
                            <AllowedValueInfo>{
                                "value": "AMD_MI350X_288GB",
                                "display_name": "AMD MI350X 288GB",
                            },
                            <AllowedValueInfo>{
                                "value": "NVIDIA_B300_288GB",
                                "display_name": "NVIDIA B300 288GB",
                            },
                            <AllowedValueInfo>{
                                "value": "NVIDIA_GB200",
                                "display_name": "NVIDIA GB200",
                            },
                            <AllowedValueInfo>{
                                "value": "NVIDIA_GB300",
                                "display_name": "NVIDIA GB300",
                            },
                        ),
                    },
                    "precision": <RestSchemaFieldOverlayInfo>{
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "PRECISION_UNSPECIFIED",
                                "display_name": "Unspecified",
                            },
                            <AllowedValueInfo>{
                                "value": "FP16",
                                "display_name": "FP16",
                            },
                            <AllowedValueInfo>{
                                "value": "FP8",
                                "display_name": "FP8",
                            },
                            <AllowedValueInfo>{
                                "value": "FP8_MM",
                                "display_name": "FP8 MM",
                            },
                            <AllowedValueInfo>{
                                "value": "FP8_AR",
                                "display_name": "FP8 AR",
                            },
                            <AllowedValueInfo>{
                                "value": "FP8_MM_KV_ATTN",
                                "display_name": "FP8 MM KV ATTN",
                            },
                            <AllowedValueInfo>{
                                "value": "FP8_KV",
                                "display_name": "FP8 KV",
                            },
                            <AllowedValueInfo>{
                                "value": "FP8_MM_V2",
                                "display_name": "FP8 MM V2",
                            },
                            <AllowedValueInfo>{
                                "value": "FP8_V2",
                                "display_name": "FP8 V2",
                            },
                            <AllowedValueInfo>{
                                "value": "FP8_MM_KV_ATTN_V2",
                                "display_name": "FP8 MM KV ATTN V2",
                            },
                            <AllowedValueInfo>{
                                "value": "NF4",
                                "display_name": "NF4",
                            },
                            <AllowedValueInfo>{
                                "value": "FP4",
                                "display_name": "FP4",
                            },
                            <AllowedValueInfo>{
                                "value": "BF16",
                                "display_name": "BF16",
                            },
                            <AllowedValueInfo>{
                                "value": "FP4_BLOCKSCALED_MM",
                                "display_name": "FP4 Blockscaled MM",
                            },
                            <AllowedValueInfo>{
                                "value": "FP4_MX_MOE",
                                "display_name": "FP4 Mx MOE",
                            },
                        ),
                    },
                    "cluster": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "draftModel": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "public-models",
                        "supports_custom_values": True,
                    },
                    "directRouteApiKeys": <RestSchemaFieldOverlayInfo>{
                        "sensitive": True,
                    },
                    "directRouteType": <RestSchemaFieldOverlayInfo>{
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "DIRECT_ROUTE_TYPE_UNSPECIFIED",
                                "display_name": "Unspecified",
                            },
                            <AllowedValueInfo>{
                                "value": "INTERNET",
                                "display_name": "Internet",
                            },
                            <AllowedValueInfo>{
                                "value": "GCP_PRIVATE_SERVICE_CONNECT",
                                "display_name": "GCP Private Service Connect",
                            },
                            <AllowedValueInfo>{
                                "value": "AWS_PRIVATELINK",
                                "display_name": "AWS PrivateLink",
                            },
                        ),
                    },
                    "directRouteHandle": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "placement": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "region": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (
                                    <AllowedValueInfo>{
                                        "value": "REGION_UNSPECIFIED",
                                        "display_name": "Unspecified",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_IOWA_1",
                                        "display_name": "US: Iowa 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_VIRGINIA_1",
                                        "display_name": "US: Virginia 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_VIRGINIA_2",
                                        "display_name": "US: Virginia 2",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_ILLINOIS_1",
                                        "display_name": "US: Illinois 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "AP_TOKYO_1",
                                        "display_name": "Asia Pacific: Tokyo 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_ARIZONA_1",
                                        "display_name": "US: Arizona 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_TEXAS_1",
                                        "display_name": "US: Texas 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_ILLINOIS_2",
                                        "display_name": "US: Illinois 2",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "EU_FRANKFURT_1",
                                        "display_name": "Europe: Frankfurt 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_TEXAS_2",
                                        "display_name": "US: Texas 2",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "EU_ICELAND_1",
                                        "display_name": "Europe: Iceland 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "EU_ICELAND_2",
                                        "display_name": "Europe: Iceland 2",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_WASHINGTON_1",
                                        "display_name": "US: Washington 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_WASHINGTON_2",
                                        "display_name": "US: Washington 2",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_WASHINGTON_3",
                                        "display_name": "US: Washington 3",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "AP_TOKYO_2",
                                        "display_name": "Asia Pacific: Tokyo 2",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_CALIFORNIA_1",
                                        "display_name": "US: California 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_UTAH_1",
                                        "display_name": "US: Utah 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_ARIZONA_3",
                                        "display_name": "US: Arizona 3",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_GEORGIA_1",
                                        "display_name": "US: Georgia 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_GEORGIA_2",
                                        "display_name": "US: Georgia 2",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_WASHINGTON_4",
                                        "display_name": "US: Washington 4",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_GEORGIA_3",
                                        "display_name": "US: Georgia 3",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "NA_BRITISHCOLUMBIA_1",
                                        "display_name": "North America: British Columbia 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_GEORGIA_4",
                                        "display_name": "US: Georgia 4",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_OHIO_1",
                                        "display_name": "US: Ohio 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_NEWYORK_1",
                                        "display_name": "US: New York 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "EU_NETHERLANDS_1",
                                        "display_name": "Europe: Netherlands 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_WASHINGTON_5",
                                        "display_name": "US: Washington 5",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_MINNESOTA_1",
                                        "display_name": "US: Minnesota 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_CALIFORNIA_2",
                                        "display_name": "US: California 2",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "NA_BRITISHCOLUMBIA_2",
                                        "display_name": "North America: British Columbia 2",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "AP_MALAYSIA_2",
                                        "display_name": "Asia Pacific: Malaysia 2",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US_OREGON_1",
                                        "display_name": "US: Oregon 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "NA_BRITISHCOLUMBIA_3",
                                        "display_name": "North America: British Columbia 3",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "AP_NEWSOUTHWALES_1",
                                        "display_name": "Asia Pacific: New South Wales 1",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "AP_TAIWAN_1",
                                        "display_name": "Asia Pacific: Taiwan 1",
                                    },
                                ),
                            },
                            "multiRegion": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (
                                    <AllowedValueInfo>{
                                        "value": "MULTI_REGION_UNSPECIFIED",
                                        "display_name": "Unspecified",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "GLOBAL",
                                        "display_name": "Global",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "US",
                                        "display_name": "Us",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "EUROPE",
                                        "display_name": "Europe",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "APAC",
                                        "display_name": "Apac",
                                    },
                                ),
                            },
                        },
                    },
                    "region": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "updateTime": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "hotLoadBucketType": <RestSchemaFieldOverlayInfo>{
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "BUCKET_TYPE_UNSPECIFIED",
                                "display_name": "Unspecified",
                            },
                            <AllowedValueInfo>{
                                "value": "MINIO",
                                "display_name": "Minio",
                            },
                            <AllowedValueInfo>{
                                "value": "S3",
                                "display_name": "S3",
                            },
                            <AllowedValueInfo>{
                                "value": "NEBIUS",
                                "display_name": "Nebius",
                            },
                            <AllowedValueInfo>{
                                "value": "FW_HOSTED",
                                "display_name": "Fireworks Hosted",
                            },
                        ),
                    },
                    "deploymentShape": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "deployment-shapes",
                        "supports_custom_values": True,
                        "depends_on": (
                            "account_id",
                        ),
                    },
                    "replicaStats": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "hotLoadTransitionType": <RestSchemaFieldOverlayInfo>{
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "HOT_LOAD_TRANSITION_TYPE_UNSPECIFIED",
                                "display_name": "Unspecified",
                            },
                            <AllowedValueInfo>{
                                "value": "ASYNC",
                                "display_name": "Asynchronous",
                            },
                            <AllowedValueInfo>{
                                "value": "SYNC",
                                "display_name": "Synchronous",
                            },
                        ),
                    },
                    "displayName": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-deployment",
                "method": "DELETE",
                "path": "/v1/accounts/{account_id}/deployments/{deployment_id}",
                "operation_id": "Gateway_DeleteDeployment",
                "display_name": "Delete Deployment",
                "short_desc": "Delete Deployment in Fireworks AI",
                "desc": "Delete Deployment. Manage the capacity serving your models. **Creating, scaling, or restoring capacity can incur charges.** **Deletion is permanent.**",
                "groups": (
                    "Deployments",
                ),
                "flatten_body": True,
                "fields": {
                    "hard": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Hard",
                    },
                    "ignoreChecks": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Ignore Checks",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "deployment_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Deployment ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-deployment",
                        "ref_data": "deployments",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-ledger",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/deployments/{deployment_id}/ledger",
                "operation_id": "Gateway_GetLedger",
                "display_name": "Get Ledger",
                "short_desc": "Get Ledger in Fireworks AI",
                "desc": "Get Ledger. Manage the capacity serving your models. **Creating, scaling, or restoring capacity can incur charges.**",
                "groups": (
                    "Deployments",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "deployment_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Deployment ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-deployment",
                        "ref_data": "deployments",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "reset-ledger",
                "method": "DELETE",
                "path": "/v1/accounts/{account_id}/deployments/{deployment_id}/ledger",
                "operation_id": "Gateway_ResetLedger",
                "display_name": "Reset Ledger",
                "short_desc": "Reset Ledger in Fireworks AI",
                "desc": "Reset Ledger. Manage the capacity serving your models. **Creating, scaling, or restoring capacity can incur charges.** **Deletion is permanent.**",
                "groups": (
                    "Deployments",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "deployment_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Deployment ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-deployment",
                        "ref_data": "deployments",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-deployment-shards",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/deployments/{deployment_id}/shards",
                "operation_id": "Gateway_ListDeploymentShards",
                "display_name": "List Deployment Shards",
                "short_desc": "List Deployment Shards in Fireworks AI",
                "desc": "List Deployment Shards. Manage the capacity serving your models. **Creating, scaling, or restoring capacity can incur charges.** Use `pageToken` with the returned `nextPageToken` to retrieve further pages.",
                "groups": (
                    "Deployments",
                ),
                "flatten_body": True,
                "fields": {
                    "pageSize": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "preselected": True,
                        "example_value": 20,
                    },
                    "pageToken": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Token",
                        "desc": "Use `nextPageToken` from the previous result to fetch another page.",
                    },
                    "filter": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Filter",
                        "desc": "Filter the results using the [Fireworks filter syntax](https://docs.fireworks.ai/api-reference/list-models).",
                        "preselected": True,
                    },
                    "orderBy": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Order By",
                        "desc": "Order results by supported resource fields, for example `name`.",
                    },
                    "readMask": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Read Mask",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "deployment_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Deployment ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-deployment",
                        "ref_data": "deployments",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-termination-message",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/deployments/{deployment_id}/terminationMessage",
                "operation_id": "Gateway_GetTerminationMessage",
                "display_name": "Get Termination Message",
                "short_desc": "Get Termination Message in Fireworks AI",
                "desc": "Get Termination Message. Manage the capacity serving your models. **Creating, scaling, or restoring capacity can incur charges.**",
                "groups": (
                    "Deployments",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "deployment_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Deployment ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-deployment",
                        "ref_data": "deployments",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-deployment-metrics",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/deployments/{deployment_id}:metrics",
                "operation_id": "Gateway_GetDeploymentMetrics",
                "display_name": "Get Deployment Metrics",
                "short_desc": "Get Deployment Metrics in Fireworks AI",
                "desc": "Get Deployment Metrics. Manage the capacity serving your models. **Creating, scaling, or restoring capacity can incur charges.**",
                "groups": (
                    "Deployments",
                ),
                "flatten_body": True,
                "fields": {
                    "timeRange": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Time Range",
                    },
                    "readMask": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Read Mask",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "deployment_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Deployment ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-deployment",
                        "ref_data": "deployments",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "scale-deployment",
                "method": "PATCH",
                "path": "/v1/accounts/{account_id}/deployments/{deployment_id}:scale",
                "operation_id": "Gateway_ScaleDeployment",
                "display_name": "Scale Deployment",
                "short_desc": "Scale Deployment in Fireworks AI",
                "desc": "Scale Deployment. Manage the capacity serving your models. **Creating, scaling, or restoring capacity can incur charges.**",
                "groups": (
                    "Deployments",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "deployment_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Deployment ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-deployment",
                        "ref_data": "deployments",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "undelete-deployment",
                "method": "POST",
                "path": "/v1/accounts/{account_id}/deployments/{deployment_id}:undelete",
                "operation_id": "Gateway_UndeleteDeployment",
                "display_name": "Undelete Deployment",
                "short_desc": "Undelete Deployment in Fireworks AI",
                "desc": "Undelete Deployment. Manage the capacity serving your models. **Creating, scaling, or restoring capacity can incur charges.**",
                "groups": (
                    "Deployments",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "deployment_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Deployment ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-deployment",
                        "ref_data": "deployments",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-dpo-jobs",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/dpoJobs",
                "operation_id": "Gateway_ListDpoJobs",
                "display_name": "List DPO Jobs",
                "short_desc": "List DPO Jobs in Fireworks AI",
                "desc": "List DPO Jobs. Train on preferred responses and manage preference optimization jobs. **Creating or resuming training incurs charges.** Use `pageToken` with the returned `nextPageToken` to retrieve further pages.",
                "groups": (
                    "Preference Fine-Tuning",
                ),
                "flatten_body": True,
                "fields": {
                    "pageSize": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "preselected": True,
                        "example_value": 20,
                    },
                    "pageToken": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Token",
                        "desc": "Use `nextPageToken` from the previous result to fetch another page.",
                    },
                    "filter": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Filter",
                        "desc": "Filter the results using the [Fireworks filter syntax](https://docs.fireworks.ai/api-reference/list-models).",
                        "preselected": True,
                    },
                    "orderBy": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Order By",
                        "desc": "Order results by supported resource fields, for example `name`.",
                    },
                    "readMask": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Read Mask",
                    },
                    "includeArchived": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Include Archived",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-dpo-job",
                "method": "POST",
                "path": "/v1/accounts/{account_id}/dpoJobs",
                "operation_id": "Gateway_CreateDpoJob",
                "display_name": "Create DPO Job",
                "short_desc": "Create DPO Job in Fireworks AI",
                "desc": "Create DPO Job. Train on preferred responses and manage preference optimization jobs. **Creating or resuming training incurs charges.**",
                "groups": (
                    "Preference Fine-Tuning",
                ),
                "flatten_body": True,
                "fields": {
                    "dpoJobId": <RestSchemaFieldOverlayInfo>{
                        "display_name": "DPO Job ID",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "createTime": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "completedTime": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "dataset": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "datasets",
                        "supports_custom_values": True,
                        "depends_on": (
                            "account_id",
                        ),
                    },
                    "state": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "status": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "createdBy": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "trainingConfig": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "baseModel": <RestSchemaFieldOverlayInfo>{
                                "ref_data": "public-models",
                                "supports_custom_values": True,
                            },
                        },
                    },
                    "wandbConfig": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "apiKey": <RestSchemaFieldOverlayInfo>{
                                "sensitive": True,
                            },
                            "url": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                        },
                    },
                    "trainerLogsSignedUrl": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "lossConfig": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "method": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (
                                    <AllowedValueInfo>{
                                        "value": "METHOD_UNSPECIFIED",
                                        "display_name": "Unspecified",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "GRPO",
                                        "display_name": "Grpo",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "DAPO",
                                        "display_name": "Dapo",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "DPO",
                                        "display_name": "Dpo",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "ORPO",
                                        "display_name": "Orpo",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "GSPO_TOKEN",
                                        "display_name": "Gspo Token",
                                    },
                                ),
                            },
                        },
                    },
                    "awsS3Config": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "credentialsSecret": <RestSchemaFieldOverlayInfo>{
                                "sensitive": True,
                            },
                        },
                    },
                    "azureBlobStorageConfig": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "credentialsSecret": <RestSchemaFieldOverlayInfo>{
                                "sensitive": True,
                            },
                        },
                    },
                    "purpose": <RestSchemaFieldOverlayInfo>{
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "PURPOSE_UNSPECIFIED",
                                "display_name": "Unspecified",
                            },
                            <AllowedValueInfo>{
                                "value": "PURPOSE_PILOT",
                                "display_name": "Pilot",
                            },
                        ),
                    },
                    "displayName": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-dpo-job",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/dpoJobs/{dpo_job_id}",
                "operation_id": "Gateway_GetDpoJob",
                "display_name": "Get DPO Job",
                "short_desc": "Get DPO Job in Fireworks AI",
                "desc": "Get DPO Job. Train on preferred responses and manage preference optimization jobs. **Creating or resuming training incurs charges.**",
                "groups": (
                    "Preference Fine-Tuning",
                ),
                "flatten_body": True,
                "fields": {
                    "readMask": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Read Mask",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "dpo_job_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "DPO Job ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-dpo-job",
                        "ref_data": "dpo-jobs",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-dpo-job",
                "method": "DELETE",
                "path": "/v1/accounts/{account_id}/dpoJobs/{dpo_job_id}",
                "operation_id": "Gateway_DeleteDpoJob",
                "display_name": "Delete DPO Job",
                "short_desc": "Delete DPO Job in Fireworks AI",
                "desc": "Delete DPO Job. Train on preferred responses and manage preference optimization jobs. **Creating or resuming training incurs charges.** **Deletion is permanent.**",
                "groups": (
                    "Preference Fine-Tuning",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "dpo_job_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "DPO Job ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-dpo-job",
                        "ref_data": "dpo-jobs",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "cancel-dpo-job",
                "method": "POST",
                "path": "/v1/accounts/{account_id}/dpoJobs/{dpo_job_id}:cancel",
                "operation_id": "Gateway_CancelDpoJob",
                "display_name": "Cancel DPO Job",
                "short_desc": "Cancel DPO Job in Fireworks AI",
                "desc": "Cancel DPO Job. Train on preferred responses and manage preference optimization jobs. **Creating or resuming training incurs charges.**",
                "groups": (
                    "Preference Fine-Tuning",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "dpo_job_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "DPO Job ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-dpo-job",
                        "ref_data": "dpo-jobs",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-dpo-job-metrics-file-endpoint",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/dpoJobs/{dpo_job_id}:getMetricsFileEndpoint",
                "operation_id": "Gateway_GetDpoJobMetricsFileEndpoint",
                "display_name": "Get DPO Job Metrics File Endpoint",
                "short_desc": "Get DPO Job Metrics File Endpoint in Fireworks AI",
                "desc": "Get DPO Job Metrics File Endpoint. Train on preferred responses and manage preference optimization jobs. **Creating or resuming training incurs charges.**",
                "groups": (
                    "Preference Fine-Tuning",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "dpo_job_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "DPO Job ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-dpo-job",
                        "ref_data": "dpo-jobs",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "resume-dpo-job",
                "method": "POST",
                "path": "/v1/accounts/{account_id}/dpoJobs/{dpo_job_id}:resume",
                "operation_id": "Gateway_ResumeDpoJob",
                "display_name": "Resume DPO Job",
                "short_desc": "Resume DPO Job in Fireworks AI",
                "desc": "Resume DPO Job. Train on preferred responses and manage preference optimization jobs. **Creating or resuming training incurs charges.**",
                "groups": (
                    "Preference Fine-Tuning",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "dpo_job_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "DPO Job ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-dpo-job",
                        "ref_data": "dpo-jobs",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-evaluation-jobs",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/evaluationJobs",
                "operation_id": "Gateway_ListEvaluationJobs",
                "display_name": "List Evaluation Jobs",
                "short_desc": "List Evaluation Jobs in Fireworks AI",
                "desc": "List Evaluation Jobs. Run and inspect model quality evaluations. **Creating jobs can incur inference charges.** Use `pageToken` with the returned `nextPageToken` to retrieve further pages.",
                "groups": (
                    "Evaluation Jobs",
                ),
                "flatten_body": True,
                "fields": {
                    "pageSize": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "preselected": True,
                        "example_value": 20,
                    },
                    "pageToken": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Token",
                        "desc": "Use `nextPageToken` from the previous result to fetch another page.",
                    },
                    "filter": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Filter",
                        "desc": "Filter the results using the [Fireworks filter syntax](https://docs.fireworks.ai/api-reference/list-models).",
                        "preselected": True,
                    },
                    "orderBy": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Order By",
                        "desc": "Order results by supported resource fields, for example `name`.",
                    },
                    "readMask": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Read Mask",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-evaluation-job",
                "method": "POST",
                "path": "/v1/accounts/{account_id}/evaluationJobs",
                "operation_id": "Gateway_CreateEvaluationJob",
                "display_name": "Create Evaluation Job",
                "short_desc": "Create Evaluation Job in Fireworks AI",
                "desc": "Create Evaluation Job. Run and inspect model quality evaluations. **Creating jobs can incur inference charges.**",
                "groups": (
                    "Evaluation Jobs",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "evaluationJob": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "name": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                            "createTime": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                            "createdBy": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                            "state": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                            "status": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                            "evaluator": <RestSchemaFieldOverlayInfo>{
                                "ref_data": "evaluators",
                                "supports_custom_values": True,
                                "depends_on": (
                                    "account_id",
                                ),
                            },
                            "metrics": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                            "updateTime": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                            "awsS3Config": <RestSchemaFieldOverlayInfo>{
                                "fields": {
                                    "credentialsSecret": <RestSchemaFieldOverlayInfo>{
                                        "sensitive": True,
                                    },
                                },
                            },
                        },
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-evaluation-job",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/evaluationJobs/{evaluation_job_id}",
                "operation_id": "Gateway_GetEvaluationJob",
                "display_name": "Get Evaluation Job",
                "short_desc": "Get Evaluation Job in Fireworks AI",
                "desc": "Get Evaluation Job. Run and inspect model quality evaluations. **Creating jobs can incur inference charges.**",
                "groups": (
                    "Evaluation Jobs",
                ),
                "flatten_body": True,
                "fields": {
                    "readMask": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Read Mask",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "evaluation_job_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Evaluation Job ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-evaluation-job",
                        "ref_data": "evaluation-jobs",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-evaluation-job",
                "method": "DELETE",
                "path": "/v1/accounts/{account_id}/evaluationJobs/{evaluation_job_id}",
                "operation_id": "Gateway_DeleteEvaluationJob",
                "display_name": "Delete Evaluation Job",
                "short_desc": "Delete Evaluation Job in Fireworks AI",
                "desc": "Delete Evaluation Job. Run and inspect model quality evaluations. **Creating jobs can incur inference charges.** **Deletion is permanent.**",
                "groups": (
                    "Evaluation Jobs",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "evaluation_job_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Evaluation Job ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-evaluation-job",
                        "ref_data": "evaluation-jobs",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-evaluation-job-execution-log-endpoint",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/evaluationJobs/{evaluation_job_id}:getExecutionLogEndpoint",
                "operation_id": "Gateway_GetEvaluationJobExecutionLogEndpoint",
                "display_name": "Get Evaluation Job Execution Log Endpoint",
                "short_desc": "Get Evaluation Job Execution Log Endpoint in Fireworks AI",
                "desc": "Get Evaluation Job Execution Log Endpoint. Run and inspect model quality evaluations. **Creating jobs can incur inference charges.**",
                "groups": (
                    "Evaluation Jobs",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "evaluation_job_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Evaluation Job ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-evaluation-job",
                        "ref_data": "evaluation-jobs",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-evaluations",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/evaluations",
                "operation_id": "Gateway_ListEvaluations",
                "display_name": "List Evaluations",
                "short_desc": "List Evaluations in Fireworks AI",
                "desc": "List Evaluations. Define and inspect model quality checks. **Running evaluations can incur inference charges.** Use `pageToken` with the returned `nextPageToken` to retrieve further pages.",
                "groups": (
                    "Evaluations",
                ),
                "flatten_body": True,
                "fields": {
                    "pageSize": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "preselected": True,
                        "example_value": 20,
                    },
                    "pageToken": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Token",
                        "desc": "Use `nextPageToken` from the previous result to fetch another page.",
                    },
                    "filter": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Filter",
                        "desc": "Filter the results using the [Fireworks filter syntax](https://docs.fireworks.ai/api-reference/list-models).",
                        "preselected": True,
                    },
                    "orderBy": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Order By",
                        "desc": "Order results by supported resource fields, for example `name`.",
                    },
                    "readMask": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Read Mask",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-evaluation",
                "method": "POST",
                "path": "/v1/accounts/{account_id}/evaluations",
                "operation_id": "Gateway_CreateEvaluation",
                "display_name": "Create Evaluation",
                "short_desc": "Create Evaluation in Fireworks AI",
                "desc": "Create Evaluation. Define and inspect model quality checks. **Running evaluations can incur inference charges.**",
                "groups": (
                    "Evaluations",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "evaluation": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "name": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                            "createTime": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                            "createdBy": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                            "status": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                            "assertions": <RestSchemaFieldOverlayInfo>{
                                "fields": {
                                    "assertionType": <RestSchemaFieldOverlayInfo>{
                                        "allowed_values": (
                                            <AllowedValueInfo>{
                                                "value": "ASSERTION_TYPE_UNSPECIFIED",
                                                "display_name": "Unspecified",
                                            },
                                            <AllowedValueInfo>{
                                                "value": "ASSERTION_TYPE_LLM",
                                                "display_name": "Assertion Type Llm",
                                            },
                                            <AllowedValueInfo>{
                                                "value": "ASSERTION_TYPE_CODE",
                                                "display_name": "Assertion Type Code",
                                            },
                                        ),
                                    },
                                },
                            },
                            "updateTime": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                        },
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-evaluation",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/evaluations/{evaluation_id}",
                "operation_id": "Gateway_GetEvaluation",
                "display_name": "Get Evaluation",
                "short_desc": "Get Evaluation in Fireworks AI",
                "desc": "Get Evaluation. Define and inspect model quality checks. **Running evaluations can incur inference charges.**",
                "groups": (
                    "Evaluations",
                ),
                "flatten_body": True,
                "fields": {
                    "readMask": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Read Mask",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "evaluation_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Evaluation ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-evaluation",
                        "ref_data": "evaluations",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-evaluation",
                "method": "DELETE",
                "path": "/v1/accounts/{account_id}/evaluations/{evaluation_id}",
                "operation_id": "Gateway_DeleteEvaluation",
                "display_name": "Delete Evaluation",
                "short_desc": "Delete Evaluation in Fireworks AI",
                "desc": "Delete Evaluation. Define and inspect model quality checks. **Running evaluations can incur inference charges.** **Deletion is permanent.**",
                "groups": (
                    "Evaluations",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "evaluation_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Evaluation ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-evaluation",
                        "ref_data": "evaluations",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "preview-evaluation",
                "method": "POST",
                "path": "/v1/accounts/{account_id}/evaluations/{evaluation_id}:preview",
                "operation_id": "Gateway_PreviewEvaluation",
                "display_name": "Preview Evaluation",
                "short_desc": "Preview Evaluation in Fireworks AI",
                "desc": "Preview Evaluation. Define and inspect model quality checks. **Running evaluations can incur inference charges.**",
                "groups": (
                    "Evaluations",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "evaluation_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Evaluation ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-evaluation",
                        "ref_data": "evaluations",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "validate-assertions",
                "method": "POST",
                "path": "/v1/accounts/{account_id}/evaluations:validateAssertions",
                "operation_id": "Gateway_ValidateAssertions",
                "display_name": "Validate Assertions",
                "short_desc": "Validate Assertions in Fireworks AI",
                "desc": "Validate Assertions. Define and inspect model quality checks. **Running evaluations can incur inference charges.**",
                "groups": (
                    "Evaluations",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "assertions": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "assertionType": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (
                                    <AllowedValueInfo>{
                                        "value": "ASSERTION_TYPE_UNSPECIFIED",
                                        "display_name": "Unspecified",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "ASSERTION_TYPE_LLM",
                                        "display_name": "Assertion Type Llm",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "ASSERTION_TYPE_CODE",
                                        "display_name": "Assertion Type Code",
                                    },
                                ),
                            },
                        },
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-evaluators",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/evaluators",
                "operation_id": "Gateway_ListEvaluators",
                "display_name": "List Evaluators",
                "short_desc": "List Evaluators in Fireworks AI",
                "desc": "List Evaluators. Manage the scoring logic used to evaluate model responses. Use `pageToken` with the returned `nextPageToken` to retrieve further pages.",
                "groups": (
                    "Evaluators",
                ),
                "flatten_body": True,
                "fields": {
                    "pageSize": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "preselected": True,
                        "example_value": 20,
                    },
                    "pageToken": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Token",
                        "desc": "Use `nextPageToken` from the previous result to fetch another page.",
                    },
                    "filter": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Filter",
                        "desc": "Filter the results using the [Fireworks filter syntax](https://docs.fireworks.ai/api-reference/list-models).",
                        "preselected": True,
                    },
                    "orderBy": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Order By",
                        "desc": "Order results by supported resource fields, for example `name`.",
                    },
                    "readMask": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Read Mask",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-evaluator",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/evaluators/{evaluator_id}",
                "operation_id": "Gateway_GetEvaluator",
                "display_name": "Get Evaluator",
                "short_desc": "Get Evaluator in Fireworks AI",
                "desc": "Get Evaluator. Manage the scoring logic used to evaluate model responses.",
                "groups": (
                    "Evaluators",
                ),
                "flatten_body": True,
                "fields": {
                    "readMask": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Read Mask",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "evaluator_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Evaluator ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-evaluator",
                        "ref_data": "evaluators",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-evaluator",
                "method": "PATCH",
                "path": "/v1/accounts/{account_id}/evaluators/{evaluator_id}",
                "operation_id": "Gateway_UpdateEvaluator",
                "display_name": "Update Evaluator",
                "short_desc": "Update Evaluator in Fireworks AI",
                "desc": "Update Evaluator. Manage the scoring logic used to evaluate model responses.",
                "groups": (
                    "Evaluators",
                ),
                "flatten_body": True,
                "fields": {
                    "prepareCodeUpload": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Prepare Code Upload",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "evaluator_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Evaluator ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-evaluator",
                        "ref_data": "evaluators",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                    "createTime": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "createdBy": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "updateTime": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "state": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "criteria": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "type": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (
                                    <AllowedValueInfo>{
                                        "value": "TYPE_UNSPECIFIED",
                                        "display_name": "Unspecified",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "CODE_SNIPPETS",
                                        "display_name": "Code Snippets",
                                    },
                                ),
                            },
                        },
                    },
                    "status": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "source": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "type": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (
                                    <AllowedValueInfo>{
                                        "value": "TYPE_UNSPECIFIED",
                                        "display_name": "Unspecified",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "TYPE_UPLOAD",
                                        "display_name": "Type Upload",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "TYPE_GITHUB",
                                        "display_name": "Type Github",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "TYPE_TEMPORARY",
                                        "display_name": "Type Temporary",
                                    },
                                ),
                            },
                        },
                    },
                    "encryptionState": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "displayName": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-evaluator",
                "method": "DELETE",
                "path": "/v1/accounts/{account_id}/evaluators/{evaluator_id}",
                "operation_id": "Gateway_DeleteEvaluator",
                "display_name": "Delete Evaluator",
                "short_desc": "Delete Evaluator in Fireworks AI",
                "desc": "Delete Evaluator. Manage the scoring logic used to evaluate model responses. **Deletion is permanent.**",
                "groups": (
                    "Evaluators",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "evaluator_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Evaluator ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-evaluator",
                        "ref_data": "evaluators",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-evaluator-versions",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/evaluators/{evaluator_id}/versions",
                "operation_id": "Gateway_ListEvaluatorVersions",
                "display_name": "List Evaluator Versions",
                "short_desc": "List Evaluator Versions in Fireworks AI",
                "desc": "List Evaluator Versions. Manage the scoring logic used to evaluate model responses. Use `pageToken` with the returned `nextPageToken` to retrieve further pages.",
                "groups": (
                    "Evaluators",
                ),
                "flatten_body": True,
                "fields": {
                    "pageSize": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "preselected": True,
                        "example_value": 20,
                    },
                    "pageToken": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Token",
                        "desc": "Use `nextPageToken` from the previous result to fetch another page.",
                    },
                    "filter": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Filter",
                        "desc": "Filter the results using the [Fireworks filter syntax](https://docs.fireworks.ai/api-reference/list-models).",
                        "preselected": True,
                    },
                    "orderBy": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Order By",
                        "desc": "Order results by supported resource fields, for example `name`.",
                    },
                    "readMask": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Read Mask",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "evaluator_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Evaluator ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-evaluator",
                        "ref_data": "evaluators",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-evaluator-version",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/evaluators/{evaluator_id}/versions/{version_id}",
                "operation_id": "Gateway_GetEvaluatorVersion",
                "display_name": "Get Evaluator Version",
                "short_desc": "Get Evaluator Version in Fireworks AI",
                "desc": "Get Evaluator Version. Manage the scoring logic used to evaluate model responses.",
                "groups": (
                    "Evaluators",
                ),
                "flatten_body": True,
                "fields": {
                    "readMask": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Read Mask",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "evaluator_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Evaluator ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-evaluator",
                        "ref_data": "evaluators",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                    "version_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Version ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-version",
                        "ref_data": "evaluator-versions",
                        "depends_on": (
                            "account_id",
                            "evaluator_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-evaluator-version",
                "method": "DELETE",
                "path": "/v1/accounts/{account_id}/evaluators/{evaluator_id}/versions/{version_id}",
                "operation_id": "Gateway_DeleteEvaluatorVersion",
                "display_name": "Delete Evaluator Version",
                "short_desc": "Delete Evaluator Version in Fireworks AI",
                "desc": "Delete Evaluator Version. Manage the scoring logic used to evaluate model responses. **Deletion is permanent.**",
                "groups": (
                    "Evaluators",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "evaluator_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Evaluator ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-evaluator",
                        "ref_data": "evaluators",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                    "version_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Version ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-version",
                        "ref_data": "evaluator-versions",
                        "depends_on": (
                            "account_id",
                            "evaluator_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "alias-evaluator-version",
                "method": "POST",
                "path": "/v1/accounts/{account_id}/evaluators/{evaluator_id}/versions/{version_id}:alias",
                "operation_id": "Gateway_AliasEvaluatorVersion",
                "display_name": "Alias Evaluator Version",
                "short_desc": "Alias Evaluator Version in Fireworks AI",
                "desc": "Alias Evaluator Version. Manage the scoring logic used to evaluate model responses.",
                "groups": (
                    "Evaluators",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "evaluator_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Evaluator ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-evaluator",
                        "ref_data": "evaluators",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                    "version_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Version ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-version",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "rollback-evaluator",
                "method": "POST",
                "path": "/v1/accounts/{account_id}/evaluators/{evaluator_id}/versions/{version_id}:rollback",
                "operation_id": "Gateway_RollbackEvaluator",
                "display_name": "Rollback Evaluator",
                "short_desc": "Rollback Evaluator in Fireworks AI",
                "desc": "Rollback Evaluator. Manage the scoring logic used to evaluate model responses.",
                "groups": (
                    "Evaluators",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "evaluator_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Evaluator ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-evaluator",
                        "ref_data": "evaluators",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                    "version_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Version ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-version",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-evaluator-build-log-endpoint",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/evaluators/{evaluator_id}:getBuildLogEndpoint",
                "operation_id": "Gateway_GetEvaluatorBuildLogEndpoint",
                "display_name": "Get Evaluator Build Log Endpoint",
                "short_desc": "Get Evaluator Build Log Endpoint in Fireworks AI",
                "desc": "Get Evaluator Build Log Endpoint. Manage the scoring logic used to evaluate model responses.",
                "groups": (
                    "Evaluators",
                ),
                "flatten_body": True,
                "fields": {
                    "readMask": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Read Mask",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "evaluator_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Evaluator ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-evaluator",
                        "ref_data": "evaluators",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-evaluator-source-code-endpoint",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/evaluators/{evaluator_id}:getSourceCodeSignedUrl",
                "operation_id": "Gateway_GetEvaluatorSourceCodeEndpoint",
                "display_name": "Get Evaluator Source Code Endpoint",
                "short_desc": "Get Evaluator Source Code Endpoint in Fireworks AI",
                "desc": "Get Evaluator Source Code Endpoint. Manage the scoring logic used to evaluate model responses.",
                "groups": (
                    "Evaluators",
                ),
                "flatten_body": True,
                "fields": {
                    "readMask": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Read Mask",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "evaluator_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Evaluator ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-evaluator",
                        "ref_data": "evaluators",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-evaluator-upload-endpoint",
                "method": "POST",
                "path": "/v1/accounts/{account_id}/evaluators/{evaluator_id}:getUploadEndpoint",
                "operation_id": "Gateway_GetEvaluatorUploadEndpoint",
                "display_name": "Get Evaluator Upload Endpoint",
                "short_desc": "Get Evaluator Upload Endpoint in Fireworks AI",
                "desc": "Get Evaluator Upload Endpoint. Manage the scoring logic used to evaluate model responses.",
                "groups": (
                    "Evaluators",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "evaluator_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Evaluator ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-evaluator",
                        "ref_data": "evaluators",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "validate-evaluator-upload",
                "method": "POST",
                "path": "/v1/accounts/{account_id}/evaluators/{evaluator_id}:validateUpload",
                "operation_id": "Gateway_ValidateEvaluatorUpload",
                "display_name": "Validate Evaluator Upload",
                "short_desc": "Validate Evaluator Upload in Fireworks AI",
                "desc": "Validate Evaluator Upload. Manage the scoring logic used to evaluate model responses.",
                "groups": (
                    "Evaluators",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "evaluator_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Evaluator ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-evaluator",
                        "ref_data": "evaluators",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "preview-evaluator",
                "method": "POST",
                "path": "/v1/accounts/{account_id}/evaluators:previewEvaluator",
                "operation_id": "Gateway_PreviewEvaluator",
                "display_name": "Preview Evaluator",
                "short_desc": "Preview Evaluator in Fireworks AI",
                "desc": "Preview Evaluator. Manage the scoring logic used to evaluate model responses.",
                "groups": (
                    "Evaluators",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "evaluator": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "name": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                            "createTime": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                            "createdBy": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                            "updateTime": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                            "state": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                            "criteria": <RestSchemaFieldOverlayInfo>{
                                "fields": {
                                    "type": <RestSchemaFieldOverlayInfo>{
                                        "allowed_values": (
                                            <AllowedValueInfo>{
                                                "value": "TYPE_UNSPECIFIED",
                                                "display_name": "Unspecified",
                                            },
                                            <AllowedValueInfo>{
                                                "value": "CODE_SNIPPETS",
                                                "display_name": "Code Snippets",
                                            },
                                        ),
                                    },
                                },
                            },
                            "status": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                            "source": <RestSchemaFieldOverlayInfo>{
                                "fields": {
                                    "type": <RestSchemaFieldOverlayInfo>{
                                        "allowed_values": (
                                            <AllowedValueInfo>{
                                                "value": "TYPE_UNSPECIFIED",
                                                "display_name": "Unspecified",
                                            },
                                            <AllowedValueInfo>{
                                                "value": "TYPE_UPLOAD",
                                                "display_name": "Type Upload",
                                            },
                                            <AllowedValueInfo>{
                                                "value": "TYPE_GITHUB",
                                                "display_name": "Type Github",
                                            },
                                            <AllowedValueInfo>{
                                                "value": "TYPE_TEMPORARY",
                                                "display_name": "Type Temporary",
                                            },
                                        ),
                                    },
                                },
                            },
                            "encryptionState": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                        },
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-evaluator",
                "method": "POST",
                "path": "/v1/accounts/{account_id}/evaluatorsV2",
                "operation_id": "Gateway_CreateEvaluatorV2",
                "display_name": "Create Evaluator",
                "short_desc": "Create Evaluator in Fireworks AI",
                "desc": "Create Evaluator. Manage the scoring logic used to evaluate model responses.",
                "groups": (
                    "Evaluators",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "evaluator": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "name": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                            "createTime": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                            "createdBy": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                            "updateTime": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                            "state": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                            "criteria": <RestSchemaFieldOverlayInfo>{
                                "fields": {
                                    "type": <RestSchemaFieldOverlayInfo>{
                                        "allowed_values": (
                                            <AllowedValueInfo>{
                                                "value": "TYPE_UNSPECIFIED",
                                                "display_name": "Unspecified",
                                            },
                                            <AllowedValueInfo>{
                                                "value": "CODE_SNIPPETS",
                                                "display_name": "Code Snippets",
                                            },
                                        ),
                                    },
                                },
                            },
                            "status": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                            "source": <RestSchemaFieldOverlayInfo>{
                                "fields": {
                                    "type": <RestSchemaFieldOverlayInfo>{
                                        "allowed_values": (
                                            <AllowedValueInfo>{
                                                "value": "TYPE_UNSPECIFIED",
                                                "display_name": "Unspecified",
                                            },
                                            <AllowedValueInfo>{
                                                "value": "TYPE_UPLOAD",
                                                "display_name": "Type Upload",
                                            },
                                            <AllowedValueInfo>{
                                                "value": "TYPE_GITHUB",
                                                "display_name": "Type Github",
                                            },
                                            <AllowedValueInfo>{
                                                "value": "TYPE_TEMPORARY",
                                                "display_name": "Type Temporary",
                                            },
                                        ),
                                    },
                                },
                            },
                            "encryptionState": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                        },
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-models",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/models",
                "operation_id": "Gateway_ListModels",
                "display_name": "List Models",
                "short_desc": "List Models in Fireworks AI",
                "desc": "List Models. Manage model artifacts and select models for inference or training. Use `pageToken` with the returned `nextPageToken` to retrieve further pages.",
                "groups": (
                    "Models",
                ),
                "flatten_body": True,
                "fields": {
                    "pageSize": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "preselected": True,
                        "example_value": 20,
                    },
                    "pageToken": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Token",
                        "desc": "Use `nextPageToken` from the previous result to fetch another page.",
                    },
                    "filter": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Filter",
                        "desc": "Filter the results using the [Fireworks filter syntax](https://docs.fireworks.ai/api-reference/list-models).",
                        "preselected": True,
                    },
                    "orderBy": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Order By",
                        "desc": "Order results by supported resource fields, for example `name`.",
                    },
                    "readMask": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Read Mask",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-model",
                "method": "POST",
                "path": "/v1/accounts/{account_id}/models",
                "operation_id": "Gateway_CreateModel",
                "display_name": "Create Model",
                "short_desc": "Create Model in Fireworks AI",
                "desc": "Create Model. Manage model artifacts and select models for inference or training.",
                "groups": (
                    "Models",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "model": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "name": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                            "createTime": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                            "state": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                            "status": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                            "kind": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (
                                    <AllowedValueInfo>{
                                        "value": "KIND_UNSPECIFIED",
                                        "display_name": "Unspecified",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "HF_BASE_MODEL",
                                        "display_name": "Hugging Face Base Model",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "HF_PEFT_ADDON",
                                        "display_name": "Hugging Face PEFT Add-on",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "HF_TEFT_ADDON",
                                        "display_name": "Hugging Face TEFT Add-on",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "FLUMINA_BASE_MODEL",
                                        "display_name": "Flumina Base Model",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "FLUMINA_ADDON",
                                        "display_name": "Flumina Add-on",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "DRAFT_ADDON",
                                        "display_name": "Draft Add-on",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "LIVE_MERGE",
                                        "display_name": "Live Merge",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "CUSTOM_MODEL",
                                        "display_name": "Custom Model",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "EMBEDDING_MODEL",
                                        "display_name": "Embedding Model",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "SNAPSHOT_MODEL",
                                        "display_name": "Snapshot Model",
                                    },
                                ),
                            },
                            "baseModelDetails": <RestSchemaFieldOverlayInfo>{
                                "fields": {
                                    "checkpointFormat": <RestSchemaFieldOverlayInfo>{
                                        "allowed_values": (
                                            <AllowedValueInfo>{
                                                "value": "CHECKPOINT_FORMAT_UNSPECIFIED",
                                                "display_name": "Unspecified",
                                            },
                                            <AllowedValueInfo>{
                                                "value": "NATIVE",
                                                "display_name": "Native",
                                            },
                                            <AllowedValueInfo>{
                                                "value": "HUGGINGFACE",
                                                "display_name": "Huggingface",
                                            },
                                            <AllowedValueInfo>{
                                                "value": "UNINITIALIZED",
                                                "display_name": "Uninitialized",
                                            },
                                        ),
                                    },
                                    "defaultPrecision": <RestSchemaFieldOverlayInfo>{
                                        "ignore": True,
                                    },
                                },
                            },
                            "peftDetails": <RestSchemaFieldOverlayInfo>{
                                "fields": {
                                    "baseModel": <RestSchemaFieldOverlayInfo>{
                                        "ref_data": "public-models",
                                        "supports_custom_values": True,
                                    },
                                    "baseModelType": <RestSchemaFieldOverlayInfo>{
                                        "ignore": True,
                                    },
                                },
                            },
                            "importedFrom": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                            "fineTuningJob": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                            "deployedModelRefs": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                            "cluster": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                            "calibrated": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                            "tunable": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                            "updateTime": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                            "defaultSamplingParams": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                            "rlTunable": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                            "snapshotType": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (
                                    <AllowedValueInfo>{
                                        "value": "FULL_SNAPSHOT",
                                        "display_name": "Full Snapshot",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "INCREMENTAL_SNAPSHOT",
                                        "display_name": "Incremental Snapshot",
                                    },
                                ),
                            },
                            "supportsServerless": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                            "supervisedLoraTunable": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                            "supervisedFullParameterTunable": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                            "rlLoraTunable": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                            "rlFullParameterTunable": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                            "encryptionState": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                            "serverlessModes": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                        },
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-model",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/models/{model_id}",
                "operation_id": "Gateway_GetModel",
                "display_name": "Get Model",
                "short_desc": "Get Model in Fireworks AI",
                "desc": "Get Model. Manage model artifacts and select models for inference or training.",
                "groups": (
                    "Models",
                ),
                "flatten_body": True,
                "fields": {
                    "readMask": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Read Mask",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "model_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-model",
                        "ref_data": "models",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-model",
                "method": "PATCH",
                "path": "/v1/accounts/{account_id}/models/{model_id}",
                "operation_id": "Gateway_UpdateModel",
                "display_name": "Update Model",
                "short_desc": "Update Model in Fireworks AI",
                "desc": "Update Model. Manage model artifacts and select models for inference or training.",
                "groups": (
                    "Models",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "model_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-model",
                        "ref_data": "models",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                    "createTime": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "state": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "status": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "kind": <RestSchemaFieldOverlayInfo>{
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "KIND_UNSPECIFIED",
                                "display_name": "Unspecified",
                            },
                            <AllowedValueInfo>{
                                "value": "HF_BASE_MODEL",
                                "display_name": "Hugging Face Base Model",
                            },
                            <AllowedValueInfo>{
                                "value": "HF_PEFT_ADDON",
                                "display_name": "Hugging Face PEFT Add-on",
                            },
                            <AllowedValueInfo>{
                                "value": "HF_TEFT_ADDON",
                                "display_name": "Hugging Face TEFT Add-on",
                            },
                            <AllowedValueInfo>{
                                "value": "FLUMINA_BASE_MODEL",
                                "display_name": "Flumina Base Model",
                            },
                            <AllowedValueInfo>{
                                "value": "FLUMINA_ADDON",
                                "display_name": "Flumina Add-on",
                            },
                            <AllowedValueInfo>{
                                "value": "DRAFT_ADDON",
                                "display_name": "Draft Add-on",
                            },
                            <AllowedValueInfo>{
                                "value": "LIVE_MERGE",
                                "display_name": "Live Merge",
                            },
                            <AllowedValueInfo>{
                                "value": "CUSTOM_MODEL",
                                "display_name": "Custom Model",
                            },
                            <AllowedValueInfo>{
                                "value": "EMBEDDING_MODEL",
                                "display_name": "Embedding Model",
                            },
                            <AllowedValueInfo>{
                                "value": "SNAPSHOT_MODEL",
                                "display_name": "Snapshot Model",
                            },
                        ),
                    },
                    "baseModelDetails": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "checkpointFormat": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (
                                    <AllowedValueInfo>{
                                        "value": "CHECKPOINT_FORMAT_UNSPECIFIED",
                                        "display_name": "Unspecified",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "NATIVE",
                                        "display_name": "Native",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "HUGGINGFACE",
                                        "display_name": "Huggingface",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "UNINITIALIZED",
                                        "display_name": "Uninitialized",
                                    },
                                ),
                            },
                            "defaultPrecision": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                        },
                    },
                    "peftDetails": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "baseModel": <RestSchemaFieldOverlayInfo>{
                                "ref_data": "public-models",
                                "supports_custom_values": True,
                            },
                            "baseModelType": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                        },
                    },
                    "importedFrom": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "fineTuningJob": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "deployedModelRefs": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "cluster": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "calibrated": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "tunable": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "updateTime": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "defaultSamplingParams": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "rlTunable": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "snapshotType": <RestSchemaFieldOverlayInfo>{
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "FULL_SNAPSHOT",
                                "display_name": "Full Snapshot",
                            },
                            <AllowedValueInfo>{
                                "value": "INCREMENTAL_SNAPSHOT",
                                "display_name": "Incremental Snapshot",
                            },
                        ),
                    },
                    "supportsServerless": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "supervisedLoraTunable": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "supervisedFullParameterTunable": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "rlLoraTunable": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "rlFullParameterTunable": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "encryptionState": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "serverlessModes": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "displayName": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-model",
                "method": "DELETE",
                "path": "/v1/accounts/{account_id}/models/{model_id}",
                "operation_id": "Gateway_DeleteModel",
                "display_name": "Delete Model",
                "short_desc": "Delete Model in Fireworks AI",
                "desc": "Delete Model. Manage model artifacts and select models for inference or training. **Deletion is permanent.**",
                "groups": (
                    "Models",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "model_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-model",
                        "ref_data": "models",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-model-version",
                "method": "POST",
                "path": "/v1/accounts/{account_id}/models/{model_id}/versions",
                "operation_id": "Gateway_CreateModelVersion",
                "display_name": "Create Model Version",
                "short_desc": "Create Model Version in Fireworks AI",
                "desc": "Create Model Version. Manage model artifacts and select models for inference or training.",
                "groups": (
                    "Models",
                ),
                "flatten_body": True,
                "fields": {
                    "versionId": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Version ID",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "model_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-model",
                        "ref_data": "models",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "createTime": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "snapshot": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "deploy-model-version",
                "method": "POST",
                "path": "/v1/accounts/{account_id}/models/{model_id}/versions/{version_id}:deploy",
                "operation_id": "Gateway_DeployModelVersion",
                "display_name": "Deploy Model Version",
                "short_desc": "Deploy Model Version in Fireworks AI",
                "desc": "Deploy Model Version. Manage model artifacts and select models for inference or training.",
                "groups": (
                    "Models",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "model_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-model",
                        "ref_data": "models",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                    "version_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Version ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-version",
                    },
                    "deployment": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "deployments",
                        "supports_custom_values": True,
                        "depends_on": (
                            "account_id",
                        ),
                    },
                    "rolloutStrategy": <RestSchemaFieldOverlayInfo>{
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "ROLLOUT_STRATEGY_UNSPECIFIED",
                                "display_name": "Unspecified",
                            },
                            <AllowedValueInfo>{
                                "value": "ROLLOUT_STRATEGY_STANDARD",
                                "display_name": "Standard",
                            },
                            <AllowedValueInfo>{
                                "value": "ROLLOUT_STRATEGY_HOT_RELOAD",
                                "display_name": "Hot Reload",
                            },
                        ),
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-model-version-count",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/models/{model_id}/versions:count",
                "operation_id": "Gateway_GetModelVersionCount",
                "display_name": "Get Model Version Count",
                "short_desc": "Get Model Version Count in Fireworks AI",
                "desc": "Get Model Version Count. Manage model artifacts and select models for inference or training.",
                "groups": (
                    "Models",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "model_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-model",
                        "ref_data": "models",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-model-download-endpoint",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/models/{model_id}:getDownloadEndpoint",
                "operation_id": "Gateway_GetModelDownloadEndpoint",
                "display_name": "Get Model Download Endpoint",
                "short_desc": "Get Model Download Endpoint in Fireworks AI",
                "desc": "Get Model Download Endpoint. Manage model artifacts and select models for inference or training.",
                "groups": (
                    "Models",
                ),
                "flatten_body": True,
                "fields": {
                    "readMask": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Read Mask",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "model_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-model",
                        "ref_data": "models",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-model-upload-endpoint",
                "method": "POST",
                "path": "/v1/accounts/{account_id}/models/{model_id}:getUploadEndpoint",
                "operation_id": "Gateway_GetModelUploadEndpoint",
                "display_name": "Get Model Upload Endpoint",
                "short_desc": "Get Model Upload Endpoint in Fireworks AI",
                "desc": "Get Model Upload Endpoint. Manage model artifacts and select models for inference or training.",
                "groups": (
                    "Models",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "model_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-model",
                        "ref_data": "models",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "import-model",
                "method": "POST",
                "path": "/v1/accounts/{account_id}/models/{model_id}:import",
                "operation_id": "Gateway_ImportModel",
                "display_name": "Import Model",
                "short_desc": "Import Model in Fireworks AI",
                "desc": "Import Model. Manage model artifacts and select models for inference or training.",
                "groups": (
                    "Models",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "model_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-model",
                        "ref_data": "models",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                    "awsS3Source": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "accessSecret": <RestSchemaFieldOverlayInfo>{
                                "sensitive": True,
                            },
                        },
                    },
                    "azureBlobSource": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "sasTokenSecret": <RestSchemaFieldOverlayInfo>{
                                "sensitive": True,
                            },
                        },
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "prepare-model",
                "method": "POST",
                "path": "/v1/accounts/{account_id}/models/{model_id}:prepare",
                "operation_id": "Gateway_PrepareModel",
                "display_name": "Prepare Model",
                "short_desc": "Prepare Model in Fireworks AI",
                "desc": "Prepare Model. Manage model artifacts and select models for inference or training.",
                "groups": (
                    "Models",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "model_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-model",
                        "ref_data": "models",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                    "precision": <RestSchemaFieldOverlayInfo>{
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "PRECISION_UNSPECIFIED",
                                "display_name": "Unspecified",
                            },
                            <AllowedValueInfo>{
                                "value": "FP16",
                                "display_name": "FP16",
                            },
                            <AllowedValueInfo>{
                                "value": "FP8",
                                "display_name": "FP8",
                            },
                            <AllowedValueInfo>{
                                "value": "FP8_MM",
                                "display_name": "FP8 MM",
                            },
                            <AllowedValueInfo>{
                                "value": "FP8_AR",
                                "display_name": "FP8 AR",
                            },
                            <AllowedValueInfo>{
                                "value": "FP8_MM_KV_ATTN",
                                "display_name": "FP8 MM KV ATTN",
                            },
                            <AllowedValueInfo>{
                                "value": "FP8_KV",
                                "display_name": "FP8 KV",
                            },
                            <AllowedValueInfo>{
                                "value": "FP8_MM_V2",
                                "display_name": "FP8 MM V2",
                            },
                            <AllowedValueInfo>{
                                "value": "FP8_V2",
                                "display_name": "FP8 V2",
                            },
                            <AllowedValueInfo>{
                                "value": "FP8_MM_KV_ATTN_V2",
                                "display_name": "FP8 MM KV ATTN V2",
                            },
                            <AllowedValueInfo>{
                                "value": "NF4",
                                "display_name": "NF4",
                            },
                            <AllowedValueInfo>{
                                "value": "FP4",
                                "display_name": "FP4",
                            },
                            <AllowedValueInfo>{
                                "value": "BF16",
                                "display_name": "BF16",
                            },
                            <AllowedValueInfo>{
                                "value": "FP4_BLOCKSCALED_MM",
                                "display_name": "FP4 Blockscaled MM",
                            },
                            <AllowedValueInfo>{
                                "value": "FP4_MX_MOE",
                                "display_name": "FP4 Mx MOE",
                            },
                        ),
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "validate-model-upload",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/models/{model_id}:validateUpload",
                "operation_id": "Gateway_ValidateModelUpload",
                "display_name": "Validate Model Upload",
                "short_desc": "Validate Model Upload in Fireworks AI",
                "desc": "Validate Model Upload. Manage model artifacts and select models for inference or training.",
                "groups": (
                    "Models",
                ),
                "flatten_body": True,
                "fields": {
                    "skipHfConfigValidation": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Skip Hf Config Validation",
                    },
                    "trustRemoteCode": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Trust Remote Code",
                    },
                    "configOnly": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Config Only",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "model_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-model",
                        "ref_data": "models",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-pricing-plans",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/pricingPlans",
                "operation_id": "Gateway_ListPricingPlans",
                "display_name": "List Pricing Plans",
                "short_desc": "List Pricing Plans in Fireworks AI",
                "desc": "List Pricing Plans. Inspect billing information to track spending. Use `pageToken` with the returned `nextPageToken` to retrieve further pages.",
                "groups": (
                    "Billing",
                ),
                "flatten_body": True,
                "fields": {
                    "pageSize": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "preselected": True,
                        "example_value": 20,
                    },
                    "pageToken": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Token",
                        "desc": "Use `nextPageToken` from the previous result to fetch another page.",
                    },
                    "filter": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Filter",
                        "desc": "Filter the results using the [Fireworks filter syntax](https://docs.fireworks.ai/api-reference/list-models).",
                        "preselected": True,
                    },
                    "orderBy": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Order By",
                        "desc": "Order results by supported resource fields, for example `name`.",
                    },
                    "readMask": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Read Mask",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-pricing-plan",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/pricingPlans/{pricing_plan_id}",
                "operation_id": "Gateway_GetPricingPlan",
                "display_name": "Get Pricing Plan",
                "short_desc": "Get Pricing Plan in Fireworks AI",
                "desc": "Get Pricing Plan. Inspect billing information to track spending.",
                "groups": (
                    "Billing",
                ),
                "flatten_body": True,
                "fields": {
                    "readMask": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Read Mask",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "pricing_plan_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Pricing Plan ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-pricing-plan",
                        "ref_data": "pricing-plans",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-quotas",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/quotas",
                "operation_id": "Gateway_ListQuotas",
                "display_name": "List Quotas",
                "short_desc": "List Quotas in Fireworks AI",
                "desc": "List Quotas. Monitor usage, limits, and costs for your account. Use `pageToken` with the returned `nextPageToken` to retrieve further pages.",
                "groups": (
                    "Usage",
                ),
                "flatten_body": True,
                "fields": {
                    "pageSize": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "preselected": True,
                        "example_value": 20,
                    },
                    "pageToken": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Token",
                        "desc": "Use `nextPageToken` from the previous result to fetch another page.",
                    },
                    "filter": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Filter",
                        "desc": "Filter the results using the [Fireworks filter syntax](https://docs.fireworks.ai/api-reference/list-models).",
                        "preselected": True,
                    },
                    "orderBy": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Order By",
                        "desc": "Order results by supported resource fields, for example `name`.",
                    },
                    "readMask": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Read Mask",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-quota",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/quotas/{quota_id}",
                "operation_id": "Gateway_GetQuota",
                "display_name": "Get Quota",
                "short_desc": "Get Quota in Fireworks AI",
                "desc": "Get Quota. Monitor usage, limits, and costs for your account.",
                "groups": (
                    "Usage",
                ),
                "flatten_body": True,
                "fields": {
                    "readMask": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Read Mask",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "quota_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Quota ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-quota",
                        "ref_data": "quotas",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-reinforcement-fine-tuning-jobs",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/reinforcementFineTuningJobs",
                "operation_id": "Gateway_ListReinforcementFineTuningJobs",
                "display_name": "List Reinforcement Fine Tuning Jobs",
                "short_desc": "List Reinforcement Fine Tuning Jobs in Fireworks AI",
                "desc": "List Reinforcement Fine Tuning Jobs. Manage reinforcement learning jobs for task-specific models. **Creating or resuming training incurs charges.** Use `pageToken` with the returned `nextPageToken` to retrieve further pages.",
                "groups": (
                    "Reinforcement Fine-Tuning",
                ),
                "flatten_body": True,
                "fields": {
                    "pageSize": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "preselected": True,
                        "example_value": 20,
                    },
                    "pageToken": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Token",
                        "desc": "Use `nextPageToken` from the previous result to fetch another page.",
                    },
                    "filter": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Filter",
                        "desc": "Filter the results using the [Fireworks filter syntax](https://docs.fireworks.ai/api-reference/list-models).",
                        "preselected": True,
                    },
                    "orderBy": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Order By",
                        "desc": "Order results by supported resource fields, for example `name`.",
                    },
                    "readMask": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Read Mask",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-reinforcement-fine-tuning-job",
                "method": "POST",
                "path": "/v1/accounts/{account_id}/reinforcementFineTuningJobs",
                "operation_id": "Gateway_CreateReinforcementFineTuningJob",
                "display_name": "Create Reinforcement Fine Tuning Job",
                "short_desc": "Create Reinforcement Fine Tuning Job in Fireworks AI",
                "desc": "Create Reinforcement Fine Tuning Job. Manage reinforcement learning jobs for task-specific models. **Creating or resuming training incurs charges.**",
                "groups": (
                    "Reinforcement Fine-Tuning",
                ),
                "flatten_body": True,
                "fields": {
                    "reinforcementFineTuningJobId": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Reinforcement Fine Tuning Job ID",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "createTime": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "completedTime": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "dataset": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "datasets",
                        "supports_custom_values": True,
                        "depends_on": (
                            "account_id",
                        ),
                    },
                    "evaluationDataset": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "datasets",
                        "supports_custom_values": True,
                        "depends_on": (
                            "account_id",
                        ),
                    },
                    "state": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "status": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "createdBy": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "trainingConfig": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "baseModel": <RestSchemaFieldOverlayInfo>{
                                "ref_data": "public-models",
                                "supports_custom_values": True,
                            },
                        },
                    },
                    "evaluator": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "evaluators",
                        "supports_custom_values": True,
                        "depends_on": (
                            "account_id",
                        ),
                    },
                    "wandbConfig": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "apiKey": <RestSchemaFieldOverlayInfo>{
                                "sensitive": True,
                            },
                            "url": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                        },
                    },
                    "awsS3Config": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "credentialsSecret": <RestSchemaFieldOverlayInfo>{
                                "sensitive": True,
                            },
                        },
                    },
                    "azureBlobStorageConfig": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "credentialsSecret": <RestSchemaFieldOverlayInfo>{
                                "sensitive": True,
                            },
                        },
                    },
                    "outputStats": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "jobProgress": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "outputMetrics": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "lossConfig": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "method": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (
                                    <AllowedValueInfo>{
                                        "value": "METHOD_UNSPECIFIED",
                                        "display_name": "Unspecified",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "GRPO",
                                        "display_name": "Grpo",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "DAPO",
                                        "display_name": "Dapo",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "DPO",
                                        "display_name": "Dpo",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "ORPO",
                                        "display_name": "Orpo",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "GSPO_TOKEN",
                                        "display_name": "Gspo Token",
                                    },
                                ),
                            },
                        },
                    },
                    "trainerLogsSignedUrl": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "acceleratorSeconds": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "purpose": <RestSchemaFieldOverlayInfo>{
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "PURPOSE_UNSPECIFIED",
                                "display_name": "Unspecified",
                            },
                            <AllowedValueInfo>{
                                "value": "PURPOSE_PILOT",
                                "display_name": "Pilot",
                            },
                        ),
                    },
                    "encryptionState": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "displayName": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-reinforcement-fine-tuning-job",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/reinforcementFineTuningJobs/{reinforcement_fine_tuning_job_id}",
                "operation_id": "Gateway_GetReinforcementFineTuningJob",
                "display_name": "Get Reinforcement Fine Tuning Job",
                "short_desc": "Get Reinforcement Fine Tuning Job in Fireworks AI",
                "desc": "Get Reinforcement Fine Tuning Job. Manage reinforcement learning jobs for task-specific models. **Creating or resuming training incurs charges.**",
                "groups": (
                    "Reinforcement Fine-Tuning",
                ),
                "flatten_body": True,
                "fields": {
                    "readMask": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Read Mask",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "reinforcement_fine_tuning_job_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Reinforcement Fine Tuning Job ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-reinforcement-fine-tuning-job",
                        "ref_data": "reinforcement-fine-tuning-jobs",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-reinforcement-fine-tuning-job",
                "method": "PATCH",
                "path": "/v1/accounts/{account_id}/reinforcementFineTuningJobs/{reinforcement_fine_tuning_job_id}",
                "operation_id": "Gateway_UpdateReinforcementFineTuningJob",
                "display_name": "Update Reinforcement Fine Tuning Job",
                "short_desc": "Update Reinforcement Fine Tuning Job in Fireworks AI",
                "desc": "Update Reinforcement Fine Tuning Job. Manage reinforcement learning jobs for task-specific models. **Creating or resuming training incurs charges.**",
                "groups": (
                    "Reinforcement Fine-Tuning",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "reinforcement_fine_tuning_job_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Reinforcement Fine Tuning Job ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-reinforcement-fine-tuning-job",
                        "ref_data": "reinforcement-fine-tuning-jobs",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                    "createTime": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "completedTime": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "dataset": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "datasets",
                        "supports_custom_values": True,
                        "depends_on": (
                            "account_id",
                        ),
                    },
                    "evaluationDataset": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "datasets",
                        "supports_custom_values": True,
                        "depends_on": (
                            "account_id",
                        ),
                    },
                    "state": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "status": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "createdBy": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "trainingConfig": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "baseModel": <RestSchemaFieldOverlayInfo>{
                                "ref_data": "public-models",
                                "supports_custom_values": True,
                            },
                        },
                    },
                    "evaluator": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "evaluators",
                        "supports_custom_values": True,
                        "depends_on": (
                            "account_id",
                        ),
                    },
                    "wandbConfig": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "apiKey": <RestSchemaFieldOverlayInfo>{
                                "sensitive": True,
                            },
                            "url": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                        },
                    },
                    "awsS3Config": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "credentialsSecret": <RestSchemaFieldOverlayInfo>{
                                "sensitive": True,
                            },
                        },
                    },
                    "azureBlobStorageConfig": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "credentialsSecret": <RestSchemaFieldOverlayInfo>{
                                "sensitive": True,
                            },
                        },
                    },
                    "outputStats": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "jobProgress": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "outputMetrics": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "lossConfig": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "method": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (
                                    <AllowedValueInfo>{
                                        "value": "METHOD_UNSPECIFIED",
                                        "display_name": "Unspecified",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "GRPO",
                                        "display_name": "Grpo",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "DAPO",
                                        "display_name": "Dapo",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "DPO",
                                        "display_name": "Dpo",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "ORPO",
                                        "display_name": "Orpo",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "GSPO_TOKEN",
                                        "display_name": "Gspo Token",
                                    },
                                ),
                            },
                        },
                    },
                    "trainerLogsSignedUrl": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "acceleratorSeconds": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "purpose": <RestSchemaFieldOverlayInfo>{
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "PURPOSE_UNSPECIFIED",
                                "display_name": "Unspecified",
                            },
                            <AllowedValueInfo>{
                                "value": "PURPOSE_PILOT",
                                "display_name": "Pilot",
                            },
                        ),
                    },
                    "encryptionState": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "displayName": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-reinforcement-fine-tuning-job",
                "method": "DELETE",
                "path": "/v1/accounts/{account_id}/reinforcementFineTuningJobs/{reinforcement_fine_tuning_job_id}",
                "operation_id": "Gateway_DeleteReinforcementFineTuningJob",
                "display_name": "Delete Reinforcement Fine Tuning Job",
                "short_desc": "Delete Reinforcement Fine Tuning Job in Fireworks AI",
                "desc": "Delete Reinforcement Fine Tuning Job. Manage reinforcement learning jobs for task-specific models. **Creating or resuming training incurs charges.** **Deletion is permanent.**",
                "groups": (
                    "Reinforcement Fine-Tuning",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "reinforcement_fine_tuning_job_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Reinforcement Fine Tuning Job ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-reinforcement-fine-tuning-job",
                        "ref_data": "reinforcement-fine-tuning-jobs",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "cancel-reinforcement-fine-tuning-job",
                "method": "POST",
                "path": "/v1/accounts/{account_id}/reinforcementFineTuningJobs/{reinforcement_fine_tuning_job_id}:cancel",
                "operation_id": "Gateway_CancelReinforcementFineTuningJob",
                "display_name": "Cancel Reinforcement Fine Tuning Job",
                "short_desc": "Cancel Reinforcement Fine Tuning Job in Fireworks AI",
                "desc": "Cancel Reinforcement Fine Tuning Job. Manage reinforcement learning jobs for task-specific models. **Creating or resuming training incurs charges.**",
                "groups": (
                    "Reinforcement Fine-Tuning",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "reinforcement_fine_tuning_job_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Reinforcement Fine Tuning Job ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-reinforcement-fine-tuning-job",
                        "ref_data": "reinforcement-fine-tuning-jobs",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-reinforcement-fine-tuning-job-metrics-file-endpoint",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/reinforcementFineTuningJobs/{reinforcement_fine_tuning_job_id}:getMetricsFileEndpoint",
                "operation_id": "Gateway_GetReinforcementFineTuningJobMetricsFileEndpoint",
                "display_name": "Get Reinforcement Fine Tuning Job Metrics File Endpoint",
                "short_desc": "Get Reinforcement Fine Tuning Job Metrics File Endpoint in Fireworks AI",
                "desc": "Get Reinforcement Fine Tuning Job Metrics File Endpoint. Manage reinforcement learning jobs for task-specific models. **Creating or resuming training incurs charges.**",
                "groups": (
                    "Reinforcement Fine-Tuning",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "reinforcement_fine_tuning_job_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Reinforcement Fine Tuning Job ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-reinforcement-fine-tuning-job",
                        "ref_data": "reinforcement-fine-tuning-jobs",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "resume-reinforcement-fine-tuning-job",
                "method": "POST",
                "path": "/v1/accounts/{account_id}/reinforcementFineTuningJobs/{reinforcement_fine_tuning_job_id}:resume",
                "operation_id": "Gateway_ResumeReinforcementFineTuningJob",
                "display_name": "Resume Reinforcement Fine Tuning Job",
                "short_desc": "Resume Reinforcement Fine Tuning Job in Fireworks AI",
                "desc": "Resume Reinforcement Fine Tuning Job. Manage reinforcement learning jobs for task-specific models. **Creating or resuming training incurs charges.**",
                "groups": (
                    "Reinforcement Fine-Tuning",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "reinforcement_fine_tuning_job_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Reinforcement Fine Tuning Job ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-reinforcement-fine-tuning-job",
                        "ref_data": "reinforcement-fine-tuning-jobs",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-routers",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/routers",
                "operation_id": "Gateway_ListRouters",
                "display_name": "List Routers",
                "short_desc": "List Routers in Fireworks AI",
                "desc": "List Routers. Distribute inference traffic across model deployments. Use `pageToken` with the returned `nextPageToken` to retrieve further pages.",
                "groups": (
                    "Routers",
                ),
                "flatten_body": True,
                "fields": {
                    "pageSize": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "preselected": True,
                        "example_value": 20,
                    },
                    "pageToken": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Token",
                        "desc": "Use `nextPageToken` from the previous result to fetch another page.",
                    },
                    "filter": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Filter",
                        "desc": "Filter the results using the [Fireworks filter syntax](https://docs.fireworks.ai/api-reference/list-models).",
                        "preselected": True,
                    },
                    "orderBy": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Order By",
                        "desc": "Order results by supported resource fields, for example `name`.",
                    },
                    "readMask": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Read Mask",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-router",
                "method": "POST",
                "path": "/v1/accounts/{account_id}/routers",
                "operation_id": "Gateway_CreateRouter",
                "display_name": "Create Router",
                "short_desc": "Create Router in Fireworks AI",
                "desc": "Create Router. Distribute inference traffic across model deployments.",
                "groups": (
                    "Routers",
                ),
                "flatten_body": True,
                "fields": {
                    "routerId": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Router ID",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "createTime": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "createdBy": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "state": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "status": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "model": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "public-models",
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                    "aliases": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "autoGenerated": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "displayName": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-router",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/routers/{router_id}",
                "operation_id": "Gateway_GetRouter",
                "display_name": "Get Router",
                "short_desc": "Get Router in Fireworks AI",
                "desc": "Get Router. Distribute inference traffic across model deployments.",
                "groups": (
                    "Routers",
                ),
                "flatten_body": True,
                "fields": {
                    "readMask": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Read Mask",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "router_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Router ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-router",
                        "ref_data": "routers",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-router",
                "method": "PATCH",
                "path": "/v1/accounts/{account_id}/routers/{router_id}",
                "operation_id": "Gateway_UpdateRouter",
                "display_name": "Update Router",
                "short_desc": "Update Router in Fireworks AI",
                "desc": "Update Router. Distribute inference traffic across model deployments.",
                "groups": (
                    "Routers",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "router_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Router ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-router",
                        "ref_data": "routers",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                    "createTime": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "createdBy": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "state": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "status": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "model": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "public-models",
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                    "aliases": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "autoGenerated": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "displayName": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-router",
                "method": "DELETE",
                "path": "/v1/accounts/{account_id}/routers/{router_id}",
                "operation_id": "Gateway_DeleteRouter",
                "display_name": "Delete Router",
                "short_desc": "Delete Router in Fireworks AI",
                "desc": "Delete Router. Distribute inference traffic across model deployments. **Deletion is permanent.**",
                "groups": (
                    "Routers",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "router_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Router ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-router",
                        "ref_data": "routers",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-serverless-models",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/serverlessModels",
                "operation_id": "Gateway_ListServerlessModels",
                "display_name": "List Serverless Models",
                "short_desc": "List Serverless Models in Fireworks AI",
                "desc": "List Serverless Models. Manage model artifacts and select models for inference or training. Use `pageToken` with the returned `nextPageToken` to retrieve further pages.",
                "groups": (
                    "Models",
                ),
                "flatten_body": True,
                "fields": {
                    "pageSize": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "preselected": True,
                        "example_value": 20,
                    },
                    "pageToken": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Token",
                        "desc": "Use `nextPageToken` from the previous result to fetch another page.",
                    },
                    "filter": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Filter",
                        "desc": "Filter the results using the [Fireworks filter syntax](https://docs.fireworks.ai/api-reference/list-models).",
                        "preselected": True,
                    },
                    "orderBy": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Order By",
                        "desc": "Order results by supported resource fields, for example `name`.",
                    },
                    "readMask": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Read Mask",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-account-serverless-rate-limits",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/serverlessRateLimits",
                "operation_id": "Gateway_ListAccountServerlessRateLimits",
                "display_name": "List Account Serverless Rate Limits",
                "short_desc": "List Account Serverless Rate Limits in Fireworks AI",
                "desc": "List Account Serverless Rate Limits. Monitor usage, limits, and costs for your account.",
                "groups": (
                    "Usage",
                ),
                "flatten_body": True,
                "fields": {
                    "deployment": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Deployment",
                    },
                    "start": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Start",
                        "codec": "iso-8601-utc",
                    },
                    "end": <RestSchemaFieldOverlayInfo>{
                        "display_name": "End",
                        "codec": "iso-8601-utc",
                    },
                    "interval": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Interval",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-account-serverless-token-usage",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/serverlessTokenUsage",
                "operation_id": "Gateway_GetAccountServerlessTokenUsage",
                "display_name": "Get Account Serverless Token Usage",
                "short_desc": "Get Account Serverless Token Usage in Fireworks AI",
                "desc": "Get Account Serverless Token Usage. Monitor usage, limits, and costs for your account.",
                "groups": (
                    "Usage",
                ),
                "flatten_body": True,
                "fields": {
                    "start": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Start",
                        "codec": "iso-8601-utc",
                    },
                    "end": <RestSchemaFieldOverlayInfo>{
                        "display_name": "End",
                        "codec": "iso-8601-utc",
                    },
                    "interval": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Interval",
                    },
                    "includePeakTokensPerMinuteByBaseModel": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Include Peak Tokens Per Minute By Base Model",
                    },
                    "includeAverageTokensPerMinuteByBaseModel": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Include Average Tokens Per Minute By Base Model",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-supervised-fine-tuning-jobs",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/supervisedFineTuningJobs",
                "operation_id": "Gateway_ListSupervisedFineTuningJobs",
                "display_name": "List Supervised Fine Tuning Jobs",
                "short_desc": "List Supervised Fine Tuning Jobs in Fireworks AI",
                "desc": "List Supervised Fine Tuning Jobs. Train models on business examples and manage the resulting jobs. **Creating or resuming training incurs charges.** Use `pageToken` with the returned `nextPageToken` to retrieve further pages.",
                "groups": (
                    "Supervised Fine-Tuning",
                ),
                "flatten_body": True,
                "fields": {
                    "pageSize": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "preselected": True,
                        "example_value": 20,
                    },
                    "pageToken": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Token",
                        "desc": "Use `nextPageToken` from the previous result to fetch another page.",
                    },
                    "filter": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Filter",
                        "desc": "Filter the results using the [Fireworks filter syntax](https://docs.fireworks.ai/api-reference/list-models).",
                        "preselected": True,
                    },
                    "orderBy": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Order By",
                        "desc": "Order results by supported resource fields, for example `name`.",
                    },
                    "readMask": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Read Mask",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-supervised-fine-tuning-job",
                "method": "POST",
                "path": "/v1/accounts/{account_id}/supervisedFineTuningJobs",
                "operation_id": "Gateway_CreateSupervisedFineTuningJob",
                "display_name": "Create Supervised Fine Tuning Job",
                "short_desc": "Create Supervised Fine Tuning Job in Fireworks AI",
                "desc": "Create Supervised Fine Tuning Job. Train models on business examples and manage the resulting jobs. **Creating or resuming training incurs charges.**",
                "groups": (
                    "Supervised Fine-Tuning",
                ),
                "flatten_body": True,
                "fields": {
                    "supervisedFineTuningJobId": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Supervised Fine Tuning Job ID",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "createTime": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "completedTime": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "dataset": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "datasets",
                        "supports_custom_values": True,
                        "depends_on": (
                            "account_id",
                        ),
                    },
                    "awsS3Config": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "credentialsSecret": <RestSchemaFieldOverlayInfo>{
                                "sensitive": True,
                            },
                        },
                    },
                    "azureBlobStorageConfig": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "credentialsSecret": <RestSchemaFieldOverlayInfo>{
                                "sensitive": True,
                            },
                        },
                    },
                    "state": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "status": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "createdBy": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "baseModel": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "public-models",
                        "supports_custom_values": True,
                    },
                    "wandbConfig": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "apiKey": <RestSchemaFieldOverlayInfo>{
                                "sensitive": True,
                            },
                            "url": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                        },
                    },
                    "evaluationDataset": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "datasets",
                        "supports_custom_values": True,
                        "depends_on": (
                            "account_id",
                        ),
                    },
                    "updateTime": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "jobProgress": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "trainerLogsSignedUrl": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "renderSamplesSignedUrl": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "estimatedCost": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "purpose": <RestSchemaFieldOverlayInfo>{
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "PURPOSE_UNSPECIFIED",
                                "display_name": "Unspecified",
                            },
                            <AllowedValueInfo>{
                                "value": "PURPOSE_PILOT",
                                "display_name": "Pilot",
                            },
                        ),
                    },
                    "encryptionState": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "displayName": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-supervised-fine-tuning-job",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/supervisedFineTuningJobs/{supervised_fine_tuning_job_id}",
                "operation_id": "Gateway_GetSupervisedFineTuningJob",
                "display_name": "Get Supervised Fine Tuning Job",
                "short_desc": "Get Supervised Fine Tuning Job in Fireworks AI",
                "desc": "Get Supervised Fine Tuning Job. Train models on business examples and manage the resulting jobs. **Creating or resuming training incurs charges.**",
                "groups": (
                    "Supervised Fine-Tuning",
                ),
                "flatten_body": True,
                "fields": {
                    "readMask": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Read Mask",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "supervised_fine_tuning_job_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Supervised Fine Tuning Job ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-supervised-fine-tuning-job",
                        "ref_data": "supervised-fine-tuning-jobs",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-supervised-fine-tuning-job",
                "method": "DELETE",
                "path": "/v1/accounts/{account_id}/supervisedFineTuningJobs/{supervised_fine_tuning_job_id}",
                "operation_id": "Gateway_DeleteSupervisedFineTuningJob",
                "display_name": "Delete Supervised Fine Tuning Job",
                "short_desc": "Delete Supervised Fine Tuning Job in Fireworks AI",
                "desc": "Delete Supervised Fine Tuning Job. Train models on business examples and manage the resulting jobs. **Creating or resuming training incurs charges.** **Deletion is permanent.**",
                "groups": (
                    "Supervised Fine-Tuning",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "supervised_fine_tuning_job_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Supervised Fine Tuning Job ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-supervised-fine-tuning-job",
                        "ref_data": "supervised-fine-tuning-jobs",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "cancel-supervised-fine-tuning-job",
                "method": "POST",
                "path": "/v1/accounts/{account_id}/supervisedFineTuningJobs/{supervised_fine_tuning_job_id}:cancel",
                "operation_id": "Gateway_CancelSupervisedFineTuningJob",
                "display_name": "Cancel Supervised Fine Tuning Job",
                "short_desc": "Cancel Supervised Fine Tuning Job in Fireworks AI",
                "desc": "Cancel Supervised Fine Tuning Job. Train models on business examples and manage the resulting jobs. **Creating or resuming training incurs charges.**",
                "groups": (
                    "Supervised Fine-Tuning",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "supervised_fine_tuning_job_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Supervised Fine Tuning Job ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-supervised-fine-tuning-job",
                        "ref_data": "supervised-fine-tuning-jobs",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "resume-supervised-fine-tuning-job",
                "method": "POST",
                "path": "/v1/accounts/{account_id}/supervisedFineTuningJobs/{supervised_fine_tuning_job_id}:resume",
                "operation_id": "Gateway_ResumeSupervisedFineTuningJob",
                "display_name": "Resume Supervised Fine Tuning Job",
                "short_desc": "Resume Supervised Fine Tuning Job in Fireworks AI",
                "desc": "Resume Supervised Fine Tuning Job. Train models on business examples and manage the resulting jobs. **Creating or resuming training incurs charges.**",
                "groups": (
                    "Supervised Fine-Tuning",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "supervised_fine_tuning_job_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Supervised Fine Tuning Job ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-supervised-fine-tuning-job",
                        "ref_data": "supervised-fine-tuning-jobs",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "estimate-supervised-fine-tuning-job-cost",
                "method": "POST",
                "path": "/v1/accounts/{account_id}/supervisedFineTuningJobs:estimateCost",
                "operation_id": "Gateway_EstimateSupervisedFineTuningJobCost",
                "display_name": "Estimate Supervised Fine Tuning Job Cost",
                "short_desc": "Estimate Supervised Fine Tuning Job Cost in Fireworks AI",
                "desc": "Estimate Supervised Fine Tuning Job Cost. Train models on business examples and manage the resulting jobs. **Creating or resuming training incurs charges.**",
                "groups": (
                    "Supervised Fine-Tuning",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "createTime": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "completedTime": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "dataset": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "datasets",
                        "supports_custom_values": True,
                        "depends_on": (
                            "account_id",
                        ),
                    },
                    "awsS3Config": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "credentialsSecret": <RestSchemaFieldOverlayInfo>{
                                "sensitive": True,
                            },
                        },
                    },
                    "azureBlobStorageConfig": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "credentialsSecret": <RestSchemaFieldOverlayInfo>{
                                "sensitive": True,
                            },
                        },
                    },
                    "state": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "status": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "createdBy": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "baseModel": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "public-models",
                        "supports_custom_values": True,
                    },
                    "wandbConfig": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "apiKey": <RestSchemaFieldOverlayInfo>{
                                "sensitive": True,
                            },
                            "url": <RestSchemaFieldOverlayInfo>{
                                "ignore": True,
                            },
                        },
                    },
                    "evaluationDataset": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "datasets",
                        "supports_custom_values": True,
                        "depends_on": (
                            "account_id",
                        ),
                    },
                    "updateTime": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "jobProgress": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "trainerLogsSignedUrl": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "renderSamplesSignedUrl": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "estimatedCost": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "purpose": <RestSchemaFieldOverlayInfo>{
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "PURPOSE_UNSPECIFIED",
                                "display_name": "Unspecified",
                            },
                            <AllowedValueInfo>{
                                "value": "PURPOSE_PILOT",
                                "display_name": "Pilot",
                            },
                        ),
                    },
                    "encryptionState": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "displayName": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-training-shapes",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/trainingShapes",
                "operation_id": "Gateway_ListTrainingShapes",
                "display_name": "List Training Shapes",
                "short_desc": "List Training Shapes in Fireworks AI",
                "desc": "List Training Shapes. Inspect supported training configurations. Use `pageToken` with the returned `nextPageToken` to retrieve further pages.",
                "groups": (
                    "Training Shapes",
                ),
                "flatten_body": True,
                "fields": {
                    "pageSize": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "preselected": True,
                        "example_value": 20,
                    },
                    "pageToken": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Token",
                        "desc": "Use `nextPageToken` from the previous result to fetch another page.",
                    },
                    "filter": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Filter",
                        "desc": "Filter the results using the [Fireworks filter syntax](https://docs.fireworks.ai/api-reference/list-models).",
                        "preselected": True,
                    },
                    "orderBy": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Order By",
                        "desc": "Order results by supported resource fields, for example `name`.",
                    },
                    "readMask": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Read Mask",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-training-shape",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/trainingShapes/{training_shape_id}",
                "operation_id": "Gateway_GetTrainingShape",
                "display_name": "Get Training Shape",
                "short_desc": "Get Training Shape in Fireworks AI",
                "desc": "Get Training Shape. Inspect supported training configurations.",
                "groups": (
                    "Training Shapes",
                ),
                "flatten_body": True,
                "fields": {
                    "readMask": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Read Mask",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "training_shape_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Training Shape ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-training-shape",
                        "ref_data": "training-shapes",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-training-shape-versions",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/trainingShapes/{training_shape_id}/versions",
                "operation_id": "Gateway_ListTrainingShapeVersions",
                "display_name": "List Training Shape Versions",
                "short_desc": "List Training Shape Versions in Fireworks AI",
                "desc": "List Training Shape Versions. Inspect supported training configurations. Use `pageToken` with the returned `nextPageToken` to retrieve further pages.",
                "groups": (
                    "Training Shapes",
                ),
                "flatten_body": True,
                "fields": {
                    "pageSize": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "preselected": True,
                        "example_value": 20,
                    },
                    "pageToken": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Token",
                        "desc": "Use `nextPageToken` from the previous result to fetch another page.",
                    },
                    "filter": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Filter",
                        "desc": "Filter the results using the [Fireworks filter syntax](https://docs.fireworks.ai/api-reference/list-models).",
                        "preselected": True,
                    },
                    "orderBy": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Order By",
                        "desc": "Order results by supported resource fields, for example `name`.",
                    },
                    "readMask": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Read Mask",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "training_shape_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Training Shape ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-training-shape",
                        "ref_data": "training-shapes",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-training-shape-version",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/trainingShapes/{training_shape_id}/versions/{version_id}",
                "operation_id": "Gateway_GetTrainingShapeVersion",
                "display_name": "Get Training Shape Version",
                "short_desc": "Get Training Shape Version in Fireworks AI",
                "desc": "Get Training Shape Version. Inspect supported training configurations.",
                "groups": (
                    "Training Shapes",
                ),
                "flatten_body": True,
                "fields": {
                    "readMask": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Read Mask",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "training_shape_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Training Shape ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-training-shape",
                        "ref_data": "training-shapes",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                    "version_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Version ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-version",
                        "ref_data": "training-shape-versions",
                        "depends_on": (
                            "account_id",
                            "training_shape_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-account-usage-limits",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/usageLimits",
                "operation_id": "Gateway_GetAccountUsageLimits",
                "display_name": "Get Account Usage Limits",
                "short_desc": "Get Account Usage Limits in Fireworks AI",
                "desc": "Get Account Usage Limits. Monitor usage, limits, and costs for your account.",
                "groups": (
                    "Usage",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-account-usage-limits",
                "method": "PATCH",
                "path": "/v1/accounts/{account_id}/usageLimits",
                "operation_id": "Gateway_UpdateAccountUsageLimits",
                "display_name": "Update Account Usage Limits",
                "short_desc": "Update Account Usage Limits in Fireworks AI",
                "desc": "Update Account Usage Limits. Monitor usage, limits, and costs for your account.",
                "groups": (
                    "Usage",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "enforcementMode": <RestSchemaFieldOverlayInfo>{
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "ENFORCEMENT_MODE_UNSPECIFIED",
                                "display_name": "Unspecified",
                            },
                            <AllowedValueInfo>{
                                "value": "DISABLED",
                                "display_name": "Disabled",
                            },
                            <AllowedValueInfo>{
                                "value": "WARN",
                                "display_name": "Warn",
                            },
                            <AllowedValueInfo>{
                                "value": "BLOCK",
                                "display_name": "Block",
                            },
                        ),
                    },
                    "used": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "updateTime": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-user-usage-limits",
                "method": "GET",
                "path": "/v1/accounts/{account_id}/userUsageLimits",
                "operation_id": "Gateway_ListUserUsageLimits",
                "display_name": "List User Usage Limits",
                "short_desc": "List User Usage Limits in Fireworks AI",
                "desc": "List User Usage Limits. Monitor usage, limits, and costs for your account. Use `pageToken` with the returned `nextPageToken` to retrieve further pages.",
                "groups": (
                    "Usage",
                ),
                "flatten_body": True,
                "fields": {
                    "pageSize": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "preselected": True,
                        "example_value": 20,
                    },
                    "pageToken": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Token",
                        "desc": "Use `nextPageToken` from the previous result to fetch another page.",
                    },
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                },
            },
        );

        const list<hash<RestSchemaActionInfo>> UploadManifest = (
            <RestSchemaActionInfo>{
                "action": "upload-dataset-file",
                "method": "POST",
                "path": "/v1/accounts/{account_id}/datasets/{dataset_id}:upload",
                "operation_id": "Gateway_UploadDatasetFile",
                "display_name": "Upload Dataset File",
                "short_desc": "Upload Dataset File in Fireworks AI",
                "desc": "Upload Dataset File. Prepare and manage the data used by batch inference and fine-tuning.",
                "groups": (
                    "Datasets",
                ),
                "flatten_body": True,
                "fields": {
                    "account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Account ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-account",
                        "ref_data": "accounts",
                        "has_dependents": True,
                        "on_change": (
                            "refetch",
                        ),
                        "desc": "Select an account or enter its ID or full `accounts/<id>` resource name.",
                    },
                    "dataset_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Dataset ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "codec": "fireworks-resource-id",
                        "example_value": "my-dataset",
                        "ref_data": "datasets",
                        "depends_on": (
                            "account_id",
                        ),
                        "desc": "Select a resource or enter its ID or full resource name.",
                    },
                    "file": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                        "required": True,
                    },
                },
            },
        );

        const list<hash<RestSchemaActionInfo>> TextManifest = (
            <RestSchemaActionInfo>{
                "action": "text-completion",
                "method": "POST",
                "path": "/v1/completions",
                "operation_id": "create_completion_v1_completions_post",
                "display_name": "Text Completion",
                "short_desc": "Text Completion in Fireworks AI",
                "desc": "Text Completion. Generate responses for conversations, agents, or text-processing workflows. **Inference is billed by Fireworks.** This action returns a completed response.",
                "groups": (
                    "Chat",
                ),
                "flatten_body": True,
                "fields": {
                    "model": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "inference-models",
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                    "service_tier": <RestSchemaFieldOverlayInfo>{
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "auto",
                                "display_name": "Automatic",
                            },
                            <AllowedValueInfo>{
                                "value": "default",
                                "display_name": "Default",
                            },
                            <AllowedValueInfo>{
                                "value": "flex",
                                "display_name": "Flex",
                            },
                            <AllowedValueInfo>{
                                "value": "priority",
                                "display_name": "Priority",
                            },
                        ),
                    },
                    "context_length_exceeded_behavior": <RestSchemaFieldOverlayInfo>{
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "error",
                                "display_name": "Error",
                            },
                            <AllowedValueInfo>{
                                "value": "truncate",
                                "display_name": "Truncate",
                            },
                        ),
                    },
                    "stream": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "stream_options": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "temperature": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                    "max_tokens": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                        "example_value": 128,
                    },
                    "prompt": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                        "example_value": "Summarize this customer request:",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "chat-completion",
                "method": "POST",
                "path": "/v1/chat/completions",
                "operation_id": "create_chat_completion_v1_chat_completions_post",
                "display_name": "Chat Completion",
                "short_desc": "Chat Completion in Fireworks AI",
                "desc": "Chat Completion. Generate responses for conversations, agents, or text-processing workflows. **Inference is billed by Fireworks.** This action returns a completed response.",
                "groups": (
                    "Chat",
                ),
                "flatten_body": True,
                "fields": {
                    "model": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "inference-models",
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                    "tools": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "type": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (
                                    <AllowedValueInfo>{
                                        "value": "function",
                                        "display_name": "Function",
                                    },
                                ),
                            },
                        },
                    },
                    "service_tier": <RestSchemaFieldOverlayInfo>{
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "auto",
                                "display_name": "Automatic",
                            },
                            <AllowedValueInfo>{
                                "value": "default",
                                "display_name": "Default",
                            },
                            <AllowedValueInfo>{
                                "value": "flex",
                                "display_name": "Flex",
                            },
                            <AllowedValueInfo>{
                                "value": "priority",
                                "display_name": "Priority",
                            },
                        ),
                    },
                    "context_length_exceeded_behavior": <RestSchemaFieldOverlayInfo>{
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "error",
                                "display_name": "Error",
                            },
                            <AllowedValueInfo>{
                                "value": "truncate",
                                "display_name": "Truncate",
                            },
                        ),
                    },
                    "stream": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "stream_options": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "temperature": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                    "max_tokens": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                        "example_value": 128,
                    },
                    "messages": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                        "example_value": (
                            {
                                "role": "user",
                                "content": "Summarize the customer request.",
                            },
                        ),
                        "required_groups": (
                            "chat-input",
                        ),
                    },
                    "prompt_token_ids": <RestSchemaFieldOverlayInfo>{
                        "required_groups": (
                            "chat-input",
                        ),
                    },
                },
                "response_fields": {
                    "created": <RestSchemaFieldOverlayInfo>{
                        "codec": "unix-seconds",
                    },
                },
            },
        );

        const list<hash<RestSchemaActionInfo>> ResponsesManifest = (
            <RestSchemaActionInfo>{
                "action": "create-response",
                "method": "POST",
                "path": "/v1/responses",
                "operation_id": "create_response_v1_responses_post",
                "display_name": "Create Response",
                "short_desc": "Create Response in Fireworks AI",
                "desc": "Create Response. Create and manage persistent model responses for agent workflows. **Creating responses incurs inference charges.**",
                "groups": (
                    "Responses",
                ),
                "flatten_body": True,
                "fields": {
                    "model": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "inference-models",
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                    "stream": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "temperature": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                    "input": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-responses",
                "method": "GET",
                "path": "/v1/responses",
                "operation_id": "list_responses_v1_responses_get",
                "display_name": "List Responses",
                "short_desc": "List Responses in Fireworks AI",
                "desc": "List Responses. Create and manage persistent model responses for agent workflows. **Creating responses incurs inference charges.**",
                "groups": (
                    "Responses",
                ),
                "flatten_body": True,
                "fields": {
                    "limit": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Limit",
                        "preselected": True,
                        "example_value": 20,
                    },
                    "after": <RestSchemaFieldOverlayInfo>{
                        "display_name": "After",
                    },
                    "before": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Before",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-response",
                "method": "GET",
                "path": "/v1/responses/{response_id}",
                "operation_id": "get_response_v1_responses__response_id__get",
                "display_name": "Get Response",
                "short_desc": "Get Response in Fireworks AI",
                "desc": "Get Response. Create and manage persistent model responses for agent workflows. **Creating responses incurs inference charges.**",
                "groups": (
                    "Responses",
                ),
                "flatten_body": True,
                "fields": {
                    "response_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Response ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "example_value": "my-response",
                        "ref_data": "responses",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-response",
                "method": "DELETE",
                "path": "/v1/responses/{response_id}",
                "operation_id": "delete_response_v1_responses__response_id__delete",
                "display_name": "Delete Response",
                "short_desc": "Delete Response in Fireworks AI",
                "desc": "Delete Response. Create and manage persistent model responses for agent workflows. **Creating responses incurs inference charges.** **Deletion is permanent.**",
                "groups": (
                    "Responses",
                ),
                "flatten_body": True,
                "fields": {
                    "response_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Response ID",
                        "preselected": True,
                        "supports_custom_values": True,
                        "example_value": "my-response",
                        "ref_data": "responses",
                    },
                },
            },
        );

        const list<hash<RestSchemaActionInfo>> MessagesManifest = (
            <RestSchemaActionInfo>{
                "action": "create-message",
                "method": "POST",
                "path": "/v1/messages",
                "operation_id": "messages_post",
                "display_name": "Create Message",
                "short_desc": "Create Message in Fireworks AI",
                "desc": "Create Message. Generate responses for conversations, agents, or text-processing workflows. **Inference is billed by Fireworks.** This action returns a completed response.",
                "groups": (
                    "Chat",
                ),
                "flatten_body": True,
                "fields": {
                    "model": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "inference-models",
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                    "messages": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "role": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (
                                    <AllowedValueInfo>{
                                        "value": "user",
                                        "display_name": "User",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "assistant",
                                        "display_name": "Assistant",
                                    },
                                ),
                            },
                        },
                        "preselected": True,
                        "example_value": (
                            {
                                "role": "user",
                                "content": "Summarize the customer request.",
                            },
                        ),
                    },
                    "stream": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "temperature": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                    "max_tokens": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                        "example_value": 128,
                    },
                },
            },
        );

    }
}
}
