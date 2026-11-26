# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore MistralDataProvider root class definition

/** MistralManifest.qc Copyright 2026 Qore Technologies, s.r.o.

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

public namespace MistralDataProvider {
#! Reviewed Mistral operations, presentation and reference data
/** Action IDs are stable and the original chat-completion, create-embeddings and ocr paths are retained.
    The exposed operations are completed-response operations of the vendored official description; a new
    operation is exposed by adding it here. SSE-only operations are excluded; the LLM provider supports streaming
    chat. Preview operations remain
    subject to account permissions. Dropdowns fetch one page and permit explicitly supplied identifiers.
*/
public class MistralManifest {
    public {
        #! The display names of the values of enumerations the actions reach at more than one location
        /** Keyed by the location of the enumeration's schema in the vendored description, then by value.  They are
            given to the schema once (see @ref MistralDataProvider::MistralSchema "MistralSchema"), so that every
            field the schema describes, at every depth of a recursive filter and in every message of a
            conversation, shows the same labels; an overlay labels one location only.
        */
        const hash<string, hash<string, string>> EnumDisplayNames = {
            "#/components/schemas/ListSortDirection": {
                "list_sort_direction_unspecified": "Default", "list_sort_direction_asc": "Ascending",
                "list_sort_direction_desc": "Descending",
            },
            "#/components/schemas/ListSortField": {
                "list_sort_field_unspecified": "Default", "list_sort_field_created_at": "Created At",
                "list_sort_field_last_modified_at": "Last Modified", "list_sort_field_name": "Name",
                "list_sort_field_title": "Title",
            },
            "#/components/schemas/ShareRelation": {
                "share_relation_unspecified": "Default", "reader": "Reader", "writer": "Writer",
            },
            "#/components/schemas/RegistrySharingScope": {
                "sharing_scope_unspecified": "Default", "private": "Private", "workspace": "Workspace",
            },
            # the type of a tool call, in every message of a conversation
            "#/components/schemas/ToolTypes": {
                "function": "Function",
            },
            # the type of each kind of tool and of a connector's authorization: each alternative of a tool, and of
            # an authorization, accepts only its own type, so an overlay of the union could not label them
            "#/components/schemas/FunctionTool/properties/type": {"function": "Function"},
            "#/components/schemas/WebSearchTool/properties/type": {"web_search": "Web Search"},
            "#/components/schemas/WebSearchPremiumTool/properties/type": {"web_search_premium": "Web Search Premium"},
            "#/components/schemas/CodeInterpreterTool/properties/type": {"code_interpreter": "Code Interpreter"},
            "#/components/schemas/ImageGenerationTool/properties/type": {"image_generation": "Image Generation"},
            "#/components/schemas/DocumentLibraryTool/properties/type": {"document_library": "Document Library"},
            "#/components/schemas/CustomConnector/properties/type": {"connector": "Connector"},
            "#/components/schemas/OAuth2TokenAuth/properties/type": {"oauth2-token": "OAuth2 Token"},
            "#/components/schemas/APIKeyAuth/properties/type": {"api-key": "API Key"},
        };

        #! Account and parent-scoped resource dropdowns
        const hash<string, hash<RestSchemaReferenceDataInfo>> ReferenceData = {
            "observability-projects": <RestSchemaReferenceDataInfo>{
                "name": "observability-projects",
                "action": "list-observability-projects",
                "value_field": "slug",
                "display_field": "name",
                "desc": "Choose an existing project.",
                "records_path": ("projects", "results"),
            },
            "project-evaluations": <RestSchemaReferenceDataInfo>{
                "name": "project-evaluations",
                "action": "list-project-evaluations",
                "value_field": "slug",
                "display_field": "name",
                "desc": "Choose an existing evaluation.",
                "records_path": ("evaluations", "results"),
                "options_reference": "observability-projects",
                "reference_option": "project_slug",
            },
            "evaluation-runs": <RestSchemaReferenceDataInfo>{
                "name": "evaluation-runs",
                "action": "search-all-evaluation-runs",
                "value_field": "id",
                "display_field": "id",
                "desc": "Choose an existing evaluation run.",
                "records_path": ("runs", "results"),
            },
            "optimizations": <RestSchemaReferenceDataInfo>{
                "name": "optimizations",
                "action": "list-optimizations",
                "value_field": "id",
                "display_field": "name",
                "desc": "Choose an existing optimization.",
                "records_path": ("optimizations", "results"),
            },
            "scorers": <RestSchemaReferenceDataInfo>{
                "name": "scorers",
                "action": "list-scorers",
                "value_field": "slug",
                "display_field": "name",
                "desc": "Choose an existing scorer.",
                "records_path": ("scorers", "results"),
            },
            "audio-agents": <RestSchemaReferenceDataInfo>{
                "name": "audio-agents",
                "action": "list-audio-agents",
                "value_field": "id",
                "display_field": "name",
                "desc": "Choose an existing audio agent.",
                "records_path": ("data",),
            },
            "audio-agent-versions": <RestSchemaReferenceDataInfo>{
                "name": "audio-agent-versions",
                "action": "list-audio-agent-versions",
                "value_field": "version",
                "display_field": "version",
                "desc": "Choose an existing audio agent version.",
                "records_path": ("data",),
                "options_from": {"agent_id": "agent_id"},
            },
            "workflow-ids": <RestSchemaReferenceDataInfo>{
                "name": "workflow-ids",
                "action": "list-workflows",
                "value_field": "id",
                "display_field": "name",
                "desc": "Choose an existing workflow.",
                "records_path": ("workflows",),
            },
            "ingestion-configurations": <RestSchemaReferenceDataInfo>{
                "name": "ingestion-configurations",
                "action": "list-ingestion-pipeline-configurations",
                "value_field": "id",
                "display_field": "name",
                "desc": "Choose an existing ingestion pipeline configuration.",
            },
            "rag-deployments": <RestSchemaReferenceDataInfo>{
                "name": "rag-deployments",
                "action": "list-rag-deployments",
                "value_field": "id",
                "display_field": "name",
                "desc": "Choose an existing search index deployment.",
                "records_path": ("deployments",),
            },
            "search-indexes": <RestSchemaReferenceDataInfo>{
                "name": "search-indexes",
                "action": "list-search-indexes",
                "value_field": "name",
                "display_field": "name",
                "desc": "Choose an existing search index.",
                "records_path": ("data",),
            },
            "triggers": <RestSchemaReferenceDataInfo>{
                "name": "triggers",
                "action": "list-triggers",
                "value_field": "id",
                "display_field": "title",
                "desc": "Choose an existing trigger.",
            },
            "prompts": <RestSchemaReferenceDataInfo>{
                "name": "prompts",
                "action": "list-prompts",
                "value_field": "id",
                "display_field": "name",
                "desc": "Choose an existing prompt.",
                "records_path": ("data",),
            },
            "prompt-versions": <RestSchemaReferenceDataInfo>{
                "name": "prompt-versions",
                "action": "list-prompt-versions",
                "value_field": "version",
                "display_field": "version",
                "desc": "Choose an existing prompt version.",
                "records_path": ("data",),
                "options_from": {"prompt_id": "prompt_id"},
            },
            "skills": <RestSchemaReferenceDataInfo>{
                "name": "skills",
                "action": "list-skills",
                "value_field": "id",
                "display_field": "name",
                "desc": "Choose an existing skill.",
                "records_path": ("data",),
            },
            "skill-versions": <RestSchemaReferenceDataInfo>{
                "name": "skill-versions",
                "action": "list-skill-versions",
                "value_field": "version",
                "display_field": "version",
                "desc": "Choose an existing skill version.",
                "records_path": ("data",),
                "options_from": {"skill_id": "skill_id"},
            },
            "models": <RestSchemaReferenceDataInfo>{
                "name": "models",
                "action": "list-models",
                "value_field": "id",
                "display_field": "id",
                "desc": "Choose an existing model. Listings return one page; identifiers can also be supplied directly.",
                "records_path": ("data",),
            },
            "files": <RestSchemaReferenceDataInfo>{
                "name": "files",
                "action": "list-files",
                "value_field": "id",
                "display_field": "filename",
                "desc": "Choose an existing file. Listings return one page; identifiers can also be supplied directly.",
                "records_path": ("data",),
                "options": {
                    "page_size": 100,
                },
            },
            "agents": <RestSchemaReferenceDataInfo>{
                "name": "agents",
                "action": "list-agents",
                "value_field": "id",
                "display_field": "name",
                "desc": "Choose an existing agent. Listings return one page; identifiers can also be supplied directly.",
                "options": {
                    "page_size": 100,
                },
            },
            "conversations": <RestSchemaReferenceDataInfo>{
                "name": "conversations",
                "action": "list-conversations",
                "value_field": "id",
                "display_field": "id",
                "desc": "Choose an existing conversation. Listings return one page; identifiers can also be supplied directly.",
                "options": {
                    "page_size": 100,
                },
            },
            "libraries": <RestSchemaReferenceDataInfo>{
                "name": "libraries",
                "action": "list-libraries",
                "value_field": "id",
                "display_field": "name",
                "desc": "Choose an existing library. Listings return one page; identifiers can also be supplied directly.",
                "records_path": ("data",),
            },
            "library-documents": <RestSchemaReferenceDataInfo>{
                "name": "library-documents",
                "action": "list-library-documents",
                "value_field": "id",
                "display_field": "name",
                "desc": "Choose an existing library document. Listings return one page; identifiers can also be supplied directly.",
                "records_path": ("data",),
                "options_from": {
                    "library_id": "library_id",
                },
            },
            "batch-jobs": <RestSchemaReferenceDataInfo>{
                "name": "batch-jobs",
                "action": "list-batch-jobs",
                "value_field": "id",
                "display_field": "id",
                "desc": "Choose an existing batch job. Listings return one page; identifiers can also be supplied directly.",
                "records_path": ("data",),
            },
            "voices": <RestSchemaReferenceDataInfo>{
                "name": "voices",
                "action": "list-voices",
                "value_field": "id",
                "display_field": "name",
                "desc": "Choose an existing voice. Listings return one page; identifiers can also be supplied directly.",
                "records_path": ("items",),
            },
            "agent-versions": <RestSchemaReferenceDataInfo>{
                "name": "agent-versions",
                "action": "list-agent-versions",
                "value_field": "version",
                "display_field": "version",
                "desc": "Choose an existing agent version. Listings return one page; identifiers can also be supplied directly.",
                "options_from": {
                    "agent_id": "agent_id",
                },
            },
            "agent-aliases": <RestSchemaReferenceDataInfo>{
                "name": "agent-aliases",
                "action": "list-agent-aliases",
                "value_field": "alias",
                "display_field": "alias",
                "desc": "Choose an existing agent alias. Listings return one page; identifiers can also be supplied directly.",
                "options_from": {
                    "agent_id": "agent_id",
                },
            },
            "connectors": <RestSchemaReferenceDataInfo>{
                "name": "connectors",
                "action": "list-connectors",
                "value_field": "id",
                "display_field": "name",
                "desc": "Choose an existing connector. Listings return one page; identifiers can also be supplied directly.",
                "records_path": ("items",),
            },
            "workflow-runs": <RestSchemaReferenceDataInfo>{
                "name": "workflow-runs",
                "action": "list-workflow-runs",
                "value_field": "execution_id",
                "display_field": "workflow_name",
                "desc": "Choose an existing workflow execution. Listings return one page; identifiers can also be supplied directly.",
                "records_path": ("executions",),
            },
            "workflow-schedules": <RestSchemaReferenceDataInfo>{
                "name": "workflow-schedules",
                "action": "list-workflow-schedules",
                "value_field": "schedule_id",
                "display_field": "schedule_id",
                "desc": "Choose an existing workflow schedule. Listings return one page; identifiers can also be supplied directly.",
                "records_path": ("schedules",),
            },
            "workflow-deployments": <RestSchemaReferenceDataInfo>{
                "name": "workflow-deployments",
                "action": "list-workflow-deployments",
                "value_field": "name",
                "display_field": "name",
                "desc": "Choose an existing workflow deployment. Listings return one page; identifiers can also be supplied directly.",
                "records_path": ("deployments",),
            },
            "workflow-registrations": <RestSchemaReferenceDataInfo>{
                "name": "workflow-registrations",
                "action": "list-workflow-registrations",
                "value_field": "id",
                "display_field": "id",
                "desc": "Choose an existing workflow registration. Listings return one page; identifiers can also be supplied directly.",
                "records_path": ("workflow_registrations",),
            },
            "datasets": <RestSchemaReferenceDataInfo>{
                "name": "datasets",
                "action": "list-datasets",
                "value_field": "id",
                "display_field": "name",
                "desc": "Choose an existing dataset. Listings return one page; identifiers can also be supplied directly.",
                "records_path": ("datasets", "results"),
            },
            "dataset-records": <RestSchemaReferenceDataInfo>{
                "name": "dataset-records", "action": "list-dataset-records",
                "value_field": "id", "display_field": "id", "records_path": ("records", "results"),
                "options_reference": "datasets", "reference_option": "dataset_id",
                "desc": "Choose a record from the first page of each listed dataset. Custom IDs are also accepted.",
            },
            "conversation-entries": <RestSchemaReferenceDataInfo>{
                "name": "conversation-entries", "action": "get-conversation-history",
                "value_field": "id", "display_field": "id", "records_path": ("entries",),
                "options_from": {"conversation_id": "conversation_id"},
                "desc": "Choose an entry from the selected conversation.",
            },
            "pipelines": <RestSchemaReferenceDataInfo>{
                "name": "pipelines", "action": "list-pipelines",
                "value_field": "id", "display_field": "name", "records_path": ("pipelines", "results"),
                "desc": "Choose an evaluation pipeline. Listings return one page; identifiers can also be supplied directly.",
            },
            "log-fields": <RestSchemaReferenceDataInfo>{
                "name": "log-fields", "action": "list-log-fields",
                "value_field": "name", "display_field": "label", "records_path": ("field_definitions",),
                "desc": "Choose a log field. Other field names can also be supplied directly.",
            },
            "trace-fields": <RestSchemaReferenceDataInfo>{
                "name": "trace-fields", "action": "list-trace-fields",
                "value_field": "name", "display_field": "label", "records_path": ("field_definitions",),
                "desc": "Choose a trace field. Other field names can also be supplied directly.",
            },
            "span-fields": <RestSchemaReferenceDataInfo>{
                "name": "span-fields", "action": "list-span-fields",
                "value_field": "name", "display_field": "label", "records_path": ("field_definitions",),
                "desc": "Choose a span field. Other field names can also be supplied directly.",
            },
            "span-evaluation-fields": <RestSchemaReferenceDataInfo>{
                "name": "span-evaluation-fields", "action": "list-span-evaluation-fields",
                "value_field": "name", "display_field": "label", "records_path": ("field_definitions",),
                "desc": "Choose a span evaluation field. Other field names can also be supplied directly.",
            },
            "dataset-tasks": <RestSchemaReferenceDataInfo>{
                "name": "dataset-tasks", "action": "list-dataset-tasks",
                "value_field": "id", "display_field": "id", "records_path": ("tasks", "results"),
                "options_from": {"dataset_id": "dataset_id"},
                "desc": "Choose an import task from the selected dataset.",
            },
            "workflows": <RestSchemaReferenceDataInfo>{
                "name": "workflows", "action": "list-workflow-registrations",
                "value_field": "workflow_id", "display_field": "workflow_id",
                "records_path": ("workflow_registrations",),
                "desc": "Choose a workflow ID from the available registrations.",
            },
            "workflow-names": <RestSchemaReferenceDataInfo>{
                "name": "workflow-names", "action": "list-workflow-runs",
                "value_field": "workflow_name", "display_field": "workflow_name",
                "records_path": ("executions",),
                "desc": "Choose a workflow name from recent executions, or supply another name directly.",
            },
            "admin-users": <RestSchemaReferenceDataInfo>{
                "name": "admin-users",
                "action": "admin-list-users",
                "value_field": "uuid",
                "display_field": "email",
                "desc": "Choose an organization member. Listings return one page; identifiers can also be supplied directly.",
                "records_path": ("members",),
            },
            "admin-invitations": <RestSchemaReferenceDataInfo>{
                "name": "admin-invitations",
                "action": "admin-list-invitations",
                "value_field": "invite_uuid",
                "display_field": "email",
                "desc": "Choose a pending invitation, or supply another identifier directly.",
            },
            "admin-user-groups": <RestSchemaReferenceDataInfo>{
                "name": "admin-user-groups",
                "action": "admin-list-user-groups",
                "value_field": "uuid",
                "display_field": "name",
                "desc": "Choose a user group. Listings return one page; identifiers can also be supplied directly.",
                "records_path": ("items",),
            },
            "admin-workspaces": <RestSchemaReferenceDataInfo>{
                "name": "admin-workspaces",
                "action": "admin-list-workspaces",
                "value_field": "uuid",
                "display_field": "name",
                "desc": "Choose a workspace of the organization. Listings return one page; identifiers can also be supplied directly.",
                "records_path": ("items",),
            },
            "admin-api-keys": <RestSchemaReferenceDataInfo>{
                "name": "admin-api-keys",
                "action": "admin-list-api-keys",
                "value_field": "key_id",
                "display_field": "name",
                "desc": "Choose an API key. Listings return one page; identifiers can also be supplied directly.",
                "records_path": ("keys",),
            },
            "service-accounts": <RestSchemaReferenceDataInfo>{
                "name": "service-accounts",
                "action": "list-service-accounts",
                "value_field": "id",
                "display_field": "name",
                "desc": "Choose a service account. Listings return one page; identifiers can also be supplied directly.",
                "records_path": ("items",),
                "options": {"offset": 0, "limit": 100},
            },
            "webhooks": <RestSchemaReferenceDataInfo>{
                "name": "webhooks",
                "action": "list-webhooks",
                "value_field": "id",
                "display_field": "name",
                "desc": "Choose a webhook, or supply another identifier directly.",
            },
            "my-workspaces": <RestSchemaReferenceDataInfo>{
                "name": "my-workspaces",
                "action": "list-my-workspaces",
                "value_field": "id",
                "display_field": "name",
                "desc": "Choose one of your workspaces, or supply another identifier directly.",
                "records_path": ("workspaces",),
            },
            "my-organizations": <RestSchemaReferenceDataInfo>{
                "name": "my-organizations",
                "action": "list-my-organizations",
                "value_field": "id",
                "display_field": "name",
                "desc": "Choose one of your organizations, or supply another identifier directly.",
                "records_path": ("organizations",),
            },
            "workflow-deployment-ids": <RestSchemaReferenceDataInfo>{
                "name": "workflow-deployment-ids",
                "action": "list-workflow-deployments",
                "value_field": "id",
                "display_field": "name",
                "desc": "Choose an existing workflow deployment by its identifier. Listings return one page; identifiers can also be supplied directly.",
                "records_path": ("deployments",),
            },
        };

        #! Stable action contracts checked against the vendored schema
        const list<hash<RestSchemaActionInfo>> Manifest = (
            <RestSchemaActionInfo>{
                "action": "list-models",
                "method": "GET",
                "path": "/v1/models",
                "operation_id": "list_models_v1_models_get",
                "display_name": "List models",
                "short_desc": "List Models",
                "desc": "List Models. Discover available models and manage custom models.",
                "groups": ("Models",),
                "fields": {
                    "provider": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Provider",
                        "short_desc": "Provider",
                        "desc": "The `provider` value for this operation.",
                    },
                    "model": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model",
                        "short_desc": "Model",
                        "desc": "The `model` value for this operation.",
                        "preselected": True,
                        "ref_data": "models",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-model",
                "method": "GET",
                "path": "/v1/models/{model_id}",
                "operation_id": "retrieve_model_v1_models__model_id__get",
                "display_name": "Get Model",
                "short_desc": "Get Model",
                "desc": "Retrieve Model. Discover available models and manage custom models.",
                "groups": ("Models",),
                "fields": {
                    "model_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model ID",
                        "short_desc": "Model ID",
                        "desc": "The `model_id` value for this operation.",
                        "ref_data": "models",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-model",
                "method": "DELETE",
                "path": "/v1/models/{model_id}",
                "operation_id": "delete_model_v1_models__model_id__delete",
                "display_name": "Delete Model",
                "short_desc": "Delete Model",
                "desc": "Delete Model. Discover available models and manage custom models.\n\n**Deletion:** This operation can permanently remove the selected resource or access.",
                "groups": ("Models",),
                "fields": {
                    "model_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model ID",
                        "short_desc": "Model ID",
                        "desc": "The `model_id` value for this operation.",
                        "ref_data": "models",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-conversation",
                "method": "POST",
                "path": "/v1/conversations",
                "operation_id": "agents_api_v1_conversations_start",
                "display_name": "Create conversation",
                "short_desc": "Create Conversation",
                "desc": "Create a conversation and append entries to it. Build and continue conversations with persistent context.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Conversations",),
                "fields": {
                    "inputs": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Inputs",
                        "short_desc": "Inputs",
                        "desc": "The `inputs` value for this operation.",
                    },
                    "stream": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "store": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Store",
                        "short_desc": "Store",
                        "desc": "The `store` value for this operation.",
                    },
                    "handoff_execution": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Handoff Execution",
                        "short_desc": "Handoff Execution",
                        "desc": "The `handoff_execution` value for this operation.",
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "client",
                                "display_name": "Client",
                            },
                            <AllowedValueInfo>{
                                "value": "server",
                                "display_name": "Server",
                            },
                        ),
                    },
                    "instructions": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Instructions",
                        "short_desc": "Instructions",
                        "desc": "The `instructions` value for this operation.",
                    },
                    "tools": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Tools",
                        "short_desc": "Tools",
                        "desc": "The `tools` value for this operation.",
                        "fields": {
                            "library_ids": <RestSchemaFieldOverlayInfo>{
                                "element_ref_data": "libraries",
                                "supports_custom_values": True,
                            },
                            "connector_id": <RestSchemaFieldOverlayInfo>{
                                "ref_data": "connectors",
                                "supports_custom_values": True,
                            },
                            "authorization": <RestSchemaFieldOverlayInfo>{
                                "sensitive": True,
                            },
                        },
                    },
                    "completion_args": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Completion Args",
                        "short_desc": "White-listed arguments from the completion API",
                        "fields": {
                            "response_format": <RestSchemaFieldOverlayInfo>{
                                "fields": {
                                    "type": <RestSchemaFieldOverlayInfo>{
                                        "allowed_values": (
                                            <AllowedValueInfo>{
                                                "value": "text",
                                                "display_name": "Text",
                                            },
                                            <AllowedValueInfo>{
                                                "value": "json_object",
                                                "display_name": "Json Object",
                                            },
                                            <AllowedValueInfo>{
                                                "value": "json_schema",
                                                "display_name": "Json Schema",
                                            },
                                        ),
                                    },
                                },
                            },
                            "tool_choice": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (
                                    <AllowedValueInfo>{
                                        "value": "auto",
                                        "display_name": "Auto",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "none",
                                        "display_name": "None",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "any",
                                        "display_name": "Any",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "required",
                                        "display_name": "Required",
                                    },
                                ),
                            },
                            "reasoning_effort": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (
                                    <AllowedValueInfo>{
                                        "value": "high",
                                        "display_name": "High",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "none",
                                        "display_name": "None",
                                    },
                                ),
                            },
                        },
                    },
                    "guardrails": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Guardrails",
                        "short_desc": "Guardrails",
                        "desc": "The `guardrails` value for this operation.",
                        "fields": {
                            "moderation_llm_v1": <RestSchemaFieldOverlayInfo>{
                                "fields": {
                                    "action": <RestSchemaFieldOverlayInfo>{
                                        "allowed_values": (
                                            <AllowedValueInfo>{
                                                "value": "none",
                                                "display_name": "None",
                                            },
                                            <AllowedValueInfo>{
                                                "value": "block",
                                                "display_name": "Block",
                                            },
                                        ),
                                    },
                                },
                            },
                            "moderation_llm_v2": <RestSchemaFieldOverlayInfo>{
                                "fields": {
                                    "action": <RestSchemaFieldOverlayInfo>{
                                        "allowed_values": (
                                            <AllowedValueInfo>{
                                                "value": "none",
                                                "display_name": "None",
                                            },
                                            <AllowedValueInfo>{
                                                "value": "block",
                                                "display_name": "Block",
                                            },
                                        ),
                                    },
                                },
                            },
                        },
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "Name",
                        "desc": "The `name` value for this operation.",
                        "preselected": True,
                    },
                    "description": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Description",
                        "short_desc": "Description",
                        "desc": "The `description` value for this operation.",
                    },
                    "metadata": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Metadata",
                        "short_desc": "Custom type for metadata with embedded validation.",
                    },
                    "agent_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Agent ID",
                        "short_desc": "Agent ID",
                        "desc": "The `agent_id` value for this operation.",
                        "ref_data": "agents",
                        "supports_custom_values": True,
                    },
                    "agent_version": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Agent Version",
                        "short_desc": "Agent Version",
                        "desc": "The `agent_version` value for this operation.",
                    },
                    "model": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model",
                        "short_desc": "Model",
                        "desc": "The `model` value for this operation.",
                        "preselected": True,
                        "ref_data": "models",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-conversations",
                "method": "GET",
                "path": "/v1/conversations",
                "operation_id": "agents_api_v1_conversations_list",
                "display_name": "List conversations",
                "short_desc": "List Conversations",
                "desc": "List all created conversations. Build and continue conversations with persistent context.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Conversations",),
                "fields": {
                    "page": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page",
                        "short_desc": "Page",
                        "desc": "The `page` value for this operation.",
                        "preselected": True,
                    },
                    "page_size": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "short_desc": "Page Size",
                        "desc": "The `page_size` value for this operation.",
                        "preselected": True,
                    },
                    "metadata": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Metadata",
                        "short_desc": "Metadata",
                        "desc": "The `metadata` value for this operation.",
                    },
                },
                "response_fields": {
                    "tools": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "authorization": <RestSchemaFieldOverlayInfo>{
                                "sensitive": True,
                            },
                        },
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-conversation",
                "method": "GET",
                "path": "/v1/conversations/{conversation_id}",
                "operation_id": "agents_api_v1_conversations_get",
                "display_name": "Get Conversation",
                "short_desc": "Get Conversation",
                "desc": "Retrieve a conversation information. Build and continue conversations with persistent context.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Conversations",),
                "fields": {
                    "conversation_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Conversation ID",
                        "short_desc": "ID of the conversation from which we are fetching metadata.",
                        "ref_data": "conversations",
                        "supports_custom_values": True,
                    },
                },
                "response_fields": {
                    "tools": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "authorization": <RestSchemaFieldOverlayInfo>{
                                "sensitive": True,
                            },
                        },
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-conversation",
                "method": "DELETE",
                "path": "/v1/conversations/{conversation_id}",
                "operation_id": "agents_api_v1_conversations_delete",
                "display_name": "Delete Conversation",
                "short_desc": "Delete Conversation",
                "desc": "Delete a conversation. Build and continue conversations with persistent context.\n\n**Public preview:** Availability depends on your account and permissions.\n\n**Deletion:** This operation can permanently remove the selected resource or access.",
                "groups": ("Conversations",),
                "fields": {
                    "conversation_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Conversation ID",
                        "short_desc": "ID of the conversation from which we are fetching metadata.",
                        "ref_data": "conversations",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "append-conversation",
                "method": "POST",
                "path": "/v1/conversations/{conversation_id}",
                "operation_id": "agents_api_v1_conversations_append",
                "display_name": "Append Conversation",
                "short_desc": "Append Conversation",
                "desc": "Append new entries to an existing conversation. Build and continue conversations with persistent context.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Conversations",),
                "fields": {
                    "inputs": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Inputs",
                        "short_desc": "Inputs",
                        "desc": "The `inputs` value for this operation.",
                    },
                    "stream": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "store": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Store",
                        "short_desc": "Whether to store the results into our servers or not.",
                    },
                    "handoff_execution": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Handoff Execution",
                        "short_desc": "Handoff Execution",
                        "desc": "The `handoff_execution` value for this operation.",
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "client",
                                "display_name": "Client",
                            },
                            <AllowedValueInfo>{
                                "value": "server",
                                "display_name": "Server",
                            },
                        ),
                    },
                    "completion_args": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Completion Args",
                        "short_desc": "Completion arguments that will be used to generate assistant responses",
                        "fields": {
                            "response_format": <RestSchemaFieldOverlayInfo>{
                                "fields": {
                                    "type": <RestSchemaFieldOverlayInfo>{
                                        "allowed_values": (
                                            <AllowedValueInfo>{
                                                "value": "text",
                                                "display_name": "Text",
                                            },
                                            <AllowedValueInfo>{
                                                "value": "json_object",
                                                "display_name": "Json Object",
                                            },
                                            <AllowedValueInfo>{
                                                "value": "json_schema",
                                                "display_name": "Json Schema",
                                            },
                                        ),
                                    },
                                },
                            },
                            "tool_choice": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (
                                    <AllowedValueInfo>{
                                        "value": "auto",
                                        "display_name": "Auto",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "none",
                                        "display_name": "None",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "any",
                                        "display_name": "Any",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "required",
                                        "display_name": "Required",
                                    },
                                ),
                            },
                            "reasoning_effort": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (
                                    <AllowedValueInfo>{
                                        "value": "high",
                                        "display_name": "High",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "none",
                                        "display_name": "None",
                                    },
                                ),
                            },
                        },
                    },
                    "tool_confirmations": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Tool Confirmations",
                        "short_desc": "Tool Confirmations",
                        "desc": "The `tool_confirmations` value for this operation.",
                        "fields": {
                            "confirmation": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (
                                    <AllowedValueInfo>{
                                        "value": "allow",
                                        "display_name": "Allow",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "deny",
                                        "display_name": "Deny",
                                    },
                                ),
                            },
                        },
                    },
                    "conversation_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Conversation ID",
                        "short_desc": "ID of the conversation to which we append entries.",
                        "ref_data": "conversations",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-conversation-history",
                "method": "GET",
                "path": "/v1/conversations/{conversation_id}/history",
                "operation_id": "agents_api_v1_conversations_history",
                "display_name": "Get Conversation History",
                "short_desc": "Get Conversation History",
                "desc": "Retrieve all entries in a conversation. Build and continue conversations with persistent context.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Conversations",),
                "fields": {
                    "conversation_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Conversation ID",
                        "short_desc": "ID of the conversation from which we are fetching entries.",
                        "ref_data": "conversations",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-conversation-messages",
                "method": "GET",
                "path": "/v1/conversations/{conversation_id}/messages",
                "operation_id": "agents_api_v1_conversations_messages",
                "display_name": "Get Conversation Messages",
                "short_desc": "Get Conversation Messages",
                "desc": "Retrieve all messages in a conversation. Build and continue conversations with persistent context.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Conversations",),
                "fields": {
                    "conversation_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Conversation ID",
                        "short_desc": "ID of the conversation from which we are fetching messages.",
                        "ref_data": "conversations",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "restart-conversation",
                "method": "POST",
                "path": "/v1/conversations/{conversation_id}/restart",
                "operation_id": "agents_api_v1_conversations_restart",
                "display_name": "Restart Conversation",
                "short_desc": "Restart Conversation",
                "desc": "Restart a conversation starting from a given entry. Build and continue conversations with persistent context.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Conversations",),
                "fields": {
                    "inputs": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Inputs",
                        "short_desc": "Inputs",
                        "desc": "The `inputs` value for this operation.",
                    },
                    "stream": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "store": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Store",
                        "short_desc": "Whether to store the results into our servers or not.",
                    },
                    "handoff_execution": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Handoff Execution",
                        "short_desc": "Handoff Execution",
                        "desc": "The `handoff_execution` value for this operation.",
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "client",
                                "display_name": "Client",
                            },
                            <AllowedValueInfo>{
                                "value": "server",
                                "display_name": "Server",
                            },
                        ),
                    },
                    "completion_args": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Completion Args",
                        "short_desc": "Completion arguments that will be used to generate assistant responses",
                        "fields": {
                            "response_format": <RestSchemaFieldOverlayInfo>{
                                "fields": {
                                    "type": <RestSchemaFieldOverlayInfo>{
                                        "allowed_values": (
                                            <AllowedValueInfo>{
                                                "value": "text",
                                                "display_name": "Text",
                                            },
                                            <AllowedValueInfo>{
                                                "value": "json_object",
                                                "display_name": "Json Object",
                                            },
                                            <AllowedValueInfo>{
                                                "value": "json_schema",
                                                "display_name": "Json Schema",
                                            },
                                        ),
                                    },
                                },
                            },
                            "tool_choice": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (
                                    <AllowedValueInfo>{
                                        "value": "auto",
                                        "display_name": "Auto",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "none",
                                        "display_name": "None",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "any",
                                        "display_name": "Any",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "required",
                                        "display_name": "Required",
                                    },
                                ),
                            },
                            "reasoning_effort": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (
                                    <AllowedValueInfo>{
                                        "value": "high",
                                        "display_name": "High",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "none",
                                        "display_name": "None",
                                    },
                                ),
                            },
                        },
                    },
                    "guardrails": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Guardrails",
                        "short_desc": "Guardrails",
                        "desc": "The `guardrails` value for this operation.",
                        "fields": {
                            "moderation_llm_v1": <RestSchemaFieldOverlayInfo>{
                                "fields": {
                                    "action": <RestSchemaFieldOverlayInfo>{
                                        "allowed_values": (
                                            <AllowedValueInfo>{
                                                "value": "none",
                                                "display_name": "None",
                                            },
                                            <AllowedValueInfo>{
                                                "value": "block",
                                                "display_name": "Block",
                                            },
                                        ),
                                    },
                                },
                            },
                            "moderation_llm_v2": <RestSchemaFieldOverlayInfo>{
                                "fields": {
                                    "action": <RestSchemaFieldOverlayInfo>{
                                        "allowed_values": (
                                            <AllowedValueInfo>{
                                                "value": "none",
                                                "display_name": "None",
                                            },
                                            <AllowedValueInfo>{
                                                "value": "block",
                                                "display_name": "Block",
                                            },
                                        ),
                                    },
                                },
                            },
                        },
                    },
                    "metadata": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Metadata",
                        "short_desc": "Custom metadata for the conversation.",
                    },
                    "from_entry_id": <RestSchemaFieldOverlayInfo>{
                        "depends_on": ("conversation_id",),
                        "display_name": "From Entry ID",
                        "short_desc": "From Entry ID",
                        "desc": "The `from_entry_id` value for this operation.",
                        "ref_data": "conversation-entries",
                        "supports_custom_values": True,
                    },
                    "agent_version": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Agent Version",
                        "short_desc": "Specific version of the agent to use when restarting",
                    },
                    "conversation_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Conversation ID",
                        "short_desc": "ID of the original conversation which is being restarted.",
                        "ref_data": "conversations",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-agent",
                "method": "POST",
                "path": "/v1/agents",
                "operation_id": "agents_api_v1_agents_create",
                "display_name": "Create agent",
                "short_desc": "Create Agent",
                "desc": "Create a agent that can be used within a conversation. Manage reusable assistants with instructions and tools.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Agents",),
                "fields": {
                    "instructions": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Instructions",
                        "short_desc": "Instruction prompt the model will follow during the conversation.",
                    },
                    "tools": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Tools",
                        "short_desc": "List of tools which are available to the model during the conversation.",
                        "fields": {
                            "library_ids": <RestSchemaFieldOverlayInfo>{
                                "element_ref_data": "libraries",
                                "supports_custom_values": True,
                            },
                            "connector_id": <RestSchemaFieldOverlayInfo>{
                                "ref_data": "connectors",
                                "supports_custom_values": True,
                            },
                            "authorization": <RestSchemaFieldOverlayInfo>{
                                "sensitive": True,
                            },
                        },
                    },
                    "completion_args": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Completion Args",
                        "short_desc": "Completion arguments that will be used to generate assistant responses",
                        "fields": {
                            "response_format": <RestSchemaFieldOverlayInfo>{
                                "fields": {
                                    "type": <RestSchemaFieldOverlayInfo>{
                                        "allowed_values": (
                                            <AllowedValueInfo>{
                                                "value": "text",
                                                "display_name": "Text",
                                            },
                                            <AllowedValueInfo>{
                                                "value": "json_object",
                                                "display_name": "Json Object",
                                            },
                                            <AllowedValueInfo>{
                                                "value": "json_schema",
                                                "display_name": "Json Schema",
                                            },
                                        ),
                                    },
                                },
                            },
                            "tool_choice": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (
                                    <AllowedValueInfo>{
                                        "value": "auto",
                                        "display_name": "Auto",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "none",
                                        "display_name": "None",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "any",
                                        "display_name": "Any",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "required",
                                        "display_name": "Required",
                                    },
                                ),
                            },
                            "reasoning_effort": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (
                                    <AllowedValueInfo>{
                                        "value": "high",
                                        "display_name": "High",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "none",
                                        "display_name": "None",
                                    },
                                ),
                            },
                        },
                    },
                    "guardrails": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Guardrails",
                        "short_desc": "Guardrails",
                        "desc": "The `guardrails` value for this operation.",
                        "fields": {
                            "moderation_llm_v1": <RestSchemaFieldOverlayInfo>{
                                "fields": {
                                    "action": <RestSchemaFieldOverlayInfo>{
                                        "allowed_values": (
                                            <AllowedValueInfo>{
                                                "value": "none",
                                                "display_name": "None",
                                            },
                                            <AllowedValueInfo>{
                                                "value": "block",
                                                "display_name": "Block",
                                            },
                                        ),
                                    },
                                },
                            },
                            "moderation_llm_v2": <RestSchemaFieldOverlayInfo>{
                                "fields": {
                                    "action": <RestSchemaFieldOverlayInfo>{
                                        "allowed_values": (
                                            <AllowedValueInfo>{
                                                "value": "none",
                                                "display_name": "None",
                                            },
                                            <AllowedValueInfo>{
                                                "value": "block",
                                                "display_name": "Block",
                                            },
                                        ),
                                    },
                                },
                            },
                        },
                    },
                    "model": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model",
                        "short_desc": "Model",
                        "desc": "The `model` value for this operation.",
                        "preselected": True,
                        "ref_data": "models",
                        "supports_custom_values": True,
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "Name",
                        "desc": "The `name` value for this operation.",
                        "preselected": True,
                    },
                    "description": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Description",
                        "short_desc": "Description",
                        "desc": "The `description` value for this operation.",
                    },
                    "handoffs": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Handoffs",
                        "short_desc": "Handoffs",
                        "desc": "The `handoffs` value for this operation.",
                    },
                    "metadata": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Metadata",
                        "short_desc": "Custom type for metadata with embedded validation.",
                    },
                    "version_message": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Version Message",
                        "short_desc": "Version Message",
                        "desc": "The `version_message` value for this operation.",
                    },
                },
                "response_fields": {
                    "tools": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "authorization": <RestSchemaFieldOverlayInfo>{
                                "sensitive": True,
                            },
                        },
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-agents",
                "method": "GET",
                "path": "/v1/agents",
                "operation_id": "agents_api_v1_agents_list",
                "display_name": "List agents",
                "short_desc": "List Agents",
                "desc": "List agent entities. Manage reusable assistants with instructions and tools.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Agents",),
                "fields": {
                    "page": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page",
                        "short_desc": "Page number (0-indexed)",
                        "preselected": True,
                    },
                    "page_size": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "short_desc": "Number of agents per page",
                        "preselected": True,
                    },
                    "deployment_chat": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Deployment Chat",
                        "short_desc": "Deployment Chat",
                        "desc": "The `deployment_chat` value for this operation.",
                    },
                    "sources": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Sources",
                        "short_desc": "Sources",
                        "desc": "The `sources` value for this operation.",
                        "element_allowed_values": (
                            <AllowedValueInfo>{
                                "value": "api",
                                "display_name": "Api",
                            },
                            <AllowedValueInfo>{
                                "value": "playground",
                                "display_name": "Playground",
                            },
                            <AllowedValueInfo>{
                                "value": "agent_builder_v1",
                                "display_name": "Agent Builder V1",
                            },
                        ),
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "Filter by agent name",
                        "preselected": True,
                    },
                    "search": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Search",
                        "short_desc": "Search agents by name or ID",
                        "preselected": True,
                    },
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Id",
                        "short_desc": "Id",
                        "desc": "The `id` value for this operation.",
                    },
                    "metadata": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Metadata",
                        "short_desc": "Metadata",
                        "desc": "The `metadata` value for this operation.",
                    },
                },
                "response_fields": {
                    "tools": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "authorization": <RestSchemaFieldOverlayInfo>{
                                "sensitive": True,
                            },
                        },
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-agent",
                "method": "GET",
                "path": "/v1/agents/{agent_id}",
                "operation_id": "agents_api_v1_agents_get",
                "display_name": "Get Agent",
                "short_desc": "Get Agent",
                "desc": "Retrieve an agent entity. Manage reusable assistants with instructions and tools.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Agents",),
                "fields": {
                    "agent_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Agent ID",
                        "short_desc": "Agent ID",
                        "desc": "The `agent_id` value for this operation.",
                        "ref_data": "agents",
                        "supports_custom_values": True,
                    },
                    "agent_version": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Agent Version",
                        "short_desc": "Agent Version",
                        "desc": "The `agent_version` value for this operation.",
                    },
                },
                "response_fields": {
                    "tools": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "authorization": <RestSchemaFieldOverlayInfo>{
                                "sensitive": True,
                            },
                        },
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-agent",
                "method": "PATCH",
                "path": "/v1/agents/{agent_id}",
                "operation_id": "agents_api_v1_agents_update",
                "display_name": "Update Agent",
                "short_desc": "Update Agent",
                "desc": "Update an agent entity. Manage reusable assistants with instructions and tools.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Agents",),
                "fields": {
                    "instructions": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Instructions",
                        "short_desc": "Instruction prompt the model will follow during the conversation.",
                    },
                    "tools": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Tools",
                        "short_desc": "List of tools which are available to the model during the conversation.",
                        "fields": {
                            "library_ids": <RestSchemaFieldOverlayInfo>{
                                "element_ref_data": "libraries",
                                "supports_custom_values": True,
                            },
                            "connector_id": <RestSchemaFieldOverlayInfo>{
                                "ref_data": "connectors",
                                "supports_custom_values": True,
                            },
                            "authorization": <RestSchemaFieldOverlayInfo>{
                                "sensitive": True,
                            },
                        },
                    },
                    "completion_args": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Completion Args",
                        "short_desc": "Completion arguments that will be used to generate assistant responses",
                        "fields": {
                            "response_format": <RestSchemaFieldOverlayInfo>{
                                "fields": {
                                    "type": <RestSchemaFieldOverlayInfo>{
                                        "allowed_values": (
                                            <AllowedValueInfo>{
                                                "value": "text",
                                                "display_name": "Text",
                                            },
                                            <AllowedValueInfo>{
                                                "value": "json_object",
                                                "display_name": "Json Object",
                                            },
                                            <AllowedValueInfo>{
                                                "value": "json_schema",
                                                "display_name": "Json Schema",
                                            },
                                        ),
                                    },
                                },
                            },
                            "tool_choice": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (
                                    <AllowedValueInfo>{
                                        "value": "auto",
                                        "display_name": "Auto",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "none",
                                        "display_name": "None",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "any",
                                        "display_name": "Any",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "required",
                                        "display_name": "Required",
                                    },
                                ),
                            },
                            "reasoning_effort": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (
                                    <AllowedValueInfo>{
                                        "value": "high",
                                        "display_name": "High",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "none",
                                        "display_name": "None",
                                    },
                                ),
                            },
                        },
                    },
                    "guardrails": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Guardrails",
                        "short_desc": "Guardrails",
                        "desc": "The `guardrails` value for this operation.",
                        "fields": {
                            "moderation_llm_v1": <RestSchemaFieldOverlayInfo>{
                                "fields": {
                                    "action": <RestSchemaFieldOverlayInfo>{
                                        "allowed_values": (
                                            <AllowedValueInfo>{
                                                "value": "none",
                                                "display_name": "None",
                                            },
                                            <AllowedValueInfo>{
                                                "value": "block",
                                                "display_name": "Block",
                                            },
                                        ),
                                    },
                                },
                            },
                            "moderation_llm_v2": <RestSchemaFieldOverlayInfo>{
                                "fields": {
                                    "action": <RestSchemaFieldOverlayInfo>{
                                        "allowed_values": (
                                            <AllowedValueInfo>{
                                                "value": "none",
                                                "display_name": "None",
                                            },
                                            <AllowedValueInfo>{
                                                "value": "block",
                                                "display_name": "Block",
                                            },
                                        ),
                                    },
                                },
                            },
                        },
                    },
                    "model": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model",
                        "short_desc": "Model",
                        "desc": "The `model` value for this operation.",
                        "preselected": True,
                        "ref_data": "models",
                        "supports_custom_values": True,
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "Name",
                        "desc": "The `name` value for this operation.",
                        "preselected": True,
                    },
                    "description": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Description",
                        "short_desc": "Description",
                        "desc": "The `description` value for this operation.",
                    },
                    "handoffs": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Handoffs",
                        "short_desc": "Handoffs",
                        "desc": "The `handoffs` value for this operation.",
                    },
                    "deployment_chat": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Deployment Chat",
                        "short_desc": "Deployment Chat",
                        "desc": "The `deployment_chat` value for this operation.",
                    },
                    "metadata": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Metadata",
                        "short_desc": "Custom type for metadata with embedded validation.",
                    },
                    "version_message": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Version Message",
                        "short_desc": "Version Message",
                        "desc": "The `version_message` value for this operation.",
                    },
                    "agent_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Agent ID",
                        "short_desc": "Agent ID",
                        "desc": "The `agent_id` value for this operation.",
                        "ref_data": "agents",
                        "supports_custom_values": True,
                    },
                },
                "response_fields": {
                    "tools": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "authorization": <RestSchemaFieldOverlayInfo>{
                                "sensitive": True,
                            },
                        },
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-agent",
                "method": "DELETE",
                "path": "/v1/agents/{agent_id}",
                "operation_id": "agents_api_v1_agents_delete",
                "display_name": "Delete Agent",
                "short_desc": "Delete Agent",
                "desc": "Delete an agent entity. Manage reusable assistants with instructions and tools.\n\n**Public preview:** Availability depends on your account and permissions.\n\n**Deletion:** This operation can permanently remove the selected resource or access.",
                "groups": ("Agents",),
                "fields": {
                    "agent_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Agent ID",
                        "short_desc": "Agent ID",
                        "desc": "The `agent_id` value for this operation.",
                        "ref_data": "agents",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-agent-pages",
                "method": "GET",
                "path": "/v1/agents/pages",
                "operation_id": "agents_api_v1_agents_list_pages",
                "display_name": "List agent pages",
                "short_desc": "List Agent Pages",
                "desc": "List agent entities, cursor-paginated. Manage reusable assistants with instructions and tools.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Agents",),
                "fields": {
                    "page_size": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "short_desc": "Number of agents per page",
                        "preselected": True,
                    },
                    "deployment_chat": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Deployment Chat",
                        "short_desc": "Deployment Chat",
                        "desc": "The `deployment_chat` value for this operation.",
                    },
                    "sources": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Sources",
                        "short_desc": "Sources",
                        "desc": "The `sources` value for this operation.",
                        "element_allowed_values": (
                            <AllowedValueInfo>{
                                "value": "api",
                                "display_name": "Api",
                            },
                            <AllowedValueInfo>{
                                "value": "playground",
                                "display_name": "Playground",
                            },
                            <AllowedValueInfo>{
                                "value": "agent_builder_v1",
                                "display_name": "Agent Builder V1",
                            },
                        ),
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "Filter by agent name",
                        "preselected": True,
                    },
                    "search": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Search",
                        "short_desc": "Search agents by name or ID",
                        "preselected": True,
                    },
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Id",
                        "short_desc": "Id",
                        "desc": "The `id` value for this operation.",
                    },
                    "metadata": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Metadata",
                        "short_desc": "Metadata",
                        "desc": "The `metadata` value for this operation.",
                    },
                    "page_token": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Token",
                        "short_desc": "Opaque cursor from a previous response's next_page_token",
                    },
                },
                "response_fields": {
                    "data": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "tools": <RestSchemaFieldOverlayInfo>{
                                "fields": {
                                    "authorization": <RestSchemaFieldOverlayInfo>{
                                        "sensitive": True,
                                    },
                                },
                            },
                        },
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-agent-version",
                "method": "PATCH",
                "path": "/v1/agents/{agent_id}/version",
                "operation_id": "agents_api_v1_agents_update_version",
                "display_name": "Update Agent Version",
                "short_desc": "Update Agent Version",
                "desc": "Update an agent version. Manage reusable assistants with instructions and tools.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Agents",),
                "fields": {
                    "agent_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Agent ID",
                        "short_desc": "Agent ID",
                        "desc": "The `agent_id` value for this operation.",
                        "ref_data": "agents",
                        "supports_custom_values": True,
                        "has_dependents": True,
                        "on_change": ("refetch",),
                    },
                    "version": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Version",
                        "short_desc": "Version",
                        "desc": "The `version` value for this operation.",
                        "ref_data": "agent-versions",
                        "depends_on": ("agent_id",),
                        "supports_custom_values": True,
                    },
                },
                "response_fields": {
                    "tools": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "authorization": <RestSchemaFieldOverlayInfo>{
                                "sensitive": True,
                            },
                        },
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-agent-versions",
                "method": "GET",
                "path": "/v1/agents/{agent_id}/versions",
                "operation_id": "agents_api_v1_agents_list_versions",
                "display_name": "List Agent Versions",
                "short_desc": "List Agent Versions",
                "desc": "List all versions of an agent. Manage reusable assistants with instructions and tools.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Agents",),
                "fields": {
                    "agent_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Agent ID",
                        "short_desc": "Agent ID",
                        "desc": "The `agent_id` value for this operation.",
                        "ref_data": "agents",
                        "supports_custom_values": True,
                    },
                    "page": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page",
                        "short_desc": "Page number (0-indexed)",
                        "preselected": True,
                    },
                    "page_size": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "short_desc": "Number of versions per page",
                        "preselected": True,
                    },
                },
                "response_fields": {
                    "tools": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "authorization": <RestSchemaFieldOverlayInfo>{
                                "sensitive": True,
                            },
                        },
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-agent-version",
                "method": "GET",
                "path": "/v1/agents/{agent_id}/versions/{version}",
                "operation_id": "agents_api_v1_agents_get_version",
                "display_name": "Get Agent Version",
                "short_desc": "Get Agent Version",
                "desc": "Retrieve a specific version of an agent. Manage reusable assistants with instructions and tools.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Agents",),
                "fields": {
                    "agent_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Agent ID",
                        "short_desc": "Agent ID",
                        "desc": "The `agent_id` value for this operation.",
                        "ref_data": "agents",
                        "supports_custom_values": True,
                        "has_dependents": True,
                        "on_change": ("refetch",),
                    },
                    "version": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Version",
                        "short_desc": "Version",
                        "desc": "The `version` value for this operation.",
                        "ref_data": "agent-versions",
                        "depends_on": ("agent_id",),
                        "supports_custom_values": True,
                    },
                },
                "response_fields": {
                    "tools": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "authorization": <RestSchemaFieldOverlayInfo>{
                                "sensitive": True,
                            },
                        },
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "set-agent-alias",
                "method": "PUT",
                "path": "/v1/agents/{agent_id}/aliases",
                "operation_id": "agents_api_v1_agents_create_or_update_alias",
                "display_name": "Set Agent Alias",
                "short_desc": "Set Agent Alias",
                "desc": "Create or update an agent version alias. Manage reusable assistants with instructions and tools.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Agents",),
                "fields": {
                    "agent_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Agent ID",
                        "short_desc": "Agent ID",
                        "desc": "The `agent_id` value for this operation.",
                        "ref_data": "agents",
                        "supports_custom_values": True,
                        "has_dependents": True,
                        "on_change": ("refetch",),
                    },
                    "alias": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Alias",
                        "short_desc": "Alias",
                        "desc": "The `alias` value for this operation.",
                        "ref_data": "agent-aliases",
                        "depends_on": ("agent_id",),
                        "supports_custom_values": True,
                    },
                    "version": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Version",
                        "short_desc": "Version",
                        "desc": "The `version` value for this operation.",
                        "ref_data": "agent-versions",
                        "depends_on": ("agent_id",),
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-agent-aliases",
                "method": "GET",
                "path": "/v1/agents/{agent_id}/aliases",
                "operation_id": "agents_api_v1_agents_list_version_aliases",
                "display_name": "List Agent Aliases",
                "short_desc": "List Agent Aliases",
                "desc": "List all aliases for an agent. Manage reusable assistants with instructions and tools.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Agents",),
                "fields": {
                    "agent_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Agent ID",
                        "short_desc": "Agent ID",
                        "desc": "The `agent_id` value for this operation.",
                        "ref_data": "agents",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-agent-alias",
                "method": "DELETE",
                "path": "/v1/agents/{agent_id}/aliases",
                "operation_id": "agents_api_v1_agents_delete_alias",
                "display_name": "Delete Agent Alias",
                "short_desc": "Delete Agent Alias",
                "desc": "Delete an agent version alias. Manage reusable assistants with instructions and tools.\n\n**Public preview:** Availability depends on your account and permissions.\n\n**Deletion:** This operation can permanently remove the selected resource or access.",
                "groups": ("Agents",),
                "fields": {
                    "agent_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Agent ID",
                        "short_desc": "Agent ID",
                        "desc": "The `agent_id` value for this operation.",
                        "ref_data": "agents",
                        "supports_custom_values": True,
                        "has_dependents": True,
                        "on_change": ("refetch",),
                    },
                    "alias": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Alias",
                        "short_desc": "Alias",
                        "desc": "The `alias` value for this operation.",
                        "ref_data": "agent-aliases",
                        "depends_on": ("agent_id",),
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "upload-file",
                "method": "POST",
                "path": "/v1/files",
                "operation_id": "files_api_routes_upload_file",
                "display_name": "Upload file",
                "short_desc": "Upload File",
                "desc": "Upload File. Manage files for document processing, batch inference and audio tasks.",
                "groups": ("Files",),
                "fields": {
                    "expiry": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Expiry",
                        "short_desc": "Expiry",
                        "desc": "The `expiry` value for this operation.",
                    },
                    "visibility": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Visibility",
                        "short_desc": "Visibility",
                        "desc": "The `visibility` value for this operation.",
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "workspace",
                                "display_name": "Workspace",
                            },
                            <AllowedValueInfo>{
                                "value": "user",
                                "display_name": "User",
                            },
                        ),
                    },
                    "purpose": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Purpose",
                        "short_desc": "Purpose",
                        "desc": "The `purpose` value for this operation.",
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "fine-tune",
                                "display_name": "Fine Tune",
                            },
                            <AllowedValueInfo>{
                                "value": "batch",
                                "display_name": "Batch",
                            },
                            <AllowedValueInfo>{
                                "value": "ocr",
                                "display_name": "Ocr",
                            },
                        ),
                    },
                    "file": <RestSchemaFieldOverlayInfo>{
                        "display_name": "File",
                        "short_desc": "The File object (not file name) to be uploaded.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-files",
                "method": "GET",
                "path": "/v1/files",
                "operation_id": "files_api_routes_list_files",
                "display_name": "List files",
                "short_desc": "List Files",
                "desc": "List Files. Manage files for document processing, batch inference and audio tasks.",
                "groups": ("Files",),
                "fields": {
                    "page": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page",
                        "short_desc": "Page",
                        "desc": "The `page` value for this operation.",
                        "preselected": True,
                    },
                    "page_size": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "short_desc": "Page Size",
                        "desc": "The `page_size` value for this operation.",
                        "preselected": True,
                    },
                    "include_total": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Include Total",
                        "short_desc": "Include Total",
                        "desc": "The `include_total` value for this operation.",
                    },
                    "sample_type": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Sample Type",
                        "short_desc": "Sample Type",
                        "desc": "The `sample_type` value for this operation.",
                        "element_allowed_values": (
                            <AllowedValueInfo>{
                                "value": "pretrain",
                                "display_name": "Pretrain",
                            },
                            <AllowedValueInfo>{
                                "value": "instruct",
                                "display_name": "Instruct",
                            },
                            <AllowedValueInfo>{
                                "value": "batch_request",
                                "display_name": "Batch Request",
                            },
                            <AllowedValueInfo>{
                                "value": "batch_result",
                                "display_name": "Batch Result",
                            },
                            <AllowedValueInfo>{
                                "value": "batch_error",
                                "display_name": "Batch Error",
                            },
                        ),
                    },
                    "source": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Source",
                        "short_desc": "Source",
                        "desc": "The `source` value for this operation.",
                        "element_allowed_values": (
                            <AllowedValueInfo>{
                                "value": "upload",
                                "display_name": "Upload",
                            },
                            <AllowedValueInfo>{
                                "value": "repository",
                                "display_name": "Repository",
                            },
                            <AllowedValueInfo>{
                                "value": "mistral",
                                "display_name": "Mistral",
                            },
                        ),
                    },
                    "search": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Search",
                        "short_desc": "Search",
                        "desc": "The `search` value for this operation.",
                        "preselected": True,
                    },
                    "purpose": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Purpose",
                        "short_desc": "Purpose",
                        "desc": "The `purpose` value for this operation.",
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "fine-tune",
                                "display_name": "Fine Tune",
                            },
                            <AllowedValueInfo>{
                                "value": "batch",
                                "display_name": "Batch",
                            },
                            <AllowedValueInfo>{
                                "value": "ocr",
                                "display_name": "Ocr",
                            },
                        ),
                    },
                    "mimetypes": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Mimetypes",
                        "short_desc": "Mimetypes",
                        "desc": "The `mimetypes` value for this operation.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-file",
                "method": "GET",
                "path": "/v1/files/{file_id}",
                "operation_id": "files_api_routes_retrieve_file",
                "display_name": "Get File",
                "short_desc": "Get File",
                "desc": "Retrieve File. Manage files for document processing, batch inference and audio tasks.",
                "groups": ("Files",),
                "fields": {
                    "file_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "File ID",
                        "short_desc": "File ID",
                        "desc": "The `file_id` value for this operation.",
                        "ref_data": "files",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-file",
                "method": "DELETE",
                "path": "/v1/files/{file_id}",
                "operation_id": "files_api_routes_delete_file",
                "display_name": "Delete File",
                "short_desc": "Delete File",
                "desc": "Delete File. Manage files for document processing, batch inference and audio tasks.\n\n**Deletion:** This operation can permanently remove the selected resource or access.",
                "groups": ("Files",),
                "fields": {
                    "file_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "File ID",
                        "short_desc": "File ID",
                        "desc": "The `file_id` value for this operation.",
                        "ref_data": "files",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "download-file",
                "method": "GET",
                "path": "/v1/files/{file_id}/content",
                "operation_id": "files_api_routes_download_file",
                "display_name": "Download File",
                "short_desc": "Download File",
                "desc": "Download File. Manage files for document processing, batch inference and audio tasks.",
                "groups": ("Files",),
                "fields": {
                    "file_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "File ID",
                        "short_desc": "File ID",
                        "desc": "The `file_id` value for this operation.",
                        "ref_data": "files",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-file-url",
                "method": "GET",
                "path": "/v1/files/{file_id}/url",
                "operation_id": "files_api_routes_get_signed_url",
                "display_name": "Get File URL",
                "short_desc": "Get File URL",
                "desc": "Get Signed Url. Manage files for document processing, batch inference and audio tasks.",
                "groups": ("Files",),
                "fields": {
                    "file_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "File ID",
                        "short_desc": "File ID",
                        "desc": "The `file_id` value for this operation.",
                        "ref_data": "files",
                        "supports_custom_values": True,
                    },
                    "expiry": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Expiry",
                        "short_desc": "Number of hours before the url becomes invalid",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-fine-tuned-model",
                "method": "PATCH",
                "path": "/v1/fine_tuning/models/{model_id}",
                "operation_id": "jobs_api_routes_fine_tuning_update_fine_tuned_model",
                "display_name": "Update Fine Tuned Model",
                "short_desc": "Update Fine Tuned Model",
                "desc": "Update Fine Tuned Model. Discover available models and manage custom models.",
                "groups": ("Models",),
                "fields": {
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "Name",
                        "desc": "The `name` value for this operation.",
                        "preselected": True,
                    },
                    "description": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Description",
                        "short_desc": "Description",
                        "desc": "The `description` value for this operation.",
                    },
                    "model_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model ID",
                        "short_desc": "Model ID",
                        "desc": "The `model_id` value for this operation.",
                        "ref_data": "models",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "archive-fine-tuned-model",
                "method": "POST",
                "path": "/v1/fine_tuning/models/{model_id}/archive",
                "operation_id": "jobs_api_routes_fine_tuning_archive_fine_tuned_model",
                "display_name": "Archive Fine Tuned Model",
                "short_desc": "Archive Fine Tuned Model",
                "desc": "Archive Fine Tuned Model. Discover available models and manage custom models.",
                "groups": ("Models",),
                "fields": {
                    "model_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model ID",
                        "short_desc": "Model ID",
                        "desc": "The `model_id` value for this operation.",
                        "ref_data": "models",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "unarchive-fine-tuned-model",
                "method": "DELETE",
                "path": "/v1/fine_tuning/models/{model_id}/archive",
                "operation_id": "jobs_api_routes_fine_tuning_unarchive_fine_tuned_model",
                "display_name": "Unarchive Fine Tuned Model",
                "short_desc": "Unarchive Fine Tuned Model",
                "desc": "Unarchive Fine Tuned Model. Discover available models and manage custom models.\n\n**Deletion:** This operation can permanently remove the selected resource or access.",
                "groups": ("Models",),
                "fields": {
                    "model_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model ID",
                        "short_desc": "Model ID",
                        "desc": "The `model_id` value for this operation.",
                        "ref_data": "models",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-batch-jobs",
                "method": "GET",
                "path": "/v1/batch/jobs",
                "operation_id": "jobs_api_routes_batch_get_batch_jobs",
                "display_name": "List batch jobs",
                "short_desc": "List Batch Jobs",
                "desc": "Get Batch Jobs. Run and monitor inference jobs over many requests.",
                "groups": ("Batch",),
                "fields": {
                    "page": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page",
                        "short_desc": "Page",
                        "desc": "The `page` value for this operation.",
                        "preselected": True,
                    },
                    "page_size": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "short_desc": "Page Size",
                        "desc": "The `page_size` value for this operation.",
                        "preselected": True,
                    },
                    "model": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model",
                        "short_desc": "Model",
                        "desc": "The `model` value for this operation.",
                        "preselected": True,
                        "ref_data": "models",
                        "supports_custom_values": True,
                    },
                    "agent_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Agent ID",
                        "short_desc": "Agent ID",
                        "desc": "The `agent_id` value for this operation.",
                        "ref_data": "agents",
                        "supports_custom_values": True,
                    },
                    "metadata": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Metadata",
                        "short_desc": "Metadata",
                        "desc": "The `metadata` value for this operation.",
                    },
                    "created_after": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Created After",
                        "short_desc": "Created After",
                        "desc": "The `created_after` value for this operation.",
                    },
                    "created_by_me": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Created By Me",
                        "short_desc": "Created By Me",
                        "desc": "The `created_by_me` value for this operation.",
                    },
                    "status": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Status",
                        "short_desc": "Status",
                        "desc": "The `status` value for this operation.",
                        "preselected": True,
                        "element_allowed_values": (
                            <AllowedValueInfo>{
                                "value": "QUEUED",
                                "display_name": "Queued",
                            },
                            <AllowedValueInfo>{
                                "value": "RUNNING",
                                "display_name": "Running",
                            },
                            <AllowedValueInfo>{
                                "value": "SUCCESS",
                                "display_name": "Success",
                            },
                            <AllowedValueInfo>{
                                "value": "FAILED",
                                "display_name": "Failed",
                            },
                            <AllowedValueInfo>{
                                "value": "TIMEOUT_EXCEEDED",
                                "display_name": "Timeout Exceeded",
                            },
                            <AllowedValueInfo>{
                                "value": "CANCELLATION_REQUESTED",
                                "display_name": "Cancellation Requested",
                            },
                            <AllowedValueInfo>{
                                "value": "CANCELLED",
                                "display_name": "Cancelled",
                            },
                        ),
                    },
                    "order_by": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Order By",
                        "short_desc": "Order By",
                        "desc": "The `order_by` value for this operation.",
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "created",
                                "display_name": "Created",
                            },
                            <AllowedValueInfo>{
                                "value": "-created",
                                "display_name": "Created",
                            },
                        ),
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-batch-job",
                "method": "POST",
                "path": "/v1/batch/jobs",
                "operation_id": "jobs_api_routes_batch_create_batch_job",
                "display_name": "Create batch job",
                "short_desc": "Create Batch Job",
                "desc": "Create Batch Job. Run and monitor inference jobs over many requests.",
                "groups": ("Batch",),
                "fields": {
                    "input_files": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Input Files",
                        "short_desc": "Input Files",
                        "element_ref_data": "files",
                        "supports_custom_values": True,
                    },
                    "requests": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Requests",
                        "short_desc": "Requests",
                        "desc": "The `requests` value for this operation.",
                    },
                    "endpoint": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Endpoint",
                        "short_desc": "The endpoint to be used for batch inference.",
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "/v1/chat/completions",
                                "display_name": "/V1/Chat/Completions",
                            },
                            <AllowedValueInfo>{
                                "value": "/v1/embeddings",
                                "display_name": "/V1/Embeddings",
                            },
                            <AllowedValueInfo>{
                                "value": "/v1/fim/completions",
                                "display_name": "/V1/Fim/Completions",
                            },
                            <AllowedValueInfo>{
                                "value": "/v1/moderations",
                                "display_name": "/V1/Moderations",
                            },
                            <AllowedValueInfo>{
                                "value": "/v1/chat/moderations",
                                "display_name": "/V1/Chat/Moderations",
                            },
                            <AllowedValueInfo>{
                                "value": "/v1/ocr",
                                "display_name": "/V1/Ocr",
                            },
                            <AllowedValueInfo>{
                                "value": "/v1/classifications",
                                "display_name": "/V1/Classifications",
                            },
                            <AllowedValueInfo>{
                                "value": "/v1/chat/classifications",
                                "display_name": "/V1/Chat/Classifications",
                            },
                            <AllowedValueInfo>{
                                "value": "/v1/conversations",
                                "display_name": "/V1/Conversations",
                            },
                            <AllowedValueInfo>{
                                "value": "/v1/audio/transcriptions",
                                "display_name": "/V1/Audio/Transcriptions",
                            },
                        ),
                    },
                    "model": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model",
                        "short_desc": "The model to be used for batch inference.",
                        "preselected": True,
                        "ref_data": "models",
                        "supports_custom_values": True,
                    },
                    "agent_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Agent ID",
                        "short_desc": "Agent ID",
                        "ref_data": "agents",
                        "supports_custom_values": True,
                    },
                    "metadata": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Metadata",
                        "short_desc": "The metadata of your choice to be associated with the batch inference job.",
                    },
                    "timeout_hours": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Timeout Hours",
                        "short_desc": "The timeout in hours for the batch inference job.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-batch-job",
                "method": "GET",
                "path": "/v1/batch/jobs/{job_id}",
                "operation_id": "jobs_api_routes_batch_get_batch_job",
                "display_name": "Get Batch Job",
                "short_desc": "Get Batch Job",
                "desc": "Get Batch Job. Run and monitor inference jobs over many requests.",
                "groups": ("Batch",),
                "fields": {
                    "job_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Job ID",
                        "short_desc": "Job ID",
                        "desc": "The `job_id` value for this operation.",
                        "ref_data": "batch-jobs",
                        "supports_custom_values": True,
                    },
                    "inline": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Inline",
                        "short_desc": "Inline",
                        "desc": "The `inline` value for this operation.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "cancel-batch-job",
                "method": "POST",
                "path": "/v1/batch/jobs/{job_id}/cancel",
                "operation_id": "jobs_api_routes_batch_cancel_batch_job",
                "display_name": "Cancel Batch Job",
                "short_desc": "Cancel Batch Job",
                "desc": "Cancel Batch Job. Run and monitor inference jobs over many requests.",
                "groups": ("Batch",),
                "fields": {
                    "job_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Job ID",
                        "short_desc": "Job ID",
                        "desc": "The `job_id` value for this operation.",
                        "ref_data": "batch-jobs",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "chat-completion",
                "method": "POST",
                "mutates_state": False,
                "path": "/v1/chat/completions",
                "response_content_type": "application/json",
                "operation_id": "chat_completion_v1_chat_completions_post",
                "display_name": "Chat completion",
                "short_desc": "Chat Completion",
                "desc": "Generate a response from conversation messages. Add `tools` for function calls or `response_format` for structured JSON. Returns the completed response, including token usage.",
                "groups": ("Chat",),
                "fields": {
                    "model": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model",
                        "short_desc": "ID of the model to use",
                        "preselected": True,
                        "ref_data": "models",
                        "supports_custom_values": True,
                        "default_value": "mistral-large-latest",
                    },
                    "temperature": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Temperature",
                        "short_desc": "What sampling temperature to use, we recommend between 0.0 and 0.7",
                        "preselected": True,
                    },
                    "top_p": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Top P",
                        "short_desc": "Top P",
                    },
                    "max_tokens": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Max Tokens",
                        "short_desc": "The maximum number of tokens to generate in the completion",
                        "preselected": True,
                        "example_value": 256,
                    },
                    "stream": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "stop": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Stop",
                        "short_desc": "Stop generation if this token is detected",
                    },
                    "random_seed": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Random Seed",
                        "short_desc": "The seed to use for random sampling",
                    },
                    "metadata": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Metadata",
                        "short_desc": "Metadata",
                        "desc": "The `metadata` value for this operation.",
                    },
                    "messages": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Messages",
                        "short_desc": "Messages",
                        "example_value": (
                            {
                                "role": "user",
                                "content": "Summarize the customer request.",
                            },
                        ),
                    },
                    "response_format": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Response Format",
                        "short_desc": "Specify the format that the model must output",
                        "fields": {
                            "type": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (
                                    <AllowedValueInfo>{
                                        "value": "text",
                                        "display_name": "Text",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "json_object",
                                        "display_name": "Json Object",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "json_schema",
                                        "display_name": "Json Schema",
                                    },
                                ),
                            },
                        },
                    },
                    "tools": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Tools",
                        "short_desc": "A list of tools the model may call",
                    },
                    "tool_choice": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Tool Choice",
                        "short_desc": "Controls which (if any) tool is called by the model",
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "auto",
                                "display_name": "Auto",
                            },
                            <AllowedValueInfo>{
                                "value": "none",
                                "display_name": "None",
                            },
                            <AllowedValueInfo>{
                                "value": "any",
                                "display_name": "Any",
                            },
                            <AllowedValueInfo>{
                                "value": "required",
                                "display_name": "Required",
                            },
                        ),
                        "supports_custom_values": True,
                    },
                    "presence_penalty": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Presence Penalty",
                        "short_desc": "Presence Penalty",
                    },
                    "frequency_penalty": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Frequency Penalty",
                        "short_desc": "Frequency Penalty",
                    },
                    "n": <RestSchemaFieldOverlayInfo>{
                        "display_name": "N",
                        "short_desc": "N",
                    },
                    "prediction": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Prediction",
                        "short_desc": "Prediction",
                    },
                    "parallel_tool_calls": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Parallel Tool Calls",
                        "short_desc": "Parallel Tool Calls",
                    },
                    "prompt_mode": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Prompt Mode",
                        "short_desc": "Allows toggling between the reasoning mode and no system prompt",
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "reasoning",
                                "display_name": "Reasoning",
                            },
                        ),
                    },
                    "reasoning_effort": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Reasoning Effort",
                        "short_desc": "Controls the reasoning effort level for reasoning models",
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "high",
                                "display_name": "High",
                            },
                            <AllowedValueInfo>{
                                "value": "none",
                                "display_name": "None",
                            },
                        ),
                    },
                    "guardrails": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Guardrails",
                        "short_desc": "A list of guardrail configurations to apply to this request",
                        "fields": {
                            "moderation_llm_v1": <RestSchemaFieldOverlayInfo>{
                                "fields": {
                                    "action": <RestSchemaFieldOverlayInfo>{
                                        "allowed_values": (
                                            <AllowedValueInfo>{
                                                "value": "none",
                                                "display_name": "None",
                                            },
                                            <AllowedValueInfo>{
                                                "value": "block",
                                                "display_name": "Block",
                                            },
                                        ),
                                    },
                                },
                            },
                            "moderation_llm_v2": <RestSchemaFieldOverlayInfo>{
                                "fields": {
                                    "action": <RestSchemaFieldOverlayInfo>{
                                        "allowed_values": (
                                            <AllowedValueInfo>{
                                                "value": "none",
                                                "display_name": "None",
                                            },
                                            <AllowedValueInfo>{
                                                "value": "block",
                                                "display_name": "Block",
                                            },
                                        ),
                                    },
                                },
                            },
                        },
                    },
                    "prompt_cache_key": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Prompt Cache Key",
                        "short_desc": "A cache key to enable prompt caching",
                    },
                    "safe_prompt": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Safe Prompt",
                        "short_desc": "Whether to inject a safety prompt before all conversations.",
                    },
                },
                "response_fields": {
                    "created": <RestSchemaFieldOverlayInfo>{
                        "codec": "unix-seconds",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "complete-code",
                "method": "POST",
                "mutates_state": False,
                "path": "/v1/fim/completions",
                "response_content_type": "application/json",
                "operation_id": "fim_completion_v1_fim_completions_post",
                "display_name": "Complete code",
                "short_desc": "Complete Code",
                "desc": "Complete source code between `prompt` and optional `suffix`. Use a code model such as `codestral-latest`.",
                "groups": ("Fim",),
                "fields": {
                    "model": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model",
                        "short_desc": "ID of the model with FIM to use.",
                        "preselected": True,
                        "ref_data": "models",
                        "supports_custom_values": True,
                        "default_value": "codestral-latest",
                    },
                    "temperature": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Temperature",
                        "short_desc": "What sampling temperature to use, we recommend between 0.0 and 0.7",
                        "preselected": True,
                    },
                    "top_p": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Top P",
                        "short_desc": "Top P",
                    },
                    "max_tokens": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Max Tokens",
                        "short_desc": "The maximum number of tokens to generate in the completion",
                        "preselected": True,
                    },
                    "stream": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "stop": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Stop",
                        "short_desc": "Stop generation if this token is detected",
                    },
                    "random_seed": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Random Seed",
                        "short_desc": "The seed to use for random sampling",
                    },
                    "metadata": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Metadata",
                        "short_desc": "Metadata",
                        "desc": "The `metadata` value for this operation.",
                    },
                    "prompt": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Prompt",
                        "short_desc": "The text/code to complete.",
                        "example_value": "def total_amount(items):\n    return ",
                    },
                    "suffix": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Suffix",
                        "short_desc": "Optional text/code that adds more context for the model",
                    },
                    "min_tokens": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Min Tokens",
                        "short_desc": "The minimum number of tokens to generate in the completion.",
                    },
                    "prompt_cache_key": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Prompt Cache Key",
                        "short_desc": "A cache key to enable prompt caching",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-embeddings",
                "method": "POST",
                "mutates_state": False,
                "path": "/v1/embeddings",
                "operation_id": "embeddings_v1_embeddings_post",
                "display_name": "Create embeddings",
                "short_desc": "Create Embeddings",
                "desc": "Convert text into embedding vectors for semantic search, clustering and retrieval. Choose an embedding model from the models available to your account.",
                "groups": ("Embeddings",),
                "fields": {
                    "model": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model",
                        "short_desc": "ID of the model to use.",
                        "preselected": True,
                        "ref_data": "models",
                        "supports_custom_values": True,
                        "default_value": "mistral-embed",
                    },
                    "metadata": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Metadata",
                        "short_desc": "Metadata",
                        "desc": "The `metadata` value for this operation.",
                    },
                    "input": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Input",
                        "short_desc": "Text to embed.",
                        "example_value": ("The customer needs an updated invoice.",),
                    },
                    "output_dimension": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Output Dimension",
                        "short_desc": "The dimension of the output embeddings when feature available",
                    },
                    "output_dtype": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Output Dtype",
                        "short_desc": "The data type of the output embeddings when feature available",
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "float",
                                "display_name": "Float",
                            },
                            <AllowedValueInfo>{
                                "value": "int8",
                                "display_name": "Int8",
                            },
                            <AllowedValueInfo>{
                                "value": "uint8",
                                "display_name": "Uint8",
                            },
                            <AllowedValueInfo>{
                                "value": "binary",
                                "display_name": "Binary",
                            },
                            <AllowedValueInfo>{
                                "value": "ubinary",
                                "display_name": "Ubinary",
                            },
                        ),
                    },
                    "encoding_format": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Encoding Format",
                        "short_desc": "The format of embeddings in the response.",
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "float",
                                "display_name": "Float",
                            },
                            <AllowedValueInfo>{
                                "value": "base64",
                                "display_name": "Base64",
                            },
                        ),
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "moderate-text",
                "method": "POST",
                "mutates_state": False,
                "path": "/v1/moderations",
                "operation_id": "moderations_v1_moderations_post",
                "display_name": "Moderate text",
                "short_desc": "Moderate Text",
                "desc": "Moderations. Check content before publishing or routing it.",
                "groups": ("Classifiers",),
                "fields": {
                    "model": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model",
                        "short_desc": "ID of the model to use.",
                        "preselected": True,
                        "ref_data": "models",
                        "supports_custom_values": True,
                    },
                    "metadata": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Metadata",
                        "short_desc": "Metadata",
                        "desc": "The `metadata` value for this operation.",
                    },
                    "input": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Input",
                        "short_desc": "Text to classify.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "moderate-chat",
                "method": "POST",
                "mutates_state": False,
                "path": "/v1/chat/moderations",
                "operation_id": "chat_moderations_v1_chat_moderations_post",
                "display_name": "Moderate chat",
                "short_desc": "Moderate Chat",
                "desc": "Chat Moderations. Check content before publishing or routing it.",
                "groups": ("Classifiers",),
                "fields": {
                    "input": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Input",
                        "short_desc": "Chat to classify",
                    },
                    "model": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model",
                        "short_desc": "Model",
                        "desc": "The `model` value for this operation.",
                        "preselected": True,
                        "ref_data": "models",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "ocr",
                "method": "POST",
                "mutates_state": False,
                "path": "/v1/ocr",
                "operation_id": "ocr_v1_ocr_post",
                "display_name": "Document OCR",
                "short_desc": "Document OCR",
                "desc": "Extract Markdown, images and structured annotations from a PDF or image. Supply a public document URL or a supported data URI in `document`.",
                "groups": ("Ocr",),
                "fields": {
                    "model": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model",
                        "short_desc": "Model",
                        "desc": "The `model` value for this operation.",
                        "preselected": True,
                        "ref_data": "models",
                        "supports_custom_values": True,
                        "default_value": "mistral-ocr-latest",
                    },
                    "document": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Document",
                        "short_desc": "Document to run OCR on",
                        "fields": {
                            "file_id": <RestSchemaFieldOverlayInfo>{
                                "ref_data": "files",
                                "supports_custom_values": True,
                            },
                            "image_url": <RestSchemaFieldOverlayInfo>{
                                "fields": {
                                    "detail": <RestSchemaFieldOverlayInfo>{
                                        "allowed_values": (
                                            <AllowedValueInfo>{
                                                "value": "low",
                                                "display_name": "Low",
                                            },
                                            <AllowedValueInfo>{
                                                "value": "auto",
                                                "display_name": "Auto",
                                            },
                                            <AllowedValueInfo>{
                                                "value": "high",
                                                "display_name": "High",
                                            },
                                        ),
                                    },
                                },
                            },
                        },
                        "example_value": {
                            "type": "document_url",
                            "document_url": "https://example.com/invoice.pdf",
                        },
                    },
                    "pages": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Pages",
                        "short_desc": "Specific pages to process",
                    },
                    "include_image_base64": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Include Image Base64",
                        "short_desc": "Include image URLs in response",
                    },
                    "image_limit": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Image Limit",
                        "short_desc": "Max images to extract",
                    },
                    "image_min_size": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Image Min Size",
                        "short_desc": "Minimum height and width of image to extract",
                    },
                    "bbox_annotation_format": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Bbox Annotation Format",
                        "short_desc": "Bbox Annotation Format",
                        "fields": {
                            "type": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (
                                    <AllowedValueInfo>{
                                        "value": "text",
                                        "display_name": "Text",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "json_object",
                                        "display_name": "Json Object",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "json_schema",
                                        "display_name": "Json Schema",
                                    },
                                ),
                            },
                        },
                    },
                    "document_annotation_format": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Document Annotation Format",
                        "short_desc": "Document Annotation Format",
                        "fields": {
                            "type": <RestSchemaFieldOverlayInfo>{
                                "allowed_values": (
                                    <AllowedValueInfo>{
                                        "value": "text",
                                        "display_name": "Text",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "json_object",
                                        "display_name": "Json Object",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "json_schema",
                                        "display_name": "Json Schema",
                                    },
                                ),
                            },
                        },
                    },
                    "document_annotation_prompt": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Document Annotation Prompt",
                        "short_desc": "Document Annotation Prompt",
                    },
                    "table_format": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Table Format",
                        "short_desc": "Table Format",
                        "desc": "The `table_format` value for this operation.",
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "markdown",
                                "display_name": "Markdown",
                            },
                            <AllowedValueInfo>{
                                "value": "html",
                                "display_name": "Html",
                            },
                        ),
                    },
                    "extract_header": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Extract Header",
                        "short_desc": "Extract Header",
                        "desc": "The `extract_header` value for this operation.",
                    },
                    "extract_footer": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Extract Footer",
                        "short_desc": "Extract Footer",
                        "desc": "The `extract_footer` value for this operation.",
                    },
                    "include_blocks": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Include Blocks",
                        "short_desc": "Return paragraph-level bounding boxes for all content blocks in the response",
                    },
                    "confidence_scores_granularity": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Confidence Scores Granularity",
                        "short_desc": "Confidence Scores Granularity",
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "word",
                                "display_name": "Word",
                            },
                            <AllowedValueInfo>{
                                "value": "page",
                                "display_name": "Page",
                            },
                        ),
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "classify-text",
                "method": "POST",
                "mutates_state": False,
                "path": "/v1/classifications",
                "operation_id": "classifications_v1_classifications_post",
                "display_name": "Classify text",
                "short_desc": "Classify Text",
                "desc": "Classifications. Check content before publishing or routing it.",
                "groups": ("Classifiers",),
                "fields": {
                    "model": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model",
                        "short_desc": "ID of the model to use.",
                        "preselected": True,
                        "ref_data": "models",
                        "supports_custom_values": True,
                    },
                    "metadata": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Metadata",
                        "short_desc": "Metadata",
                        "desc": "The `metadata` value for this operation.",
                    },
                    "input": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Input",
                        "short_desc": "Text to classify.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "classify-chat",
                "method": "POST",
                "mutates_state": False,
                "path": "/v1/chat/classifications",
                "operation_id": "chat_classifications_v1_chat_classifications_post",
                "display_name": "Classify chat",
                "short_desc": "Classify Chat",
                "desc": "Chat Classifications. Check content before publishing or routing it.",
                "groups": ("Classifiers",),
                "fields": {
                    "model": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model",
                        "short_desc": "Model",
                        "desc": "The `model` value for this operation.",
                        "preselected": True,
                        "ref_data": "models",
                        "supports_custom_values": True,
                    },
                    "input": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Input",
                        "short_desc": "Chat to classify",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "transcribe-audio",
                "method": "POST",
                "mutates_state": False,
                "path": "/v1/audio/transcriptions",
                "operation_id": "audio_api_v1_transcriptions_post",
                "display_name": "Transcribe audio",
                "short_desc": "Transcribe Audio",
                "desc": "Transcribe a recording from an uploaded `file`, an existing `file_id`, or a downloadable `file_url`. Optional timestamps and speaker diarization help organize meeting notes.",
                "groups": ("Audio Transcriptions",),
                "fields": {
                    "model": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model",
                        "short_desc": "ID of the model to be used.",
                        "preselected": True,
                        "ref_data": "models",
                        "supports_custom_values": True,
                        "default_value": "voxtral-mini-latest",
                    },
                    "file": <RestSchemaFieldOverlayInfo>{
                        "display_name": "File",
                        "short_desc": "The File object (not file name) to be uploaded.",
                        "required_groups": ("audio",),
                        "exclusive_with": ("file_id", "file_url",),
                        "preselected": True,
                    },
                    "file_url": <RestSchemaFieldOverlayInfo>{
                        "display_name": "File URL",
                        "short_desc": "Url of a file to be transcribed",
                        "required_groups": ("audio",),
                        "exclusive_with": ("file", "file_id",),
                        "preselected": True,
                    },
                    "file_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "File ID",
                        "short_desc": "ID of a file uploaded to /v1/files",
                        "ref_data": "files",
                        "supports_custom_values": True,
                        "required_groups": ("audio",),
                        "exclusive_with": ("file", "file_url",),
                        "preselected": True,
                    },
                    "language": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Language",
                        "short_desc": "Language of the audio, e.g",
                    },
                    "temperature": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Temperature",
                        "short_desc": "Temperature",
                        "desc": "The `temperature` value for this operation.",
                        "preselected": True,
                    },
                    "stream": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "diarize": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Diarize",
                        "short_desc": "Diarize",
                        "desc": "The `diarize` value for this operation.",
                    },
                    "context_bias": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Context Bias",
                        "short_desc": "Context Bias",
                        "desc": "The `context_bias` value for this operation.",
                    },
                    "timestamp_granularities": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Timestamp Granularities",
                        "short_desc": "Granularities of timestamps to include in the response.",
                        "element_allowed_values": (
                            <AllowedValueInfo>{
                                "value": "segment",
                                "display_name": "Segment",
                            },
                            <AllowedValueInfo>{
                                "value": "word",
                                "display_name": "Word",
                            },
                        ),
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "generate-speech",
                "method": "POST",
                "mutates_state": False,
                "path": "/v1/audio/speech",
                "response_content_type": "application/json",
                "operation_id": "speech_v1_audio_speech_post",
                "display_name": "Generate speech",
                "short_desc": "Generate Speech",
                "desc": "Generate spoken audio from `input` text. Choose a preset or custom `voice_id`, or provide base64 reference audio in `ref_audio`. Returns the completed audio response.",
                "groups": ("Audio Speech",),
                "fields": {
                    "model": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model",
                        "short_desc": "Model",
                        "desc": "The `model` value for this operation.",
                        "preselected": True,
                        "ref_data": "models",
                        "supports_custom_values": True,
                    },
                    "stream": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "voice_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Voice ID",
                        "short_desc": "The preset or custom voice to use for generating the speech.",
                        "ref_data": "voices",
                        "supports_custom_values": True,
                    },
                    "ref_audio": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Ref Audio",
                        "short_desc": "The base64-encoded audio reference for zero-shot voice cloning.",
                    },
                    "input": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Input",
                        "short_desc": "Text to generate speech from.",
                    },
                    "response_format": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Response Format",
                        "short_desc": "Output audio format",
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "pcm",
                                "display_name": "Pcm",
                            },
                            <AllowedValueInfo>{
                                "value": "wav",
                                "display_name": "Wav",
                            },
                            <AllowedValueInfo>{
                                "value": "mp3",
                                "display_name": "Mp3",
                            },
                            <AllowedValueInfo>{
                                "value": "flac",
                                "display_name": "Flac",
                            },
                            <AllowedValueInfo>{
                                "value": "opus",
                                "display_name": "Opus",
                            },
                        ),
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-voices",
                "method": "GET",
                "path": "/v1/audio/voices",
                "operation_id": "list_voices_v1_audio_voices_get",
                "display_name": "List voices",
                "short_desc": "List Voices",
                "desc": "List all voices. Turn recorded speech into text or generate spoken content.",
                "groups": ("Audio Voices",),
                "fields": {
                    "limit": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Limit",
                        "short_desc": "Maximum number of voices to return",
                        "preselected": True,
                    },
                    "offset": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Offset",
                        "short_desc": "Offset for pagination",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-voice",
                "method": "POST",
                "path": "/v1/audio/voices",
                "operation_id": "create_voice_v1_audio_voices_post",
                "display_name": "Create voice",
                "short_desc": "Create Voice",
                "desc": "Create a new voice. Turn recorded speech into text or generate spoken content.",
                "groups": ("Audio Voices",),
                "fields": {
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "Name",
                        "desc": "The `name` value for this operation.",
                        "preselected": True,
                    },
                    "languages": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Languages",
                        "short_desc": "Languages",
                        "desc": "The `languages` value for this operation.",
                    },
                    "gender": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Gender",
                        "short_desc": "Gender",
                        "desc": "The `gender` value for this operation.",
                    },
                    "age": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Age",
                        "short_desc": "Age",
                        "desc": "The `age` value for this operation.",
                    },
                    "tags": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Tags",
                        "short_desc": "Tags",
                        "desc": "The `tags` value for this operation.",
                    },
                    "color": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Color",
                        "short_desc": "Color",
                        "desc": "The `color` value for this operation.",
                    },
                    "retention_notice": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Retention Notice",
                        "short_desc": "Retention Notice",
                        "desc": "The `retention_notice` value for this operation.",
                    },
                    "sample_audio": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Sample Audio",
                        "short_desc": "Base64-encoded audio file",
                    },
                    "sample_filename": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Sample Filename",
                        "short_desc": "Original filename for extension detection",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-voice",
                "method": "GET",
                "path": "/v1/audio/voices/{voice_id}",
                "operation_id": "get_voice_v1_audio_voices__voice_id__get",
                "display_name": "Get Voice",
                "short_desc": "Get Voice",
                "desc": "Get voice details. Turn recorded speech into text or generate spoken content.",
                "groups": ("Audio Voices",),
                "fields": {
                    "voice_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Voice ID",
                        "short_desc": "Voice ID",
                        "desc": "The `voice_id` value for this operation.",
                        "ref_data": "voices",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-voice",
                "method": "PATCH",
                "path": "/v1/audio/voices/{voice_id}",
                "operation_id": "update_voice_v1_audio_voices__voice_id__patch",
                "display_name": "Update Voice",
                "short_desc": "Update Voice",
                "desc": "Update voice metadata. Turn recorded speech into text or generate spoken content.",
                "groups": ("Audio Voices",),
                "fields": {
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "Name",
                        "desc": "The `name` value for this operation.",
                        "preselected": True,
                    },
                    "languages": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Languages",
                        "short_desc": "Languages",
                        "desc": "The `languages` value for this operation.",
                    },
                    "gender": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Gender",
                        "short_desc": "Gender",
                        "desc": "The `gender` value for this operation.",
                    },
                    "age": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Age",
                        "short_desc": "Age",
                        "desc": "The `age` value for this operation.",
                    },
                    "tags": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Tags",
                        "short_desc": "Tags",
                        "desc": "The `tags` value for this operation.",
                    },
                    "voice_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Voice ID",
                        "short_desc": "Voice ID",
                        "desc": "The `voice_id` value for this operation.",
                        "ref_data": "voices",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-voice",
                "method": "DELETE",
                "path": "/v1/audio/voices/{voice_id}",
                "operation_id": "delete_voice_v1_audio_voices__voice_id__delete",
                "display_name": "Delete Voice",
                "short_desc": "Delete Voice",
                "desc": "Delete a custom voice. Turn recorded speech into text or generate spoken content.\n\n**Deletion:** This operation can permanently remove the selected resource or access.",
                "groups": ("Audio Voices",),
                "fields": {
                    "voice_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Voice ID",
                        "short_desc": "Voice ID",
                        "desc": "The `voice_id` value for this operation.",
                        "ref_data": "voices",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-voice-sample",
                "method": "GET",
                "path": "/v1/audio/voices/{voice_id}/sample",
                "response_content_type": "audio/wav",
                "operation_id": "get_voice_sample_audio_v1_audio_voices__voice_id__sample_get",
                "display_name": "Get Voice Sample",
                "short_desc": "Get Voice Sample",
                "desc": "Get voice sample audio. Turn recorded speech into text or generate spoken content.",
                "groups": ("Audio Voices",),
                "fields": {
                    "voice_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Voice ID",
                        "short_desc": "Voice ID",
                        "desc": "The `voice_id` value for this operation.",
                        "ref_data": "voices",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-libraries",
                "method": "GET",
                "path": "/v1/libraries",
                "operation_id": "libraries_list_v1",
                "display_name": "List libraries",
                "short_desc": "List Libraries",
                "desc": "List all libraries you have access to. Manage document libraries used to ground assistant answers.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Libraries",),
            },
            <RestSchemaActionInfo>{
                "action": "create-library",
                "method": "POST",
                "path": "/v1/libraries",
                "operation_id": "libraries_create_v1",
                "display_name": "Create library",
                "short_desc": "Create Library",
                "desc": "Create a new Library. Manage document libraries used to ground assistant answers.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Libraries",),
                "fields": {
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "Name",
                        "desc": "The `name` value for this operation.",
                        "preselected": True,
                    },
                    "description": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Description",
                        "short_desc": "Description",
                        "desc": "The `description` value for this operation.",
                    },
                    "chunk_size": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Chunk Size",
                        "short_desc": "Chunk Size",
                        "desc": "The `chunk_size` value for this operation.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-library",
                "method": "GET",
                "path": "/v1/libraries/{library_id}",
                "operation_id": "libraries_get_v1",
                "display_name": "Get Library",
                "short_desc": "Get Library",
                "desc": "Detailed information about a specific Library. Manage document libraries used to ground assistant answers.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Libraries",),
                "fields": {
                    "library_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Library ID",
                        "short_desc": "Library ID",
                        "desc": "The `library_id` value for this operation.",
                        "ref_data": "libraries",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-library",
                "method": "DELETE",
                "path": "/v1/libraries/{library_id}",
                "operation_id": "libraries_delete_v1",
                "display_name": "Delete Library",
                "short_desc": "Delete Library",
                "desc": "Delete a library and all of it's document. Manage document libraries used to ground assistant answers.\n\n**Public preview:** Availability depends on your account and permissions.\n\n**Deletion:** This operation can permanently remove the selected resource or access.",
                "groups": ("Libraries",),
                "fields": {
                    "library_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Library ID",
                        "short_desc": "Library ID",
                        "desc": "The `library_id` value for this operation.",
                        "ref_data": "libraries",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-library",
                "method": "PUT",
                "path": "/v1/libraries/{library_id}",
                "operation_id": "libraries_update_v1",
                "display_name": "Update Library",
                "short_desc": "Update Library",
                "desc": "Update a library. Manage document libraries used to ground assistant answers.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Libraries",),
                "fields": {
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "Name",
                        "desc": "The `name` value for this operation.",
                        "preselected": True,
                    },
                    "description": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Description",
                        "short_desc": "Description",
                        "desc": "The `description` value for this operation.",
                    },
                    "library_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Library ID",
                        "short_desc": "Library ID",
                        "desc": "The `library_id` value for this operation.",
                        "ref_data": "libraries",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-library-documents",
                "method": "GET",
                "path": "/v1/libraries/{library_id}/documents",
                "operation_id": "libraries_documents_list_v1",
                "display_name": "List Library Documents",
                "short_desc": "List Library Documents",
                "desc": "List documents in a given library. Manage document libraries used to ground assistant answers.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Libraries Documents",),
                "fields": {
                    "library_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Library ID",
                        "short_desc": "Library ID",
                        "desc": "The `library_id` value for this operation.",
                        "ref_data": "libraries",
                        "supports_custom_values": True,
                    },
                    "search": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Search",
                        "short_desc": "Search",
                        "desc": "The `search` value for this operation.",
                        "preselected": True,
                    },
                    "page_size": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "short_desc": "Page Size",
                        "desc": "The `page_size` value for this operation.",
                        "preselected": True,
                    },
                    "page": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page",
                        "short_desc": "Page",
                        "desc": "The `page` value for this operation.",
                        "preselected": True,
                    },
                    "filters_attributes": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Filters Attributes",
                        "short_desc": "Filters Attributes",
                        "desc": "The `filters_attributes` value for this operation.",
                    },
                    "sort_by": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Sort By",
                        "short_desc": "Sort By",
                        "desc": "The `sort_by` value for this operation.",
                    },
                    "sort_order": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Sort Order",
                        "short_desc": "Sort Order",
                        "desc": "The `sort_order` value for this operation.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "upload-library-document",
                "method": "POST",
                "path": "/v1/libraries/{library_id}/documents",
                "operation_id": "libraries_documents_upload_v1",
                "display_name": "Upload Library Document",
                "short_desc": "Upload Library Document",
                "desc": "Upload a new document. Manage document libraries used to ground assistant answers.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Libraries Documents",),
                "fields": {
                    "file": <RestSchemaFieldOverlayInfo>{
                        "display_name": "File",
                        "short_desc": "The File object (not file name) to be uploaded.",
                    },
                    "library_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Library ID",
                        "short_desc": "Library ID",
                        "desc": "The `library_id` value for this operation.",
                        "ref_data": "libraries",
                        "supports_custom_values": True,
                    },
                },
                "response_code": "200",
            },
            <RestSchemaActionInfo>{
                "action": "get-library-document",
                "method": "GET",
                "path": "/v1/libraries/{library_id}/documents/{document_id}",
                "operation_id": "libraries_documents_get_v1",
                "display_name": "Get Library Document",
                "short_desc": "Get Library Document",
                "desc": "Retrieve the metadata of a specific document. Manage document libraries used to ground assistant answers.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Libraries Documents",),
                "fields": {
                    "library_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Library ID",
                        "short_desc": "Library ID",
                        "desc": "The `library_id` value for this operation.",
                        "ref_data": "libraries",
                        "supports_custom_values": True,
                        "has_dependents": True,
                        "on_change": ("refetch",),
                    },
                    "document_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Document ID",
                        "short_desc": "Document ID",
                        "desc": "The `document_id` value for this operation.",
                        "ref_data": "library-documents",
                        "depends_on": ("library_id",),
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-library-document",
                "method": "PUT",
                "path": "/v1/libraries/{library_id}/documents/{document_id}",
                "operation_id": "libraries_documents_update_v1",
                "display_name": "Update Library Document",
                "short_desc": "Update Library Document",
                "desc": "Update the metadata of a specific document. Manage document libraries used to ground assistant answers.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Libraries Documents",),
                "fields": {
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "Name",
                        "desc": "The `name` value for this operation.",
                        "preselected": True,
                    },
                    "attributes": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Attributes",
                        "short_desc": "Attributes",
                        "desc": "The `attributes` value for this operation.",
                    },
                    "library_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Library ID",
                        "short_desc": "Library ID",
                        "desc": "The `library_id` value for this operation.",
                        "ref_data": "libraries",
                        "supports_custom_values": True,
                        "has_dependents": True,
                        "on_change": ("refetch",),
                    },
                    "document_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Document ID",
                        "short_desc": "Document ID",
                        "desc": "The `document_id` value for this operation.",
                        "ref_data": "library-documents",
                        "depends_on": ("library_id",),
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-library-document",
                "method": "DELETE",
                "path": "/v1/libraries/{library_id}/documents/{document_id}",
                "operation_id": "libraries_documents_delete_v1",
                "display_name": "Delete Library Document",
                "short_desc": "Delete Library Document",
                "desc": "Delete a document. Manage document libraries used to ground assistant answers.\n\n**Public preview:** Availability depends on your account and permissions.\n\n**Deletion:** This operation can permanently remove the selected resource or access.",
                "groups": ("Libraries Documents",),
                "fields": {
                    "library_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Library ID",
                        "short_desc": "Library ID",
                        "desc": "The `library_id` value for this operation.",
                        "ref_data": "libraries",
                        "supports_custom_values": True,
                        "has_dependents": True,
                        "on_change": ("refetch",),
                    },
                    "document_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Document ID",
                        "short_desc": "Document ID",
                        "desc": "The `document_id` value for this operation.",
                        "ref_data": "library-documents",
                        "depends_on": ("library_id",),
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-library-document-text",
                "method": "GET",
                "path": "/v1/libraries/{library_id}/documents/{document_id}/text_content",
                "operation_id": "libraries_documents_get_text_content_v1",
                "display_name": "Get Library Document Text",
                "short_desc": "Get Library Document Text",
                "desc": "Retrieve the text content of a specific document. Manage document libraries used to ground assistant answers.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Libraries Documents",),
                "fields": {
                    "library_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Library ID",
                        "short_desc": "Library ID",
                        "desc": "The `library_id` value for this operation.",
                        "ref_data": "libraries",
                        "supports_custom_values": True,
                        "has_dependents": True,
                        "on_change": ("refetch",),
                    },
                    "document_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Document ID",
                        "short_desc": "Document ID",
                        "desc": "The `document_id` value for this operation.",
                        "ref_data": "library-documents",
                        "depends_on": ("library_id",),
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-library-document-status",
                "method": "GET",
                "path": "/v1/libraries/{library_id}/documents/{document_id}/status",
                "operation_id": "libraries_documents_get_status_v1",
                "display_name": "Get Library Document Status",
                "short_desc": "Get Library Document Status",
                "desc": "Retrieve the processing status of a specific document. Manage document libraries used to ground assistant answers.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Libraries Documents",),
                "fields": {
                    "library_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Library ID",
                        "short_desc": "Library ID",
                        "desc": "The `library_id` value for this operation.",
                        "ref_data": "libraries",
                        "supports_custom_values": True,
                        "has_dependents": True,
                        "on_change": ("refetch",),
                    },
                    "document_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Document ID",
                        "short_desc": "Document ID",
                        "desc": "The `document_id` value for this operation.",
                        "ref_data": "library-documents",
                        "depends_on": ("library_id",),
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-library-document-url",
                "method": "GET",
                "path": "/v1/libraries/{library_id}/documents/{document_id}/signed-url",
                "operation_id": "libraries_documents_get_signed_url_v1",
                "display_name": "Get Library Document URL",
                "short_desc": "Get Library Document URL",
                "desc": "Retrieve the signed URL of a specific document. Manage document libraries used to ground assistant answers.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Libraries Documents",),
                "fields": {
                    "library_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Library ID",
                        "short_desc": "Library ID",
                        "desc": "The `library_id` value for this operation.",
                        "ref_data": "libraries",
                        "supports_custom_values": True,
                        "has_dependents": True,
                        "on_change": ("refetch",),
                    },
                    "document_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Document ID",
                        "short_desc": "Document ID",
                        "desc": "The `document_id` value for this operation.",
                        "ref_data": "library-documents",
                        "depends_on": ("library_id",),
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-library-document-text-url",
                "method": "GET",
                "path": "/v1/libraries/{library_id}/documents/{document_id}/extracted-text-signed-url",
                "operation_id": "libraries_documents_get_extracted_text_signed_url_v1",
                "display_name": "Get Library Document Text URL",
                "short_desc": "Get Library Document Text URL",
                "desc": "Retrieve the signed URL of text extracted from a given document. Manage document libraries used to ground assistant answers.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Libraries Documents",),
                "fields": {
                    "library_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Library ID",
                        "short_desc": "Library ID",
                        "desc": "The `library_id` value for this operation.",
                        "ref_data": "libraries",
                        "supports_custom_values": True,
                        "has_dependents": True,
                        "on_change": ("refetch",),
                    },
                    "document_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Document ID",
                        "short_desc": "Document ID",
                        "desc": "The `document_id` value for this operation.",
                        "ref_data": "library-documents",
                        "depends_on": ("library_id",),
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "reprocess-library-document",
                "method": "POST",
                "path": "/v1/libraries/{library_id}/documents/{document_id}/reprocess",
                "operation_id": "libraries_documents_reprocess_v1",
                "display_name": "Reprocess Library Document",
                "short_desc": "Reprocess Library Document",
                "desc": "Reprocess a document. Manage document libraries used to ground assistant answers.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Libraries Documents",),
                "fields": {
                    "library_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Library ID",
                        "short_desc": "Library ID",
                        "desc": "The `library_id` value for this operation.",
                        "ref_data": "libraries",
                        "supports_custom_values": True,
                        "has_dependents": True,
                        "on_change": ("refetch",),
                    },
                    "document_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Document ID",
                        "short_desc": "Document ID",
                        "desc": "The `document_id` value for this operation.",
                        "ref_data": "library-documents",
                        "depends_on": ("library_id",),
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-library-access",
                "method": "GET",
                "path": "/v1/libraries/{library_id}/share",
                "operation_id": "libraries_share_list_v1",
                "display_name": "List Library Access",
                "short_desc": "List Library Access",
                "desc": "List all of the access to this library. Manage document libraries used to ground assistant answers.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Libraries Accesses",),
                "fields": {
                    "library_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Library ID",
                        "short_desc": "Library ID",
                        "desc": "The `library_id` value for this operation.",
                        "ref_data": "libraries",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "set-library-access",
                "method": "PUT",
                "path": "/v1/libraries/{library_id}/share",
                "operation_id": "libraries_share_create_v1",
                "display_name": "Set Library Access",
                "short_desc": "Set Library Access",
                "desc": "Create or update an access level. Manage document libraries used to ground assistant answers.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Libraries Accesses",),
                "fields": {
                    "org_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Org ID",
                        "short_desc": "Org ID",
                        "desc": "The `org_id` value for this operation.",
                    },
                    "level": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Level",
                        "short_desc": "Level",
                        "desc": "The `level` value for this operation.",
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "Viewer",
                                "display_name": "Viewer",
                            },
                            <AllowedValueInfo>{
                                "value": "Editor",
                                "display_name": "Editor",
                            },
                        ),
                    },
                    "share_with_uuid": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Share With UUID",
                        "short_desc": "The id of the entity (user, workspace or organization) to share with",
                    },
                    "share_with_type": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Share With Type",
                        "short_desc": "The type of entity, used to share a library.",
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "User",
                                "display_name": "User",
                            },
                            <AllowedValueInfo>{
                                "value": "Workspace",
                                "display_name": "Workspace",
                            },
                        ),
                    },
                    "library_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Library ID",
                        "short_desc": "Library ID",
                        "desc": "The `library_id` value for this operation.",
                        "ref_data": "libraries",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-library-access",
                "method": "DELETE",
                "path": "/v1/libraries/{library_id}/share",
                "operation_id": "libraries_share_delete_v1",
                "display_name": "Delete Library Access",
                "short_desc": "Delete Library Access",
                "desc": "Delete an access level. Manage document libraries used to ground assistant answers.\n\n**Public preview:** Availability depends on your account and permissions.\n\n**Deletion:** This operation can permanently remove the selected resource or access.",
                "groups": ("Libraries Accesses",),
                "fields": {
                    "org_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Org ID",
                        "short_desc": "Org ID",
                        "desc": "The `org_id` value for this operation.",
                    },
                    "share_with_uuid": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Share With UUID",
                        "short_desc": "The id of the entity (user, workspace or organization) to share with",
                    },
                    "share_with_type": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Share With Type",
                        "short_desc": "The type of entity, used to share a library.",
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "User",
                                "display_name": "User",
                            },
                            <AllowedValueInfo>{
                                "value": "Workspace",
                                "display_name": "Workspace",
                            },
                        ),
                    },
                    "library_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Library ID",
                        "short_desc": "Library ID",
                        "desc": "The `library_id` value for this operation.",
                        "ref_data": "libraries",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-dataset",
                "method": "POST",
                "path": "/v1/observability/datasets",
                "operation_id": "create_dataset_v1_observability_datasets_post",
                "display_name": "Create dataset",
                "short_desc": "Create Dataset",
                "desc": "Create a new empty dataset. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Datasets",),
                "fields": {
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "Name",
                        "desc": "The `name` value for this operation.",
                        "preselected": True,
                    },
                    "description": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Description",
                        "short_desc": "Description",
                        "desc": "The `description` value for this operation.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-datasets",
                "method": "GET",
                "path": "/v1/observability/datasets",
                "operation_id": "get_datasets_v1_observability_datasets_get",
                "display_name": "List datasets",
                "short_desc": "List Datasets",
                "desc": "List existing datasets. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Datasets",),
                "fields": {
                    "page_size": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "short_desc": "Page Size",
                        "desc": "The `page_size` value for this operation.",
                        "preselected": True,
                    },
                    "page": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page",
                        "short_desc": "Page",
                        "desc": "The `page` value for this operation.",
                        "preselected": True,
                    },
                    "q": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Q",
                        "short_desc": "Q",
                        "desc": "The `q` value for this operation.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-dataset",
                "method": "GET",
                "path": "/v1/observability/datasets/{dataset_id}",
                "operation_id": "get_dataset_by_id_v1_observability_datasets__dataset_id__get",
                "display_name": "Get Dataset",
                "short_desc": "Get Dataset",
                "desc": "Get dataset by id. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Datasets",),
                "fields": {
                    "dataset_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Dataset ID",
                        "short_desc": "Dataset ID",
                        "desc": "The `dataset_id` value for this operation.",
                        "ref_data": "datasets",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-dataset",
                "method": "DELETE",
                "path": "/v1/observability/datasets/{dataset_id}",
                "operation_id": "delete_dataset_v1_observability_datasets__dataset_id__delete",
                "display_name": "Delete Dataset",
                "short_desc": "Delete Dataset",
                "desc": "Delete a dataset. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.\n\n**Deletion:** This operation can permanently remove the selected resource or access.",
                "groups": ("Observability Datasets",),
                "fields": {
                    "dataset_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Dataset ID",
                        "short_desc": "Dataset ID",
                        "desc": "The `dataset_id` value for this operation.",
                        "ref_data": "datasets",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-dataset",
                "method": "PATCH",
                "path": "/v1/observability/datasets/{dataset_id}",
                "operation_id": "update_dataset_v1_observability_datasets__dataset_id__patch",
                "display_name": "Update Dataset",
                "short_desc": "Update Dataset",
                "desc": "Patch dataset. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Datasets",),
                "fields": {
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "Name",
                        "desc": "The `name` value for this operation.",
                        "preselected": True,
                    },
                    "description": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Description",
                        "short_desc": "Description",
                        "desc": "The `description` value for this operation.",
                    },
                    "dataset_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Dataset ID",
                        "short_desc": "Dataset ID",
                        "desc": "The `dataset_id` value for this operation.",
                        "ref_data": "datasets",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-dataset-records",
                "method": "GET",
                "path": "/v1/observability/datasets/{dataset_id}/records",
                "operation_id": "get_dataset_records_v1_observability_datasets__dataset_id__records_get",
                "display_name": "List Dataset Records",
                "short_desc": "List Dataset Records",
                "desc": "List existing records in the dataset. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Datasets",),
                "fields": {
                    "dataset_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Dataset ID",
                        "short_desc": "Dataset ID",
                        "desc": "The `dataset_id` value for this operation.",
                        "ref_data": "datasets",
                        "supports_custom_values": True,
                    },
                    "page_size": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "short_desc": "Page Size",
                        "desc": "The `page_size` value for this operation.",
                        "preselected": True,
                    },
                    "page": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page",
                        "short_desc": "Page",
                        "desc": "The `page` value for this operation.",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "add-dataset-record",
                "method": "POST",
                "path": "/v1/observability/datasets/{dataset_id}/records",
                "operation_id": "create_dataset_record_v1_observability_datasets__dataset_id__records_post",
                "display_name": "Add Dataset Record",
                "short_desc": "Add Dataset Record",
                "desc": "Add a conversation to the dataset. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Datasets",),
                "fields": {
                    "payload": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Payload",
                        "short_desc": "Payload",
                        "desc": "The `payload` value for this operation.",
                    },
                    "properties": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Properties",
                        "short_desc": "Properties",
                        "desc": "The `properties` value for this operation.",
                    },
                    "dataset_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Dataset ID",
                        "short_desc": "Dataset ID",
                        "desc": "The `dataset_id` value for this operation.",
                        "ref_data": "datasets",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "import-dataset-from-file",
                "method": "POST",
                "path": "/v1/observability/datasets/{dataset_id}/imports/from-file",
                "operation_id": "post_dataset_records_from_file_v1_observability_datasets__dataset_id__imports_from_file_post",
                "display_name": "Import Dataset From File",
                "short_desc": "Import Dataset From File",
                "desc": "Populate the dataset with samples from an uploaded file. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Datasets",),
                "fields": {
                    "file_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "File ID",
                        "short_desc": "File ID",
                        "desc": "The `file_id` value for this operation.",
                        "ref_data": "files",
                        "supports_custom_values": True,
                    },
                    "dataset_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Dataset ID",
                        "short_desc": "Dataset ID",
                        "desc": "The `dataset_id` value for this operation.",
                        "ref_data": "datasets",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "import-dataset-from-playground",
                "method": "POST",
                "path": "/v1/observability/datasets/{dataset_id}/imports/from-playground",
                "operation_id": "post_dataset_records_from_playground_v1_observability_datasets__dataset_id__imports_from_playground_post",
                "display_name": "Import Dataset From Playground",
                "short_desc": "Import Dataset From Playground",
                "desc": "Populate the dataset with samples from the playground. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Datasets",),
                "fields": {
                    "conversation_ids": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Conversation IDs",
                        "short_desc": "Conversation IDs",
                        "desc": "The `conversation_ids` value for this operation.",
                        "element_ref_data": "conversations",
                        "supports_custom_values": True,
                    },
                    "dataset_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Dataset ID",
                        "short_desc": "Dataset ID",
                        "desc": "The `dataset_id` value for this operation.",
                        "ref_data": "datasets",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "import-dataset-from-dataset",
                "method": "POST",
                "path": "/v1/observability/datasets/{dataset_id}/imports/from-dataset",
                "operation_id": "post_dataset_records_from_dataset_v1_observability_datasets__dataset_id__imports_from_dataset_post",
                "display_name": "Import Dataset From Dataset",
                "short_desc": "Import Dataset From Dataset",
                "desc": "Populate the dataset with samples from another dataset. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Datasets",),
                "fields": {
                    "dataset_record_ids": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Dataset Record IDs",
                        "short_desc": "Dataset Record IDs",
                        "desc": "The `dataset_record_ids` value for this operation.",
                        "element_ref_data": "dataset-records",
                        "supports_custom_values": True,
                    },
                    "dataset_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Dataset ID",
                        "short_desc": "Dataset ID",
                        "desc": "The `dataset_id` value for this operation.",
                        "ref_data": "datasets",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "export-dataset-jsonl",
                "method": "GET",
                "path": "/v1/observability/datasets/{dataset_id}/exports/to-jsonl",
                "operation_id": "export_dataset_to_jsonl_v1_observability_datasets__dataset_id__exports_to_jsonl_get",
                "display_name": "Export Dataset JSONl",
                "short_desc": "Export Dataset JSONl",
                "desc": "Export to the Files API and retrieve presigned URL to download the resulting JSONL file. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Datasets",),
                "fields": {
                    "dataset_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Dataset ID",
                        "short_desc": "Dataset ID",
                        "desc": "The `dataset_id` value for this operation.",
                        "ref_data": "datasets",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-dataset-task",
                "method": "GET",
                "path": "/v1/observability/datasets/{dataset_id}/tasks/{task_id}",
                "operation_id": "get_dataset_import_task_v1_observability_datasets__dataset_id__tasks__task_id__get",
                "display_name": "Get Dataset Task",
                "short_desc": "Get Dataset Task",
                "desc": "Get status of a dataset import task. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Datasets",),
                "fields": {
                    "dataset_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Dataset ID",
                        "short_desc": "Dataset ID",
                        "desc": "The `dataset_id` value for this operation.",
                        "ref_data": "datasets",
                        "supports_custom_values": True,
                    },
                    "task_id": <RestSchemaFieldOverlayInfo>{
                        "depends_on": ("dataset_id",),
                        "display_name": "Task ID",
                        "short_desc": "Task ID",
                        "desc": "The `task_id` value for this operation.",
                        "ref_data": "dataset-tasks",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-dataset-tasks",
                "method": "GET",
                "path": "/v1/observability/datasets/{dataset_id}/tasks",
                "operation_id": "get_dataset_import_tasks_v1_observability_datasets__dataset_id__tasks_get",
                "display_name": "List Dataset Tasks",
                "short_desc": "List Dataset Tasks",
                "desc": "List import tasks for the given dataset. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Datasets",),
                "fields": {
                    "dataset_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Dataset ID",
                        "short_desc": "Dataset ID",
                        "desc": "The `dataset_id` value for this operation.",
                        "ref_data": "datasets",
                        "supports_custom_values": True,
                    },
                    "page_size": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "short_desc": "Page Size",
                        "desc": "The `page_size` value for this operation.",
                        "preselected": True,
                    },
                    "page": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page",
                        "short_desc": "Page",
                        "desc": "The `page` value for this operation.",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-dataset-record",
                "method": "GET",
                "path": "/v1/observability/dataset-records/{dataset_record_id}",
                "operation_id": "get_dataset_record_v1_observability_dataset_records__dataset_record_id__get",
                "display_name": "Get Dataset Record",
                "short_desc": "Get Dataset Record",
                "desc": "Get the content of a given conversation from a dataset. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Datasets Records",),
                "fields": {
                    "dataset_record_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Dataset Record ID",
                        "short_desc": "Dataset Record ID",
                        "desc": "The `dataset_record_id` value for this operation.",
                        "ref_data": "dataset-records",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-dataset-record",
                "method": "DELETE",
                "path": "/v1/observability/dataset-records/{dataset_record_id}",
                "operation_id": "delete_dataset_record_v1_observability_dataset_records__dataset_record_id__delete",
                "display_name": "Delete Dataset Record",
                "short_desc": "Delete Dataset Record",
                "desc": "Delete a record from a dataset. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.\n\n**Deletion:** This operation can permanently remove the selected resource or access.",
                "groups": ("Observability Datasets Records",),
                "fields": {
                    "dataset_record_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Dataset Record ID",
                        "short_desc": "Dataset Record ID",
                        "desc": "The `dataset_record_id` value for this operation.",
                        "ref_data": "dataset-records",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-dataset-records",
                "method": "POST",
                "path": "/v1/observability/dataset-records/bulk-delete",
                "operation_id": "delete_dataset_records_v1_observability_dataset_records_bulk_delete_post",
                "display_name": "Delete dataset records",
                "short_desc": "Delete Dataset Records",
                "desc": "Delete multiple records from datasets. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Datasets Records",),
                "fields": {
                    "dataset_record_ids": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Dataset Record IDs",
                        "short_desc": "Dataset Record IDs",
                        "desc": "The `dataset_record_ids` value for this operation.",
                        "element_ref_data": "dataset-records",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-dataset-record-payload",
                "method": "PUT",
                "path": "/v1/observability/dataset-records/{dataset_record_id}/payload",
                "operation_id": "update_dataset_record_payload_v1_observability_dataset_records__dataset_record_id__payload_put",
                "display_name": "Update Dataset Record Payload",
                "short_desc": "Update Dataset Record Payload",
                "desc": "Update a dataset record conversation payload. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Datasets Records",),
                "fields": {
                    "payload": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Payload",
                        "short_desc": "Payload",
                        "desc": "The `payload` value for this operation.",
                    },
                    "dataset_record_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Dataset Record ID",
                        "short_desc": "Dataset Record ID",
                        "desc": "The `dataset_record_id` value for this operation.",
                        "ref_data": "dataset-records",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-dataset-record-properties",
                "method": "PUT",
                "path": "/v1/observability/dataset-records/{dataset_record_id}/properties",
                "operation_id": "update_dataset_record_properties_v1_observability_dataset_records__dataset_record_id__properties_put",
                "display_name": "Update Dataset Record Properties",
                "short_desc": "Update Dataset Record Properties",
                "desc": "Update conversation properties. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Datasets Records",),
                "fields": {
                    "properties": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Properties",
                        "short_desc": "Properties",
                        "desc": "The `properties` value for this operation.",
                    },
                    "dataset_record_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Dataset Record ID",
                        "short_desc": "Dataset Record ID",
                        "desc": "The `dataset_record_id` value for this operation.",
                        "ref_data": "dataset-records",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-workflow-execution",
                "method": "GET",
                "path": "/v1/workflows/executions/{execution_id}",
                "operation_id": "get_workflow_execution_v1_workflows_executions__execution_id__get",
                "display_name": "Get Workflow Execution",
                "short_desc": "Get Workflow Execution",
                "desc": "Get Workflow Execution. Manage automated workflows and inspect their execution.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Workflows Executions",),
                "fields": {
                    "execution_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Execution ID",
                        "short_desc": "Execution ID",
                        "desc": "The `execution_id` value for this operation.",
                        "ref_data": "workflow-runs",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-workflow-execution-history",
                "method": "GET",
                "path": "/v1/workflows/executions/{execution_id}/history",
                "operation_id": "get_workflow_execution_history_v1_workflows_executions__execution_id__history_get",
                "display_name": "Get Workflow Execution History",
                "short_desc": "Get Workflow Execution History",
                "desc": "Get Workflow Execution History. Manage automated workflows and inspect their execution.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Workflows Executions",),
                "fields": {
                    "execution_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Execution ID",
                        "short_desc": "Execution ID",
                        "desc": "The `execution_id` value for this operation.",
                        "ref_data": "workflow-runs",
                        "supports_custom_values": True,
                    },
                    "decode_payloads": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Decode Payloads",
                        "short_desc": "Decode Payloads",
                        "desc": "The `decode_payloads` value for this operation.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "signal-workflow-execution",
                "method": "POST",
                "path": "/v1/workflows/executions/{execution_id}/signals",
                "operation_id": "signal_workflow_execution_v1_workflows_executions__execution_id__signals_post",
                "display_name": "Signal Workflow Execution",
                "short_desc": "Signal Workflow Execution",
                "desc": "Signal Workflow Execution. Manage automated workflows and inspect their execution.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Workflows Executions",),
                "fields": {
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "The name of the signal to send",
                        "preselected": True,
                    },
                    "input": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Input",
                        "short_desc": "Input data for the signal, matching its schema",
                        "fields": {
                            "encoding_options": <RestSchemaFieldOverlayInfo>{
                                "element_allowed_values": (
                                    <AllowedValueInfo>{
                                        "value": "offloaded",
                                        "display_name": "Offloaded",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "encrypted",
                                        "display_name": "Encrypted",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "encrypted-partial",
                                        "display_name": "Encrypted Partial",
                                    },
                                ),
                            },
                        },
                    },
                    "execution_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Execution ID",
                        "short_desc": "Execution ID",
                        "desc": "The `execution_id` value for this operation.",
                        "ref_data": "workflow-runs",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "query-workflow-execution",
                "method": "POST",
                "mutates_state": False,
                "path": "/v1/workflows/executions/{execution_id}/queries",
                "operation_id": "query_workflow_execution_v1_workflows_executions__execution_id__queries_post",
                "display_name": "Query Workflow Execution",
                "short_desc": "Query Workflow Execution",
                "desc": "Query Workflow Execution. Manage automated workflows and inspect their execution.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Workflows Executions",),
                "fields": {
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "The name of the query to request",
                        "preselected": True,
                    },
                    "input": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Input",
                        "short_desc": "Input data for the query, matching its schema",
                        "fields": {
                            "encoding_options": <RestSchemaFieldOverlayInfo>{
                                "element_allowed_values": (
                                    <AllowedValueInfo>{
                                        "value": "offloaded",
                                        "display_name": "Offloaded",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "encrypted",
                                        "display_name": "Encrypted",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "encrypted-partial",
                                        "display_name": "Encrypted Partial",
                                    },
                                ),
                            },
                        },
                    },
                    "execution_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Execution ID",
                        "short_desc": "Execution ID",
                        "desc": "The `execution_id` value for this operation.",
                        "ref_data": "workflow-runs",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "terminate-workflow-execution",
                "method": "POST",
                "path": "/v1/workflows/executions/{execution_id}/terminate",
                "operation_id": "terminate_workflow_execution_v1_workflows_executions__execution_id__terminate_post",
                "display_name": "Terminate Workflow Execution",
                "short_desc": "Terminate Workflow Execution",
                "desc": "Terminate Workflow Execution. Manage automated workflows and inspect their execution.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Workflows Executions",),
                "fields": {
                    "execution_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Execution ID",
                        "short_desc": "Execution ID",
                        "desc": "The `execution_id` value for this operation.",
                        "ref_data": "workflow-runs",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "batch-terminate-workflow-executions",
                "method": "POST",
                "path": "/v1/workflows/executions/terminate",
                "operation_id": "batch_terminate_workflow_executions_v1_workflows_executions_terminate_post",
                "display_name": "Batch terminate workflow executions",
                "short_desc": "Batch Terminate Workflow Executions",
                "desc": "Batch Terminate Workflow Executions. Manage automated workflows and inspect their execution.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Workflows Executions",),
                "fields": {
                    "execution_ids": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Execution IDs",
                        "short_desc": "List of execution IDs to process",
                        "element_ref_data": "workflow-runs",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "cancel-workflow-execution",
                "method": "POST",
                "path": "/v1/workflows/executions/{execution_id}/cancel",
                "operation_id": "cancel_workflow_execution_v1_workflows_executions__execution_id__cancel_post",
                "display_name": "Cancel Workflow Execution",
                "short_desc": "Cancel Workflow Execution",
                "desc": "Cancel Workflow Execution. Manage automated workflows and inspect their execution.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Workflows Executions",),
                "fields": {
                    "execution_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Execution ID",
                        "short_desc": "Execution ID",
                        "desc": "The `execution_id` value for this operation.",
                        "ref_data": "workflow-runs",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "batch-cancel-workflow-executions",
                "method": "POST",
                "path": "/v1/workflows/executions/cancel",
                "operation_id": "batch_cancel_workflow_executions_v1_workflows_executions_cancel_post",
                "display_name": "Batch cancel workflow executions",
                "short_desc": "Batch Cancel Workflow Executions",
                "desc": "Batch Cancel Workflow Executions. Manage automated workflows and inspect their execution.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Workflows Executions",),
                "fields": {
                    "execution_ids": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Execution IDs",
                        "short_desc": "List of execution IDs to process",
                        "element_ref_data": "workflow-runs",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "reset-workflow",
                "method": "POST",
                "path": "/v1/workflows/executions/{execution_id}/reset",
                "operation_id": "reset_workflow_v1_workflows_executions__execution_id__reset_post",
                "display_name": "Reset Workflow",
                "short_desc": "Reset Workflow",
                "desc": "Reset Workflow. Manage automated workflows and inspect their execution.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Workflows Executions",),
                "fields": {
                    "event_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Event ID",
                        "short_desc": "The event ID to reset the workflow execution to",
                    },
                    "reason": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Reason",
                        "short_desc": "Reason for resetting the workflow execution",
                    },
                    "exclude_signals": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Exclude Signals",
                        "short_desc": "Whether to exclude signals that happened after the reset point",
                    },
                    "exclude_updates": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Exclude Updates",
                        "short_desc": "Whether to exclude updates that happened after the reset point",
                    },
                    "execution_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Execution ID",
                        "short_desc": "Execution ID",
                        "desc": "The `execution_id` value for this operation.",
                        "ref_data": "workflow-runs",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-workflow-execution",
                "method": "POST",
                "path": "/v1/workflows/executions/{execution_id}/updates",
                "operation_id": "update_workflow_execution_v1_workflows_executions__execution_id__updates_post",
                "display_name": "Update Workflow Execution",
                "short_desc": "Update Workflow Execution",
                "desc": "Update Workflow Execution. Manage automated workflows and inspect their execution.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Workflows Executions",),
                "fields": {
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "The name of the update to request",
                        "preselected": True,
                    },
                    "input": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Input",
                        "short_desc": "Input data for the update, matching its schema",
                        "fields": {
                            "encoding_options": <RestSchemaFieldOverlayInfo>{
                                "element_allowed_values": (
                                    <AllowedValueInfo>{
                                        "value": "offloaded",
                                        "display_name": "Offloaded",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "encrypted",
                                        "display_name": "Encrypted",
                                    },
                                    <AllowedValueInfo>{
                                        "value": "encrypted-partial",
                                        "display_name": "Encrypted Partial",
                                    },
                                ),
                            },
                        },
                    },
                    "execution_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Execution ID",
                        "short_desc": "Execution ID",
                        "desc": "The `execution_id` value for this operation.",
                        "ref_data": "workflow-runs",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-workflow-execution-trace-otel",
                "method": "GET",
                "path": "/v1/workflows/executions/{execution_id}/trace/otel",
                "operation_id": "get_workflow_execution_trace_otel",
                "display_name": "Get Workflow Execution Trace Otel",
                "short_desc": "Get Workflow Execution Trace Otel",
                "desc": "Get Workflow Execution Trace Otel. Manage automated workflows and inspect their execution.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Workflows Executions",),
                "fields": {
                    "execution_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Execution ID",
                        "short_desc": "Execution ID",
                        "desc": "The `execution_id` value for this operation.",
                        "ref_data": "workflow-runs",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-workflow-execution-trace-summary",
                "method": "GET",
                "path": "/v1/workflows/executions/{execution_id}/trace/summary",
                "operation_id": "get_workflow_execution_trace_summary",
                "display_name": "Get Workflow Execution Trace Summary",
                "short_desc": "Get Workflow Execution Trace Summary",
                "desc": "Get Workflow Execution Trace Summary. Manage automated workflows and inspect their execution.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Workflows Executions",),
                "fields": {
                    "execution_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Execution ID",
                        "short_desc": "Execution ID",
                        "desc": "The `execution_id` value for this operation.",
                        "ref_data": "workflow-runs",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-workflow-execution-trace-events",
                "method": "GET",
                "path": "/v1/workflows/executions/{execution_id}/trace/events",
                "operation_id": "get_workflow_execution_trace_events",
                "display_name": "Get Workflow Execution Trace Events",
                "short_desc": "Get Workflow Execution Trace Events",
                "desc": "Get Workflow Execution Trace Events. Manage automated workflows and inspect their execution.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Workflows Executions",),
                "fields": {
                    "execution_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Execution ID",
                        "short_desc": "Execution ID",
                        "desc": "The `execution_id` value for this operation.",
                        "ref_data": "workflow-runs",
                        "supports_custom_values": True,
                    },
                    "merge_same_id_events": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Merge Same ID Events",
                        "short_desc": "Merge Same ID Events",
                        "desc": "The `merge_same_id_events` value for this operation.",
                    },
                    "include_internal_events": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Include Internal Events",
                        "short_desc": "Include Internal Events",
                        "desc": "The `include_internal_events` value for this operation.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-workflow-metrics",
                "method": "GET",
                "path": "/v1/workflows/{workflow_name}/metrics",
                "operation_id": "get_workflow_metrics_v1_workflows__workflow_name__metrics_get",
                "display_name": "Get Workflow Metrics",
                "short_desc": "Get Workflow Metrics",
                "desc": "Get Workflow Metrics. Manage automated workflows and inspect their execution.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Workflows Metrics",),
                "fields": {
                    "workflow_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Workflow Name",
                        "short_desc": "Workflow Name",
                        "desc": "The `workflow_name` value for this operation.",
                        "ref_data": "workflow-names",
                        "supports_custom_values": True,
                    },
                    "start_time": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Start Time",
                        "short_desc": "Filter workflows started after this time (ISO 8601)",
                    },
                    "end_time": <RestSchemaFieldOverlayInfo>{
                        "display_name": "End Time",
                        "short_desc": "Filter workflows started before this time (ISO 8601)",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-workflow-runs",
                "method": "GET",
                "path": "/v1/workflows/runs",
                "operation_id": "list_runs_v1_workflows_runs_get",
                "display_name": "List workflow runs",
                "short_desc": "List Workflow Runs",
                "desc": "List Runs. Manage automated workflows and inspect their execution.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Workflows Runs",),
                "fields": {
                    "workflow_identifier": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Workflow Identifier",
                        "short_desc": "Filter by workflow name or id",
                        "ref_data": "workflows",
                        "supports_custom_values": True,
                    },
                    "search": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Search",
                        "short_desc": "Search by workflow name, display name or id",
                        "preselected": True,
                    },
                    "status": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Status",
                        "short_desc": "Filter by workflow status",
                        "preselected": True,
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "RUNNING",
                                "display_name": "Running",
                            },
                            <AllowedValueInfo>{
                                "value": "COMPLETED",
                                "display_name": "Completed",
                            },
                            <AllowedValueInfo>{
                                "value": "FAILED",
                                "display_name": "Failed",
                            },
                            <AllowedValueInfo>{
                                "value": "CANCELED",
                                "display_name": "Canceled",
                            },
                            <AllowedValueInfo>{
                                "value": "TERMINATED",
                                "display_name": "Terminated",
                            },
                            <AllowedValueInfo>{
                                "value": "CONTINUED_AS_NEW",
                                "display_name": "Continued As New",
                            },
                            <AllowedValueInfo>{
                                "value": "TIMED_OUT",
                                "display_name": "Timed Out",
                            },
                            <AllowedValueInfo>{
                                "value": "RETRYING_AFTER_ERROR",
                                "display_name": "Retrying After Error",
                            },
                        ),
                        "supports_custom_values": True,
                    },
                    "page_size": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "short_desc": "Number of items per page",
                        "preselected": True,
                    },
                    "next_page_token": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Next Page Token",
                        "short_desc": "Token for the next page of results",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-workflow-run",
                "method": "GET",
                "path": "/v1/workflows/runs/{run_id}",
                "operation_id": "get_run_v1_workflows_runs__run_id__get",
                "display_name": "Get Workflow Run",
                "short_desc": "Get Workflow Run",
                "desc": "Get Run. Manage automated workflows and inspect their execution.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Workflows Runs",),
                "fields": {
                    "run_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Run ID",
                        "short_desc": "Run ID",
                        "desc": "The `run_id` value for this operation.",
                        "ref_data": "workflow-runs",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-workflow-run-history",
                "method": "GET",
                "path": "/v1/workflows/runs/{run_id}/history",
                "operation_id": "get_run_history_v1_workflows_runs__run_id__history_get",
                "display_name": "Get Workflow Run History",
                "short_desc": "Get Workflow Run History",
                "desc": "Get Run History. Manage automated workflows and inspect their execution.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Workflows Runs",),
                "fields": {
                    "run_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Run ID",
                        "short_desc": "Run ID",
                        "desc": "The `run_id` value for this operation.",
                        "ref_data": "workflow-runs",
                        "supports_custom_values": True,
                    },
                    "decode_payloads": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Decode Payloads",
                        "short_desc": "Decode Payloads",
                        "desc": "The `decode_payloads` value for this operation.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-workflow-schedules",
                "method": "GET",
                "path": "/v1/workflows/schedules",
                "operation_id": "get_schedules_v1_workflows_schedules_get",
                "display_name": "List workflow schedules",
                "short_desc": "List Workflow Schedules",
                "desc": "Get Schedules. Manage automated workflows and inspect their execution.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Workflows Schedules",),
            },
            <RestSchemaActionInfo>{
                "action": "schedule-workflow",
                "method": "POST",
                "path": "/v1/workflows/schedules",
                "operation_id": "schedule_workflow_v1_workflows_schedules_post",
                "display_name": "Schedule workflow",
                "short_desc": "Schedule Workflow",
                "desc": "Schedule Workflow. Manage automated workflows and inspect their execution.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Workflows Schedules",),
                "fields": {
                    "schedule": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Schedule",
                        "short_desc": "The schedule definition",
                        "fields": {
                            "policy": <RestSchemaFieldOverlayInfo>{
                                "fields": {
                                    "overlap": <RestSchemaFieldOverlayInfo>{
                                        "allowed_values": (
                                            <AllowedValueInfo>{
                                                "value": 1,
                                                "display_name": "1",
                                            },
                                            <AllowedValueInfo>{
                                                "value": 2,
                                                "display_name": "2",
                                            },
                                            <AllowedValueInfo>{
                                                "value": 3,
                                                "display_name": "3",
                                            },
                                            <AllowedValueInfo>{
                                                "value": 4,
                                                "display_name": "4",
                                            },
                                            <AllowedValueInfo>{
                                                "value": 5,
                                                "display_name": "5",
                                            },
                                            <AllowedValueInfo>{
                                                "value": 6,
                                                "display_name": "6",
                                            },
                                        ),
                                    },
                                },
                            },
                            "schedule_id": <RestSchemaFieldOverlayInfo>{
                                "ref_data": "workflow-schedules",
                                "supports_custom_values": True,
                            },
                        },
                    },
                    "workflow_registration_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Workflow Registration ID",
                        "short_desc": "The ID of the workflow registration to schedule",
                        "ref_data": "workflow-registrations",
                        "supports_custom_values": True,
                    },
                    "workflow_version_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Workflow Version ID",
                        "short_desc": "Deprecated: use workflow_registration_id",
                        "ref_data": "workflow-registrations",
                        "supports_custom_values": True,
                    },
                    "workflow_identifier": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Workflow Identifier",
                        "short_desc": "The name or ID of the workflow to schedule",
                        "ref_data": "workflows",
                        "supports_custom_values": True,
                    },
                    "workflow_task_queue": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Workflow Task Queue",
                        "short_desc": "Deprecated",
                    },
                    "schedule_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Schedule ID",
                        "short_desc": "Allows you to specify a custom schedule ID",
                        "ref_data": "workflow-schedules",
                        "supports_custom_values": True,
                    },
                    "deployment_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Deployment Name",
                        "short_desc": "Name of the deployment to route this schedule to",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "unschedule-workflow",
                "method": "DELETE",
                "path": "/v1/workflows/schedules/{schedule_id}",
                "operation_id": "unschedule_workflow_v1_workflows_schedules__schedule_id__delete",
                "display_name": "Unschedule Workflow",
                "short_desc": "Unschedule Workflow",
                "desc": "Unschedule Workflow. Manage automated workflows and inspect their execution.\n\n**Public preview:** Availability depends on your account and permissions.\n\n**Deletion:** This operation can permanently remove the selected resource or access.",
                "groups": ("Workflows Schedules",),
                "fields": {
                    "schedule_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Schedule ID",
                        "short_desc": "Schedule ID",
                        "desc": "The `schedule_id` value for this operation.",
                        "ref_data": "workflow-schedules",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-workflow-events",
                "method": "GET",
                "path": "/v1/workflows/events/list",
                "operation_id": "get_workflow_events_v1_workflows_events_list_get",
                "display_name": "Get workflow events",
                "short_desc": "Get Workflow Events",
                "desc": "Get Workflow Events. Manage automated workflows and inspect their execution.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Workflows Events",),
                "fields": {
                    "root_workflow_exec_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Root Workflow Exec ID",
                        "short_desc": "Execution ID of the root workflow that initiated this execution chain.",
                        "ref_data": "workflow-runs",
                        "supports_custom_values": True,
                    },
                    "workflow_exec_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Workflow Exec ID",
                        "short_desc": "Execution ID of the workflow that emitted this event.",
                        "ref_data": "workflow-runs",
                        "supports_custom_values": True,
                    },
                    "workflow_run_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Workflow Run ID",
                        "short_desc": "Run ID of the workflow that emitted this event.",
                        "ref_data": "workflow-runs",
                        "supports_custom_values": True,
                    },
                    "limit": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Limit",
                        "short_desc": "Maximum number of events to return.",
                        "preselected": True,
                    },
                    "cursor": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Cursor",
                        "short_desc": "Cursor for pagination.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-workflow-deployments",
                "method": "GET",
                "path": "/v1/workflows/deployments",
                "operation_id": "list_deployments_v1_workflows_deployments_get",
                "display_name": "List workflow deployments",
                "short_desc": "List Workflow Deployments",
                "desc": "List Deployments. Manage automated workflows and inspect their execution.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Workflows Deployments",),
                "fields": {
                    "active_only": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Active Only",
                        "short_desc": "Active Only",
                        "desc": "The `active_only` value for this operation.",
                    },
                    "workflow_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Workflow Name",
                        "short_desc": "Workflow Name",
                        "desc": "The `workflow_name` value for this operation.",
                        "ref_data": "workflow-names",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-workflow-deployment",
                "method": "GET",
                "path": "/v1/workflows/deployments/{name}",
                "operation_id": "get_deployment_v1_workflows_deployments__name__get",
                "display_name": "Get Workflow Deployment",
                "short_desc": "Get Workflow Deployment",
                "desc": "Get Deployment. Manage automated workflows and inspect their execution.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Workflows Deployments",),
                "fields": {
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "Name",
                        "desc": "The `name` value for this operation.",
                        "preselected": True,
                        "ref_data": "workflow-deployments",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-workflow-registrations",
                "method": "GET",
                "path": "/v1/workflows/registrations",
                "operation_id": "get_workflow_registrations_v1_workflows_registrations_get",
                "display_name": "List workflow registrations",
                "short_desc": "List Workflow Registrations",
                "desc": "Get Workflow Registrations. Manage automated workflows and inspect their execution.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Workflows",),
                "fields": {
                    "workflow_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Workflow ID",
                        "short_desc": "The workflow ID to filter by",
                        "ref_data": "workflows",
                        "supports_custom_values": True,
                    },
                    "task_queue": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Task Queue",
                        "short_desc": "The task queue to filter by",
                    },
                    "active_only": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Active Only",
                        "short_desc": "Whether to only return active workflows versions",
                    },
                    "include_shared": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Include Shared",
                        "short_desc": "Whether to include shared workflow versions",
                    },
                    "workflow_search": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Workflow Search",
                        "short_desc": "The workflow name to filter by",
                    },
                    "archived": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Archived",
                        "short_desc": "Filter by archived state",
                    },
                    "with_workflow": <RestSchemaFieldOverlayInfo>{
                        "display_name": "With Workflow",
                        "short_desc": "Whether to include the workflow definition",
                    },
                    "available_in_chat_assistant": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Available In Chat Assistant",
                        "short_desc": "Whether to only return workflows compatible with chat assistant",
                    },
                    "limit": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Limit",
                        "short_desc": "The maximum number of workflows versions to return",
                        "preselected": True,
                    },
                    "cursor": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Cursor",
                        "short_desc": "The cursor for pagination",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "execute-workflow",
                "method": "POST",
                "path": "/v1/workflows/{workflow_identifier}/execute",
                "operation_id": "execute_workflow_v1_workflows__workflow_identifier__execute_post",
                "display_name": "Execute Workflow",
                "short_desc": "Execute Workflow",
                "desc": "Execute Workflow. Manage automated workflows and inspect their execution.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Workflows",),
                "fields": {
                    "execution_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Execution ID",
                        "short_desc": "Allows you to specify a custom execution ID",
                        "ref_data": "workflow-runs",
                        "supports_custom_values": True,
                    },
                    "input": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Input",
                        "short_desc": "The input to the workflow",
                    },
                    "wait_for_result": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Wait For Result",
                        "short_desc": "If true, wait for the workflow to complete and return the result directly.",
                    },
                    "timeout_seconds": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Timeout Seconds",
                        "short_desc": "Maximum time to wait for completion when wait_for_result is true.",
                    },
                    "custom_tracing_attributes": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Custom Tracing Attributes",
                        "short_desc": "Custom Tracing Attributes",
                        "desc": "The `custom_tracing_attributes` value for this operation.",
                    },
                    "task_queue": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Task Queue",
                        "short_desc": "Deprecated",
                    },
                    "deployment_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Deployment Name",
                        "short_desc": "Name of the deployment to route this execution to",
                    },
                    "workflow_identifier": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Workflow Identifier",
                        "short_desc": "Workflow Identifier",
                        "desc": "The `workflow_identifier` value for this operation.",
                        "ref_data": "workflows",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "execute-workflow-registration",
                "method": "POST",
                "path": "/v1/workflows/registrations/{workflow_registration_id}/execute",
                "operation_id": "execute_workflow_registration_v1_workflows_registrations__workflow_registration_id__execute_post",
                "display_name": "Execute Workflow Registration",
                "short_desc": "Execute Workflow Registration",
                "desc": "Execute Workflow Registration. Manage automated workflows and inspect their execution.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Workflows",),
                "fields": {
                    "execution_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Execution ID",
                        "short_desc": "Allows you to specify a custom execution ID",
                        "ref_data": "workflow-runs",
                        "supports_custom_values": True,
                    },
                    "input": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Input",
                        "short_desc": "The input to the workflow",
                    },
                    "wait_for_result": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Wait For Result",
                        "short_desc": "If true, wait for the workflow to complete and return the result directly.",
                    },
                    "timeout_seconds": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Timeout Seconds",
                        "short_desc": "Maximum time to wait for completion when wait_for_result is true.",
                    },
                    "custom_tracing_attributes": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Custom Tracing Attributes",
                        "short_desc": "Custom Tracing Attributes",
                        "desc": "The `custom_tracing_attributes` value for this operation.",
                    },
                    "task_queue": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Task Queue",
                        "short_desc": "Deprecated",
                    },
                    "deployment_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Deployment Name",
                        "short_desc": "Name of the deployment to route this execution to",
                    },
                    "workflow_registration_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Workflow Registration ID",
                        "short_desc": "Workflow Registration ID",
                        "desc": "The `workflow_registration_id` value for this operation.",
                        "ref_data": "workflow-registrations",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-workflow",
                "method": "GET",
                "path": "/v1/workflows/{workflow_identifier}",
                "operation_id": "get_workflow_v1_workflows__workflow_identifier__get",
                "display_name": "Get Workflow",
                "short_desc": "Get Workflow",
                "desc": "Get Workflow. Manage automated workflows and inspect their execution.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Workflows",),
                "fields": {
                    "workflow_identifier": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Workflow Identifier",
                        "short_desc": "Workflow Identifier",
                        "desc": "The `workflow_identifier` value for this operation.",
                        "ref_data": "workflows",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-workflow",
                "method": "PUT",
                "path": "/v1/workflows/{workflow_identifier}",
                "operation_id": "update_workflow_v1_workflows__workflow_identifier__put",
                "display_name": "Update Workflow",
                "short_desc": "Update Workflow",
                "desc": "Update Workflow. Manage automated workflows and inspect their execution.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Workflows",),
                "fields": {
                    "display_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Display Name",
                        "short_desc": "New display name value",
                    },
                    "description": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Description",
                        "short_desc": "New description value",
                    },
                    "available_in_chat_assistant": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Available In Chat Assistant",
                        "short_desc": "Whether to make the workflow available in the chat assistant",
                    },
                    "workflow_identifier": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Workflow Identifier",
                        "short_desc": "Workflow Identifier",
                        "desc": "The `workflow_identifier` value for this operation.",
                        "ref_data": "workflows",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-workflow-registration",
                "method": "GET",
                "path": "/v1/workflows/registrations/{workflow_registration_id}",
                "operation_id": "get_workflow_registration_v1_workflows_registrations__workflow_registration_id__get",
                "display_name": "Get Workflow Registration",
                "short_desc": "Get Workflow Registration",
                "desc": "Get Workflow Registration. Manage automated workflows and inspect their execution.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Workflows",),
                "fields": {
                    "workflow_registration_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Workflow Registration ID",
                        "short_desc": "Workflow Registration ID",
                        "desc": "The `workflow_registration_id` value for this operation.",
                        "ref_data": "workflow-registrations",
                        "supports_custom_values": True,
                    },
                    "with_workflow": <RestSchemaFieldOverlayInfo>{
                        "display_name": "With Workflow",
                        "short_desc": "Whether to include the workflow definition",
                    },
                    "include_shared": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Include Shared",
                        "short_desc": "Whether to include shared workflow versions",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "archive-workflow",
                "method": "PUT",
                "path": "/v1/workflows/{workflow_identifier}/archive",
                "operation_id": "archive_workflow_v1_workflows__workflow_identifier__archive_put",
                "display_name": "Archive Workflow",
                "short_desc": "Archive Workflow",
                "desc": "Archive Workflow. Manage automated workflows and inspect their execution.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Workflows",),
                "fields": {
                    "workflow_identifier": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Workflow Identifier",
                        "short_desc": "Workflow Identifier",
                        "desc": "The `workflow_identifier` value for this operation.",
                        "ref_data": "workflows",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "unarchive-workflow",
                "method": "PUT",
                "path": "/v1/workflows/{workflow_identifier}/unarchive",
                "operation_id": "unarchive_workflow_v1_workflows__workflow_identifier__unarchive_put",
                "display_name": "Unarchive Workflow",
                "short_desc": "Unarchive Workflow",
                "desc": "Unarchive Workflow. Manage automated workflows and inspect their execution.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Workflows",),
                "fields": {
                    "workflow_identifier": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Workflow Identifier",
                        "short_desc": "Workflow Identifier",
                        "desc": "The `workflow_identifier` value for this operation.",
                        "ref_data": "workflows",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-connector",
                "method": "POST",
                "path": "/v1/connectors",
                "operation_id": "connector_create_v1",
                "display_name": "Create connector",
                "short_desc": "Create Connector",
                "desc": "Create a new connector. Connect assistants to external business tools and manage their access.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Connectors",),
                "fields": {
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "The name of the connector",
                        "preselected": True,
                    },
                    "description": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Description",
                        "short_desc": "The description of the connector.",
                    },
                    "icon_url": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Icon URL",
                        "short_desc": "The optional url of the icon you want to associate to the connector.",
                    },
                    "visibility": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Visibility",
                        "short_desc": "Visibility of the connector",
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "shared_org",
                                "display_name": "Shared Org",
                            },
                            <AllowedValueInfo>{
                                "value": "shared_workspace",
                                "display_name": "Shared Workspace",
                            },
                            <AllowedValueInfo>{
                                "value": "private",
                                "display_name": "Private",
                            },
                        ),
                    },
                    "server": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Server",
                        "short_desc": "The url of the MCP server.",
                    },
                    "headers": <RestSchemaFieldOverlayInfo>{
                        "sensitive": True,
                        "display_name": "Headers",
                        "short_desc": "Headers",
                    },
                    "system_prompt": <RestSchemaFieldOverlayInfo>{
                        "display_name": "System Prompt",
                        "short_desc": "Optional system prompt for the connector.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-connectors",
                "method": "GET",
                "path": "/v1/connectors",
                "operation_id": "connector_list_v1",
                "display_name": "List connectors",
                "short_desc": "List Connectors",
                "desc": "List all connectors. Connect assistants to external business tools and manage their access.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Connectors",),
                "fields": {
                    "query_filters": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Query Filters",
                        "short_desc": "Query Filters",
                        "desc": "The `query_filters` value for this operation.",
                    },
                    "cursor": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Cursor",
                        "short_desc": "Cursor",
                        "desc": "The `cursor` value for this operation.",
                    },
                    "page_size": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "short_desc": "Page Size",
                        "desc": "The `page_size` value for this operation.",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-connector-auth-url",
                "method": "GET",
                "path": "/v1/connectors/{connector_id_or_name}/auth_url",
                "operation_id": "connector_get_auth_url_v1",
                "display_name": "Get Connector Auth URL",
                "short_desc": "Get Connector Auth URL",
                "desc": "Get the auth URL for a connector. Connect assistants to external business tools and manage their access.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Connectors",),
                "fields": {
                    "app_return_url": <RestSchemaFieldOverlayInfo>{
                        "display_name": "App Return URL",
                        "short_desc": "App Return URL",
                        "desc": "The `app_return_url` value for this operation.",
                    },
                    "credentials_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Credentials Name",
                        "short_desc": "Credentials Name",
                        "desc": "The `credentials_name` value for this operation.",
                    },
                    "connector_id_or_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Connector ID Or Name",
                        "short_desc": "Connector ID Or Name",
                        "desc": "The `connector_id_or_name` value for this operation.",
                        "ref_data": "connectors",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "call-connector-tool",
                "method": "POST",
                "path": "/v1/connectors/{connector_id_or_name}/tools/{tool_name}/call",
                "operation_id": "connector_call_tool_v1",
                "display_name": "Call Connector Tool",
                "short_desc": "Call Connector Tool",
                "desc": "Call Connector Tool. Connect assistants to external business tools and manage their access.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Connectors",),
                "fields": {
                    "arguments": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Arguments",
                        "short_desc": "Arguments",
                        "desc": "The `arguments` value for this operation.",
                    },
                    "tool_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Tool Name",
                        "short_desc": "Tool Name",
                        "desc": "The `tool_name` value for this operation.",
                    },
                    "credentials_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Credentials Name",
                        "short_desc": "Credentials Name",
                        "desc": "The `credentials_name` value for this operation.",
                    },
                    "connector_id_or_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Connector ID Or Name",
                        "short_desc": "Connector ID Or Name",
                        "desc": "The `connector_id_or_name` value for this operation.",
                        "ref_data": "connectors",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-connector-tools",
                "method": "GET",
                "path": "/v1/connectors/{connector_id_or_name}/tools",
                "operation_id": "connector_list_tools_v1",
                "display_name": "List Connector Tools",
                "short_desc": "List Connector Tools",
                "desc": "List tools for a connector. Connect assistants to external business tools and manage their access.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Connectors",),
                "fields": {
                    "page_size": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "short_desc": "Page Size",
                        "desc": "The `page_size` value for this operation.",
                        "preselected": True,
                    },
                    "refresh": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Refresh",
                        "short_desc": "Refresh",
                        "desc": "The `refresh` value for this operation.",
                    },
                    "pretty": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Pretty",
                        "short_desc": "Pretty",
                    },
                    "credentials_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Credentials Name",
                        "short_desc": "Credentials Name",
                        "desc": "The `credentials_name` value for this operation.",
                    },
                    "connector_id_or_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Connector ID Or Name",
                        "short_desc": "Connector ID Or Name",
                        "desc": "The `connector_id_or_name` value for this operation.",
                        "ref_data": "connectors",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-connector-authentication-methods",
                "method": "GET",
                "path": "/v1/connectors/{connector_id_or_name}/authentication_methods",
                "operation_id": "connector_get_authentication_methods_v1",
                "display_name": "Get Connector Authentication Methods",
                "short_desc": "Get Connector Authentication Methods",
                "desc": "Get authentication methods for a connector. Connect assistants to external business tools and manage their access.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Connectors",),
                "fields": {
                    "connector_id_or_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Connector ID Or Name",
                        "short_desc": "Connector ID Or Name",
                        "desc": "The `connector_id_or_name` value for this operation.",
                        "ref_data": "connectors",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-organization-credentials-for-a-connector",
                "method": "GET",
                "path": "/v1/connectors/{connector_id_or_name}/organization/credentials",
                "operation_id": "connector_list_organization_credentials_v1",
                "display_name": "List Organization Credentials For A Connector",
                "short_desc": "List Organization Credentials For A Connector",
                "desc": "List organization credentials for a connector. Connect assistants to external business tools and manage their access.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Connectors",),
                "fields": {
                    "auth_type": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Auth Type",
                        "short_desc": "Auth Type",
                        "desc": "The `auth_type` value for this operation.",
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "oauth2",
                                "display_name": "Oauth2",
                            },
                            <AllowedValueInfo>{
                                "value": "bearer",
                                "display_name": "Bearer",
                            },
                            <AllowedValueInfo>{
                                "value": "none",
                                "display_name": "None",
                            },
                        ),
                    },
                    "fetch_default": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Fetch Default",
                        "short_desc": "Fetch Default",
                        "desc": "The `fetch_default` value for this operation.",
                    },
                    "connector_id_or_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Connector ID Or Name",
                        "short_desc": "Connector ID Or Name",
                        "desc": "The `connector_id_or_name` value for this operation.",
                        "ref_data": "connectors",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-or-update-organization-credentials-for-a-connector",
                "method": "PATCH",
                "path": "/v1/connectors/{connector_id_or_name}/{consumer_scope}/credentials",
                "operation_id": "connector_update_credentials",
                "display_name": "Create Or Update Organization Credentials For A Connector",
                "short_desc": "Create Or Update Organization Credentials For A Connector",
                "desc": "Create or update organization credentials for a connector. Connect assistants to external business tools and manage their access.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Connectors",),
                "fields": {
                    "consumer_scope": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Consumer Scope",
                        "short_desc": "Always \"organization\" for this action",
                        "desc": "Fixed to `organization`: this action manages the organization credentials of the connector.",
                        "required": True,
                        "default_value": "organization",
                        "allowed_values": ("organization",),
                        "preselected": True,
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "Name of the credentials",
                        "preselected": True,
                    },
                    "is_default": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Is Default",
                        "short_desc": "Controls whether this credential is the default for its auth method",
                    },
                    "credentials": <RestSchemaFieldOverlayInfo>{
                        "sensitive": True,
                        "display_name": "Credentials",
                        "short_desc": "The credential data (headers, bearer_token).",
                        "fields": {
                            "oauth": <RestSchemaFieldOverlayInfo>{
                                "fields": {
                                    "client_secret": <RestSchemaFieldOverlayInfo>{
                                        "sensitive": True,
                                    },
                                    "client_private_key": <RestSchemaFieldOverlayInfo>{
                                        "sensitive": True,
                                    },
                                },
                            },
                        },
                    },
                    "connector_id_or_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Connector ID Or Name",
                        "short_desc": "Connector ID Or Name",
                        "desc": "The `connector_id_or_name` value for this operation.",
                        "ref_data": "connectors",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-workspace-credentials-for-a-connector",
                "method": "GET",
                "path": "/v1/connectors/{connector_id_or_name}/workspace/credentials",
                "operation_id": "connector_list_workspace_credentials_v1",
                "display_name": "List Workspace Credentials For A Connector",
                "short_desc": "List Workspace Credentials For A Connector",
                "desc": "List workspace credentials for a connector. Connect assistants to external business tools and manage their access.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Connectors",),
                "fields": {
                    "auth_type": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Auth Type",
                        "short_desc": "Auth Type",
                        "desc": "The `auth_type` value for this operation.",
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "oauth2",
                                "display_name": "Oauth2",
                            },
                            <AllowedValueInfo>{
                                "value": "bearer",
                                "display_name": "Bearer",
                            },
                            <AllowedValueInfo>{
                                "value": "none",
                                "display_name": "None",
                            },
                        ),
                    },
                    "fetch_default": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Fetch Default",
                        "short_desc": "Fetch Default",
                        "desc": "The `fetch_default` value for this operation.",
                    },
                    "connector_id_or_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Connector ID Or Name",
                        "short_desc": "Connector ID Or Name",
                        "desc": "The `connector_id_or_name` value for this operation.",
                        "ref_data": "connectors",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-or-update-workspace-credentials-for-a-connector",
                "method": "PATCH",
                "path": "/v1/connectors/{connector_id_or_name}/{consumer_scope}/credentials",
                "operation_id": "connector_update_credentials",
                "variant_of": "create-or-update-organization-credentials-for-a-connector",
                "display_name": "Create Or Update Workspace Credentials For A Connector",
                "short_desc": "Create Or Update Workspace Credentials For A Connector",
                "desc": "Create or update workspace credentials for a connector. Connect assistants to external business tools and manage their access.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Connectors",),
                "fields": {
                    "consumer_scope": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Consumer Scope",
                        "short_desc": "Always \"workspace\" for this action",
                        "desc": "Fixed to `workspace`: this action manages the workspace credentials of the connector.",
                        "required": True,
                        "default_value": "workspace",
                        "allowed_values": ("workspace",),
                        "preselected": True,
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "Name of the credentials",
                        "preselected": True,
                    },
                    "is_default": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Is Default",
                        "short_desc": "Controls whether this credential is the default for its auth method",
                    },
                    "credentials": <RestSchemaFieldOverlayInfo>{
                        "sensitive": True,
                        "display_name": "Credentials",
                        "short_desc": "The credential data (headers, bearer_token).",
                        "fields": {
                            "oauth": <RestSchemaFieldOverlayInfo>{
                                "fields": {
                                    "client_secret": <RestSchemaFieldOverlayInfo>{
                                        "sensitive": True,
                                    },
                                    "client_private_key": <RestSchemaFieldOverlayInfo>{
                                        "sensitive": True,
                                    },
                                },
                            },
                        },
                    },
                    "connector_id_or_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Connector ID Or Name",
                        "short_desc": "Connector ID Or Name",
                        "desc": "The `connector_id_or_name` value for this operation.",
                        "ref_data": "connectors",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-user-credentials-for-a-connector",
                "method": "GET",
                "path": "/v1/connectors/{connector_id_or_name}/user/credentials",
                "operation_id": "connector_list_user_credentials_v1",
                "display_name": "List User Credentials For A Connector",
                "short_desc": "List User Credentials For A Connector",
                "desc": "List user credentials for a connector. Connect assistants to external business tools and manage their access.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Connectors",),
                "fields": {
                    "auth_type": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Auth Type",
                        "short_desc": "Auth Type",
                        "desc": "The `auth_type` value for this operation.",
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "oauth2",
                                "display_name": "Oauth2",
                            },
                            <AllowedValueInfo>{
                                "value": "bearer",
                                "display_name": "Bearer",
                            },
                            <AllowedValueInfo>{
                                "value": "none",
                                "display_name": "None",
                            },
                        ),
                    },
                    "fetch_default": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Fetch Default",
                        "short_desc": "Fetch Default",
                        "desc": "The `fetch_default` value for this operation.",
                    },
                    "connector_id_or_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Connector ID Or Name",
                        "short_desc": "Connector ID Or Name",
                        "desc": "The `connector_id_or_name` value for this operation.",
                        "ref_data": "connectors",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-or-update-user-credentials-for-a-connector",
                "method": "PATCH",
                "path": "/v1/connectors/{connector_id_or_name}/{consumer_scope}/credentials",
                "operation_id": "connector_update_credentials",
                "variant_of": "create-or-update-organization-credentials-for-a-connector",
                "display_name": "Create Or Update User Credentials For A Connector",
                "short_desc": "Create Or Update User Credentials For A Connector",
                "desc": "Create or update user credentials for a connector. Connect assistants to external business tools and manage their access.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Connectors",),
                "fields": {
                    "consumer_scope": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Consumer Scope",
                        "short_desc": "Always \"user\" for this action",
                        "desc": "Fixed to `user`: this action manages the user credentials of the connector.",
                        "required": True,
                        "default_value": "user",
                        "allowed_values": ("user",),
                        "preselected": True,
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "Name of the credentials",
                        "preselected": True,
                    },
                    "is_default": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Is Default",
                        "short_desc": "Controls whether this credential is the default for its auth method",
                    },
                    "credentials": <RestSchemaFieldOverlayInfo>{
                        "sensitive": True,
                        "display_name": "Credentials",
                        "short_desc": "The credential data (headers, bearer_token).",
                        "fields": {
                            "oauth": <RestSchemaFieldOverlayInfo>{
                                "fields": {
                                    "client_secret": <RestSchemaFieldOverlayInfo>{
                                        "sensitive": True,
                                    },
                                    "client_private_key": <RestSchemaFieldOverlayInfo>{
                                        "sensitive": True,
                                    },
                                },
                            },
                        },
                    },
                    "connector_id_or_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Connector ID Or Name",
                        "short_desc": "Connector ID Or Name",
                        "desc": "The `connector_id_or_name` value for this operation.",
                        "ref_data": "connectors",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-organization-credentials-for-a-connector",
                "method": "DELETE",
                "path": "/v1/connectors/{connector_id_or_name}/{consumer_scope}/credentials/{credentials_name}",
                "operation_id": "connector_delete_credentials",
                "display_name": "Delete Organization Credentials For A Connector",
                "short_desc": "Delete Organization Credentials For A Connector",
                "desc": "Delete organization credentials for a connector. Connect assistants to external business tools and manage their access.\n\n**Public preview:** Availability depends on your account and permissions.\n\n**Deletion:** This operation can permanently remove the selected resource or access.",
                "groups": ("Connectors",),
                "fields": {
                    "consumer_scope": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Consumer Scope",
                        "short_desc": "Always \"organization\" for this action",
                        "desc": "Fixed to `organization`: this action manages the organization credentials of the connector.",
                        "required": True,
                        "default_value": "organization",
                        "allowed_values": ("organization",),
                        "preselected": True,
                    },
                    "credentials_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Credentials Name",
                        "short_desc": "Credentials Name",
                        "desc": "The `credentials_name` value for this operation.",
                    },
                    "connector_id_or_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Connector ID Or Name",
                        "short_desc": "Connector ID Or Name",
                        "desc": "The `connector_id_or_name` value for this operation.",
                        "ref_data": "connectors",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-workspace-credentials-for-a-connector",
                "method": "DELETE",
                "path": "/v1/connectors/{connector_id_or_name}/{consumer_scope}/credentials/{credentials_name}",
                "operation_id": "connector_delete_credentials",
                "variant_of": "delete-organization-credentials-for-a-connector",
                "display_name": "Delete Workspace Credentials For A Connector",
                "short_desc": "Delete Workspace Credentials For A Connector",
                "desc": "Delete workspace credentials for a connector. Connect assistants to external business tools and manage their access.\n\n**Public preview:** Availability depends on your account and permissions.\n\n**Deletion:** This operation can permanently remove the selected resource or access.",
                "groups": ("Connectors",),
                "fields": {
                    "consumer_scope": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Consumer Scope",
                        "short_desc": "Always \"workspace\" for this action",
                        "desc": "Fixed to `workspace`: this action manages the workspace credentials of the connector.",
                        "required": True,
                        "default_value": "workspace",
                        "allowed_values": ("workspace",),
                        "preselected": True,
                    },
                    "credentials_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Credentials Name",
                        "short_desc": "Credentials Name",
                        "desc": "The `credentials_name` value for this operation.",
                    },
                    "connector_id_or_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Connector ID Or Name",
                        "short_desc": "Connector ID Or Name",
                        "desc": "The `connector_id_or_name` value for this operation.",
                        "ref_data": "connectors",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-user-credentials-for-a-connector",
                "method": "DELETE",
                "path": "/v1/connectors/{connector_id_or_name}/{consumer_scope}/credentials/{credentials_name}",
                "operation_id": "connector_delete_credentials",
                "variant_of": "delete-organization-credentials-for-a-connector",
                "display_name": "Delete User Credentials For A Connector",
                "short_desc": "Delete User Credentials For A Connector",
                "desc": "Delete user credentials for a connector. Connect assistants to external business tools and manage their access.\n\n**Public preview:** Availability depends on your account and permissions.\n\n**Deletion:** This operation can permanently remove the selected resource or access.",
                "groups": ("Connectors",),
                "fields": {
                    "consumer_scope": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Consumer Scope",
                        "short_desc": "Always \"user\" for this action",
                        "desc": "Fixed to `user`: this action manages the user credentials of the connector.",
                        "required": True,
                        "default_value": "user",
                        "allowed_values": ("user",),
                        "preselected": True,
                    },
                    "credentials_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Credentials Name",
                        "short_desc": "Credentials Name",
                        "desc": "The `credentials_name` value for this operation.",
                    },
                    "connector_id_or_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Connector ID Or Name",
                        "short_desc": "Connector ID Or Name",
                        "desc": "The `connector_id_or_name` value for this operation.",
                        "ref_data": "connectors",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-connector",
                "method": "GET",
                "path": "/v1/connectors/{connector_id_or_name}",
                "operation_id": "connector_get_v1",
                "display_name": "Get Connector",
                "short_desc": "Get Connector",
                "desc": "Get a connector. Connect assistants to external business tools and manage their access.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Connectors",),
                "fields": {
                    "fetch_user_data": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Fetch User Data",
                        "short_desc": "Fetch the user-level data associated with the connector, such as connection credentials",
                    },
                    "connector_id_or_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Connector ID Or Name",
                        "short_desc": "Connector ID Or Name",
                        "desc": "The `connector_id_or_name` value for this operation.",
                        "ref_data": "connectors",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-connector",
                "method": "PATCH",
                "path": "/v1/connectors/{connector_id_or_name}",
                "operation_id": "connector_update_v1",
                "display_name": "Update Connector",
                "short_desc": "Update Connector",
                "desc": "Update a connector. Connect assistants to external business tools and manage their access.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Connectors",),
                "fields": {
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "The name of the connector.",
                        "preselected": True,
                    },
                    "description": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Description",
                        "short_desc": "The description of the connector.",
                    },
                    "icon_url": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Icon URL",
                        "short_desc": "The optional url of the icon you want to associate to the connector.",
                    },
                    "system_prompt": <RestSchemaFieldOverlayInfo>{
                        "display_name": "System Prompt",
                        "short_desc": "Optional system prompt for the connector.",
                    },
                    "server": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Server",
                        "short_desc": "New server url for your mcp connector.",
                    },
                    "connector_id_or_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Connector ID Or Name",
                        "short_desc": "Connector ID Or Name",
                        "desc": "The `connector_id_or_name` value for this operation.",
                        "ref_data": "connectors",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-connector",
                "method": "DELETE",
                "path": "/v1/connectors/{connector_id_or_name}",
                "operation_id": "connector_delete_v1",
                "display_name": "Delete Connector",
                "short_desc": "Delete Connector",
                "desc": "Delete a connector. Connect assistants to external business tools and manage their access.\n\n**Public preview:** Availability depends on your account and permissions.\n\n**Deletion:** This operation can permanently remove the selected resource or access.",
                "groups": ("Connectors",),
                "fields": {
                    "connector_id_or_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Connector ID Or Name",
                        "short_desc": "Connector ID Or Name",
                        "desc": "The `connector_id_or_name` value for this operation.",
                        "ref_data": "connectors",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "search-logs",
                "method": "POST",
                "mutates_state": False,
                "path": "/v1/observability/logs/search",
                "operation_id": "search_logs_v1_observability_logs_search_post",
                "display_name": "Search logs",
                "short_desc": "Search the logs of your AI activity",
                "desc": "Search the OpenTelemetry logs recorded for your AI activity with a search expression, in the order you choose. Results are returned one page at a time; pass the returned `cursor` to read the next page. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Logs",),
                "fields": {
                    "from": <RestSchemaFieldOverlayInfo>{
                        "display_name": "From",
                        "short_desc": "Start of the time range",
                        "desc": "The start of the time range.",
                    },
                    "to": <RestSchemaFieldOverlayInfo>{
                        "display_name": "To",
                        "short_desc": "End of the time range",
                        "desc": "The end of the time range.",
                    },
                    "page_size": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "short_desc": "Maximum number of results per page",
                        "desc": "The maximum number of results to return in one page.",
                        "preselected": True,
                    },
                    "cursor": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Cursor",
                        "short_desc": "Cursor of the page to return",
                        "desc": "The `cursor` returned with the previous page; leave it out for the first page.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-log-fields",
                "method": "GET",
                "path": "/v1/observability/logs/fields",
                "operation_id": "get_log_fields_v1_observability_logs_fields_get",
                "display_name": "List log fields",
                "short_desc": "List the fields that log searches can use",
                "desc": "List the fields that log search expressions can refer to, with their labels and types. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Logs",),
            },
            <RestSchemaActionInfo>{
                "action": "list-log-field-options",
                "method": "GET",
                "path": "/v1/observability/logs/fields/{field_name}/options",
                "operation_id": "get_log_field_options_v1_observability_logs_fields__field_name__options_get",
                "display_name": "List Log Field Options",
                "short_desc": "List the values recorded for a log field",
                "desc": "List the values recorded for a log field in a time range, for building log search expressions. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Logs",),
                "fields": {
                    "field_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Field Name",
                        "short_desc": "Name of the field",
                        "desc": "The name of the field, as listed by the field listing action.",
                        "ref_data": "log-fields",
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                    "from": <RestSchemaFieldOverlayInfo>{
                        "display_name": "From",
                        "short_desc": "Start of the time range",
                        "desc": "The start of the time range.",
                    },
                    "to": <RestSchemaFieldOverlayInfo>{
                        "display_name": "To",
                        "short_desc": "End of the time range",
                        "desc": "The end of the time range.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "search-traces",
                "method": "POST",
                "mutates_state": False,
                "path": "/v1/observability/traces/search",
                "operation_id": "search_traces_v1_observability_traces_search_post",
                "display_name": "Search traces",
                "short_desc": "Search the traces of your AI activity",
                "desc": "Search the OpenTelemetry traces recorded for your AI activity with a search expression. Results are returned one page at a time; pass the returned `cursor` to read the next page. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Traces",),
                "fields": {
                    "from": <RestSchemaFieldOverlayInfo>{
                        "display_name": "From",
                        "short_desc": "Start of the time range",
                        "desc": "The start of the time range.",
                    },
                    "to": <RestSchemaFieldOverlayInfo>{
                        "display_name": "To",
                        "short_desc": "End of the time range",
                        "desc": "The end of the time range.",
                    },
                    "page_size": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "short_desc": "Maximum number of results per page",
                        "desc": "The maximum number of results to return in one page.",
                        "preselected": True,
                    },
                    "cursor": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Cursor",
                        "short_desc": "Cursor of the page to return",
                        "desc": "The `cursor` returned with the previous page; leave it out for the first page.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "aggregate-traces",
                "method": "POST",
                "mutates_state": False,
                "path": "/v1/observability/traces/aggregate",
                "operation_id": "aggregate_traces_v1_observability_traces_aggregate_post",
                "display_name": "Aggregate traces",
                "short_desc": "Aggregate trace metrics over time",
                "desc": "Compute a metric over the traces that match a search expression, grouped by the dimensions you choose and optionally by time. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Traces",),
                "fields": {
                    "from": <RestSchemaFieldOverlayInfo>{
                        "display_name": "From",
                        "short_desc": "Start of the time range",
                        "desc": "The start of the time range.",
                    },
                    "to": <RestSchemaFieldOverlayInfo>{
                        "display_name": "To",
                        "short_desc": "End of the time range",
                        "desc": "The end of the time range.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-trace-fields",
                "method": "GET",
                "path": "/v1/observability/traces/fields",
                "operation_id": "get_trace_fields_v1_observability_traces_fields_get",
                "display_name": "List trace fields",
                "short_desc": "List the fields that trace searches can use",
                "desc": "List the fields that trace search expressions and aggregations can refer to, with their labels and types. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Traces",),
            },
            <RestSchemaActionInfo>{
                "action": "list-trace-field-options",
                "method": "GET",
                "path": "/v1/observability/traces/fields/{field_name}/options",
                "operation_id": "get_trace_field_options_v1_observability_traces_fields__field_name__options_get",
                "display_name": "List Trace Field Options",
                "short_desc": "List the values recorded for a trace field",
                "desc": "List the values recorded for a trace field in a time range, for building trace search expressions. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Traces",),
                "fields": {
                    "field_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Field Name",
                        "short_desc": "Name of the field",
                        "desc": "The name of the field, as listed by the field listing action.",
                        "ref_data": "trace-fields",
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                    "from": <RestSchemaFieldOverlayInfo>{
                        "display_name": "From",
                        "short_desc": "Start of the time range",
                        "desc": "The start of the time range.",
                    },
                    "to": <RestSchemaFieldOverlayInfo>{
                        "display_name": "To",
                        "short_desc": "End of the time range",
                        "desc": "The end of the time range.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-trace",
                "method": "GET",
                "path": "/v1/observability/traces/{trace_id}",
                "operation_id": "get_trace_by_id_v1_observability_traces__trace_id__get",
                "display_name": "Get Trace",
                "short_desc": "Get a trace by its ID",
                "desc": "Get a trace by its ID, with its timing, token usage, counts of spans, model calls and tool calls, and its first and last messages. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Traces",),
                "fields": {
                    "trace_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Trace ID",
                        "short_desc": "ID of the trace",
                        "desc": "The 32-digit hexadecimal ID of the trace.",
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-trace-spans",
                "method": "GET",
                "path": "/v1/observability/traces/{trace_id}/spans",
                "operation_id": "get_trace_spans_v1_observability_traces__trace_id__spans_get",
                "display_name": "List Trace Spans",
                "short_desc": "List the spans of a trace",
                "desc": "List the spans of a trace, to follow each step of an AI request. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Traces",),
                "fields": {
                    "trace_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Trace ID",
                        "short_desc": "ID of the trace",
                        "desc": "The 32-digit hexadecimal ID of the trace.",
                        "preselected": True,
                    },
                    "from": <RestSchemaFieldOverlayInfo>{
                        "display_name": "From",
                        "short_desc": "Start of the time range",
                        "desc": "The start of the time range.",
                    },
                    "to": <RestSchemaFieldOverlayInfo>{
                        "display_name": "To",
                        "short_desc": "End of the time range",
                        "desc": "The end of the time range.",
                    },
                    "page_size": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "short_desc": "Maximum number of results per page",
                        "desc": "The maximum number of results to return in one page.",
                        "preselected": True,
                    },
                    "cursor": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Cursor",
                        "short_desc": "Cursor of the page to return",
                        "desc": "The `cursor` returned with the previous page; leave it out for the first page.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-trace-span",
                "method": "GET",
                "path": "/v1/observability/traces/{trace_id}/spans/{span_id}",
                "operation_id": "get_span_by_id_v1_observability_traces__trace_id__spans__span_id__get",
                "display_name": "Get Trace Span",
                "short_desc": "Get a span of a trace",
                "desc": "Get one span of a trace by its trace ID and span ID, with its timing, model request settings, messages, tool call and attributes. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Traces",),
                "fields": {
                    "trace_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Trace ID",
                        "short_desc": "ID of the trace",
                        "desc": "The 32-digit hexadecimal ID of the trace.",
                        "preselected": True,
                    },
                    "span_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Span ID",
                        "short_desc": "ID of the span",
                        "desc": "The 16-digit hexadecimal ID of the span.",
                        "preselected": True,
                    },
                    "from": <RestSchemaFieldOverlayInfo>{
                        "display_name": "From",
                        "short_desc": "Start of the time range",
                        "desc": "The start of the time range.",
                    },
                    "to": <RestSchemaFieldOverlayInfo>{
                        "display_name": "To",
                        "short_desc": "End of the time range",
                        "desc": "The end of the time range.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "search-spans",
                "method": "POST",
                "mutates_state": False,
                "path": "/v1/observability/spans/search",
                "operation_id": "search_spans_v1_observability_spans_search_post",
                "display_name": "Search spans",
                "short_desc": "Search the spans of your AI activity",
                "desc": "Search the OpenTelemetry spans recorded for your AI activity with a search expression. Results are returned one page at a time; pass the returned `cursor` to read the next page. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Spans",),
                "fields": {
                    "from": <RestSchemaFieldOverlayInfo>{
                        "display_name": "From",
                        "short_desc": "Start of the time range",
                        "desc": "The start of the time range.",
                    },
                    "to": <RestSchemaFieldOverlayInfo>{
                        "display_name": "To",
                        "short_desc": "End of the time range",
                        "desc": "The end of the time range.",
                    },
                    "page_size": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "short_desc": "Maximum number of results per page",
                        "desc": "The maximum number of results to return in one page.",
                        "preselected": True,
                    },
                    "cursor": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Cursor",
                        "short_desc": "Cursor of the page to return",
                        "desc": "The `cursor` returned with the previous page; leave it out for the first page.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "aggregate-spans",
                "method": "POST",
                "mutates_state": False,
                "path": "/v1/observability/spans/aggregate",
                "operation_id": "aggregate_spans_v1_observability_spans_aggregate_post",
                "display_name": "Aggregate spans",
                "short_desc": "Aggregate span metrics over time",
                "desc": "Compute a metric over the spans that match a search expression, grouped by the dimensions you choose and optionally by time. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Spans",),
                "fields": {
                    "from": <RestSchemaFieldOverlayInfo>{
                        "display_name": "From",
                        "short_desc": "Start of the time range",
                        "desc": "The start of the time range.",
                    },
                    "to": <RestSchemaFieldOverlayInfo>{
                        "display_name": "To",
                        "short_desc": "End of the time range",
                        "desc": "The end of the time range.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-span-fields",
                "method": "GET",
                "path": "/v1/observability/spans/fields",
                "operation_id": "get_span_fields_v1_observability_spans_fields_get",
                "display_name": "List span fields",
                "short_desc": "List the fields that span searches can use",
                "desc": "List the fields that span search expressions and aggregations can refer to, with their labels and types. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Spans",),
            },
            <RestSchemaActionInfo>{
                "action": "list-span-field-options",
                "method": "GET",
                "path": "/v1/observability/spans/fields/{field_name}/options",
                "operation_id": "get_span_field_options_v1_observability_spans_fields__field_name__options_get",
                "display_name": "List Span Field Options",
                "short_desc": "List the values recorded for a span field",
                "desc": "List the values recorded for a span field in a time range, for building span search expressions. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Spans",),
                "fields": {
                    "field_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Field Name",
                        "short_desc": "Name of the field",
                        "desc": "The name of the field, as listed by the field listing action.",
                        "ref_data": "span-fields",
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                    "from": <RestSchemaFieldOverlayInfo>{
                        "display_name": "From",
                        "short_desc": "Start of the time range",
                        "desc": "The start of the time range.",
                    },
                    "to": <RestSchemaFieldOverlayInfo>{
                        "display_name": "To",
                        "short_desc": "End of the time range",
                        "desc": "The end of the time range.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "search-span-evaluations",
                "method": "POST",
                "mutates_state": False,
                "path": "/v1/observability/spans/evaluations/search",
                "operation_id": "search_span_evaluations_v1_observability_spans_evaluations_search_post",
                "display_name": "Search span evaluations",
                "short_desc": "Search the evaluations of spans",
                "desc": "Search the evaluation results recorded for spans with a search expression. Results are returned one page at a time; pass the returned `cursor` to read the next page. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Span Evaluations",),
                "fields": {
                    "from": <RestSchemaFieldOverlayInfo>{
                        "display_name": "From",
                        "short_desc": "Start of the time range",
                        "desc": "The start of the time range.",
                    },
                    "to": <RestSchemaFieldOverlayInfo>{
                        "display_name": "To",
                        "short_desc": "End of the time range",
                        "desc": "The end of the time range.",
                    },
                    "page_size": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "short_desc": "Maximum number of results per page",
                        "desc": "The maximum number of results to return in one page.",
                        "preselected": True,
                    },
                    "cursor": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Cursor",
                        "short_desc": "Cursor of the page to return",
                        "desc": "The `cursor` returned with the previous page; leave it out for the first page.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "search-latest-span-evaluations",
                "method": "POST",
                "mutates_state": False,
                "path": "/v1/observability/spans/evaluations/search/latest",
                "operation_id": "search_latest_span_evaluations_v1_observability_spans_evaluations_search_latest_post",
                "display_name": "Search latest span evaluations",
                "short_desc": "Search the latest span evaluation results",
                "desc": "Search the evaluation results recorded for spans with a search expression, returning only the latest results. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Span Evaluations",),
                "fields": {
                    "from": <RestSchemaFieldOverlayInfo>{
                        "display_name": "From",
                        "short_desc": "Start of the time range",
                        "desc": "The start of the time range.",
                    },
                    "to": <RestSchemaFieldOverlayInfo>{
                        "display_name": "To",
                        "short_desc": "End of the time range",
                        "desc": "The end of the time range.",
                    },
                    "page_size": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "short_desc": "Maximum number of results per page",
                        "desc": "The maximum number of results to return in one page.",
                        "preselected": True,
                    },
                    "cursor": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Cursor",
                        "short_desc": "Cursor of the page to return",
                        "desc": "The `cursor` returned with the previous page; leave it out for the first page.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "aggregate-span-evaluations",
                "method": "POST",
                "mutates_state": False,
                "path": "/v1/observability/spans/evaluations/aggregate",
                "operation_id": "aggregate_span_evaluations_v1_observability_spans_evaluations_aggregate_post",
                "display_name": "Aggregate span evaluations",
                "short_desc": "Aggregate span evaluation results over time",
                "desc": "Compute a metric over the span evaluation results that match a search expression, grouped by the dimensions you choose and optionally by time. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Span Evaluations",),
                "fields": {
                    "from": <RestSchemaFieldOverlayInfo>{
                        "display_name": "From",
                        "short_desc": "Start of the time range",
                        "desc": "The start of the time range.",
                    },
                    "to": <RestSchemaFieldOverlayInfo>{
                        "display_name": "To",
                        "short_desc": "End of the time range",
                        "desc": "The end of the time range.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-span-evaluation-fields",
                "method": "GET",
                "path": "/v1/observability/spans/evaluations/fields",
                "operation_id": "get_span_evaluation_fields_v1_observability_spans_evaluations_fields_get",
                "display_name": "List span evaluation fields",
                "short_desc": "List the fields that evaluation searches can use",
                "desc": "List the fields that span evaluation search expressions and aggregations can refer to, with their labels and types. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Span Evaluations",),
            },
            <RestSchemaActionInfo>{
                "action": "list-span-evaluation-field-options",
                "method": "GET",
                "path": "/v1/observability/spans/evaluations/fields/{field_name}/options",
                "operation_id": "get_span_evaluation_field_options_v1_observability_spans_evaluations_fields__field_name__options_get",
                "display_name": "List Span Evaluation Field Options",
                "short_desc": "List the values recorded for an evaluation field",
                "desc": "List the values recorded for a span evaluation field in a time range, for building evaluation search expressions. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Span Evaluations",),
                "fields": {
                    "field_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Field Name",
                        "short_desc": "Name of the field",
                        "desc": "The name of the field, as listed by the field listing action.",
                        "ref_data": "span-evaluation-fields",
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                    "from": <RestSchemaFieldOverlayInfo>{
                        "display_name": "From",
                        "short_desc": "Start of the time range",
                        "desc": "The start of the time range.",
                    },
                    "to": <RestSchemaFieldOverlayInfo>{
                        "display_name": "To",
                        "short_desc": "End of the time range",
                        "desc": "The end of the time range.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-pipelines",
                "method": "GET",
                "path": "/v1/observability/pipelines",
                "operation_id": "list_pipelines_v1_observability_pipelines_get",
                "display_name": "List pipelines",
                "short_desc": "List evaluation pipelines",
                "desc": "List the evaluation pipelines that score your AI activity automatically. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Pipelines",),
                "fields": {
                    "enabled": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Enabled",
                        "short_desc": "Return only enabled or disabled pipelines",
                        "desc": "When set, return only the pipelines whose enabled state matches.",
                    },
                    "from": <RestSchemaFieldOverlayInfo>{
                        "display_name": "From",
                        "short_desc": "Start of the time range",
                        "desc": "The start of the time range.",
                    },
                    "to": <RestSchemaFieldOverlayInfo>{
                        "display_name": "To",
                        "short_desc": "End of the time range",
                        "desc": "The end of the time range.",
                    },
                    "page_size": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "short_desc": "Maximum number of results per page",
                        "desc": "The maximum number of results to return in one page.",
                        "preselected": True,
                    },
                    "page": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page",
                        "short_desc": "Page number to return",
                        "desc": "The number of the page to return, starting at 1.",
                        "preselected": True,
                    },
                    "q": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Search Text",
                        "short_desc": "Text to filter by",
                        "desc": "Text to filter the listed pipelines by.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-pipeline",
                "method": "POST",
                "path": "/v1/observability/pipelines",
                "operation_id": "create_pipeline_v1_observability_pipelines_post",
                "display_name": "Create pipeline",
                "short_desc": "Create an evaluation pipeline",
                "desc": "Create an evaluation pipeline: its selectors choose the telemetry to evaluate (spans, logs or metrics) and its definitions name the judges that score it. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Pipelines",),
            },
            <RestSchemaActionInfo>{
                "action": "get-pipeline",
                "method": "GET",
                "path": "/v1/observability/pipelines/{pipeline_id}",
                "operation_id": "get_pipeline_v1_observability_pipelines__pipeline_id__get",
                "display_name": "Get Pipeline",
                "short_desc": "Get an evaluation pipeline",
                "desc": "Get an evaluation pipeline with its selectors and judge definitions. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Pipelines",),
                "fields": {
                    "pipeline_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Pipeline ID",
                        "short_desc": "Evaluation pipeline",
                        "desc": "The ID of the evaluation pipeline.",
                        "ref_data": "pipelines",
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-pipeline",
                "method": "PUT",
                "path": "/v1/observability/pipelines/{pipeline_id}",
                "operation_id": "update_pipeline_v1_observability_pipelines__pipeline_id__put",
                "display_name": "Update Pipeline",
                "short_desc": "Update an evaluation pipeline",
                "desc": "Update the name, description, selectors, judge definitions or enabled state of an evaluation pipeline. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Pipelines",),
                "fields": {
                    "pipeline_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Pipeline ID",
                        "short_desc": "Evaluation pipeline",
                        "desc": "The ID of the evaluation pipeline.",
                        "ref_data": "pipelines",
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-pipeline",
                "method": "DELETE",
                "path": "/v1/observability/pipelines/{pipeline_id}",
                "operation_id": "delete_pipeline_v1_observability_pipelines__pipeline_id__delete",
                "display_name": "Delete Pipeline",
                "short_desc": "Delete an evaluation pipeline",
                "desc": "Delete an evaluation pipeline. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.\n\n**Deletion:** This operation can permanently remove the selected resource or access.",
                "groups": ("Observability Pipelines",),
                "fields": {
                    "pipeline_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Pipeline ID",
                        "short_desc": "Evaluation pipeline",
                        "desc": "The ID of the evaluation pipeline.",
                        "ref_data": "pipelines",
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-pipeline-result-fields",
                "method": "GET",
                "path": "/v1/observability/pipelines/{pipeline_id}/results/fields",
                "operation_id": "get_pipeline_result_fields_v1_observability_pipelines__pipeline_id__results_fields_get",
                "display_name": "List Pipeline Result Fields",
                "short_desc": "List the result fields of a pipeline",
                "desc": "List the fields of the results that an evaluation pipeline records, for searching its results. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Pipelines",),
                "fields": {
                    "pipeline_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Pipeline ID",
                        "short_desc": "Evaluation pipeline",
                        "desc": "The ID of the evaluation pipeline.",
                        "ref_data": "pipelines",
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "search-pipeline-results",
                "method": "POST",
                "mutates_state": False,
                "path": "/v1/observability/pipeline-results/search",
                "operation_id": "search_pipeline_results_v1_observability_pipeline_results_search_post",
                "display_name": "Search pipeline results",
                "short_desc": "Search the results of evaluation pipelines",
                "desc": "Search the results that evaluation pipelines recorded for the pipeline configurations you name by slug. Results are returned one page at a time. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Pipelines",),
                "fields": {
                    "pipeline_config_slugs": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Pipeline Config Slugs",
                        "short_desc": "Slugs of the pipeline configurations to search",
                        "desc": "The slugs of the pipeline configurations whose results to search, as listed under `pipeline_configs` of a pipeline; at least one is required.",
                        "preselected": True,
                    },
                    "from": <RestSchemaFieldOverlayInfo>{
                        "display_name": "From",
                        "short_desc": "Start of the time range",
                        "desc": "The start of the time range.",
                    },
                    "to": <RestSchemaFieldOverlayInfo>{
                        "display_name": "To",
                        "short_desc": "End of the time range",
                        "desc": "The end of the time range.",
                    },
                    "page_size": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "short_desc": "Maximum number of results per page",
                        "desc": "The maximum number of results to return in one page.",
                        "preselected": True,
                    },
                    "cursor": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Cursor",
                        "short_desc": "Cursor of the page to return",
                        "desc": "The `cursor` returned with the previous page; leave it out for the first page.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "import-dataset-from-spans",
                "method": "POST",
                "path": "/v1/observability/datasets/{dataset_id}/imports/from-spans",
                "operation_id": "post_dataset_records_from_spans_v1_observability_datasets__dataset_id__imports_from_spans_post",
                "display_name": "Import Dataset From Spans",
                "short_desc": "Import spans into a dataset",
                "desc": "Populate the dataset with records made from up to 500 spans, mapping span or resource attributes to record payload keys. The import runs as a dataset task; unavailable spans are skipped. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Datasets",),
                "fields": {
                    "dataset_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Dataset ID",
                        "short_desc": "Dataset to import into",
                        "desc": "The ID or workspace-scoped slug of the dataset.",
                        "ref_data": "datasets",
                        "supports_custom_values": True,
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-batch-job",
                "method": "DELETE",
                "path": "/v1/batch/jobs/{job_id}",
                "operation_id": "jobs_api_routes_batch_delete_batch_job",
                "display_name": "Delete Batch Job",
                "short_desc": "Delete Batch Job",
                "desc": "Delete the selected batch job.",
                "groups": ("Batch",),
                "fields": {
                    "job_id": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "batch-jobs",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "agent-completion",
                "method": "POST",
                "path": "/v1/agents/completions",
                "operation_id": "agents_completion_v1_agents_completions_post",
                "display_name": "Agent completion",
                "short_desc": "Agent Completion",
                "desc": "Generate a completed reply from an agent and a list of messages.",
                "groups": ("Agents",),
                "mutates_state": False,
                "response_content_type": "application/json",
                "fields": {
                    "stream": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                    "messages": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                    "agent_id": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "agents",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "patch-library",
                "method": "PATCH",
                "path": "/v1/libraries/{library_id}",
                "operation_id": "libraries_patch_v1",
                "display_name": "Patch Library",
                "short_desc": "Patch Library",
                "desc": "Change the name or description of a document library.",
                "groups": ("Libraries",),
                "fields": {
                    "library_id": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "libraries",
                        "supports_custom_values": True,
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "patch-library-document",
                "method": "PATCH",
                "path": "/v1/libraries/{library_id}/documents/{document_id}",
                "operation_id": "libraries_documents_patch_v1",
                "display_name": "Patch Library Document",
                "short_desc": "Patch Library Document",
                "desc": "Update a library document’s name, attributes, or expiration date.",
                "groups": ("Libraries",),
                "fields": {
                    "library_id": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "libraries",
                        "supports_custom_values": True,
                    },
                    "document_id": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "library-documents",
                        "supports_custom_values": True,
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-response",
                "method": "POST",
                "path": "/v1/responses",
                "operation_id": "create_response_v1_responses_post",
                "display_name": "Create response",
                "short_desc": "Create Response",
                "desc": "Generate a completed model response from the supplied input.",
                "groups": ("Responses",),
                "mutates_state": False,
                "response_content_type": "application/json",
                "fields": {
                    "model": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "models",
                        "supports_custom_values": True,
                    },
                    "input": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                    "stream": <RestSchemaFieldOverlayInfo>{
                        "ignore": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "count-response-input-tokens",
                "method": "POST",
                "path": "/v1/responses/input_tokens",
                "operation_id": "count_input_tokens_v1_responses_input_tokens_post",
                "display_name": "Count response input tokens",
                "short_desc": "Count Response Input Tokens",
                "desc": "Count the input tokens for a response without generating it.",
                "groups": ("Responses",),
                "mutates_state": False,
                "fields": {
                    "model": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "models",
                        "supports_custom_values": True,
                    },
                    "input": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-voices-v2",
                "method": "GET",
                "path": "/v2/audio/voices",
                "operation_id": "audio_v2_voices_list",
                "display_name": "List voices (v2)",
                "short_desc": "List Voices (v2)",
                "desc": "List voices using filters and cursor pagination.",
                "groups": ("Audio",),
            },
            <RestSchemaActionInfo>{
                "action": "list-prompts",
                "method": "GET",
                "path": "/v2/prompts",
                "operation_id": "prompts_list",
                "display_name": "List prompts",
                "short_desc": "List Prompts",
                "desc": "List prompts with pagination and optional filters.",
                "groups": ("Prompts",),
                "fields": {
                    "pageSize": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                    },
                    "pageToken": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Token",
                    },
                    "sort.field": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Sort Field",
                    },
                    "sort.direction": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Sort Direction",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-prompt",
                "method": "POST",
                "path": "/v2/prompts",
                "operation_id": "prompts_create",
                "display_name": "Create prompt",
                "short_desc": "Create Prompt",
                "desc": "Create a prompt with its first version.",
                "groups": ("Prompts",),
                "fields": {
                    "name": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                    "definition": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                    "sharingScope": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Sharing Scope",
                    },
                    "workspaceRelation": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Workspace Relation",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-prompt",
                "method": "GET",
                "path": "/v2/prompts/{prompt_id}",
                "operation_id": "prompts_get",
                "display_name": "Get Prompt",
                "short_desc": "Get Prompt",
                "desc": "Retrieve a prompt by ID and optional version or alias.",
                "groups": ("Prompts",),
                "fields": {
                    "version": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "prompt-versions",
                        "supports_custom_values": True,
                    },
                    "prompt_id": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "prompts",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-prompt",
                "method": "DELETE",
                "path": "/v2/prompts/{prompt_id}",
                "operation_id": "prompts_delete",
                "display_name": "Delete Prompt",
                "short_desc": "Delete Prompt",
                "desc": "Delete the selected prompt.",
                "groups": ("Prompts",),
                "fields": {
                    "prompt_id": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "prompts",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-prompt",
                "method": "PATCH",
                "path": "/v2/prompts/{prompt_id}",
                "operation_id": "prompts_update",
                "display_name": "Update Prompt",
                "short_desc": "Update Prompt",
                "desc": "Update the metadata of an existing prompt.",
                "groups": ("Prompts",),
                "fields": {
                    "prompt_id": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "prompts",
                        "supports_custom_values": True,
                    },
                    "sharingScope": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Sharing Scope",
                    },
                    "workspaceRelation": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Workspace Relation",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-prompt-versions",
                "method": "GET",
                "path": "/v2/prompts/{prompt_id}/versions",
                "operation_id": "prompts_list_versions",
                "display_name": "List Prompt Versions",
                "short_desc": "List Prompt Versions",
                "desc": "List the saved versions of a prompt.",
                "groups": ("Prompts",),
                "fields": {
                    "prompt_id": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "prompts",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-prompt-version",
                "method": "POST",
                "path": "/v2/prompts/{prompt_id}/versions",
                "operation_id": "prompts_create_version",
                "display_name": "Create Prompt Version",
                "short_desc": "Create Prompt Version",
                "desc": "Save a new version of a prompt definition.",
                "groups": ("Prompts",),
                "fields": {
                    "prompt_id": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "prompts",
                        "supports_custom_values": True,
                    },
                    "definition": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-prompt-version",
                "method": "GET",
                "path": "/v2/prompts/{prompt_id}/versions/{version}",
                "operation_id": "prompts_get_version",
                "display_name": "Get Prompt Version",
                "short_desc": "Get Prompt Version",
                "desc": "Retrieve a specific version of a prompt.",
                "groups": ("Prompts",),
                "fields": {
                    "prompt_id": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "prompts",
                        "supports_custom_values": True,
                    },
                    "version": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "prompt-versions",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-prompt-version",
                "method": "PATCH",
                "path": "/v2/prompts/{prompt_id}/versions/{version}",
                "operation_id": "prompts_update_version_metadata",
                "display_name": "Update Prompt Version",
                "short_desc": "Update Prompt Version",
                "desc": "Update the notes or aliases of a prompt version.",
                "groups": ("Prompts",),
                "fields": {
                    "prompt_id": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "prompts",
                        "supports_custom_values": True,
                    },
                    "version": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "prompt-versions",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-skills",
                "method": "GET",
                "path": "/v2/skills",
                "operation_id": "skills_list",
                "display_name": "List skills",
                "short_desc": "List Skills",
                "desc": "List skills with pagination and optional filters.",
                "groups": ("Skills",),
                "fields": {
                    "pageSize": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                    },
                    "pageToken": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Token",
                    },
                    "sort.field": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Sort Field",
                    },
                    "sort.direction": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Sort Direction",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-skill",
                "method": "POST",
                "path": "/v2/skills",
                "operation_id": "skills_create",
                "display_name": "Create skill",
                "short_desc": "Create Skill",
                "desc": "Create a skill with its first version.",
                "groups": ("Skills",),
                "fields": {
                    "name": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                    "definition": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                    "sharingScope": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Sharing Scope",
                    },
                    "workspaceRelation": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Workspace Relation",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-skill",
                "method": "GET",
                "path": "/v2/skills/{skill_id}",
                "operation_id": "skills_get",
                "display_name": "Get Skill",
                "short_desc": "Get Skill",
                "desc": "Retrieve a skill by ID and optional version or alias.",
                "groups": ("Skills",),
                "fields": {
                    "version": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "skill-versions",
                        "supports_custom_values": True,
                    },
                    "skill_id": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "skills",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-skill",
                "method": "DELETE",
                "path": "/v2/skills/{skill_id}",
                "operation_id": "skills_delete",
                "display_name": "Delete Skill",
                "short_desc": "Delete Skill",
                "desc": "Delete the selected skill.",
                "groups": ("Skills",),
                "fields": {
                    "skill_id": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "skills",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-skill",
                "method": "PATCH",
                "path": "/v2/skills/{skill_id}",
                "operation_id": "skills_update",
                "display_name": "Update Skill",
                "short_desc": "Update Skill",
                "desc": "Update the metadata of an existing skill.",
                "groups": ("Skills",),
                "fields": {
                    "skill_id": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "skills",
                        "supports_custom_values": True,
                    },
                    "sharingScope": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Sharing Scope",
                    },
                    "workspaceRelation": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Workspace Relation",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-skill-versions",
                "method": "GET",
                "path": "/v2/skills/{skill_id}/versions",
                "operation_id": "skills_list_versions",
                "display_name": "List Skill Versions",
                "short_desc": "List Skill Versions",
                "desc": "List the saved versions of a skill.",
                "groups": ("Skills",),
                "fields": {
                    "skill_id": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "skills",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-skill-version",
                "method": "POST",
                "path": "/v2/skills/{skill_id}/versions",
                "operation_id": "skills_create_version",
                "display_name": "Create Skill Version",
                "short_desc": "Create Skill Version",
                "desc": "Save a new version of a skill definition.",
                "groups": ("Skills",),
                "fields": {
                    "skill_id": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "skills",
                        "supports_custom_values": True,
                    },
                    "definition": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-skill-version",
                "method": "GET",
                "path": "/v2/skills/{skill_id}/versions/{version}",
                "operation_id": "skills_get_version",
                "display_name": "Get Skill Version",
                "short_desc": "Get Skill Version",
                "desc": "Retrieve a specific version of a skill.",
                "groups": ("Skills",),
                "fields": {
                    "skill_id": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "skills",
                        "supports_custom_values": True,
                    },
                    "version": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "skill-versions",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-skill-version",
                "method": "PATCH",
                "path": "/v2/skills/{skill_id}/versions/{version}",
                "operation_id": "skills_update_version_metadata",
                "display_name": "Update Skill Version",
                "short_desc": "Update Skill Version",
                "desc": "Update the notes or aliases of a skill version.",
                "groups": ("Skills",),
                "fields": {
                    "skill_id": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "skills",
                        "supports_custom_values": True,
                    },
                    "version": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "skill-versions",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-observability-projects",
                "method": "GET",
                "path": "/v1/observability/projects",
                "operation_id": "get_projects_v1_observability_projects_get",
                "display_name": "List observability projects",
                "short_desc": "List Observability Projects",
                "desc": "List observability projects by name or search term.",
                "groups": ("Observability",),
            },
            <RestSchemaActionInfo>{
                "action": "get-observability-project",
                "method": "GET",
                "path": "/v1/observability/projects/{project_slug}",
                "operation_id": "get_project_v1_observability_projects__project_slug__get",
                "display_name": "Get Observability Project",
                "short_desc": "Get Observability Project",
                "desc": "Retrieve a project by its slug.",
                "groups": ("Observability",),
                "fields": {
                    "project_slug": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "observability-projects",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-project-evaluations",
                "method": "GET",
                "path": "/v1/observability/projects/{project_slug}/evaluations_v2",
                "operation_id": "get_project_evaluations_v2_v1_observability_projects__project_slug__evaluations_v2_get",
                "display_name": "List Project Evaluations",
                "short_desc": "List Project Evaluations",
                "desc": "List the evaluations in a project.",
                "groups": ("Observability",),
                "fields": {
                    "project_slug": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "observability-projects",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-evaluation",
                "method": "GET",
                "path": "/v1/observability/evaluations_v2/{evaluation_slug}",
                "operation_id": "get_evaluation_v2_v1_observability_evaluations_v2__evaluation_slug__get",
                "display_name": "Get Evaluation",
                "short_desc": "Get Evaluation",
                "desc": "Retrieve an evaluation by its slug.",
                "groups": ("Observability",),
                "fields": {
                    "evaluation_slug": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "project-evaluations",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "search-evaluation-runs",
                "method": "POST",
                "path": "/v1/observability/evaluations_v2/{evaluation_slug}/runs/search",
                "operation_id": "search_evaluation_runs_v2_v1_observability_evaluations_v2__evaluation_slug__runs_search_post",
                "display_name": "Search Evaluation Runs",
                "short_desc": "Search Evaluation Runs",
                "desc": "Search the runs of a single evaluation.",
                "groups": ("Observability",),
                "mutates_state": False,
                "fields": {
                    "evaluation_slug": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "project-evaluations",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "search-all-evaluation-runs",
                "method": "POST",
                "path": "/v1/observability/evaluation-runs_v2/search",
                "operation_id": "search_all_evaluation_runs_v2_v1_observability_evaluation_runs_v2_search_post",
                "display_name": "Search all evaluation runs",
                "short_desc": "Search All Evaluation Runs",
                "desc": "Search runs across evaluations.",
                "groups": ("Observability",),
                "mutates_state": False,
            },
            <RestSchemaActionInfo>{
                "action": "get-evaluation-run",
                "method": "GET",
                "path": "/v1/observability/evaluation-runs_v2/{run_id}",
                "operation_id": "get_evaluation_run_v2_v1_observability_evaluation_runs_v2__run_id__get",
                "display_name": "Get Evaluation Run",
                "short_desc": "Get Evaluation Run",
                "desc": "Retrieve an evaluation run by ID.",
                "groups": ("Observability",),
                "fields": {
                    "run_id": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "evaluation-runs",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-evaluation-run-statistics",
                "method": "GET",
                "path": "/v1/observability/evaluation-runs_v2/{run_id}/statistics",
                "operation_id": "get_single_evaluation_run_statistics_v2_v1_observability_evaluation_runs_v2__run_id__statistics_get",
                "display_name": "Get Evaluation Run Statistics",
                "short_desc": "Get Evaluation Run Statistics",
                "desc": "Retrieve statistics for an evaluation run.",
                "groups": ("Observability",),
                "fields": {
                    "run_id": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "evaluation-runs",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "compute-evaluation-run-statistics",
                "method": "POST",
                "path": "/v1/observability/evaluation-runs_v2/{run_id}/statistics/compute",
                "operation_id": "compute_filtered_run_statistics_v2_v1_observability_evaluation_runs_v2__run_id__statistics_compute_post",
                "display_name": "Compute Evaluation Run Statistics",
                "short_desc": "Compute Evaluation Run Statistics",
                "desc": "Calculate statistics for the records matching the supplied filters.",
                "groups": ("Observability",),
                "mutates_state": False,
                "fields": {
                    "run_id": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "evaluation-runs",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-evaluation-run-record",
                "method": "GET",
                "path": "/v1/observability/evaluation-runs_v2/{run_id}/records/{record_id}",
                "operation_id": "get_evaluation_run_record_v2_v1_observability_evaluation_runs_v2__run_id__records__record_id__get",
                "display_name": "Get Evaluation Run Record",
                "short_desc": "Get Evaluation Run Record",
                "desc": "Retrieve one record from an evaluation run.",
                "groups": ("Observability",),
                "fields": {
                    "run_id": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "evaluation-runs",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "search-evaluation-run-records",
                "method": "POST",
                "path": "/v1/observability/evaluation-runs_v2/{run_id}/records/search",
                "operation_id": "search_evaluation_run_records_v2_v1_observability_evaluation_runs_v2__run_id__records_search_post",
                "display_name": "Search Evaluation Run Records",
                "short_desc": "Search Evaluation Run Records",
                "desc": "Search the records of an evaluation run.",
                "groups": ("Observability",),
                "mutates_state": False,
                "fields": {
                    "run_id": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "evaluation-runs",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-optimizations",
                "method": "GET",
                "path": "/v1/observability/optimizations",
                "operation_id": "list_optimizations_v1_observability_optimizations_get",
                "display_name": "List optimizations",
                "short_desc": "List Optimizations",
                "desc": "List optimizations with optional project and evaluation filters.",
                "groups": ("Observability",),
                "fields": {
                    "project": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "observability-projects",
                        "supports_custom_values": True,
                    },
                    "evaluation": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "project-evaluations",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-optimization",
                "method": "GET",
                "path": "/v1/observability/optimizations/{optimization_id}",
                "operation_id": "get_optimization_v1_observability_optimizations__optimization_id__get",
                "display_name": "Get Optimization",
                "short_desc": "Get Optimization",
                "desc": "Retrieve an optimization and its associated evaluation and project.",
                "groups": ("Observability",),
                "fields": {
                    "optimization_id": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "optimizations",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-optimization-trials",
                "method": "GET",
                "path": "/v1/observability/optimizations/{optimization_id}/trials",
                "operation_id": "list_optimization_trials_v1_observability_optimizations__optimization_id__trials_get",
                "display_name": "List Optimization Trials",
                "short_desc": "List Optimization Trials",
                "desc": "List the trials and observation runs of an optimization.",
                "groups": ("Observability",),
                "fields": {
                    "optimization_id": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "optimizations",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-scorers",
                "method": "GET",
                "path": "/v1/observability/scorers",
                "operation_id": "list_scorers_v1_observability_scorers_get",
                "display_name": "List scorers",
                "short_desc": "List Scorers",
                "desc": "List available scorers.",
                "groups": ("Observability",),
            },
            <RestSchemaActionInfo>{
                "action": "create-scorer",
                "method": "POST",
                "path": "/v1/observability/scorers",
                "operation_id": "create_scorer_v1_observability_scorers_post",
                "display_name": "Create scorer",
                "short_desc": "Create Scorer",
                "desc": "Create a scorer with its evaluation configuration.",
                "groups": ("Observability",),
                "fields": {
                    "name": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                    "kind": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                    "config": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-scorer",
                "method": "GET",
                "path": "/v1/observability/scorers/{scorer_slug}",
                "operation_id": "get_scorer_v1_observability_scorers__scorer_slug__get",
                "display_name": "Get Scorer",
                "short_desc": "Get Scorer",
                "desc": "Retrieve a scorer by its slug.",
                "groups": ("Observability",),
                "fields": {
                    "scorer_slug": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "scorers",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-scorer",
                "method": "PATCH",
                "path": "/v1/observability/scorers/{scorer_slug}",
                "operation_id": "edit_scorer_v1_observability_scorers__scorer_slug__patch",
                "display_name": "Update Scorer",
                "short_desc": "Update Scorer",
                "desc": "Update the name, description or configuration of a scorer.",
                "groups": ("Observability",),
                "fields": {
                    "scorer_slug": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "scorers",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-audio-session",
                "method": "POST",
                "path": "/v1/audio/sessions",
                "operation_id": "create_session_v1_audio_sessions_post",
                "display_name": "Create audio session",
                "short_desc": "Create Audio Session",
                "desc": "Start an audio agent session.",
                "groups": ("Audio",),
                "fields": {
                    "agent_id": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "audio-agents",
                        "supports_custom_values": True,
                    },
                    "agent_version": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "audio-agent-versions",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-audio-session",
                "method": "GET",
                "path": "/v1/audio/sessions/{session_id}",
                "operation_id": "get_session_v1_audio_sessions__session_id__get",
                "display_name": "Get Audio Session",
                "short_desc": "Get Audio Session",
                "desc": "Retrieve the status of an audio session.",
                "groups": ("Audio",),
            },
            <RestSchemaActionInfo>{
                "action": "update-audio-session",
                "method": "PATCH",
                "path": "/v1/audio/sessions/{session_id}",
                "operation_id": "update_session_v1_audio_sessions__session_id__patch",
                "display_name": "Update Audio Session",
                "short_desc": "Update Audio Session",
                "desc": "Update the instructions, tools or turn settings of a live audio session.",
                "groups": ("Audio",),
            },
            <RestSchemaActionInfo>{
                "action": "stop-audio-session",
                "method": "DELETE",
                "path": "/v1/audio/sessions/{session_id}",
                "operation_id": "delete_session_v1_audio_sessions__session_id__delete",
                "display_name": "Stop Audio Session",
                "short_desc": "Stop Audio Session",
                "desc": "Stop a running audio session.",
                "groups": ("Audio",),
            },
            <RestSchemaActionInfo>{
                "action": "list-audio-agents",
                "method": "GET",
                "path": "/v1/audio/agents",
                "operation_id": "list_agents_v1_audio_agents_get",
                "display_name": "List audio agents",
                "short_desc": "List Audio Agents",
                "desc": "List audio agents with optional search and pagination.",
                "groups": ("Audio",),
            },
            <RestSchemaActionInfo>{
                "action": "create-audio-agent",
                "method": "POST",
                "path": "/v1/audio/agents",
                "operation_id": "create_agent_v1_audio_agents_post",
                "display_name": "Create audio agent",
                "short_desc": "Create Audio Agent",
                "desc": "Create an audio agent with a name and configuration.",
                "groups": ("Audio",),
                "fields": {
                    "name": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                    "configuration": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-audio-agent",
                "method": "GET",
                "path": "/v1/audio/agents/{agent_id}",
                "operation_id": "get_agent_v1_audio_agents__agent_id__get",
                "display_name": "Get Audio Agent",
                "short_desc": "Get Audio Agent",
                "desc": "Retrieve an audio agent or a specific version.",
                "groups": ("Audio",),
                "fields": {
                    "agent_id": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "audio-agents",
                        "supports_custom_values": True,
                    },
                    "version": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "audio-agent-versions",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-audio-agent",
                "method": "PUT",
                "path": "/v1/audio/agents/{agent_id}",
                "operation_id": "update_agent_v1_audio_agents__agent_id__put",
                "display_name": "Update Audio Agent",
                "short_desc": "Update Audio Agent",
                "desc": "Update an audio agent's name and configuration.",
                "groups": ("Audio",),
                "fields": {
                    "agent_id": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "audio-agents",
                        "supports_custom_values": True,
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                    "configuration": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-audio-agent",
                "method": "DELETE",
                "path": "/v1/audio/agents/{agent_id}",
                "operation_id": "delete_agent_v1_audio_agents__agent_id__delete",
                "display_name": "Delete Audio Agent",
                "short_desc": "Delete Audio Agent",
                "desc": "Delete an audio agent.",
                "groups": ("Audio",),
                "fields": {
                    "agent_id": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "audio-agents",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-audio-agent-versions",
                "method": "GET",
                "path": "/v1/audio/agents/{agent_id}/versions",
                "operation_id": "list_agent_versions_v1_audio_agents__agent_id__versions_get",
                "display_name": "List Audio Agent Versions",
                "short_desc": "List Audio Agent Versions",
                "desc": "List the versions of an audio agent.",
                "groups": ("Audio",),
                "fields": {
                    "agent_id": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "audio-agents",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-audio-agent-sessions",
                "method": "GET",
                "path": "/v1/audio/agents/{agent_id}/sessions",
                "operation_id": "list_agent_sessions_v1_audio_agents__agent_id__sessions_get",
                "display_name": "List Audio Agent Sessions",
                "short_desc": "List Audio Agent Sessions",
                "desc": "List sessions associated with an audio agent.",
                "groups": ("Audio",),
                "fields": {
                    "agent_id": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "audio-agents",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-workflow-execution-trace",
                "method": "GET",
                "path": "/v1/workflows/executions/{execution_id}/trace/info",
                "operation_id": "get_workflow_execution_trace_info",
                "display_name": "Get Workflow Execution Trace",
                "short_desc": "Get Workflow Execution Trace",
                "desc": "Retrieve trace information for a workflow execution.",
                "groups": ("Workflows",),
                "fields": {
                    "execution_id": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "workflow-runs",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-workflow-execution-logs",
                "method": "GET",
                "path": "/v1/workflows/executions/{execution_id}/logs",
                "operation_id": "get_workflow_execution_logs",
                "display_name": "Get Workflow Execution Logs",
                "short_desc": "Get Workflow Execution Logs",
                "desc": "Retrieve log entries for a workflow execution.",
                "groups": ("Workflows",),
                "fields": {
                    "execution_id": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "workflow-runs",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-workflow-schedule",
                "method": "GET",
                "path": "/v1/workflows/schedules/{schedule_id}",
                "operation_id": "get_schedule_v1_workflows_schedules__schedule_id__get",
                "display_name": "Get Workflow Schedule",
                "short_desc": "Get Workflow Schedule",
                "desc": "Retrieve a workflow schedule by ID.",
                "groups": ("Workflows",),
                "fields": {
                    "schedule_id": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "workflow-schedules",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-workflow-schedule",
                "method": "PATCH",
                "path": "/v1/workflows/schedules/{schedule_id}",
                "operation_id": "update_schedule_v1_workflows_schedules__schedule_id__patch",
                "display_name": "Update Workflow Schedule",
                "short_desc": "Update Workflow Schedule",
                "desc": "Update the configuration of a workflow schedule.",
                "groups": ("Workflows",),
                "fields": {
                    "schedule_id": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "workflow-schedules",
                        "supports_custom_values": True,
                    },
                    "schedule": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "pause-workflow-schedule",
                "method": "POST",
                "path": "/v1/workflows/schedules/{schedule_id}/pause",
                "operation_id": "pause_schedule_v1_workflows_schedules__schedule_id__pause_post",
                "display_name": "Pause Workflow Schedule",
                "short_desc": "Pause Workflow Schedule",
                "desc": "Pause a workflow schedule.",
                "groups": ("Workflows",),
                "fields": {
                    "schedule_id": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "workflow-schedules",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "resume-workflow-schedule",
                "method": "POST",
                "path": "/v1/workflows/schedules/{schedule_id}/resume",
                "operation_id": "resume_schedule_v1_workflows_schedules__schedule_id__resume_post",
                "display_name": "Resume Workflow Schedule",
                "short_desc": "Resume Workflow Schedule",
                "desc": "Resume a paused workflow schedule.",
                "groups": ("Workflows",),
                "fields": {
                    "schedule_id": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "workflow-schedules",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "trigger-workflow-schedule",
                "method": "POST",
                "path": "/v1/workflows/schedules/{schedule_id}/trigger",
                "operation_id": "trigger_schedule_v1_workflows_schedules__schedule_id__trigger_post",
                "display_name": "Trigger Workflow Schedule",
                "short_desc": "Trigger Workflow Schedule",
                "desc": "Trigger a workflow schedule immediately.",
                "groups": ("Workflows",),
                "fields": {
                    "schedule_id": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "workflow-schedules",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-workflow-deployment",
                "method": "POST",
                "path": "/v1/workflows/deployments",
                "operation_id": "create_deployment_v1_workflows_deployments_post",
                "display_name": "Create workflow deployment",
                "short_desc": "Create Workflow Deployment",
                "desc": "Create a deployment for workflow workers.",
                "groups": ("Workflows",),
                "fields": {
                    "name": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                    "spec": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-workflow-deployment",
                "method": "PATCH",
                "path": "/v1/workflows/deployments/{name}",
                "operation_id": "update_deployment_v1_workflows_deployments__name__patch",
                "display_name": "Update Workflow Deployment",
                "short_desc": "Update Workflow Deployment",
                "desc": "Update the specification or resources of a workflow deployment.",
                "groups": ("Workflows",),
                "fields": {
                    "name": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "workflow-deployments",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-workflow-deployment",
                "method": "DELETE",
                "path": "/v1/workflows/deployments/{name}",
                "operation_id": "delete_deployment_v1_workflows_deployments__name__delete",
                "display_name": "Delete Workflow Deployment",
                "short_desc": "Delete Workflow Deployment",
                "desc": "Delete a workflow deployment.",
                "groups": ("Workflows",),
                "fields": {
                    "name": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "workflow-deployments",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "stop-workflow-deployment",
                "method": "POST",
                "path": "/v1/workflows/deployments/{name}/stop",
                "operation_id": "stop_deployment_v1_workflows_deployments__name__stop_post",
                "display_name": "Stop Workflow Deployment",
                "short_desc": "Stop Workflow Deployment",
                "desc": "Stop a workflow deployment.",
                "groups": ("Workflows",),
                "fields": {
                    "name": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "workflow-deployments",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "start-workflow-deployment",
                "method": "POST",
                "path": "/v1/workflows/deployments/{name}/start",
                "operation_id": "start_deployment_v1_workflows_deployments__name__start_post",
                "display_name": "Start Workflow Deployment",
                "short_desc": "Start Workflow Deployment",
                "desc": "Start a workflow deployment.",
                "groups": ("Workflows",),
                "fields": {
                    "name": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "workflow-deployments",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "restart-workflow-deployment",
                "method": "POST",
                "path": "/v1/workflows/deployments/{name}/restart",
                "operation_id": "restart_deployment_v1_workflows_deployments__name__restart_post",
                "display_name": "Restart Workflow Deployment",
                "short_desc": "Restart Workflow Deployment",
                "desc": "Restart a workflow deployment.",
                "groups": ("Workflows",),
                "fields": {
                    "name": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "workflow-deployments",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-workflow-deployment-workers",
                "method": "GET",
                "path": "/v1/workflows/deployments/{name}/workers",
                "operation_id": "list_deployment_workers_v1_workflows_deployments__name__workers_get",
                "display_name": "List Workflow Deployment Workers",
                "short_desc": "List Workflow Deployment Workers",
                "desc": "List workers for a workflow deployment.",
                "groups": ("Workflows",),
                "fields": {
                    "name": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "workflow-deployments",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-workflow-deployment-archive",
                "method": "GET",
                "path": "/v1/workflows/deployments/{name}/archive",
                "operation_id": "get_deployment_archive_v1_workflows_deployments__name__archive_get",
                "display_name": "Get Workflow Deployment Archive",
                "short_desc": "Get Workflow Deployment Archive",
                "desc": "Retrieve archive information for a workflow deployment.",
                "groups": ("Workflows",),
                "fields": {
                    "name": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "workflow-deployments",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-workflow-deployment-logs",
                "method": "GET",
                "path": "/v1/workflows/deployments/{name}/logs",
                "operation_id": "get_deployment_logs",
                "display_name": "Get Workflow Deployment Logs",
                "short_desc": "Get Workflow Deployment Logs",
                "desc": "Retrieve log entries for a workflow deployment.",
                "groups": ("Workflows",),
                "fields": {
                    "name": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "workflow-deployments",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-workflows",
                "method": "GET",
                "path": "/v1/workflows",
                "operation_id": "get_workflows_v1_workflows_get",
                "display_name": "List workflows",
                "short_desc": "List Workflows",
                "desc": "List workflows with optional search, status and tag filters.",
                "groups": ("Workflows",),
                "fields": {
                    "deployment_name": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "workflow-deployments",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "archive-workflows",
                "method": "PUT",
                "path": "/v1/workflows/archive",
                "operation_id": "bulk_archive_workflows_v1_workflows_archive_put",
                "display_name": "Archive workflows",
                "short_desc": "Archive Workflows",
                "desc": "Archive the selected workflows.",
                "groups": ("Workflows",),
                "fields": {
                    "workflow_ids": <RestSchemaFieldOverlayInfo>{
                        "element_ref_data": "workflow-ids",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "unarchive-workflows",
                "method": "PUT",
                "path": "/v1/workflows/unarchive",
                "operation_id": "bulk_unarchive_workflows_v1_workflows_unarchive_put",
                "display_name": "Unarchive workflows",
                "short_desc": "Unarchive Workflows",
                "desc": "Restore the selected workflows from the archive.",
                "groups": ("Workflows",),
                "fields": {
                    "workflow_ids": <RestSchemaFieldOverlayInfo>{
                        "element_ref_data": "workflow-ids",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "add-workflow-tags",
                "method": "POST",
                "path": "/v1/workflows/{workflow_identifier}/tags",
                "operation_id": "add_workflow_tags_v1_workflows__workflow_identifier__tags_post",
                "display_name": "Add Workflow Tags",
                "short_desc": "Add Workflow Tags",
                "desc": "Add tags to a workflow.",
                "groups": ("Workflows",),
                "fields": {
                    "workflow_identifier": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "workflow-ids",
                        "supports_custom_values": True,
                    },
                    "tags": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-ingestion-pipeline-configurations",
                "method": "GET",
                "path": "/v1/rag/ingestion_pipeline_configurations",
                "operation_id": "get_configs_v1_rag_ingestion_pipeline_configurations_get",
                "display_name": "List ingestion pipeline configurations",
                "short_desc": "List Ingestion Pipeline Configurations",
                "desc": "List registered ingestion pipeline configurations.",
                "groups": ("RAG",),
            },
            <RestSchemaActionInfo>{
                "action": "register-ingestion-pipeline-configuration",
                "method": "PUT",
                "path": "/v1/rag/ingestion_pipeline_configurations",
                "operation_id": "register_config_v1_rag_ingestion_pipeline_configurations_put",
                "display_name": "Register ingestion pipeline configuration",
                "short_desc": "Register Ingestion Pipeline Configuration",
                "desc": "Register an ingestion pipeline configuration.",
                "groups": ("RAG",),
                "fields": {
                    "name": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-ingestion-pipeline-run",
                "method": "PUT",
                "path": "/v1/rag/ingestion_pipeline_configurations/{id}/run_info",
                "operation_id": "update_run_info_v1_rag_ingestion_pipeline_configurations__id__run_info_put",
                "display_name": "Update Ingestion Pipeline Run",
                "short_desc": "Update Ingestion Pipeline Run",
                "desc": "Update the execution time and chunk count of an ingestion pipeline run.",
                "groups": ("RAG",),
                "fields": {
                    "id": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "ingestion-configurations",
                        "supports_custom_values": True,
                    },
                    "execution_time": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                    "chunks_count": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-rag-deployments",
                "method": "GET",
                "path": "/v1/rag/deployments",
                "operation_id": "get_deployment_summaries_v1_rag_deployments_get",
                "display_name": "List RAG deployments",
                "short_desc": "List RAG Deployments",
                "desc": "List registered search index deployments.",
                "groups": ("RAG",),
            },
            <RestSchemaActionInfo>{
                "action": "register-rag-deployment",
                "method": "PUT",
                "path": "/v1/rag/deployments",
                "operation_id": "register_deployment_v1_rag_deployments_put",
                "display_name": "Register RAG deployment",
                "short_desc": "Register RAG Deployment",
                "desc": "Register or replace a search index deployment.",
                "groups": ("RAG",),
                "fields": {
                    "name": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                    "deployment": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "unregister-rag-deployment",
                "method": "DELETE",
                "path": "/v1/rag/deployments/{deployment_id}",
                "operation_id": "unregister_deployment_v1_rag_deployments__deployment_id__delete",
                "display_name": "Unregister RAG Deployment",
                "short_desc": "Unregister RAG Deployment",
                "desc": "Remove a search index deployment from the registry.",
                "groups": ("RAG",),
                "fields": {
                    "deployment_id": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "rag-deployments",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-rag-deployment-metrics",
                "method": "PUT",
                "path": "/v1/rag/deployments/{deployment_id}/metrics",
                "operation_id": "update_index_metrics_v1_rag_deployments__deployment_id__metrics_put",
                "display_name": "Update RAG Deployment Metrics",
                "short_desc": "Update RAG Deployment Metrics",
                "desc": "Report status and metrics for a search index deployment.",
                "groups": ("RAG",),
            },
            <RestSchemaActionInfo>{
                "action": "create-search-index",
                "method": "POST",
                "path": "/v1/context/search-indexes",
                "operation_id": "create_index_v1_context_search_indexes_post",
                "display_name": "Create search index",
                "short_desc": "Create Search Index",
                "desc": "Create a managed search index.",
                "groups": ("Search Indexes",),
                "fields": {
                    "name": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                    "config": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-search-indexes",
                "method": "GET",
                "path": "/v1/context/search-indexes",
                "operation_id": "list_indexes_v1_context_search_indexes_get",
                "display_name": "List search indexes",
                "short_desc": "List Search Indexes",
                "desc": "List managed search indexes.",
                "groups": ("Search Indexes",),
            },
            <RestSchemaActionInfo>{
                "action": "get-search-index",
                "method": "GET",
                "path": "/v1/context/search-indexes/{index_name}",
                "operation_id": "get_index_v1_context_search_indexes__index_name__get",
                "display_name": "Get Search Index",
                "short_desc": "Get Search Index",
                "desc": "Retrieve a managed search index by name.",
                "groups": ("Search Indexes",),
                "fields": {
                    "index_name": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "search-indexes",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-search-index-schema",
                "method": "PUT",
                "path": "/v1/context/search-indexes/{index_name}",
                "operation_id": "update_index_v1_context_search_indexes__index_name__put",
                "display_name": "Update Search Index Schema",
                "short_desc": "Update Search Index Schema",
                "desc": "Update the document and chunk fields of a managed search index.",
                "groups": ("Search Indexes",),
                "fields": {
                    "index_name": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "search-indexes",
                        "supports_custom_values": True,
                    },
                    "schema": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-search-index",
                "method": "DELETE",
                "path": "/v1/context/search-indexes/{index_name}",
                "operation_id": "delete_index_v1_context_search_indexes__index_name__delete",
                "display_name": "Delete Search Index",
                "short_desc": "Delete Search Index",
                "desc": "Delete a managed search index.",
                "groups": ("Search Indexes",),
                "fields": {
                    "index_name": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "search-indexes",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "ingest-search-index-documents",
                "method": "POST",
                "path": "/v1/context/search-indexes/{index_name}/documents",
                "operation_id": "ingest_documents_v1_context_search_indexes__index_name__documents_post",
                "display_name": "Ingest Search Index Documents",
                "short_desc": "Ingest Search Index Documents",
                "desc": "Add documents to a managed search index.",
                "groups": ("Search Indexes",),
                "fields": {
                    "index_name": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "search-indexes",
                        "supports_custom_values": True,
                    },
                    "documents": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-search-index-documents",
                "method": "DELETE",
                "path": "/v1/context/search-indexes/{index_name}/documents",
                "operation_id": "delete_documents_v1_context_search_indexes__index_name__documents_delete",
                "display_name": "Delete Search Index Documents",
                "short_desc": "Delete Search Index Documents",
                "desc": "Delete selected documents from a managed search index.",
                "groups": ("Search Indexes",),
                "fields": {
                    "index_name": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "search-indexes",
                        "supports_custom_values": True,
                    },
                    "document_ids": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "search-index-chunks",
                "method": "POST",
                "path": "/v1/context/search-indexes/{index_name}/search",
                "operation_id": "search_index_v1_context_search_indexes__index_name__search_post",
                "display_name": "Search Index Chunks",
                "short_desc": "Search Index Chunks",
                "desc": "Search a managed index using a retriever.",
                "groups": ("Search Indexes",),
                "mutates_state": False,
                "fields": {
                    "index_name": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "search-indexes",
                        "supports_custom_values": True,
                    },
                    "retriever": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "navigate-search-index-chunks",
                "method": "POST",
                "path": "/v1/context/search-indexes/{index_name}/navigate",
                "operation_id": "navigate_index_v1_context_search_indexes__index_name__navigate_post",
                "display_name": "Navigate Search Index Chunks",
                "short_desc": "Navigate Search Index Chunks",
                "desc": "Retrieve chunks adjacent to a source span.",
                "groups": ("Search Indexes",),
                "mutates_state": False,
                "fields": {
                    "index_name": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "search-indexes",
                        "supports_custom_values": True,
                    },
                    "source_id": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                    "start_offset": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                    "end_offset": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                    "direction": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "read-search-index-chunks",
                "method": "POST",
                "path": "/v1/context/search-indexes/{index_name}/read",
                "operation_id": "read_index_v1_context_search_indexes__index_name__read_post",
                "display_name": "Read Search Index Chunks",
                "short_desc": "Read Search Index Chunks",
                "desc": "Read the chunks within a span of a source document.",
                "groups": ("Search Indexes",),
                "mutates_state": False,
                "fields": {
                    "index_name": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "search-indexes",
                        "supports_custom_values": True,
                    },
                    "source_id": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "find-text-in-search-index",
                "method": "POST",
                "path": "/v1/context/search-indexes/{index_name}/grep",
                "operation_id": "grep_index_v1_context_search_indexes__index_name__grep_post",
                "display_name": "Find Text in Search Index",
                "short_desc": "Find Text in Search Index",
                "desc": "Find a text pattern within an indexed source.",
                "groups": ("Search Indexes",),
                "mutates_state": False,
                "fields": {
                    "index_name": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "search-indexes",
                        "supports_custom_values": True,
                    },
                    "source_id": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                    "pattern": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-search-index-chunk",
                "method": "GET",
                "path": "/v1/context/search-indexes/{index_name}/chunks/{chunk_id}",
                "operation_id": "get_chunk_index_v1_context_search_indexes__index_name__chunks__chunk_id__get",
                "display_name": "Get Search Index Chunk",
                "short_desc": "Get Search Index Chunk",
                "desc": "Retrieve an indexed chunk by ID.",
                "groups": ("Search Indexes",),
                "fields": {
                    "index_name": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "search-indexes",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-search-index-summary",
                "method": "GET",
                "path": "/v1/context/search-indexes/{index_name}/summary_field/{language}",
                "operation_id": "get_managed_index_summary_v1_context_search_indexes__index_name__summary_field__language__get",
                "display_name": "Get Search Index Summary",
                "short_desc": "Get Search Index Summary",
                "desc": "Retrieve an index summary in the selected language.",
                "groups": ("Search Indexes",),
                "fields": {
                    "index_name": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "search-indexes",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "set-search-index-summary",
                "method": "PUT",
                "path": "/v1/context/search-indexes/{index_name}/summary_field/{language}",
                "operation_id": "set_managed_index_summary_v1_context_search_indexes__index_name__summary_field__language__put",
                "display_name": "Set Search Index Summary",
                "short_desc": "Set Search Index Summary",
                "desc": "Set the summary of a managed index in the selected language.",
                "groups": ("Search Indexes",),
                "fields": {
                    "index_name": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "search-indexes",
                        "supports_custom_values": True,
                    },
                    "content": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                    "status": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                    "translated": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-trigger",
                "method": "POST",
                "path": "/v1/triggers",
                "operation_id": "create_trigger_v1_triggers_post",
                "display_name": "Create trigger",
                "short_desc": "Create Trigger",
                "desc": "Create an event trigger for the selected handler and action.",
                "groups": ("Triggers",),
                "fields": {
                    "event_source_id": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                    "handler": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                    "action": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                    "owner_type": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                    "title": <RestSchemaFieldOverlayInfo>{
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-triggers",
                "method": "GET",
                "path": "/v1/triggers",
                "operation_id": "list_triggers_v1_triggers_get",
                "display_name": "List triggers",
                "short_desc": "List Triggers",
                "desc": "List triggers with optional handler and event source filters.",
                "groups": ("Triggers",),
            },
            <RestSchemaActionInfo>{
                "action": "get-trigger",
                "method": "GET",
                "path": "/v1/triggers/{trigger_id}",
                "operation_id": "get_trigger_v1_triggers__trigger_id__get",
                "display_name": "Get Trigger",
                "short_desc": "Get Trigger",
                "desc": "Retrieve an event trigger by ID.",
                "groups": ("Triggers",),
                "fields": {
                    "trigger_id": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "triggers",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-trigger",
                "method": "DELETE",
                "path": "/v1/triggers/{trigger_id}",
                "operation_id": "delete_trigger_v1_triggers__trigger_id__delete",
                "display_name": "Delete Trigger",
                "short_desc": "Delete Trigger",
                "desc": "Delete an event trigger.",
                "groups": ("Triggers",),
                "fields": {
                    "trigger_id": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "triggers",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-trigger-events",
                "method": "GET",
                "path": "/v1/triggers/{trigger_id}/events",
                "operation_id": "list_trigger_events_v1_triggers__trigger_id__events_get",
                "display_name": "List Trigger Events",
                "short_desc": "List Trigger Events",
                "desc": "List the events received by a trigger.",
                "groups": ("Triggers",),
                "fields": {
                    "trigger_id": <RestSchemaFieldOverlayInfo>{
                        "ref_data": "triggers",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-client-session",
                "method": "POST",
                "path": "/v1/client/sessions",
                "operation_id": "create_client_session_v1_client_sessions_post",
                "display_name": "Create client session",
                "short_desc": "Create Client Session",
                "desc": "Create a client session. Requires the `create_client_session` permission. Create sessions for clients that connect to realtime services.",
                "groups": ("Realtime Sessions",),
                "fields": {
                    "purpose": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Purpose",
                        "short_desc": "Purpose",
                        "desc": "The `purpose` value for this operation.",
                    },
                    "model": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model",
                        "short_desc": "Model",
                        "desc": "The `model` value for this operation.",
                    },
                    "ttl_seconds": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Ttl Seconds",
                        "short_desc": "Ttl Seconds",
                        "desc": "The `ttl_seconds` value for this operation.",
                    },
                    "metadata": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Metadata",
                        "short_desc": "Request metadata applied to the realtime session, as if sent in the `x-metadata` header.",
                    },
                },
                "response_fields": {
                    "client_secret": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "value": <RestSchemaFieldOverlayInfo>{
                                "sensitive": True,
                            },
                        },
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-service-account",
                "method": "POST",
                "path": "/v1/service-accounts",
                "operation_id": "create_service_account_v1_service_accounts_post",
                "display_name": "Create service account",
                "short_desc": "Create Service Account",
                "desc": "Create a service account in a workspace. Requires the Workspace admin (`workspace_admin`) role. Manage workspace service accounts and their roles.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Service Accounts",),
                "fields": {
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "Name",
                        "desc": "The `name` value for this operation.",
                    },
                    "workspace_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Workspace ID",
                        "short_desc": "Workspace ID",
                        "desc": "The `workspace_id` value for this operation.",
                        "ref_data": "my-workspaces",
                        "supports_custom_values": True,
                    },
                    "description": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Description",
                        "short_desc": "Description",
                        "desc": "The `description` value for this operation.",
                    },
                    "role_ids": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Role IDs",
                        "short_desc": "Workspace role UUIDs to assign. Omitted or empty lists default to member, which grants no role permissions. Workspace-wide sharing may still grant access.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-service-accounts",
                "method": "GET",
                "path": "/v1/service-accounts",
                "operation_id": "list_service_accounts_v1_service_accounts_get",
                "display_name": "List service accounts",
                "short_desc": "List Service Accounts",
                "desc": "List the service accounts in a workspace, or across the organization. Manage workspace service accounts and their roles.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Service Accounts",),
                "fields": {
                    "workspace_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Workspace ID",
                        "short_desc": "Workspace ID",
                        "desc": "The `workspace_id` value for this operation.",
                        "ref_data": "my-workspaces",
                        "supports_custom_values": True,
                    },
                    "include_deleted": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Include Deleted",
                        "short_desc": "Include Deleted",
                        "desc": "The `include_deleted` value for this operation.",
                    },
                    "q": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Q",
                        "short_desc": "Q",
                        "desc": "The `q` value for this operation.",
                    },
                    "order": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Order",
                        "short_desc": "Order",
                        "desc": "The `order` value for this operation.",
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "asc",
                                "display_name": "Ascending",
                            },
                            <AllowedValueInfo>{
                                "value": "desc",
                                "display_name": "Descending",
                            },
                        ),
                    },
                    "offset": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Offset",
                        "short_desc": "Offset",
                        "desc": "The `offset` value for this operation.",
                    },
                    "limit": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Limit",
                        "short_desc": "Limit",
                        "desc": "The `limit` value for this operation.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-assignable-service-account-roles",
                "method": "GET",
                "path": "/v1/service-accounts/assignable-roles",
                "operation_id": "list_assignable_service_account_roles_v1_service_accounts_assignable_roles_get",
                "display_name": "List assignable service account roles",
                "short_desc": "List Assignable Service Account Roles",
                "desc": "List the workspace roles that can be assigned to a service account. Manage workspace service accounts and their roles.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Service Accounts",),
                "fields": {
                    "workspace_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Workspace ID",
                        "short_desc": "Workspace ID",
                        "desc": "The `workspace_id` value for this operation.",
                        "ref_data": "my-workspaces",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-service-account",
                "method": "GET",
                "path": "/v1/service-accounts/{service_account_id}",
                "operation_id": "get_service_account_v1_service_accounts__service_account_id__get",
                "display_name": "Get Service Account",
                "short_desc": "Get Service Account",
                "desc": "Retrieve a service account. Manage workspace service accounts and their roles.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Service Accounts",),
                "fields": {
                    "service_account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Service Account ID",
                        "short_desc": "Service Account ID",
                        "desc": "The `service_account_id` value for this operation.",
                        "ref_data": "service-accounts",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-service-account",
                "method": "PATCH",
                "path": "/v1/service-accounts/{service_account_id}",
                "operation_id": "update_service_account_v1_service_accounts__service_account_id__patch",
                "display_name": "Update Service Account",
                "short_desc": "Update Service Account",
                "desc": "Update a service account. Manage workspace service accounts and their roles.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Service Accounts",),
                "fields": {
                    "service_account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Service Account ID",
                        "short_desc": "Service Account ID",
                        "desc": "The `service_account_id` value for this operation.",
                        "ref_data": "service-accounts",
                        "supports_custom_values": True,
                    },
                    "description": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Description",
                        "short_desc": "Description",
                        "desc": "The `description` value for this operation.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-service-account",
                "method": "DELETE",
                "path": "/v1/service-accounts/{service_account_id}",
                "operation_id": "delete_service_account_v1_service_accounts__service_account_id__delete",
                "display_name": "Delete Service Account",
                "short_desc": "Delete Service Account",
                "desc": "Delete a service account. Manage workspace service accounts and their roles.\n\n**Public preview:** Availability depends on your account and permissions.\n\n**Deletion:** This operation can permanently remove the selected resource or access.",
                "groups": ("Service Accounts",),
                "fields": {
                    "service_account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Service Account ID",
                        "short_desc": "Service Account ID",
                        "desc": "The `service_account_id` value for this operation.",
                        "ref_data": "service-accounts",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "set-service-account-roles",
                "method": "PUT",
                "path": "/v1/service-accounts/{service_account_id}/roles",
                "operation_id": "set_service_account_roles_v1_service_accounts__service_account_id__roles_put",
                "display_name": "Set Service Account Roles",
                "short_desc": "Set Service Account Roles",
                "desc": "Replace the workspace roles assigned to a service account. Manage workspace service accounts and their roles.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Service Accounts",),
                "fields": {
                    "service_account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Service Account ID",
                        "short_desc": "Service Account ID",
                        "desc": "The `service_account_id` value for this operation.",
                        "ref_data": "service-accounts",
                        "supports_custom_values": True,
                    },
                    "role_ids": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Role IDs",
                        "short_desc": "Workspace role UUIDs to assign. Required and non-empty; omitted or empty lists return 422. Select member explicitly for no role-granted permissions; workspace-wide sharing may still grant access.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-service-account-roles",
                "method": "GET",
                "path": "/v1/service-accounts/{service_account_id}/roles",
                "operation_id": "list_service_account_roles_v1_service_accounts__service_account_id__roles_get",
                "display_name": "List Service Account Roles",
                "short_desc": "List Service Account Roles",
                "desc": "List the workspace roles assigned to a service account. Manage workspace service accounts and their roles.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Service Accounts",),
                "fields": {
                    "service_account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Service Account ID",
                        "short_desc": "Service Account ID",
                        "desc": "The `service_account_id` value for this operation.",
                        "ref_data": "service-accounts",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "share-connector-to-workspace",
                "method": "PUT",
                "path": "/v1/connectors/{connector_id}/share",
                "operation_id": "connector_share_v1",
                "display_name": "Share Connector To Workspace",
                "short_desc": "Share Connector To Workspace",
                "desc": "Transfers ownership of a private user-owned connector to the current workspace, making it available to all workspace members. The creator can later revert this via the unshare endpoint. Any authentication flows that rely on the original owner's identity (e.g. OAuth on-behalf-of) will be affected and must be reconfigured after sharing. Only the connector's creator can call this endpoint. Requires the ShareConnectorToWorkspace workspace permission. Connect assistants to external business tools and manage their access.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Connectors",),
                "fields": {
                    "connector_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Connector ID",
                        "short_desc": "Connector ID",
                        "desc": "The `connector_id` value for this operation.",
                        "ref_data": "connectors",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "unshare-connector-from-workspace",
                "method": "DELETE",
                "path": "/v1/connectors/{connector_id}/share",
                "operation_id": "connector_unshare_v1",
                "display_name": "Unshare Connector From Workspace",
                "short_desc": "Unshare Connector From Workspace",
                "desc": "Reverts a workspace-shared connector back to a private, creator-owned connector. Workspace-scoped connections and other members' connections are removed; the creator's own connection is preserved. Only the connector's creator can call this endpoint. Requires the ShareConnectorToWorkspace workspace permission. Connect assistants to external business tools and manage their access.\n\n**Public preview:** Availability depends on your account and permissions.\n\n**Deletion:** This operation can permanently remove the selected resource or access.",
                "groups": ("Connectors",),
                "fields": {
                    "connector_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Connector ID",
                        "short_desc": "Connector ID",
                        "desc": "The `connector_id` value for this operation.",
                        "ref_data": "connectors",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "share-connector-to-organization",
                "method": "PUT",
                "path": "/v1/connectors/{connector_id}/organization/share",
                "operation_id": "connector_share_to_organization_v1",
                "display_name": "Share Connector To Organization",
                "short_desc": "Share Connector To Organization",
                "desc": "Transfers ownership of a private user-owned connector to the current organization, making it available to all organization members. The creator can later revert this via the unshare endpoint. Any authentication flows that rely on the original owner's identity (e.g. OAuth on-behalf-of) will be affected and must be reconfigured after sharing. Requires the ShareConnectorToOrg organization permission. Connect assistants to external business tools and manage their access.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Connectors",),
                "fields": {
                    "connector_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Connector ID",
                        "short_desc": "Connector ID",
                        "desc": "The `connector_id` value for this operation.",
                        "ref_data": "connectors",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "unshare-connector-from-organization",
                "method": "DELETE",
                "path": "/v1/connectors/{connector_id}/organization/share",
                "operation_id": "connector_unshare_from_organization_v1",
                "display_name": "Unshare Connector From Organization",
                "short_desc": "Unshare Connector From Organization",
                "desc": "Reverts an organization-shared connector back to a private, creator-owned connector. Organization-scoped connections and other members' connections are removed; the creator's own connection is preserved. Requires the ShareConnectorToOrg organization permission. Connect assistants to external business tools and manage their access.\n\n**Public preview:** Availability depends on your account and permissions.\n\n**Deletion:** This operation can permanently remove the selected resource or access.",
                "groups": ("Connectors",),
                "fields": {
                    "connector_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Connector ID",
                        "short_desc": "Connector ID",
                        "desc": "The `connector_id` value for this operation.",
                        "ref_data": "connectors",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "activate-connector-for-organization",
                "method": "POST",
                "path": "/v1/connectors/{connector_id}/{consumer_scope}/activate",
                "operation_id": "connector_activate_for_consumer_v1",
                "display_name": "Activate Connector For Organization",
                "short_desc": "Activate Connector For Organization",
                "desc": "Enable a connector for the consumer. Connect assistants to external business tools and manage their access.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Connectors",),
                "fields": {
                    "connector_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Connector ID",
                        "short_desc": "Connector ID",
                        "desc": "The `connector_id` value for this operation.",
                        "ref_data": "connectors",
                        "supports_custom_values": True,
                    },
                    "consumer_scope": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Consumer Scope",
                        "short_desc": "Always \"organization\" for this action",
                        "desc": "Fixed to `organization`: this action applies to the organization.",
                        "required": True,
                        "default_value": "organization",
                        "allowed_values": ("organization",),
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "activate-connector-for-workspace",
                "method": "POST",
                "path": "/v1/connectors/{connector_id}/{consumer_scope}/activate",
                "operation_id": "connector_activate_for_consumer_v1",
                "variant_of": "activate-connector-for-organization",
                "display_name": "Activate Connector For Workspace",
                "short_desc": "Activate Connector For Workspace",
                "desc": "Enable a connector for the consumer. Connect assistants to external business tools and manage their access.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Connectors",),
                "fields": {
                    "connector_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Connector ID",
                        "short_desc": "Connector ID",
                        "desc": "The `connector_id` value for this operation.",
                        "ref_data": "connectors",
                        "supports_custom_values": True,
                    },
                    "consumer_scope": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Consumer Scope",
                        "short_desc": "Always \"workspace\" for this action",
                        "desc": "Fixed to `workspace`: this action applies to the current workspace.",
                        "required": True,
                        "default_value": "workspace",
                        "allowed_values": ("workspace",),
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "activate-connector-for-user",
                "method": "POST",
                "path": "/v1/connectors/{connector_id}/{consumer_scope}/activate",
                "operation_id": "connector_activate_for_consumer_v1",
                "variant_of": "activate-connector-for-organization",
                "display_name": "Activate Connector For User",
                "short_desc": "Activate Connector For User",
                "desc": "Enable a connector for the consumer. Connect assistants to external business tools and manage their access.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Connectors",),
                "fields": {
                    "connector_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Connector ID",
                        "short_desc": "Connector ID",
                        "desc": "The `connector_id` value for this operation.",
                        "ref_data": "connectors",
                        "supports_custom_values": True,
                    },
                    "consumer_scope": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Consumer Scope",
                        "short_desc": "Always \"user\" for this action",
                        "desc": "Fixed to `user`: this action applies to the calling user.",
                        "required": True,
                        "default_value": "user",
                        "allowed_values": ("user",),
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "deactivate-connector-for-organization",
                "method": "POST",
                "path": "/v1/connectors/{connector_id}/{consumer_scope}/deactivate",
                "operation_id": "connector_deactivate_for_consumer_v1",
                "display_name": "Deactivate Connector For Organization",
                "short_desc": "Deactivate Connector For Organization",
                "desc": "Disable a connector for the calling consumer only. Connect assistants to external business tools and manage their access.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Connectors",),
                "fields": {
                    "connector_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Connector ID",
                        "short_desc": "Connector ID",
                        "desc": "The `connector_id` value for this operation.",
                        "ref_data": "connectors",
                        "supports_custom_values": True,
                    },
                    "consumer_scope": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Consumer Scope",
                        "short_desc": "Always \"organization\" for this action",
                        "desc": "Fixed to `organization`: this action applies to the organization.",
                        "required": True,
                        "default_value": "organization",
                        "allowed_values": ("organization",),
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "deactivate-connector-for-workspace",
                "method": "POST",
                "path": "/v1/connectors/{connector_id}/{consumer_scope}/deactivate",
                "operation_id": "connector_deactivate_for_consumer_v1",
                "variant_of": "deactivate-connector-for-organization",
                "display_name": "Deactivate Connector For Workspace",
                "short_desc": "Deactivate Connector For Workspace",
                "desc": "Disable a connector for the calling consumer only. Connect assistants to external business tools and manage their access.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Connectors",),
                "fields": {
                    "connector_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Connector ID",
                        "short_desc": "Connector ID",
                        "desc": "The `connector_id` value for this operation.",
                        "ref_data": "connectors",
                        "supports_custom_values": True,
                    },
                    "consumer_scope": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Consumer Scope",
                        "short_desc": "Always \"workspace\" for this action",
                        "desc": "Fixed to `workspace`: this action applies to the current workspace.",
                        "required": True,
                        "default_value": "workspace",
                        "allowed_values": ("workspace",),
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "deactivate-connector-for-user",
                "method": "POST",
                "path": "/v1/connectors/{connector_id}/{consumer_scope}/deactivate",
                "operation_id": "connector_deactivate_for_consumer_v1",
                "variant_of": "deactivate-connector-for-organization",
                "display_name": "Deactivate Connector For User",
                "short_desc": "Deactivate Connector For User",
                "desc": "Disable a connector for the calling consumer only. Connect assistants to external business tools and manage their access.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Connectors",),
                "fields": {
                    "connector_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Connector ID",
                        "short_desc": "Connector ID",
                        "desc": "The `connector_id` value for this operation.",
                        "ref_data": "connectors",
                        "supports_custom_values": True,
                    },
                    "consumer_scope": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Consumer Scope",
                        "short_desc": "Always \"user\" for this action",
                        "desc": "Fixed to `user`: this action applies to the calling user.",
                        "required": True,
                        "default_value": "user",
                        "allowed_values": ("user",),
                        "preselected": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-all-user-credentials-for-a-connector",
                "method": "DELETE",
                "path": "/v1/connectors/{connector_id_or_name}/user/credentials",
                "operation_id": "connector_delete_all_user_credentials_v1",
                "display_name": "Delete All User Credentials For A Connector",
                "short_desc": "Delete All User Credentials For A Connector",
                "desc": "Delete all credentials configured at the user level for a given connector. Connect assistants to external business tools and manage their access.\n\n**Public preview:** Availability depends on your account and permissions.\n\n**Deletion:** This operation can permanently remove the selected resource or access.",
                "groups": ("Connectors",),
                "fields": {
                    "connector_id_or_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Connector ID Or Name",
                        "short_desc": "Connector ID Or Name",
                        "desc": "The `connector_id_or_name` value for this operation.",
                        "ref_data": "connectors",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-workspace-credentials-for-a-connector",
                "method": "POST",
                "path": "/v1/connectors/{connector_id_or_name}/{consumer_scope}/credentials",
                "operation_id": "connector_create_credentials_v1",
                "display_name": "Create Workspace Credentials For A Connector",
                "short_desc": "Create Workspace Credentials For A Connector",
                "desc": "Create consumer credentials for a given connector and consumer scope (workspace, user). Connect assistants to external business tools and manage their access.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Connectors",),
                "fields": {
                    "connector_id_or_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Connector ID Or Name",
                        "short_desc": "Connector ID Or Name",
                        "desc": "The `connector_id_or_name` value for this operation.",
                        "ref_data": "connectors",
                        "supports_custom_values": True,
                    },
                    "consumer_scope": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Consumer Scope",
                        "short_desc": "Always \"workspace\" for this action",
                        "desc": "Fixed to `workspace`: this action applies to the current workspace.",
                        "required": True,
                        "default_value": "workspace",
                        "allowed_values": ("workspace",),
                        "preselected": True,
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "Name of the credentials. Use this name to access or modify your credentials.",
                    },
                    "title": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Title",
                        "short_desc": "Human-readable title for the credentials.",
                    },
                    "is_default": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Is Default",
                        "short_desc": "Controls whether this credential is the default for its auth method. On creation: if no credential exists yet for this auth method, the credential is automatically set as default when is_default is true or omitted; setting is_default to false is rejected because a default must exist. If other credentials already exist, setting is_default to true promotes this credential (demoting the previous default); false or omitted creates it as non-default. On update: true promotes this credential, false is rejected if it is currently the default (promote another credential first), omitted leaves the default status unchanged.",
                    },
                    "credentials": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Credentials",
                        "short_desc": "The credential data (headers, query_params, bearer_token, or OAuth2 client credentials).",
                        "sensitive": True,
                        "fields": {
                            "oauth": <RestSchemaFieldOverlayInfo>{
                                "fields": {
                                    "client_secret": <RestSchemaFieldOverlayInfo>{
                                        "sensitive": True,
                                    },
                                    "client_private_key": <RestSchemaFieldOverlayInfo>{
                                        "sensitive": True,
                                    },
                                },
                            },
                        },
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-user-credentials-for-a-connector",
                "method": "POST",
                "path": "/v1/connectors/{connector_id_or_name}/{consumer_scope}/credentials",
                "operation_id": "connector_create_credentials_v1",
                "variant_of": "create-workspace-credentials-for-a-connector",
                "display_name": "Create User Credentials For A Connector",
                "short_desc": "Create User Credentials For A Connector",
                "desc": "Create consumer credentials for a given connector and consumer scope (workspace, user). Connect assistants to external business tools and manage their access.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Connectors",),
                "fields": {
                    "connector_id_or_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Connector ID Or Name",
                        "short_desc": "Connector ID Or Name",
                        "desc": "The `connector_id_or_name` value for this operation.",
                        "ref_data": "connectors",
                        "supports_custom_values": True,
                    },
                    "consumer_scope": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Consumer Scope",
                        "short_desc": "Always \"user\" for this action",
                        "desc": "Fixed to `user`: this action applies to the calling user.",
                        "required": True,
                        "default_value": "user",
                        "allowed_values": ("user",),
                        "preselected": True,
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "Name of the credentials. Use this name to access or modify your credentials.",
                    },
                    "title": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Title",
                        "short_desc": "Human-readable title for the credentials.",
                    },
                    "is_default": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Is Default",
                        "short_desc": "Controls whether this credential is the default for its auth method. On creation: if no credential exists yet for this auth method, the credential is automatically set as default when is_default is true or omitted; setting is_default to false is rejected because a default must exist. If other credentials already exist, setting is_default to true promotes this credential (demoting the previous default); false or omitted creates it as non-default. On update: true promotes this credential, false is rejected if it is currently the default (promote another credential first), omitted leaves the default status unchanged.",
                    },
                    "credentials": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Credentials",
                        "short_desc": "The credential data (headers, query_params, bearer_token, or OAuth2 client credentials).",
                        "sensitive": True,
                        "fields": {
                            "oauth": <RestSchemaFieldOverlayInfo>{
                                "fields": {
                                    "client_secret": <RestSchemaFieldOverlayInfo>{
                                        "sensitive": True,
                                    },
                                    "client_private_key": <RestSchemaFieldOverlayInfo>{
                                        "sensitive": True,
                                    },
                                },
                            },
                        },
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "unharden-workflow-deployment",
                "method": "POST",
                "path": "/v1/workflows/deployments/{deployment_id}/unharden",
                "operation_id": "unharden_deployment_v1_workflows_deployments__deployment_id__unharden_post",
                "display_name": "Unharden Workflow Deployment",
                "short_desc": "Unharden Workflow Deployment",
                "desc": "Unharden Deployment. Manage automated workflows and inspect their execution.",
                "groups": ("Workflows Deployments",),
                "fields": {
                    "deployment_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Deployment ID",
                        "short_desc": "Deployment ID",
                        "desc": "The `deployment_id` value for this operation.",
                        "ref_data": "workflow-deployment-ids",
                        "supports_custom_values": True,
                    },
                    "workspace_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Workspace ID",
                        "short_desc": "Workspace ID to scope the request to. Defaults to the caller's context.",
                        "ref_data": "my-workspaces",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "generate-search-index-summary",
                "method": "POST",
                "response_content_type": "application/x-ndjson",
                "path": "/v1/context/search-indexes/{index_name}/summary_field/{language}",
                "operation_id": "generate_managed_index_summary_post_v1_context_search_indexes__index_name__summary_field__language__post",
                "display_name": "Generate Search Index Summary",
                "short_desc": "Generate Search Index Summary",
                "desc": "Generate the summary field of a managed index in a language and return it as the list of its records: the summary metadata first, then the chunks of content to join, or an error record. With force, an existing summary is regenerated.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Search Indexes",),
                "fields": {
                    "language": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Language",
                        "short_desc": "Language",
                        "desc": "The `language` value for this operation.",
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "en",
                                "display_name": "English",
                            },
                            <AllowedValueInfo>{
                                "value": "fr",
                                "display_name": "French",
                            },
                            <AllowedValueInfo>{
                                "value": "es",
                                "display_name": "Spanish",
                            },
                            <AllowedValueInfo>{
                                "value": "de",
                                "display_name": "German",
                            },
                            <AllowedValueInfo>{
                                "value": "it",
                                "display_name": "Italian",
                            },
                            <AllowedValueInfo>{
                                "value": "pt_br",
                                "display_name": "Brazilian Portuguese",
                            },
                            <AllowedValueInfo>{
                                "value": "pl",
                                "display_name": "Polish",
                            },
                            <AllowedValueInfo>{
                                "value": "ar",
                                "display_name": "Arabic",
                            },
                            <AllowedValueInfo>{
                                "value": "nl",
                                "display_name": "Dutch",
                            },
                        ),
                    },
                    "index_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Index Name",
                        "short_desc": "Index Name",
                        "desc": "The `index_name` value for this operation.",
                        "ref_data": "search-indexes",
                        "supports_custom_values": True,
                    },
                    "force": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Force",
                        "short_desc": "Regenerate the summary even if one already exists for this language",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-webhooks",
                "method": "GET",
                "path": "/v1/webhooks",
                "operation_id": "list_webhooks_v1_webhooks_get",
                "display_name": "List webhooks",
                "short_desc": "List Webhooks",
                "desc": "List Webhooks. Register webhooks that receive events from third-party providers.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Webhooks",),
                "response_fields": {
                    "url_verification_token": <RestSchemaFieldOverlayInfo>{
                        "sensitive": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-webhook",
                "method": "POST",
                "path": "/v1/webhooks",
                "operation_id": "create_webhook_v1_webhooks_post",
                "display_name": "Create webhook",
                "short_desc": "Create Webhook",
                "desc": "Create Webhook. Register webhooks that receive events from third-party providers.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Webhooks",),
                "fields": {
                    "provider": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Provider",
                        "short_desc": "Provider",
                        "desc": "The `provider` value for this operation.",
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "slack",
                                "display_name": "Slack",
                            },
                            <AllowedValueInfo>{
                                "value": "github",
                                "display_name": "GitHub",
                            },
                            <AllowedValueInfo>{
                                "value": "notion",
                                "display_name": "Notion",
                            },
                            <AllowedValueInfo>{
                                "value": "lucca",
                                "display_name": "Lucca",
                            },
                            <AllowedValueInfo>{
                                "value": "linear",
                                "display_name": "Linear",
                            },
                            <AllowedValueInfo>{
                                "value": "microsoft",
                                "display_name": "Microsoft",
                            },
                            <AllowedValueInfo>{
                                "value": "ashby",
                                "display_name": "Ashby",
                            },
                            <AllowedValueInfo>{
                                "value": "whatsapp",
                                "display_name": "WhatsApp",
                            },
                            <AllowedValueInfo>{
                                "value": "custom",
                                "display_name": "Custom",
                            },
                        ),
                    },
                    "owner_type": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Owner Type",
                        "short_desc": "Owner Type",
                        "desc": "The `owner_type` value for this operation.",
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "workspace",
                                "display_name": "Workspace",
                            },
                            <AllowedValueInfo>{
                                "value": "user",
                                "display_name": "User",
                            },
                        ),
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "Human-readable name for this webhook",
                    },
                    "secret": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Secret",
                        "short_desc": "Signing secret provided by the third-party provider",
                        "sensitive": True,
                    },
                    "encryption_key": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Encryption Key",
                        "short_desc": "Encryption key used to decrypt webhook payloads",
                        "sensitive": True,
                    },
                    "url_verification_token": <RestSchemaFieldOverlayInfo>{
                        "display_name": "URL Verification Token",
                        "short_desc": "Token the provider echoes back when verifying the URL. Required for WhatsApp (paste the same value in the Verify token field of the Meta App Dashboard), forbidden for other providers",
                        "sensitive": True,
                    },
                    "expires_at": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Expires At",
                        "short_desc": "Expires At",
                        "desc": "The `expires_at` value for this operation.",
                    },
                    "signature_hash_algorithm": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Signature Hash Algorithm",
                        "short_desc": "Signature Hash Algorithm",
                        "desc": "The `signature_hash_algorithm` value for this operation.",
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "sha256",
                                "display_name": "SHA-256",
                            },
                            <AllowedValueInfo>{
                                "value": "sha1",
                                "display_name": "SHA-1",
                            },
                        ),
                    },
                    "signature_header_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Signature Header Name",
                        "short_desc": "Signature Header Name",
                        "desc": "The `signature_header_name` value for this operation.",
                    },
                    "validity_period_second": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Validity Period Second",
                        "short_desc": "Maximum allowed drift in seconds between the signed timestamp and reception time, eg 300",
                    },
                    "signature_extractor_regex": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Signature Extractor Regex",
                        "short_desc": "Regex to extract signature, eg sha256=(.*)",
                    },
                    "timestamp_extractor_regex": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Timestamp Extractor Regex",
                        "short_desc": "Regex to extract the timestamp from the signature header, used as {timestamp}, eg t=([^,]+)",
                    },
                    "signature_payload_pattern": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Signature Payload Pattern",
                        "short_desc": "Pattern for signature payload, eg {body} or {timestamp}.{body}",
                    },
                    "signature_encoding": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Signature Encoding",
                        "short_desc": "Signature Encoding",
                        "desc": "The `signature_encoding` value for this operation.",
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "hex",
                                "display_name": "Hexadecimal",
                            },
                            <AllowedValueInfo>{
                                "value": "base64",
                                "display_name": "Base64",
                            },
                        ),
                    },
                    "success_status_code": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Success Status Code",
                        "short_desc": "Success Status Code",
                        "desc": "The `success_status_code` value for this operation.",
                    },
                    "event_type_pattern": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Event Type Pattern",
                        "short_desc": "Pattern for the event type, with {body.<path>} and {headers.<Header-Name>} placeholders, eg {body.type} or {headers.X-GitHub-Event}",
                    },
                    "event_id_pattern": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Event ID Pattern",
                        "short_desc": "Pattern for a unique event identifier used to deduplicate deliveries, with {body.<path>} and {headers.<Header-Name>} placeholders, eg {body.id} or {body.event_id}.{headers.X-Delivery-Id}. Defaults to a hash of the request body",
                    },
                },
                "response_fields": {
                    "url_verification_token": <RestSchemaFieldOverlayInfo>{
                        "sensitive": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-webhook",
                "method": "GET",
                "path": "/v1/webhooks/{webhook_id}",
                "operation_id": "get_webhook_v1_webhooks__webhook_id__get",
                "display_name": "Get Webhook",
                "short_desc": "Get Webhook",
                "desc": "Get Webhook. Register webhooks that receive events from third-party providers.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Webhooks",),
                "fields": {
                    "webhook_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Webhook ID",
                        "short_desc": "Webhook ID",
                        "desc": "The `webhook_id` value for this operation.",
                        "ref_data": "webhooks",
                        "supports_custom_values": True,
                    },
                },
                "response_fields": {
                    "url_verification_token": <RestSchemaFieldOverlayInfo>{
                        "sensitive": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-webhook",
                "method": "DELETE",
                "path": "/v1/webhooks/{webhook_id}",
                "operation_id": "delete_webhook_v1_webhooks__webhook_id__delete",
                "display_name": "Delete Webhook",
                "short_desc": "Delete Webhook",
                "desc": "Delete Webhook. Register webhooks that receive events from third-party providers.\n\n**Public preview:** Availability depends on your account and permissions.\n\n**Deletion:** This operation can permanently remove the selected resource or access.",
                "groups": ("Webhooks",),
                "fields": {
                    "webhook_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Webhook ID",
                        "short_desc": "Webhook ID",
                        "desc": "The `webhook_id` value for this operation.",
                        "ref_data": "webhooks",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-webhook-signing-secret",
                "method": "GET",
                "path": "/v1/webhooks/{webhook_id}/signing-secret",
                "operation_id": "get_webhook_signing_secret_v1_webhooks__webhook_id__signing_secret_get",
                "display_name": "Get Webhook Signing Secret",
                "short_desc": "Get Webhook Signing Secret",
                "desc": "Get Webhook Signing Secret. Register webhooks that receive events from third-party providers.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Webhooks",),
                "fields": {
                    "webhook_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Webhook ID",
                        "short_desc": "Webhook ID",
                        "desc": "The `webhook_id` value for this operation.",
                        "ref_data": "webhooks",
                        "supports_custom_values": True,
                    },
                },
                "response_fields": {
                    "signing_secret": <RestSchemaFieldOverlayInfo>{
                        "sensitive": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-identity",
                "method": "GET",
                "path": "/v1/users/me",
                "operation_id": "users_api_get_identity",
                "display_name": "Get identity",
                "short_desc": "Get Identity",
                "desc": "Get Identity. Read the identity, organizations and workspaces of the authenticated caller.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Identity",),
            },
            <RestSchemaActionInfo>{
                "action": "list-my-organizations",
                "method": "GET",
                "path": "/v1/users/me/organizations",
                "operation_id": "users_api_list_organizations",
                "display_name": "List my organizations",
                "short_desc": "List My Organizations",
                "desc": "List every organization the authenticated user is a member of. Read the identity, organizations and workspaces of the authenticated caller.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Identity",),
                "fields": {
                    "offset": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Offset",
                        "short_desc": "Number of organizations to skip before returning results.",
                    },
                    "limit": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Limit",
                        "short_desc": "Maximum number of organizations to return.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-my-workspaces",
                "method": "GET",
                "path": "/v1/users/me/workspaces",
                "operation_id": "users_api_list_workspaces",
                "display_name": "List my workspaces",
                "short_desc": "List My Workspaces",
                "desc": "List every workspace the authenticated user is a member of, across all their organizations, each tagged with the organization it belongs to. Read the identity, organizations and workspaces of the authenticated caller.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Identity",),
                "fields": {
                    "organization_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Organization ID",
                        "short_desc": "Return only workspaces belonging to this organization.",
                        "ref_data": "my-organizations",
                        "supports_custom_values": True,
                    },
                    "offset": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Offset",
                        "short_desc": "Number of workspaces to skip before returning results.",
                    },
                    "limit": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Limit",
                        "short_desc": "Maximum number of workspaces to return.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "whoami",
                "method": "GET",
                "path": "/v1/whoami",
                "operation_id": "users_api_whoami",
                "display_name": "Who am I",
                "short_desc": "Who Am I",
                "desc": "Return the authenticated caller's identity. Read the identity, organizations and workspaces of the authenticated caller.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Identity",),
            },
            <RestSchemaActionInfo>{
                "action": "get-billing-budget",
                "method": "GET",
                "path": "/v1/billing/budget",
                "operation_id": "billing_api_public_get_budget",
                "display_name": "Get billing budget",
                "short_desc": "Get Billing Budget",
                "desc": "Return the organization's API and Vibe budgets for the current billing period. Read the plan and budgets of the organization.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Billing",),
            },
            <RestSchemaActionInfo>{
                "action": "get-billing-subscription",
                "method": "GET",
                "path": "/v1/billing/subscription",
                "operation_id": "billing_api_public_get_subscription",
                "display_name": "Get billing subscription",
                "short_desc": "Get Billing Subscription",
                "desc": "Return the organization's plan and whether pay-as-you-go is enabled. Read the plan and budgets of the organization.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Billing",),
            },
            <RestSchemaActionInfo>{
                "action": "admin-query-usage",
                "method": "POST",
                "mutates_state": False,
                "path": "/v1/admin/usage/query",
                "operation_id": "users_api_admin_usage_query_usage",
                "display_name": "Query detailed usage (admin)",
                "short_desc": "Query Detailed Usage (Admin)",
                "desc": "Query the organization's detailed usage for a required time range. Read usage and manage the spend and rate limits of the organization. Requires an admin API key.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Admin Billing",),
                "fields": {
                    "usage_type": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Usage Type",
                        "short_desc": "Usage event types to include.",
                    },
                    "billing_metric": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Billing Metric",
                        "short_desc": "Billing metric identifiers to include.",
                    },
                    "billing_display_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Billing Display Name",
                        "short_desc": "Customer-facing billing metric names to include.",
                    },
                    "billing_group": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Billing Group",
                        "short_desc": "Billing metric components to include.",
                    },
                    "workspace_ids": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Workspace IDs",
                        "short_desc": "Workspace IDs to include. Use null to include usage without a workspace.",
                        "element_ref_data": "admin-workspaces",
                        "supports_custom_values": True,
                    },
                    "organization_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Organization ID",
                        "short_desc": "Organization IDs to include within the authenticated customer.",
                        "element_ref_data": "my-organizations",
                        "supports_custom_values": True,
                    },
                    "user_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "User ID",
                        "short_desc": "User IDs to include.",
                        "element_ref_data": "admin-users",
                        "supports_custom_values": True,
                    },
                    "service_account_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Service Account ID",
                        "short_desc": "Service account IDs to include.",
                        "element_ref_data": "service-accounts",
                        "supports_custom_values": True,
                    },
                    "api_key_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "API Key ID",
                        "short_desc": "API key IDs to include.",
                        "element_ref_data": "admin-api-keys",
                        "supports_custom_values": True,
                    },
                    "api_zone": <RestSchemaFieldOverlayInfo>{
                        "display_name": "API Zone",
                        "short_desc": "Inference regions to include.",
                    },
                    "service_tier": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Service Tier",
                        "short_desc": "Service tiers to include.",
                    },
                    "start": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Start",
                        "short_desc": "Inclusive start of the usage interval.",
                    },
                    "end": <RestSchemaFieldOverlayInfo>{
                        "display_name": "End",
                        "short_desc": "Exclusive end of the usage interval.",
                    },
                    "filters": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Filters",
                        "short_desc": "Additional inclusion or exclusion filters applied alongside the dimension fields.",
                    },
                    "group_by": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Group By",
                        "short_desc": "Dimensions used to group the returned usage rows.",
                    },
                    "time_bucket": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Time Bucket",
                        "short_desc": "Time interval used to bucket grouped rows.",
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "second",
                                "display_name": "Second",
                            },
                            <AllowedValueInfo>{
                                "value": "1min",
                                "display_name": "1 Minute",
                            },
                            <AllowedValueInfo>{
                                "value": "5min",
                                "display_name": "5 Minutes",
                            },
                            <AllowedValueInfo>{
                                "value": "15min",
                                "display_name": "15 Minutes",
                            },
                            <AllowedValueInfo>{
                                "value": "30min",
                                "display_name": "30 Minutes",
                            },
                            <AllowedValueInfo>{
                                "value": "hour",
                                "display_name": "Hour",
                            },
                            <AllowedValueInfo>{
                                "value": "4hours",
                                "display_name": "4 Hours",
                            },
                            <AllowedValueInfo>{
                                "value": "day",
                                "display_name": "Day",
                            },
                            <AllowedValueInfo>{
                                "value": "week",
                                "display_name": "Week",
                            },
                            <AllowedValueInfo>{
                                "value": "month",
                                "display_name": "Month",
                            },
                        ),
                    },
                    "untracked": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Untracked",
                        "short_desc": "Filter by attribution status. Omit or use null to include both tracked and untracked usage.",
                    },
                    "limit": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Limit",
                        "short_desc": "Maximum number of grouped rows to return.",
                    },
                    "offset": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Offset",
                        "short_desc": "Number of grouped rows to skip.",
                    },
                    "include_aggregates": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Include Aggregates",
                        "short_desc": "Return aggregates computed over the complete filtered result set.",
                    },
                    "include_nb_api_calls": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Include Nb API Calls",
                        "short_desc": "Include the number of distinct API calls in grouped rows and aggregates.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "admin-list-users",
                "method": "GET",
                "path": "/v1/admin/users",
                "operation_id": "users_api_admin_users_get_users",
                "display_name": "List users (admin)",
                "short_desc": "List Users (Admin)",
                "desc": "List Organization members and pending invitations. Manage organization members, invitations and roles. Requires an admin API key.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Admin Users",),
                "fields": {
                    "page": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page",
                        "short_desc": "Page number to return.",
                    },
                    "page_size": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "short_desc": "Maximum number of results per page.",
                    },
                    "email": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Email",
                        "short_desc": "Email address to filter users and invitations.",
                    },
                    "include_inactive": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Include Inactive",
                        "short_desc": "Also return deactivated and blocked members.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "admin-create-users",
                "method": "POST",
                "path": "/v1/admin/users",
                "operation_id": "users_api_admin_users_create_users",
                "display_name": "Create users (admin)",
                "short_desc": "Create Users (Admin)",
                "desc": "Create Organization members. Manage organization members, invitations and roles. Requires an admin API key.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Admin Users",),
                "fields": {
                    "body": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Body",
                        "short_desc": "The organization members to create.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "admin-invite-users",
                "method": "POST",
                "path": "/v1/admin/users-invite",
                "operation_id": "users_api_admin_users_invite_users",
                "display_name": "Invite users (admin)",
                "short_desc": "Invite Users (Admin)",
                "desc": "Invite users to the Organization. Manage organization members, invitations and roles. Requires an admin API key.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Admin Users",),
                "fields": {
                    "role_names": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Role Names",
                        "short_desc": "Simplified role names to assign. Mutually exclusive with 'roles'.",
                    },
                    "roles": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Roles",
                        "short_desc": "Role values to assign. Mutually exclusive with 'role_names'.",
                    },
                    "role_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Role Name",
                        "short_desc": "Deprecated single role name, kept for backward compatibility. Mutually exclusive with 'role'.",
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "member",
                                "display_name": "Member",
                            },
                            <AllowedValueInfo>{
                                "value": "billing_manager",
                                "display_name": "Billing Manager",
                            },
                            <AllowedValueInfo>{
                                "value": "organization_admin",
                                "display_name": "Organization Admin",
                            },
                        ),
                    },
                    "role": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Role",
                        "short_desc": "Deprecated legacy single role value, kept for backward compatibility. Mutually exclusive with 'role_name'.",
                    },
                    "email": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Email",
                        "short_desc": "Email address, comma-separated emails, or newline-separated emails to invite.",
                    },
                    "subscription_type": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Subscription Type",
                        "short_desc": "Deprecated single product seat to assign.",
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "API",
                                "display_name": "API",
                            },
                            <AllowedValueInfo>{
                                "value": "CHAT",
                                "display_name": "Chat",
                            },
                            <AllowedValueInfo>{
                                "value": "ON_PREMISE",
                                "display_name": "On-Premises",
                            },
                            <AllowedValueInfo>{
                                "value": "LICENSE",
                                "display_name": "License",
                            },
                            <AllowedValueInfo>{
                                "value": "MISTRAL_CODE",
                                "display_name": "Mistral Code",
                            },
                        ),
                    },
                    "subscription_types": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Subscription Types",
                        "short_desc": "Product seats to assign to invited users.",
                    },
                    "subscription_seat_automatic_granting": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Subscription Seat Automatic Granting",
                        "short_desc": "Whether to grant subscription seats automatically.",
                    },
                    "email_language": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Email Language",
                        "short_desc": "Language used for invitation emails.",
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "en",
                                "display_name": "English",
                            },
                            <AllowedValueInfo>{
                                "value": "fr",
                                "display_name": "French",
                            },
                            <AllowedValueInfo>{
                                "value": "es",
                                "display_name": "Spanish",
                            },
                            <AllowedValueInfo>{
                                "value": "de",
                                "display_name": "German",
                            },
                            <AllowedValueInfo>{
                                "value": "it",
                                "display_name": "Italian",
                            },
                            <AllowedValueInfo>{
                                "value": "pt_br",
                                "display_name": "Brazilian Portuguese",
                            },
                            <AllowedValueInfo>{
                                "value": "pl",
                                "display_name": "Polish",
                            },
                            <AllowedValueInfo>{
                                "value": "ar",
                                "display_name": "Arabic",
                            },
                            <AllowedValueInfo>{
                                "value": "nl",
                                "display_name": "Dutch",
                            },
                            <AllowedValueInfo>{
                                "value": "ko",
                                "display_name": "Korean",
                            },
                        ),
                    },
                    "workspace_uuids": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Workspace UUIDs",
                        "short_desc": "Workspace IDs the invited users should join.",
                        "element_ref_data": "admin-workspaces",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "admin-list-invitations",
                "method": "GET",
                "path": "/v1/admin/users-invite",
                "operation_id": "users_api_admin_users_get_invite",
                "display_name": "List invitations (admin)",
                "short_desc": "List Invitations (Admin)",
                "desc": "List pending Organization invitations. Manage organization members, invitations and roles. Requires an admin API key.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Admin Users",),
            },
            <RestSchemaActionInfo>{
                "action": "admin-delete-invitation",
                "method": "DELETE",
                "path": "/v1/admin/users-invite/{invite_uuid}",
                "operation_id": "users_api_admin_users_delete_invite",
                "display_name": "Delete Invitation (Admin)",
                "short_desc": "Delete Invitation (Admin)",
                "desc": "Delete Invite. Manage organization members, invitations and roles. Requires an admin API key.\n\n**Public preview:** Availability depends on your account and permissions.\n\n**Deletion:** This operation can permanently remove the selected resource or access.",
                "groups": ("Admin Users",),
                "fields": {
                    "invite_uuid": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Invite UUID",
                        "short_desc": "Organization invitation ID.",
                        "ref_data": "admin-invitations",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "admin-update-user",
                "method": "PATCH",
                "path": "/v1/admin/users/{user_id}",
                "operation_id": "users_api_admin_users_update_user",
                "display_name": "Update User (Admin)",
                "short_desc": "Update User (Admin)",
                "desc": "Update an Organization member's roles and product seats. Manage organization members, invitations and roles. Requires an admin API key.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Admin Users",),
                "fields": {
                    "user_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "User ID",
                        "short_desc": "User ID.",
                        "ref_data": "admin-users",
                        "supports_custom_values": True,
                    },
                    "role_names": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Role Names",
                        "short_desc": "Simplified role names to assign. Mutually exclusive with 'roles'.",
                    },
                    "roles": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Roles",
                        "short_desc": "Role values to assign. Mutually exclusive with 'role_names'.",
                    },
                    "role_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Role Name",
                        "short_desc": "Deprecated single role name, kept for backward compatibility. Mutually exclusive with 'role'.",
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "member",
                                "display_name": "Member",
                            },
                            <AllowedValueInfo>{
                                "value": "billing_manager",
                                "display_name": "Billing Manager",
                            },
                            <AllowedValueInfo>{
                                "value": "organization_admin",
                                "display_name": "Organization Admin",
                            },
                        ),
                    },
                    "role": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Role",
                        "short_desc": "Deprecated legacy single role value, kept for backward compatibility. Mutually exclusive with 'role_name'.",
                    },
                    "subscription_types": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Subscription Types",
                        "short_desc": "Product seats to assign to the member.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "admin-delete-user",
                "method": "DELETE",
                "path": "/v1/admin/users/{user_id}",
                "operation_id": "users_api_admin_users_delete_user",
                "display_name": "Delete User (Admin)",
                "short_desc": "Delete User (Admin)",
                "desc": "Remove a member from the Organization, whatever their account status. Manage organization members, invitations and roles. Requires an admin API key.\n\n**Public preview:** Availability depends on your account and permissions.\n\n**Deletion:** This operation can permanently remove the selected resource or access.",
                "groups": ("Admin Users",),
                "fields": {
                    "user_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "User ID",
                        "short_desc": "User ID.",
                        "ref_data": "admin-users",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "admin-get-user",
                "method": "GET",
                "path": "/v1/admin/users/{user_id}",
                "operation_id": "users_api_admin_users_get_user",
                "display_name": "Get User (Admin)",
                "short_desc": "Get User (Admin)",
                "desc": "Get details for an Organization member, including deactivated and blocked members. Manage organization members, invitations and roles. Requires an admin API key.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Admin Users",),
                "fields": {
                    "user_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "User ID",
                        "short_desc": "User ID.",
                        "ref_data": "admin-users",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "admin-deactivate-user",
                "method": "POST",
                "path": "/v1/admin/users/{user_id}/deactivate",
                "operation_id": "users_api_admin_users_deactivate_user",
                "display_name": "Deactivate User (Admin)",
                "short_desc": "Deactivate User (Admin)",
                "desc": "Deactivate a user whose email address is on one of the Organization's verified domains: end their sessions, delete their API keys and release their seats. Their memberships, groups and roles are kept, so that `POST /users/{user_id}/reactivate` can give their access back. Manage organization members, invitations and roles. Requires an admin API key.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Admin Users",),
                "fields": {
                    "user_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "User ID",
                        "short_desc": "User ID.",
                        "ref_data": "admin-users",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "admin-reactivate-user",
                "method": "POST",
                "path": "/v1/admin/users/{user_id}/reactivate",
                "operation_id": "users_api_admin_users_reactivate_user",
                "display_name": "Reactivate User (Admin)",
                "short_desc": "Reactivate User (Admin)",
                "desc": "Reactivate a deactivated user whose email address is on one of the Organization's verified domains: give them their access back, with their seats when free ones remain. Manage organization members, invitations and roles. Requires an admin API key.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Admin Users",),
                "fields": {
                    "user_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "User ID",
                        "short_desc": "User ID.",
                        "ref_data": "admin-users",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "admin-list-user-workspaces",
                "method": "GET",
                "path": "/v1/admin/users/{user_id}/workspaces",
                "operation_id": "users_api_admin_users_get_user_workspaces",
                "display_name": "List User Workspaces (Admin)",
                "short_desc": "List User Workspaces (Admin)",
                "desc": "List the Workspaces an Organization member belongs to, with their roles. Manage organization members, invitations and roles. Requires an admin API key.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Admin Users",),
                "fields": {
                    "user_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "User ID",
                        "short_desc": "User ID.",
                        "ref_data": "admin-users",
                        "supports_custom_values": True,
                    },
                    "page": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page",
                        "short_desc": "Page number to return.",
                    },
                    "page_size": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "short_desc": "Maximum number of results per page.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "admin-list-roles",
                "method": "GET",
                "path": "/v1/admin/roles",
                "operation_id": "users_api_admin_roles_get_roles",
                "display_name": "List roles (admin)",
                "short_desc": "List Roles (Admin)",
                "desc": "List Organization and Workspace roles. Manage organization members, invitations and roles. Requires an admin API key.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Admin Users",),
            },
            <RestSchemaActionInfo>{
                "action": "admin-list-workspaces",
                "method": "GET",
                "path": "/v1/admin/workspaces",
                "operation_id": "users_api_admin_workspaces_get_workspaces",
                "display_name": "List workspaces (admin)",
                "short_desc": "List Workspaces (Admin)",
                "desc": "List Workspaces in the Organization. Manage the workspaces of the organization and their members. Requires an admin API key.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Admin Workspaces",),
                "fields": {
                    "is_archived": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Is Archived",
                        "short_desc": "Whether to include archived Workspaces.",
                    },
                    "page": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page",
                        "short_desc": "Page number to return.",
                    },
                    "page_size": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "short_desc": "Maximum number of results per page.",
                    },
                    "search": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Search",
                        "short_desc": "Search term to filter Workspaces by name.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "admin-create-workspace",
                "method": "POST",
                "path": "/v1/admin/workspaces",
                "operation_id": "users_api_admin_workspaces_create_workspace",
                "display_name": "Create workspace (admin)",
                "short_desc": "Create Workspace (Admin)",
                "desc": "Create a Workspace. Manage the workspaces of the organization and their members. Requires an admin API key.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Admin Workspaces",),
                "fields": {
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "Workspace name.",
                    },
                    "description": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Description",
                        "short_desc": "Workspace description.",
                    },
                    "icon": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Icon",
                        "short_desc": "Workspace icon.",
                    },
                    "add_all_org_members": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Add All Org Members",
                        "short_desc": "Whether to add all Organization members to the Workspace.",
                    },
                    "admin_user_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Admin User ID",
                        "short_desc": "User ID to grant the Workspace Admin role to.",
                        "ref_data": "admin-users",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "admin-update-workspace",
                "method": "PATCH",
                "path": "/v1/admin/workspaces/{workspace_uuid}",
                "operation_id": "users_api_admin_workspaces_update_workspaces",
                "display_name": "Update Workspace (Admin)",
                "short_desc": "Update Workspace (Admin)",
                "desc": "Update a Workspace. Manage the workspaces of the organization and their members. Requires an admin API key.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Admin Workspaces",),
                "fields": {
                    "workspace_uuid": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Workspace UUID",
                        "short_desc": "Workspace ID.",
                        "ref_data": "admin-workspaces",
                        "supports_custom_values": True,
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "Updated Workspace name.",
                    },
                    "description": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Description",
                        "short_desc": "Updated Workspace description.",
                    },
                    "icon": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Icon",
                        "short_desc": "Updated Workspace icon.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "admin-archive-workspace",
                "method": "DELETE",
                "path": "/v1/admin/workspaces/{workspace_uuid}",
                "operation_id": "users_api_admin_workspaces_delete_workspaces",
                "display_name": "Archive Workspace (Admin)",
                "short_desc": "Archive Workspace (Admin)",
                "desc": "Archive a Workspace. Manage the workspaces of the organization and their members. Requires an admin API key.\n\n**Public preview:** Availability depends on your account and permissions.\n\n**Deletion:** This operation can permanently remove the selected resource or access.",
                "groups": ("Admin Workspaces",),
                "fields": {
                    "workspace_uuid": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Workspace UUID",
                        "short_desc": "Workspace ID.",
                        "ref_data": "admin-workspaces",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "admin-add-workspace-members",
                "method": "POST",
                "path": "/v1/admin/workspaces/{workspace_uuid}/add-users",
                "operation_id": "users_api_admin_workspaces_add_users_workspaces",
                "display_name": "Add Workspace Members (Admin)",
                "short_desc": "Add Workspace Members (Admin)",
                "desc": "Add members to a Workspace. Manage the workspaces of the organization and their members. Requires an admin API key.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Admin Workspaces",),
                "fields": {
                    "workspace_uuid": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Workspace UUID",
                        "short_desc": "Workspace ID.",
                        "ref_data": "admin-workspaces",
                        "supports_custom_values": True,
                    },
                    "members": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Members",
                        "short_desc": "Workspace members to add or update.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "admin-add-or-update-workspace-members",
                "method": "PATCH",
                "path": "/v1/admin/workspaces/{workspace_uuid}/users",
                "operation_id": "users_api_admin_workspaces_add_or_update_users_workspaces",
                "display_name": "Add Or Update Workspace Members (Admin)",
                "short_desc": "Add Or Update Workspace Members (Admin)",
                "desc": "Add or update Workspace members. Manage the workspaces of the organization and their members. Requires an admin API key.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Admin Workspaces",),
                "fields": {
                    "workspace_uuid": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Workspace UUID",
                        "short_desc": "Workspace ID.",
                        "ref_data": "admin-workspaces",
                        "supports_custom_values": True,
                    },
                    "members": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Members",
                        "short_desc": "Workspace members to add or update.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "admin-remove-workspace-members",
                "method": "DELETE",
                "path": "/v1/admin/workspaces/{workspace_uuid}/remove-users",
                "operation_id": "users_api_admin_workspaces_remove_users_workspaces",
                "display_name": "Remove Workspace Members (Admin)",
                "short_desc": "Remove Workspace Members (Admin)",
                "desc": "Remove members from a Workspace. Manage the workspaces of the organization and their members. Requires an admin API key.\n\n**Public preview:** Availability depends on your account and permissions.\n\n**Deletion:** This operation can permanently remove the selected resource or access.",
                "groups": ("Admin Workspaces",),
                "fields": {
                    "workspace_uuid": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Workspace UUID",
                        "short_desc": "Workspace ID.",
                        "ref_data": "admin-workspaces",
                        "supports_custom_values": True,
                    },
                    "members": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Members",
                        "short_desc": "Workspace members to remove.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "admin-get-rate-limits",
                "method": "GET",
                "path": "/v1/admin/rate-limit",
                "operation_id": "users_api_admin_rate_limits_get_rate_limits",
                "display_name": "Get rate limits (admin)",
                "short_desc": "Get Rate Limits (Admin)",
                "desc": "Get Rate Limits. Read usage and manage the spend and rate limits of the organization. Requires an admin API key.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Admin Billing",),
            },
            <RestSchemaActionInfo>{
                "action": "admin-get-spend-limits",
                "method": "GET",
                "path": "/v1/admin/spend-limit",
                "operation_id": "users_api_admin_spend_limits_get_spend_limits",
                "display_name": "Get spend limits (admin)",
                "short_desc": "Get Spend Limits (Admin)",
                "desc": "Get usage, rate, and job limits for the Organization. Read usage and manage the spend and rate limits of the organization. Requires an admin API key.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Admin Billing",),
            },
            <RestSchemaActionInfo>{
                "action": "admin-update-spend-limits",
                "method": "POST",
                "path": "/v1/admin/spend-limit",
                "operation_id": "users_api_admin_spend_limits_update_spend_limits",
                "display_name": "Update spend limits (admin)",
                "short_desc": "Update Spend Limits (Admin)",
                "desc": "Update the Organization usage limit. Read usage and manage the spend and rate limits of the organization. Requires an admin API key.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Admin Billing",),
                "fields": {
                    "amount": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Amount",
                        "short_desc": "New monthly usage limit amount.",
                    },
                    "no_monthly_limit": <RestSchemaFieldOverlayInfo>{
                        "display_name": "No Monthly Limit",
                        "short_desc": "Whether to remove the monthly usage limit.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "admin-get-monthly-usage",
                "method": "GET",
                "path": "/v1/admin/usage",
                "operation_id": "users_api_admin_usage_get_usage",
                "display_name": "Get monthly usage (admin)",
                "short_desc": "Get Monthly Usage (Admin)",
                "desc": "Return the organization's usage for a calendar month in a fixed, product-oriented format. Read usage and manage the spend and rate limits of the organization. Requires an admin API key.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Admin Billing",),
                "fields": {
                    "month": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Month",
                        "short_desc": "Month to return usage for.",
                    },
                    "year": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Year",
                        "short_desc": "Year to return usage for.",
                    },
                    "workspace_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Workspace ID",
                        "short_desc": "Workspace ID to filter results.",
                        "ref_data": "admin-workspaces",
                        "supports_custom_values": True,
                    },
                    "api_zone": <RestSchemaFieldOverlayInfo>{
                        "display_name": "API Zone",
                        "short_desc": "Regional inference zone to filter results.",
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "global",
                                "display_name": "Global",
                            },
                            <AllowedValueInfo>{
                                "value": "us",
                                "display_name": "United States",
                            },
                            <AllowedValueInfo>{
                                "value": "eu",
                                "display_name": "European Union",
                            },
                        ),
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "admin-list-audit-logs",
                "method": "GET",
                "path": "/v1/admin/audit-logs",
                "operation_id": "users_api_admin_audit_logs_get_audit_logs",
                "display_name": "List audit logs (admin)",
                "short_desc": "List Audit Logs (Admin)",
                "desc": "List audit log entries for the Organization. Read the audit log of the organization. Requires an admin API key.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Admin Audit Logs",),
                "fields": {
                    "actor_type": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Actor Type",
                        "short_desc": "Actor types to include in the audit log results.",
                    },
                    "event_type": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Event Type",
                        "short_desc": "Event types to include in the audit log results.",
                    },
                    "target_type": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Target Type",
                        "short_desc": "Target resource types to include in the audit log results.",
                    },
                    "actor_user_uuid": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Actor User UUID",
                        "short_desc": "Filter logs by the UUID of the user who performed the action.",
                        "ref_data": "admin-users",
                        "supports_custom_values": True,
                    },
                    "sort": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Sort",
                        "short_desc": "Sort order for audit log entries.",
                    },
                    "after": <RestSchemaFieldOverlayInfo>{
                        "display_name": "After",
                        "short_desc": "Return audit log entries after this time.",
                    },
                    "before": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Before",
                        "short_desc": "Return audit log entries before this time.",
                    },
                    "limit": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Limit",
                        "short_desc": "Maximum number of results to return.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "admin-list-user-groups",
                "method": "GET",
                "path": "/v1/admin/user-groups",
                "operation_id": "users_api_admin_user_groups_get_user_groups",
                "display_name": "List user groups (admin)",
                "short_desc": "List User Groups (Admin)",
                "desc": "Get all user groups across the organization. Manage user groups, their members and workspace assignments. Requires an admin API key.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Admin User Groups",),
                "fields": {
                    "page": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page",
                        "short_desc": "Page number to return.",
                    },
                    "page_size": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "short_desc": "Maximum number of results per page.",
                    },
                    "search": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Search",
                        "short_desc": "Search term used to filter user groups by name.",
                    },
                    "workspace_uuid": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Workspace UUID",
                        "short_desc": "Filter groups to those assigned to this Workspace. Assignments to archived Workspaces never match.",
                        "ref_data": "admin-workspaces",
                        "supports_custom_values": True,
                    },
                    "role": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Role",
                        "short_desc": "Filter groups to those holding this role on a Workspace. When combined with workspace_uuid, the role must be held on that Workspace. Assignments to archived Workspaces never match.",
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "billing",
                                "display_name": "Billing",
                            },
                            <AllowedValueInfo>{
                                "value": "user",
                                "display_name": "User",
                            },
                            <AllowedValueInfo>{
                                "value": "contributor",
                                "display_name": "Contributor",
                            },
                            <AllowedValueInfo>{
                                "value": "dev",
                                "display_name": "Dev",
                            },
                            <AllowedValueInfo>{
                                "value": "dev_contributor",
                                "display_name": "Dev Contributor",
                            },
                            <AllowedValueInfo>{
                                "value": "mistral_code_user",
                                "display_name": "Mistral Code User",
                            },
                            <AllowedValueInfo>{
                                "value": "vibe_code_cli_user",
                                "display_name": "Vibe Code CLI User",
                            },
                            <AllowedValueInfo>{
                                "value": "cloud_user",
                                "display_name": "Cloud User",
                            },
                            <AllowedValueInfo>{
                                "value": "workspace_contributor",
                                "display_name": "Workspace Contributor",
                            },
                            <AllowedValueInfo>{
                                "value": "workspace_admin",
                                "display_name": "Workspace Admin",
                            },
                            <AllowedValueInfo>{
                                "value": "observability_viewer",
                                "display_name": "Observability Viewer",
                            },
                            <AllowedValueInfo>{
                                "value": "workflow_executor",
                                "display_name": "Workflow Executor",
                            },
                            <AllowedValueInfo>{
                                "value": "member",
                                "display_name": "Member",
                            },
                        ),
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "admin-create-user-group",
                "method": "POST",
                "path": "/v1/admin/user-groups",
                "operation_id": "users_api_admin_user_groups_create_user_group",
                "display_name": "Create user group (admin)",
                "short_desc": "Create User Group (Admin)",
                "desc": "Create a new user group. Manage user groups, their members and workspace assignments. Requires an admin API key.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Admin User Groups",),
                "fields": {
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "Name of the user group.",
                    },
                    "description": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Description",
                        "short_desc": "Optional description of the user group.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "admin-get-user-group",
                "method": "GET",
                "path": "/v1/admin/user-groups/{group_uuid}",
                "operation_id": "users_api_admin_user_groups_get_user_group",
                "display_name": "Get User Group (Admin)",
                "short_desc": "Get User Group (Admin)",
                "desc": "Get a specific user group. Manage user groups, their members and workspace assignments. Requires an admin API key.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Admin User Groups",),
                "fields": {
                    "group_uuid": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Group UUID",
                        "short_desc": "User group ID.",
                        "ref_data": "admin-user-groups",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "admin-update-user-group",
                "method": "PATCH",
                "path": "/v1/admin/user-groups/{group_uuid}",
                "operation_id": "users_api_admin_user_groups_update_user_group",
                "display_name": "Update User Group (Admin)",
                "short_desc": "Update User Group (Admin)",
                "desc": "Update User Group. Manage user groups, their members and workspace assignments. Requires an admin API key.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Admin User Groups",),
                "fields": {
                    "group_uuid": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Group UUID",
                        "short_desc": "Group UUID",
                        "desc": "The `group_uuid` value for this operation.",
                        "ref_data": "admin-user-groups",
                        "supports_custom_values": True,
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "Updated name of the user group.",
                    },
                    "description": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Description",
                        "short_desc": "Updated description of the user group. Pass an empty string to clear it; omit the field or pass null to leave it unchanged.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "admin-delete-user-group",
                "method": "DELETE",
                "path": "/v1/admin/user-groups/{group_uuid}",
                "operation_id": "users_api_admin_user_groups_delete_user_group",
                "display_name": "Delete User Group (Admin)",
                "short_desc": "Delete User Group (Admin)",
                "desc": "Delete a user group. Manage user groups, their members and workspace assignments. Requires an admin API key.\n\n**Public preview:** Availability depends on your account and permissions.\n\n**Deletion:** This operation can permanently remove the selected resource or access.",
                "groups": ("Admin User Groups",),
                "fields": {
                    "group_uuid": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Group UUID",
                        "short_desc": "User group ID.",
                        "ref_data": "admin-user-groups",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "admin-list-user-group-members",
                "method": "GET",
                "path": "/v1/admin/user-groups/{group_uuid}/members",
                "operation_id": "users_api_admin_user_groups_get_user_group_members",
                "display_name": "List User Group Members (Admin)",
                "short_desc": "List User Group Members (Admin)",
                "desc": "Get members of a user group. Manage user groups, their members and workspace assignments. Requires an admin API key.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Admin User Groups",),
                "fields": {
                    "group_uuid": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Group UUID",
                        "short_desc": "User group ID.",
                        "ref_data": "admin-user-groups",
                        "supports_custom_values": True,
                    },
                    "page": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page",
                        "short_desc": "Page number to return.",
                    },
                    "page_size": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "short_desc": "Maximum number of results per page.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "admin-add-user-group-members",
                "method": "POST",
                "path": "/v1/admin/user-groups/{group_uuid}/members",
                "operation_id": "users_api_admin_user_groups_assign_users_to_group",
                "display_name": "Add User Group Members (Admin)",
                "short_desc": "Add User Group Members (Admin)",
                "desc": "Assign users to a user group. Manage user groups, their members and workspace assignments. Requires an admin API key.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Admin User Groups",),
                "fields": {
                    "group_uuid": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Group UUID",
                        "short_desc": "Group UUID",
                        "desc": "The `group_uuid` value for this operation.",
                        "ref_data": "admin-user-groups",
                        "supports_custom_values": True,
                    },
                    "user_uuids": <RestSchemaFieldOverlayInfo>{
                        "display_name": "User UUIDs",
                        "short_desc": "User IDs to add to the group.",
                        "element_ref_data": "admin-users",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "admin-remove-user-group-members",
                "method": "DELETE",
                "path": "/v1/admin/user-groups/{group_uuid}/members",
                "operation_id": "users_api_admin_user_groups_remove_users_from_group",
                "display_name": "Remove User Group Members (Admin)",
                "short_desc": "Remove User Group Members (Admin)",
                "desc": "Remove users from a user group. Manage user groups, their members and workspace assignments. Requires an admin API key.\n\n**Public preview:** Availability depends on your account and permissions.\n\n**Deletion:** This operation can permanently remove the selected resource or access.",
                "groups": ("Admin User Groups",),
                "fields": {
                    "group_uuid": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Group UUID",
                        "short_desc": "Group UUID",
                        "desc": "The `group_uuid` value for this operation.",
                        "ref_data": "admin-user-groups",
                        "supports_custom_values": True,
                    },
                    "user_uuids": <RestSchemaFieldOverlayInfo>{
                        "display_name": "User UUIDs",
                        "short_desc": "User IDs to add to the group.",
                        "element_ref_data": "admin-users",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "admin-list-user-group-workspaces",
                "method": "GET",
                "path": "/v1/admin/user-groups/{group_uuid}/workspaces",
                "operation_id": "users_admin_user_groups_get_group_workspace_assignments",
                "display_name": "List User Group Workspaces (Admin)",
                "short_desc": "List User Group Workspaces (Admin)",
                "desc": "List workspace assignments for a user group. Manage user groups, their members and workspace assignments. Requires an admin API key.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Admin User Groups",),
                "fields": {
                    "group_uuid": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Group UUID",
                        "short_desc": "User group ID.",
                        "ref_data": "admin-user-groups",
                        "supports_custom_values": True,
                    },
                    "page": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page",
                        "short_desc": "Page number to return.",
                    },
                    "page_size": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "short_desc": "Maximum number of results per page.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "admin-assign-user-group-to-workspace",
                "method": "POST",
                "path": "/v1/admin/user-groups/{group_uuid}/workspaces",
                "operation_id": "users_api_admin_user_groups_assign_group_to_workspace",
                "display_name": "Assign User Group To Workspace (Admin)",
                "short_desc": "Assign User Group To Workspace (Admin)",
                "desc": "Assign a user group to a workspace. Manage user groups, their members and workspace assignments. Requires an admin API key.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Admin User Groups",),
                "fields": {
                    "group_uuid": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Group UUID",
                        "short_desc": "User group ID.",
                        "ref_data": "admin-user-groups",
                        "supports_custom_values": True,
                    },
                    "role_names": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Role Names",
                        "short_desc": "Simplified role names to assign. Mutually exclusive with 'roles'.",
                    },
                    "roles": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Roles",
                        "short_desc": "Role values to assign. Mutually exclusive with 'role_names'.",
                    },
                    "role": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Role",
                        "short_desc": "Deprecated single role value. Use 'role_names' instead.",
                    },
                    "workspace_uuid": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Workspace UUID",
                        "short_desc": "Workspace UUID",
                        "desc": "The `workspace_uuid` value for this operation.",
                        "ref_data": "admin-workspaces",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "admin-update-user-group-workspace-role",
                "method": "PATCH",
                "path": "/v1/admin/user-groups/{group_uuid}/workspaces/{workspace_uuid}",
                "operation_id": "users_admin_user_groups_update_group_workspace_assignment",
                "display_name": "Update User Group Workspace Role (Admin)",
                "short_desc": "Update User Group Workspace Role (Admin)",
                "desc": "Update the workspace role assignment for a user group. Manage user groups, their members and workspace assignments. Requires an admin API key.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Admin User Groups",),
                "fields": {
                    "group_uuid": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Group UUID",
                        "short_desc": "User group ID.",
                        "ref_data": "admin-user-groups",
                        "supports_custom_values": True,
                    },
                    "workspace_uuid": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Workspace UUID",
                        "short_desc": "Workspace ID.",
                        "ref_data": "admin-workspaces",
                        "supports_custom_values": True,
                    },
                    "role_names": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Role Names",
                        "short_desc": "Simplified role names to assign. Mutually exclusive with 'roles'.",
                    },
                    "roles": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Roles",
                        "short_desc": "Role values to assign. Mutually exclusive with 'role_names'.",
                    },
                    "role": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Role",
                        "short_desc": "Deprecated single role value. Use 'role_names' instead.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "admin-remove-user-group-from-workspace",
                "method": "DELETE",
                "path": "/v1/admin/user-groups/{group_uuid}/workspaces/{workspace_uuid}",
                "operation_id": "users_admin_user_groups_remove_group_from_workspace",
                "display_name": "Remove User Group From Workspace (Admin)",
                "short_desc": "Remove User Group From Workspace (Admin)",
                "desc": "Remove a user group from a workspace. Manage user groups, their members and workspace assignments. Requires an admin API key.\n\n**Public preview:** Availability depends on your account and permissions.\n\n**Deletion:** This operation can permanently remove the selected resource or access.",
                "groups": ("Admin User Groups",),
                "fields": {
                    "group_uuid": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Group UUID",
                        "short_desc": "User group ID.",
                        "ref_data": "admin-user-groups",
                        "supports_custom_values": True,
                    },
                    "workspace_uuid": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Workspace UUID",
                        "short_desc": "Workspace ID.",
                        "ref_data": "admin-workspaces",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "admin-update-user-group-organization-role",
                "method": "PATCH",
                "path": "/v1/admin/user-groups/{group_uuid}/organization-role",
                "operation_id": "users_admin_user_groups_update_user_group_organization_role",
                "display_name": "Update User Group Organization Role (Admin)",
                "short_desc": "Update User Group Organization Role (Admin)",
                "desc": "Update the organization role for a user group. Manage user groups, their members and workspace assignments. Requires an admin API key.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Admin User Groups",),
                "fields": {
                    "group_uuid": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Group UUID",
                        "short_desc": "User group ID.",
                        "ref_data": "admin-user-groups",
                        "supports_custom_values": True,
                    },
                    "organization_role": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Organization Role",
                        "short_desc": "Organization role to assign to the group: a role name (`member`, `billing_manager`, `organization_admin`) or a role UUID.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "admin-list-api-keys",
                "method": "GET",
                "path": "/v1/admin/api-keys",
                "operation_id": "users_api_admin_api_keys_get_api_keys",
                "display_name": "List API keys (admin)",
                "short_desc": "List API Keys (Admin)",
                "desc": "List API keys for the Organization. Manage the API keys of the organization. Requires an admin API key.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Admin API Keys",),
                "fields": {
                    "limit": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Limit",
                        "short_desc": "Maximum number of results to return.",
                    },
                    "offset": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Offset",
                        "short_desc": "Number of results to skip before returning results.",
                    },
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "Filter API keys by name substring or the last 4 characters of the key. Matching is case-insensitive and accent-insensitive.",
                    },
                },
                "response_fields": {
                    "keys": <RestSchemaFieldOverlayInfo>{
                        "fields": {
                            "key": <RestSchemaFieldOverlayInfo>{
                                "sensitive": True,
                            },
                        },
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "admin-create-api-key",
                "method": "POST",
                "path": "/v1/admin/api-keys",
                "operation_id": "users_api_admin_api_keys_create_api_key",
                "display_name": "Create API key (admin)",
                "short_desc": "Create API Key (Admin)",
                "desc": "Create a Workspace API key. Manage the API keys of the organization. Requires an admin API key.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Admin API Keys",),
                "fields": {
                    "name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Name",
                        "short_desc": "Optional name for the API key.",
                    },
                    "expiration": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Expiration",
                        "short_desc": "Date when the API key should expire.",
                    },
                    "workspace_uuid": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Workspace UUID",
                        "short_desc": "Workspace ID for the API key.",
                        "ref_data": "admin-workspaces",
                        "supports_custom_values": True,
                    },
                    "user_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "User ID",
                        "short_desc": "ID of the user the API key is created for.",
                        "ref_data": "admin-users",
                        "supports_custom_values": True,
                    },
                },
                "response_fields": {
                    "key": <RestSchemaFieldOverlayInfo>{
                        "sensitive": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "admin-delete-api-key",
                "method": "DELETE",
                "path": "/v1/admin/api-keys/{key_id}",
                "operation_id": "users_api_admin_api_keys_delete_api_key",
                "display_name": "Delete API Key (Admin)",
                "short_desc": "Delete API Key (Admin)",
                "desc": "Delete an API key. Manage the API keys of the organization. Requires an admin API key.\n\n**Public preview:** Availability depends on your account and permissions.\n\n**Deletion:** This operation can permanently remove the selected resource or access.",
                "groups": ("Admin API Keys",),
                "fields": {
                    "key_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Key ID",
                        "short_desc": "API key ID.",
                        "ref_data": "admin-api-keys",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "admin-start-scim-sync",
                "method": "POST",
                "path": "/v1/admin/scim/sync",
                "operation_id": "users_api_admin_scim_sync_trigger_scim_sync",
                "display_name": "Start SCIM synchronization (admin)",
                "short_desc": "Start SCIM Synchronization (Admin)",
                "desc": "Trigger an on-demand SCIM synchronization for the Organization. Start and inspect SCIM synchronizations. Requires an admin API key.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Admin SCIM",),
                "fields": {
                    "dry_run": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Dry Run",
                        "short_desc": "Preview all synchronization changes without applying them.",
                    },
                    "sync_config": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Sync Config",
                        "short_desc": "Categories to synchronize. Required when dry_run is false; ignored for dry runs.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "admin-get-scim-sync-run",
                "method": "GET",
                "path": "/v1/admin/scim/sync/{run_id}",
                "operation_id": "users_api_admin_scim_sync_get_scim_sync_run",
                "display_name": "Get SCIM Synchronization Run (Admin)",
                "short_desc": "Get SCIM Synchronization Run (Admin)",
                "desc": "Retrieve an on-demand SCIM synchronization run for the Organization. Start and inspect SCIM synchronizations. Requires an admin API key.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Admin SCIM",),
                "fields": {
                    "run_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Run ID",
                        "short_desc": "Identifier of the SCIM synchronization run.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "admin-get-vibe-work-usage-by-user",
                "method": "GET",
                "path": "/v1/admin/analytics/vibe/work/usage/by_user_stats",
                "operation_id": "get_by_user_stats_v1_admin_analytics_vibe_work_usage_by_user_stats",
                "display_name": "Get Vibe Work usage by user (admin)",
                "short_desc": "Get Vibe Work Usage By User (Admin)",
                "desc": "Get Vibe Work usage by user for a time range. Read Vibe Work and Vibe Code usage analytics. Requires an admin API key.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Admin Analytics",),
                "fields": {
                    "start_time": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Start Time",
                        "short_desc": "Start of the queried window, as a Unix timestamp in seconds.",
                    },
                    "end_time": <RestSchemaFieldOverlayInfo>{
                        "display_name": "End Time",
                        "short_desc": "End of the queried window, as a Unix timestamp in seconds.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "admin-get-vibe-work-usage-by-agent",
                "method": "GET",
                "path": "/v1/admin/analytics/vibe/work/usage/by_agent_stats",
                "operation_id": "get_by_agent_stats_v1_admin_analytics_vibe_work_usage_by_agent_stats",
                "display_name": "Get Vibe Work usage by agent (admin)",
                "short_desc": "Get Vibe Work Usage By Agent (Admin)",
                "desc": "Get Vibe Work usage by agent for a time range. Read Vibe Work and Vibe Code usage analytics. Requires an admin API key.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Admin Analytics",),
                "fields": {
                    "start_time": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Start Time",
                        "short_desc": "Start of the queried window, as a Unix timestamp in seconds.",
                    },
                    "end_time": <RestSchemaFieldOverlayInfo>{
                        "display_name": "End Time",
                        "short_desc": "End of the queried window, as a Unix timestamp in seconds.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "admin-get-vibe-work-usage-over-time",
                "method": "GET",
                "path": "/v1/admin/analytics/vibe/work/usage/by_time_stats",
                "operation_id": "get_by_time_stats_v1_admin_analytics_vibe_work_usage_by_time_stats",
                "display_name": "Get Vibe Work usage over time (admin)",
                "short_desc": "Get Vibe Work Usage Over Time (Admin)",
                "desc": "Get Vibe Work usage over time. Read Vibe Work and Vibe Code usage analytics. Requires an admin API key.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Admin Analytics",),
                "fields": {
                    "start_time": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Start Time",
                        "short_desc": "Start of the queried window, as a Unix timestamp in seconds.",
                    },
                    "end_time": <RestSchemaFieldOverlayInfo>{
                        "display_name": "End Time",
                        "short_desc": "End of the queried window, as a Unix timestamp in seconds.",
                    },
                    "granularity": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Granularity",
                        "short_desc": "Time interval used to group usage results.",
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "hour",
                                "display_name": "Hour",
                            },
                            <AllowedValueInfo>{
                                "value": "day",
                                "display_name": "Day",
                            },
                            <AllowedValueInfo>{
                                "value": "week",
                                "display_name": "Week",
                            },
                            <AllowedValueInfo>{
                                "value": "month",
                                "display_name": "Month",
                            },
                        ),
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "admin-get-vibe-code-usage-by-workspace",
                "method": "GET",
                "path": "/v1/admin/analytics/vibe/code/usage/by_workspace",
                "operation_id": "get_workspace_stats_v1_admin_analytics_vibe_code_usage_by_workspace",
                "display_name": "Get Vibe Code usage by workspace (admin)",
                "short_desc": "Get Vibe Code Usage By Workspace (Admin)",
                "desc": "Get Vibe Code usage for a Workspace. Read Vibe Work and Vibe Code usage analytics. Requires an admin API key.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Admin Analytics",),
                "fields": {
                    "start_time": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Start Time",
                        "short_desc": "Start of the queried window, as a Unix timestamp in seconds.",
                    },
                    "end_time": <RestSchemaFieldOverlayInfo>{
                        "display_name": "End Time",
                        "short_desc": "End of the queried window, as a Unix timestamp in seconds.",
                    },
                    "workspace_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Workspace ID",
                        "short_desc": "Workspace ID to filter results.",
                        "ref_data": "admin-workspaces",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "admin-get-vibe-code-usage-by-organization",
                "method": "GET",
                "path": "/v1/admin/analytics/vibe/code/usage/by_organization",
                "operation_id": "get_organization_stats_v1_admin_analytics_vibe_code_usage_by_organization",
                "display_name": "Get Vibe Code usage by organization (admin)",
                "short_desc": "Get Vibe Code Usage By Organization (Admin)",
                "desc": "Get Vibe Code usage for the Organization. Read Vibe Work and Vibe Code usage analytics. Requires an admin API key.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Admin Analytics",),
                "fields": {
                    "start_time": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Start Time",
                        "short_desc": "Start of the queried window, as a Unix timestamp in seconds.",
                    },
                    "end_time": <RestSchemaFieldOverlayInfo>{
                        "display_name": "End Time",
                        "short_desc": "End of the queried window, as a Unix timestamp in seconds.",
                    },
                },
            },
        );
    }
}
}
