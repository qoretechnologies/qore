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
    All non-deprecated, completed-response operations in the pinned official description are exposed.
    SSE-only operations are excluded; the LLM provider supports streaming chat. Preview operations remain
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
            # the operator of a filter condition, which filter groups nest without a limit
            "#/components/schemas/FilterCondition/properties/op": {
                "lt": "Lt",
                "lte": "Lte",
                "gt": "Gt",
                "gte": "Gte",
                "startswith": "Startswith",
                "istartswith": "Istartswith",
                "endswith": "Endswith",
                "iendswith": "Iendswith",
                "contains": "Contains",
                "icontains": "Icontains",
                "matches": "Matches",
                "notcontains": "Notcontains",
                "inotcontains": "Inotcontains",
                "eq": "Eq",
                "neq": "Neq",
                "isnull": "Isnull",
                "includes": "Includes",
                "excludes": "Excludes",
                "len_eq": "Len Eq",
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
            "judges": <RestSchemaReferenceDataInfo>{
                "name": "judges",
                "action": "list-judges",
                "value_field": "id",
                "display_field": "name",
                "desc": "Choose an existing judge. Listings return one page; identifiers can also be supplied directly.",
                "records_path": ("judges", "results"),
            },
            "campaigns": <RestSchemaReferenceDataInfo>{
                "name": "campaigns",
                "action": "list-campaigns",
                "value_field": "id",
                "display_field": "name",
                "desc": "Choose an existing campaign. Listings return one page; identifiers can also be supplied directly.",
                "records_path": ("campaigns", "results"),
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
            "dataset-tasks": <RestSchemaReferenceDataInfo>{
                "name": "dataset-tasks", "action": "list-dataset-tasks",
                "value_field": "id", "display_field": "id", "records_path": ("tasks", "results"),
                "options_from": {"dataset_id": "dataset_id"},
                "desc": "Choose an import task from the selected dataset.",
            },
            "chat-events": <RestSchemaReferenceDataInfo>{
                "name": "chat-events", "action": "search-chat-events",
                "value_field": "event_id", "display_field": "event_id",
                "records_path": ("completion_events", "results"),
                "options": {"search_params": {"filters": NOTHING}, "page_size": 100},
                "desc": "Choose a recorded chat completion event from the first page of account history.",
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
        };

        #! Stable action contracts checked against the vendored schema
        const list<hash<RestSchemaActionInfo>> Manifest = (
            <RestSchemaActionInfo>{
                "action": "list-models",
                "method": "GET",
                "path": "/v1/models",
                "operation_id": "list_models_v1_models_get",
                "display_name": "List Models",
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
                "display_name": "Create Conversation",
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
                "display_name": "List Conversations",
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
                "display_name": "Create Agent",
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
                "display_name": "List Agents",
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
                "display_name": "List Agent Pages",
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
                "display_name": "Upload File",
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
                "display_name": "List Files",
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
                "display_name": "List Batch Jobs",
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
                "display_name": "Create Batch Job",
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
                "path": "/v1/chat/completions",
                "response_content_type": "application/json",
                "operation_id": "chat_completion_v1_chat_completions_post",
                "display_name": "Chat Completion",
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
                "path": "/v1/fim/completions",
                "response_content_type": "application/json",
                "operation_id": "fim_completion_v1_fim_completions_post",
                "display_name": "Complete Code",
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
                "path": "/v1/embeddings",
                "operation_id": "embeddings_v1_embeddings_post",
                "display_name": "Create Embeddings",
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
                "path": "/v1/moderations",
                "operation_id": "moderations_v1_moderations_post",
                "display_name": "Moderate Text",
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
                "path": "/v1/chat/moderations",
                "operation_id": "chat_moderations_v1_chat_moderations_post",
                "display_name": "Moderate Chat",
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
                    "id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Id",
                        "short_desc": "Id",
                        "desc": "The `id` value for this operation.",
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
                "path": "/v1/classifications",
                "operation_id": "classifications_v1_classifications_post",
                "display_name": "Classify Text",
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
                "path": "/v1/chat/classifications",
                "operation_id": "chat_classifications_v1_chat_classifications_post",
                "display_name": "Classify Chat",
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
                "path": "/v1/audio/transcriptions",
                "operation_id": "audio_api_v1_transcriptions_post",
                "display_name": "Transcribe Audio",
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
                "path": "/v1/audio/speech",
                "response_content_type": "application/json",
                "operation_id": "speech_v1_audio_speech_post",
                "display_name": "Generate Speech",
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
                "display_name": "List Voices",
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
                "display_name": "Create Voice",
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
                    "slug": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Slug",
                        "short_desc": "Slug",
                        "desc": "The `slug` value for this operation.",
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
                "response_content_type": "application/json",
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
                "display_name": "List Libraries",
                "short_desc": "List Libraries",
                "desc": "List all libraries you have access to. Manage document libraries used to ground assistant answers.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Libraries",),
            },
            <RestSchemaActionInfo>{
                "action": "create-library",
                "method": "POST",
                "path": "/v1/libraries",
                "operation_id": "libraries_create_v1",
                "display_name": "Create Library",
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
                            <AllowedValueInfo>{
                                "value": "Org",
                                "display_name": "Org",
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
                            <AllowedValueInfo>{
                                "value": "Org",
                                "display_name": "Org",
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
                "action": "search-chat-events",
                "method": "POST",
                "path": "/v1/observability/chat-completion-events/search",
                "operation_id": "get_chat_completion_events_v1_observability_chat_completion_events_search_post",
                "display_name": "Search Chat Events",
                "short_desc": "Search Chat Events",
                "desc": "Get Chat Completion Events. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Chat Completion Events",),
                "fields": {
                    "search_params": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Search Params",
                        "short_desc": "Search Params",
                        "desc": "The `search_params` value for this operation.",
                    },
                    "extra_fields": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Extra Fields",
                        "short_desc": "Extra Fields",
                        "desc": "The `extra_fields` value for this operation.",
                    },
                    "page_size": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Page Size",
                        "short_desc": "Page Size",
                        "desc": "The `page_size` value for this operation.",
                        "preselected": True,
                    },
                    "cursor": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Cursor",
                        "short_desc": "Cursor",
                        "desc": "The `cursor` value for this operation.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "search-chat-event-ids",
                "method": "POST",
                "path": "/v1/observability/chat-completion-events/search-ids",
                "operation_id": "get_chat_completion_event_ids_v1_observability_chat_completion_events_search_ids_post",
                "display_name": "Search Chat Event IDs",
                "short_desc": "Search Chat Event IDs",
                "desc": "Alternative to /search that returns only the IDs and that can return many IDs at once. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Chat Completion Events",),
                "fields": {
                    "search_params": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Search Params",
                        "short_desc": "Search Params",
                        "desc": "The `search_params` value for this operation.",
                    },
                    "extra_fields": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Extra Fields",
                        "short_desc": "Extra Fields",
                        "desc": "The `extra_fields` value for this operation.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-chat-completion-event",
                "method": "GET",
                "path": "/v1/observability/chat-completion-events/{event_id}",
                "operation_id": "get_chat_completion_event_v1_observability_chat_completion_events__event_id__get",
                "display_name": "Get Chat Completion Event",
                "short_desc": "Get Chat Completion Event",
                "desc": "Get Chat Completion Event. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Chat Completion Events",),
                "fields": {
                    "event_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Event ID",
                        "short_desc": "Event ID",
                        "desc": "The `event_id` value for this operation.",
                        "ref_data": "chat-events",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-similar-chat-completion-events",
                "method": "GET",
                "path": "/v1/observability/chat-completion-events/{event_id}/similar-events",
                "operation_id": "get_similar_chat_completion_events_v1_observability_chat_completion_events__event_id__similar_events_get",
                "display_name": "Get Similar Chat Completion Events",
                "short_desc": "Get Similar Chat Completion Events",
                "desc": "Get Similar Chat Completion Events. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Chat Completion Events",),
                "fields": {
                    "event_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Event ID",
                        "short_desc": "Event ID",
                        "desc": "The `event_id` value for this operation.",
                        "ref_data": "chat-events",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-chat-completion-fields",
                "method": "GET",
                "path": "/v1/observability/chat-completion-fields",
                "operation_id": "get_chat_completion_fields_v1_observability_chat_completion_fields_get",
                "display_name": "Get Chat Completion Fields",
                "short_desc": "Get Chat Completion Fields",
                "desc": "Get Chat Completion Fields. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Chat Completion Events Fields",),
            },
            <RestSchemaActionInfo>{
                "action": "get-chat-completion-field-options",
                "method": "GET",
                "path": "/v1/observability/chat-completion-fields/{field_name}/options",
                "operation_id": "get_chat_completion_field_options_v1_observability_chat_completion_fields__field_name__options_get",
                "display_name": "Get Chat Completion Field Options",
                "short_desc": "Get Chat Completion Field Options",
                "desc": "Get Chat Completion Field Options. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Chat Completion Events Fields",),
                "fields": {
                    "field_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Field Name",
                        "short_desc": "Field Name",
                        "desc": "The `field_name` value for this operation.",
                    },
                    "operator": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Operator",
                        "short_desc": "The operator to use for filtering options",
                        "allowed_values": (
                            <AllowedValueInfo>{
                                "value": "lt",
                                "display_name": "Lt",
                            },
                            <AllowedValueInfo>{
                                "value": "lte",
                                "display_name": "Lte",
                            },
                            <AllowedValueInfo>{
                                "value": "gt",
                                "display_name": "Gt",
                            },
                            <AllowedValueInfo>{
                                "value": "gte",
                                "display_name": "Gte",
                            },
                            <AllowedValueInfo>{
                                "value": "startswith",
                                "display_name": "Startswith",
                            },
                            <AllowedValueInfo>{
                                "value": "istartswith",
                                "display_name": "Istartswith",
                            },
                            <AllowedValueInfo>{
                                "value": "endswith",
                                "display_name": "Endswith",
                            },
                            <AllowedValueInfo>{
                                "value": "iendswith",
                                "display_name": "Iendswith",
                            },
                            <AllowedValueInfo>{
                                "value": "contains",
                                "display_name": "Contains",
                            },
                            <AllowedValueInfo>{
                                "value": "icontains",
                                "display_name": "Icontains",
                            },
                            <AllowedValueInfo>{
                                "value": "matches",
                                "display_name": "Matches",
                            },
                            <AllowedValueInfo>{
                                "value": "notcontains",
                                "display_name": "Notcontains",
                            },
                            <AllowedValueInfo>{
                                "value": "inotcontains",
                                "display_name": "Inotcontains",
                            },
                            <AllowedValueInfo>{
                                "value": "eq",
                                "display_name": "Eq",
                            },
                            <AllowedValueInfo>{
                                "value": "neq",
                                "display_name": "Neq",
                            },
                            <AllowedValueInfo>{
                                "value": "isnull",
                                "display_name": "Isnull",
                            },
                            <AllowedValueInfo>{
                                "value": "includes",
                                "display_name": "Includes",
                            },
                            <AllowedValueInfo>{
                                "value": "excludes",
                                "display_name": "Excludes",
                            },
                            <AllowedValueInfo>{
                                "value": "len_eq",
                                "display_name": "Len Eq",
                            },
                        ),
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-chat-completion-field-options-counts",
                "method": "POST",
                "path": "/v1/observability/chat-completion-fields/{field_name}/options-counts",
                "operation_id": "get_chat_completion_field_options_counts_v1_observability_chat_completion_fields__field_name__options_counts_post",
                "display_name": "Get Chat Completion Field Options Counts",
                "short_desc": "Get Chat Completion Field Options Counts",
                "desc": "Get Chat Completion Field Options Counts. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Chat Completion Events Fields",),
                "fields": {
                    "filter_params": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Filter Params",
                        "short_desc": "Filter Params",
                        "desc": "The `filter_params` value for this operation.",
                    },
                    "field_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Field Name",
                        "short_desc": "Field Name",
                        "desc": "The `field_name` value for this operation.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "judge-chat-event",
                "method": "POST",
                "path": "/v1/observability/chat-completion-events/{event_id}/live-judging",
                "operation_id": "judge_chat_completion_event_v1_observability_chat_completion_events__event_id__live_judging_post",
                "display_name": "Judge Chat Event",
                "short_desc": "Judge Chat Event",
                "desc": "Run Judge on an event based on the given options. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Chat Completion Events",),
                "fields": {
                    "judge_definition": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Judge Definition",
                        "short_desc": "Judge Definition",
                        "desc": "The `judge_definition` value for this operation.",
                    },
                    "event_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Event ID",
                        "short_desc": "Event ID",
                        "desc": "The `event_id` value for this operation.",
                        "ref_data": "chat-events",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-judge",
                "method": "POST",
                "path": "/v1/observability/judges",
                "operation_id": "create_judge_v1_observability_judges_post",
                "display_name": "Create Judge",
                "short_desc": "Create Judge",
                "desc": "Create a new judge. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Judges",),
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
                    "model_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model Name",
                        "short_desc": "Model Name",
                        "desc": "The `model_name` value for this operation.",
                    },
                    "output": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Output",
                        "short_desc": "Output",
                        "desc": "The `output` value for this operation.",
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
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-judges",
                "method": "GET",
                "path": "/v1/observability/judges",
                "operation_id": "get_judges_v1_observability_judges_get",
                "display_name": "List Judges",
                "short_desc": "List Judges",
                "desc": "Get judges with optional filtering and search. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Judges",),
                "fields": {
                    "type_filter": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Type Filter",
                        "short_desc": "Filter by judge output types",
                        "element_allowed_values": (
                            <AllowedValueInfo>{
                                "value": "REGRESSION",
                                "display_name": "Regression",
                            },
                            <AllowedValueInfo>{
                                "value": "CLASSIFICATION",
                                "display_name": "Classification",
                            },
                        ),
                    },
                    "model_filter": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model Filter",
                        "short_desc": "Filter by model names",
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
                    "q": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Q",
                        "short_desc": "Q",
                        "desc": "The `q` value for this operation.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-judge",
                "method": "GET",
                "path": "/v1/observability/judges/{judge_id}",
                "operation_id": "get_judge_by_id_v1_observability_judges__judge_id__get",
                "display_name": "Get Judge",
                "short_desc": "Get Judge",
                "desc": "Get judge by id. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Judges",),
                "fields": {
                    "judge_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Judge ID",
                        "short_desc": "Judge ID",
                        "desc": "The `judge_id` value for this operation.",
                        "ref_data": "judges",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-judge",
                "method": "DELETE",
                "path": "/v1/observability/judges/{judge_id}",
                "operation_id": "delete_judge_v1_observability_judges__judge_id__delete",
                "display_name": "Delete Judge",
                "short_desc": "Delete Judge",
                "desc": "Delete a judge. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.\n\n**Deletion:** This operation can permanently remove the selected resource or access.",
                "groups": ("Observability Judges",),
                "fields": {
                    "judge_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Judge ID",
                        "short_desc": "Judge ID",
                        "desc": "The `judge_id` value for this operation.",
                        "ref_data": "judges",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-judge",
                "method": "PUT",
                "path": "/v1/observability/judges/{judge_id}",
                "operation_id": "update_judge_v1_observability_judges__judge_id__put",
                "display_name": "Update Judge",
                "short_desc": "Update Judge",
                "desc": "Update a judge. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Judges",),
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
                    "model_name": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Model Name",
                        "short_desc": "Model Name",
                        "desc": "The `model_name` value for this operation.",
                    },
                    "output": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Output",
                        "short_desc": "Output",
                        "desc": "The `output` value for this operation.",
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
                    },
                    "judge_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Judge ID",
                        "short_desc": "Judge ID",
                        "desc": "The `judge_id` value for this operation.",
                        "ref_data": "judges",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "run-judge",
                "method": "POST",
                "path": "/v1/observability/judges/{judge_id}/live-judging",
                "operation_id": "judge_conversation_v1_observability_judges__judge_id__live_judging_post",
                "display_name": "Run Judge",
                "short_desc": "Run Judge",
                "desc": "Run a saved judge on a conversation. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Judges",),
                "fields": {
                    "messages": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Messages",
                        "short_desc": "Messages",
                        "desc": "The `messages` value for this operation.",
                    },
                    "properties": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Properties",
                        "short_desc": "Properties",
                        "desc": "The `properties` value for this operation.",
                    },
                    "judge_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Judge ID",
                        "short_desc": "Judge ID",
                        "desc": "The `judge_id` value for this operation.",
                        "ref_data": "judges",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "create-campaign",
                "method": "POST",
                "path": "/v1/observability/campaigns",
                "operation_id": "create_campaign_v1_observability_campaigns_post",
                "display_name": "Create Campaign",
                "short_desc": "Create Campaign",
                "desc": "Create and start a new campaign. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Campaigns",),
                "fields": {
                    "search_params": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Search Params",
                        "short_desc": "Search Params",
                        "desc": "The `search_params` value for this operation.",
                    },
                    "judge_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Judge ID",
                        "short_desc": "Judge ID",
                        "desc": "The `judge_id` value for this operation.",
                        "ref_data": "judges",
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
                    "max_nb_events": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Max Nb Events",
                        "short_desc": "Max Nb Events",
                        "desc": "The `max_nb_events` value for this operation.",
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-campaigns",
                "method": "GET",
                "path": "/v1/observability/campaigns",
                "operation_id": "get_campaigns_v1_observability_campaigns_get",
                "display_name": "List Campaigns",
                "short_desc": "List Campaigns",
                "desc": "Get all campaigns. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Campaigns",),
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
                "action": "get-campaign",
                "method": "GET",
                "path": "/v1/observability/campaigns/{campaign_id}",
                "operation_id": "get_campaign_by_id_v1_observability_campaigns__campaign_id__get",
                "display_name": "Get Campaign",
                "short_desc": "Get Campaign",
                "desc": "Get campaign by id. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Campaigns",),
                "fields": {
                    "campaign_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Campaign ID",
                        "short_desc": "Campaign ID",
                        "desc": "The `campaign_id` value for this operation.",
                        "ref_data": "campaigns",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-campaign",
                "method": "DELETE",
                "path": "/v1/observability/campaigns/{campaign_id}",
                "operation_id": "delete_campaign_v1_observability_campaigns__campaign_id__delete",
                "display_name": "Delete Campaign",
                "short_desc": "Delete Campaign",
                "desc": "Delete a campaign. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.\n\n**Deletion:** This operation can permanently remove the selected resource or access.",
                "groups": ("Observability Campaigns",),
                "fields": {
                    "campaign_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Campaign ID",
                        "short_desc": "Campaign ID",
                        "desc": "The `campaign_id` value for this operation.",
                        "ref_data": "campaigns",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-campaign-status",
                "method": "GET",
                "path": "/v1/observability/campaigns/{campaign_id}/status",
                "operation_id": "get_campaign_status_by_id_v1_observability_campaigns__campaign_id__status_get",
                "display_name": "Get Campaign Status",
                "short_desc": "Get Campaign Status",
                "desc": "Get campaign status by campaign id. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Campaigns",),
                "fields": {
                    "campaign_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Campaign ID",
                        "short_desc": "Campaign ID",
                        "desc": "The `campaign_id` value for this operation.",
                        "ref_data": "campaigns",
                        "supports_custom_values": True,
                    },
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-campaign-events",
                "method": "GET",
                "path": "/v1/observability/campaigns/{campaign_id}/selected-events",
                "operation_id": "get_campaign_selected_events_v1_observability_campaigns__campaign_id__selected_events_get",
                "display_name": "Get Campaign Events",
                "short_desc": "Get Campaign Events",
                "desc": "Get event ids that were selected by the given campaign. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Campaigns",),
                "fields": {
                    "campaign_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Campaign ID",
                        "short_desc": "Campaign ID",
                        "desc": "The `campaign_id` value for this operation.",
                        "ref_data": "campaigns",
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
                "action": "create-dataset",
                "method": "POST",
                "path": "/v1/observability/datasets",
                "operation_id": "create_dataset_v1_observability_datasets_post",
                "display_name": "Create Dataset",
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
                "display_name": "List Datasets",
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
                "action": "import-dataset-from-campaign",
                "method": "POST",
                "path": "/v1/observability/datasets/{dataset_id}/imports/from-campaign",
                "operation_id": "post_dataset_records_from_campaign_v1_observability_datasets__dataset_id__imports_from_campaign_post",
                "display_name": "Import Dataset From Campaign",
                "short_desc": "Import Dataset From Campaign",
                "desc": "Populate the dataset with a campaign. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Datasets",),
                "fields": {
                    "campaign_id": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Campaign ID",
                        "short_desc": "Campaign ID",
                        "desc": "The `campaign_id` value for this operation.",
                        "ref_data": "campaigns",
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
                "action": "import-dataset-from-explorer",
                "method": "POST",
                "path": "/v1/observability/datasets/{dataset_id}/imports/from-explorer",
                "operation_id": "post_dataset_records_from_explorer_v1_observability_datasets__dataset_id__imports_from_explorer_post",
                "display_name": "Import Dataset From Explorer",
                "short_desc": "Import Dataset From Explorer",
                "desc": "Populate the dataset with samples from the explorer. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Datasets",),
                "fields": {
                    "completion_event_ids": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Completion Event IDs",
                        "short_desc": "Completion Event IDs",
                        "desc": "The `completion_event_ids` value for this operation.",
                        "element_ref_data": "chat-events",
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
                "display_name": "Delete Dataset Records",
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
                "action": "judge-dataset-record",
                "method": "POST",
                "path": "/v1/observability/dataset-records/{dataset_record_id}/live-judging",
                "operation_id": "judge_dataset_record_v1_observability_dataset_records__dataset_record_id__live_judging_post",
                "display_name": "Judge Dataset Record",
                "short_desc": "Judge Dataset Record",
                "desc": "Run Judge on a dataset record based on the given options. Inspect AI activity and evaluate response quality.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Observability Datasets Records",),
                "fields": {
                    "judge_definition": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Judge Definition",
                        "short_desc": "Judge Definition",
                        "desc": "The `judge_definition` value for this operation.",
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
                "display_name": "Batch Terminate Workflow Executions",
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
                "display_name": "Batch Cancel Workflow Executions",
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
                "display_name": "List Workflow Runs",
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
                "display_name": "List Workflow Schedules",
                "short_desc": "List Workflow Schedules",
                "desc": "Get Schedules. Manage automated workflows and inspect their execution.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Workflows Schedules",),
            },
            <RestSchemaActionInfo>{
                "action": "schedule-workflow",
                "method": "POST",
                "path": "/v1/workflows/schedules",
                "operation_id": "schedule_workflow_v1_workflows_schedules_post",
                "display_name": "Schedule Workflow",
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
                "action": "get-workflow-worker",
                "method": "GET",
                "path": "/v1/workflows/workers/whoami",
                "operation_id": "get_worker_info_v1_workflows_workers_whoami_get",
                "display_name": "Get Workflow Worker",
                "short_desc": "Get Workflow Worker",
                "desc": "Get Worker Info. Manage automated workflows and inspect their execution.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Workflows Workers",),
            },
            <RestSchemaActionInfo>{
                "action": "get-workflow-events",
                "method": "GET",
                "path": "/v1/workflows/events/list",
                "operation_id": "get_workflow_events_v1_workflows_events_list_get",
                "display_name": "Get Workflow Events",
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
                "display_name": "List Workflow Deployments",
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
                "display_name": "List Workflow Registrations",
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
                    "encoded_input": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Encoded Input",
                        "short_desc": "Encoded input to the workflow, used when payload encoding is enabled.",
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
                    "encoded_input": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Encoded Input",
                        "short_desc": "Encoded input to the workflow, used when payload encoding is enabled.",
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
                "display_name": "Create Connector",
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
                                "value": "shared_global",
                                "display_name": "Shared Global",
                            },
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
                    "auth_data": <RestSchemaFieldOverlayInfo>{
                        "sensitive": True,
                        "display_name": "Auth Data",
                        "short_desc": "Optional additional authentication data for the connector.",
                        "fields": {
                            "client_secret": <RestSchemaFieldOverlayInfo>{
                                "sensitive": True,
                            },
                        },
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
                "display_name": "List Connectors",
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
                "method": "POST",
                "path": "/v1/connectors/{connector_id_or_name}/organization/credentials",
                "operation_id": "connector_create_or_update_organization_credentials_v1",
                "display_name": "Create Or Update Organization Credentials For A Connector",
                "short_desc": "Create Or Update Organization Credentials For A Connector",
                "desc": "Create or update organization credentials for a connector. Connect assistants to external business tools and manage their access.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Connectors",),
                "fields": {
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
                                    "access_token": <RestSchemaFieldOverlayInfo>{
                                        "sensitive": True,
                                    },
                                    "refresh_token": <RestSchemaFieldOverlayInfo>{
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
                "method": "POST",
                "path": "/v1/connectors/{connector_id_or_name}/workspace/credentials",
                "operation_id": "connector_create_or_update_workspace_credentials_v1",
                "display_name": "Create Or Update Workspace Credentials For A Connector",
                "short_desc": "Create Or Update Workspace Credentials For A Connector",
                "desc": "Create or update workspace credentials for a connector. Connect assistants to external business tools and manage their access.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Connectors",),
                "fields": {
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
                                    "access_token": <RestSchemaFieldOverlayInfo>{
                                        "sensitive": True,
                                    },
                                    "refresh_token": <RestSchemaFieldOverlayInfo>{
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
                "method": "POST",
                "path": "/v1/connectors/{connector_id_or_name}/user/credentials",
                "operation_id": "connector_create_or_update_user_credentials_v1",
                "display_name": "Create Or Update User Credentials For A Connector",
                "short_desc": "Create Or Update User Credentials For A Connector",
                "desc": "Create or update user credentials for a connector. Connect assistants to external business tools and manage their access.\n\n**Public preview:** Availability depends on your account and permissions.",
                "groups": ("Connectors",),
                "fields": {
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
                                    "access_token": <RestSchemaFieldOverlayInfo>{
                                        "sensitive": True,
                                    },
                                    "refresh_token": <RestSchemaFieldOverlayInfo>{
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
                "path": "/v1/connectors/{connector_id_or_name}/organization/credentials/{credentials_name}",
                "operation_id": "connector_delete_organization_credentials_v1",
                "display_name": "Delete Organization Credentials For A Connector",
                "short_desc": "Delete Organization Credentials For A Connector",
                "desc": "Delete organization credentials for a connector. Connect assistants to external business tools and manage their access.\n\n**Public preview:** Availability depends on your account and permissions.\n\n**Deletion:** This operation can permanently remove the selected resource or access.",
                "groups": ("Connectors",),
                "fields": {
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
                "path": "/v1/connectors/{connector_id_or_name}/workspace/credentials/{credentials_name}",
                "operation_id": "connector_delete_workspace_credentials_v1",
                "display_name": "Delete Workspace Credentials For A Connector",
                "short_desc": "Delete Workspace Credentials For A Connector",
                "desc": "Delete workspace credentials for a connector. Connect assistants to external business tools and manage their access.\n\n**Public preview:** Availability depends on your account and permissions.\n\n**Deletion:** This operation can permanently remove the selected resource or access.",
                "groups": ("Connectors",),
                "fields": {
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
                "path": "/v1/connectors/{connector_id_or_name}/user/credentials/{credentials_name}",
                "operation_id": "connector_delete_user_credentials_v1",
                "display_name": "Delete User Credentials For A Connector",
                "short_desc": "Delete User Credentials For A Connector",
                "desc": "Delete user credentials for a connector. Connect assistants to external business tools and manage their access.\n\n**Public preview:** Availability depends on your account and permissions.\n\n**Deletion:** This operation can permanently remove the selected resource or access.",
                "groups": ("Connectors",),
                "fields": {
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
                    "fetch_customer_data": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Fetch Customer Data",
                        "short_desc": "Fetch the customer data associated with the connector (e.g",
                    },
                    "fetch_connection_secrets": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Fetch Connection Secrets",
                        "short_desc": "Fetch the general connection secrets associated with the connector.",
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
                    "connection_config": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Connection Config",
                        "short_desc": "Optional new connection config.",
                    },
                    "connection_secrets": <RestSchemaFieldOverlayInfo>{
                        "sensitive": True,
                        "display_name": "Connection Secrets",
                        "short_desc": "Optional new connection secrets",
                    },
                    "server": <RestSchemaFieldOverlayInfo>{
                        "display_name": "Server",
                        "short_desc": "New server url for your mcp connector.",
                    },
                    "headers": <RestSchemaFieldOverlayInfo>{
                        "sensitive": True,
                        "display_name": "Headers",
                        "short_desc": "New headers for your mcp connector.",
                    },
                    "auth_data": <RestSchemaFieldOverlayInfo>{
                        "sensitive": True,
                        "display_name": "Auth Data",
                        "short_desc": "New authentication data for your mcp connector.",
                        "fields": {
                            "client_secret": <RestSchemaFieldOverlayInfo>{
                                "sensitive": True,
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
        );
    }
}
}
