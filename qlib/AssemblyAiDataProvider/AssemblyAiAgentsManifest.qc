# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore AssemblyAiDataProvider API action manifest

/** AssemblyAiAgentsManifest.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Curated actions and resource selections for the agents API
public class AssemblyAiAgentsManifest {
    public {
        #! Resource listings used by action dropdowns
        const hash<string, hash<RestSchemaReferenceDataInfo>> ReferenceData = {
            "agents": <RestSchemaReferenceDataInfo>{"name": "agents", "action": "list-agents", "value_field": "id", "display_field": "name", "desc": "Recent agents; values from earlier workflow steps are also accepted."},
            "sessions": <RestSchemaReferenceDataInfo>{"name": "sessions", "action": "list-sessions", "records_path": ("sessions",), "value_field": "id", "desc": "Recent sessions; values from earlier workflow steps are also accepted."},
        };

        #! Headers for external tools and startup requests can contain third-party credentials
        const hash<RestSchemaFieldOverlayInfo> HttpSecrets = <RestSchemaFieldOverlayInfo>{"fields": {
            "http": <RestSchemaFieldOverlayInfo>{"fields": {
                "headers": <RestSchemaFieldOverlayInfo>{"sensitive": True},
            }},
        }};

        #! Stable action contracts resolved against the vendored API description
        const list<hash<RestSchemaActionInfo>> Manifest = (
            <RestSchemaActionInfo>{
                "action": "create-agent",
                "method": "POST",
                "path": "/v1/agents",
                "operation_id": "createAgent",
                "display_name": "Create Voice Agent",
                "short_desc": "Save a reusable voice agent configuration",
                "desc": "Create a voice agent with a name, system prompt, and voice. Optionally configure audio, model, tools, and requests made before connecting. Voice Agents uses its global service endpoint.",
                "groups": ("Voice Agents",),
                "fields": {
                    "tools": HttpSecrets,
                    "pre_connect_requests": HttpSecrets,
                    "llm": <RestSchemaFieldOverlayInfo>{"fields": {
                        "api_key": <RestSchemaFieldOverlayInfo>{"sensitive": True},
                    }},
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-agents",
                "method": "GET",
                "path": "/v1/agents",
                "operation_id": "listAgents",
                "display_name": "List Voice Agents",
                "short_desc": "Browse saved voice agents",
                "desc": "List lightweight agent records for this account, newest first. Use Get Voice Agent to retrieve a complete configuration.",
                "groups": ("Voice Agents",),
            },
            <RestSchemaActionInfo>{
                "action": "get-agent",
                "method": "GET",
                "path": "/v1/agents/{agent_id}",
                "operation_id": "getAgent",
                "display_name": "Get Voice Agent",
                "short_desc": "Retrieve a saved voice agent configuration",
                "desc": "Retrieve an agent by ID. Secret tool header values are masked by the service.",
                "groups": ("Voice Agents",),
                "fields": {
                    "agent_id": <RestSchemaFieldOverlayInfo>{"display_name": "Voice Agent", "ref_data": "agents", "supports_custom_values": True},
                },
            },
            <RestSchemaActionInfo>{
                "action": "update-agent",
                "method": "PUT",
                "path": "/v1/agents/{agent_id}",
                "operation_id": "updateAgent",
                "display_name": "Update Voice Agent",
                "short_desc": "Change selected settings of a saved voice agent",
                "desc": "Update only the agent fields you provide. Omitted fields retain their existing values.",
                "groups": ("Voice Agents",),
                "fields": {
                    "agent_id": <RestSchemaFieldOverlayInfo>{"display_name": "Voice Agent", "ref_data": "agents", "supports_custom_values": True},
                    "llm": <RestSchemaFieldOverlayInfo>{"fields": {
                        "api_key": <RestSchemaFieldOverlayInfo>{"sensitive": True},
                    }},
                    "tools": HttpSecrets,
                    "pre_connect_requests": HttpSecrets,
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-agent",
                "method": "DELETE",
                "path": "/v1/agents/{agent_id}",
                "operation_id": "deleteAgent",
                "display_name": "Delete Voice Agent",
                "short_desc": "Remove a saved voice agent",
                "desc": "Delete the selected agent. A successful request returns no body.",
                "groups": ("Voice Agents",),
                "fields": {
                    "agent_id": <RestSchemaFieldOverlayInfo>{"display_name": "Voice Agent", "ref_data": "agents", "supports_custom_values": True},
                },
            },
            <RestSchemaActionInfo>{
                "action": "list-sessions",
                "method": "GET",
                "path": "/v1/sessions",
                "operation_id": "listSessions",
                "display_name": "List Voice Agent Sessions",
                "short_desc": "Browse sessions by agent and status",
                "desc": "List recent sessions. Pass `response_metadata.next_cursor` as `cursor` to retrieve the next page.",
                "groups": ("Voice Agents",),
                "fields": {
                    "agent_id": <RestSchemaFieldOverlayInfo>{"display_name": "Voice Agent", "ref_data": "agents", "supports_custom_values": True},
                },
            },
            <RestSchemaActionInfo>{
                "action": "get-session",
                "method": "GET",
                "path": "/v1/sessions/{session_id}",
                "operation_id": "getSession",
                "display_name": "Get Voice Agent Session",
                "short_desc": "Retrieve session details and artifact download URLs",
                "desc": "Retrieve a session and short-lived URLs for its recording, conversation timeline, and metadata.",
                "groups": ("Voice Agents",),
                "fields": {
                    "session_id": <RestSchemaFieldOverlayInfo>{"display_name": "Session", "ref_data": "sessions", "supports_custom_values": True},
                },
            },
            <RestSchemaActionInfo>{
                "action": "delete-session",
                "method": "DELETE",
                "path": "/v1/sessions/{session_id}",
                "operation_id": "deleteSession",
                "display_name": "Delete Voice Agent Session",
                "short_desc": "Remove a voice agent session",
                "desc": "Soft-delete the selected session. A successful request returns no body.",
                "groups": ("Voice Agents",),
                "fields": {
                    "session_id": <RestSchemaFieldOverlayInfo>{"display_name": "Session", "ref_data": "sessions", "supports_custom_values": True},
                },
            },
        );
    }
}
}
