# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file A2aVersionTranslation.qc @brief A2A protocol version translation functions

/*  A2aVersionTranslation.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! the %A2aClient namespace holds all public definitions in the %A2aClient module
public namespace A2aClient {

#! A2A protocol version constants
public const A2aVersionV03 = "0.3";
public const A2aVersionV10 = "1.0";
public const A2aVersionHeader = "A2A-Version";

#! The A2A media type registered in section 14.1 of the A2A specification
/** @note This media type is defined for the HTTP+JSON/REST protocol binding and for push-notification webhook
    bodies.  The JSON-RPC protocol binding that this module implements uses \c application/json for requests and
    responses; do not send this media type on JSON-RPC calls.

    @since A2aClient 1.0
*/
public const MimeTypeA2aJson = "application/a2a+json";

#! Agent card paths for v1.0
public const AgentCardPathV10 = "/.well-known/a2a-agent-card";
public const AgentCardExtendedPathV10 = "/.well-known/a2a-agent-card:extended";

#! v0.3 → v1.0 task state mapping
/** The keys are the canonical A2A v0.3 \c TaskState values.  The legacy Qore-only spellings \c "pending" and
    \c "active" are also accepted on input so that existing callers keep working, but they are never emitted;
    no A2A peer accepts them.
*/
public const A2aStateMapV03toV10 = {
    "submitted": "TASK_STATE_SUBMITTED",
    "working": "TASK_STATE_WORKING",
    "input-required": "TASK_STATE_INPUT_REQUIRED",
    "auth-required": "TASK_STATE_AUTH_REQUIRED",
    "completed": "TASK_STATE_COMPLETED",
    "failed": "TASK_STATE_FAILED",
    "canceled": "TASK_STATE_CANCELED",
    "rejected": "TASK_STATE_REJECTED",
    "unknown": "TASK_STATE_UNSPECIFIED",

    # legacy input-only aliases; see the note above
    "pending": "TASK_STATE_SUBMITTED",
    "active": "TASK_STATE_WORKING",
};

#! v1.0 → v0.3 task state mapping
/** Only canonical A2A v0.3 \c TaskState values are emitted.
*/
public const A2aStateMapV10toV03 = {
    "TASK_STATE_SUBMITTED": "submitted",
    "TASK_STATE_WORKING": "working",
    "TASK_STATE_INPUT_REQUIRED": "input-required",
    "TASK_STATE_AUTH_REQUIRED": "auth-required",
    "TASK_STATE_COMPLETED": "completed",
    "TASK_STATE_FAILED": "failed",
    "TASK_STATE_CANCELED": "canceled",
    "TASK_STATE_REJECTED": "rejected",
    "TASK_STATE_UNSPECIFIED": "unknown",
};

#! v0.3 → v1.0 role mapping
public const A2aRoleMapV03toV10 = {
    "user": "ROLE_USER",
    "agent": "ROLE_AGENT",
};

#! v1.0 → v0.3 role mapping
public const A2aRoleMapV10toV03 = {
    "ROLE_USER": "user",
    "ROLE_AGENT": "agent",
    "ROLE_UNSPECIFIED": "user",
};

#! v0.3 → v1.0 method name mapping
public const A2aMethodMapV03toV10 = {
    "message/send": "SendMessage",
    "message/stream": "SendStreamingMessage",
    "tasks/get": "GetTask",
    "tasks/cancel": "CancelTask",
    "tasks/list": "ListTasks",
    "tasks/resubscribe": "SubscribeToTask",
    "tasks/pushNotificationConfig/set": "CreateTaskPushNotificationConfig",
    "tasks/pushNotificationConfig/get": "GetTaskPushNotificationConfig",
    "tasks/pushNotificationConfig/list": "ListTaskPushNotificationConfigs",
    "tasks/pushNotificationConfig/delete": "DeleteTaskPushNotificationConfig",
    "agent/getAuthenticatedExtendedCard": "GetExtendedAgentCard",
};

#! v1.0 → v0.3 method name mapping
public const A2aMethodMapV10toV03 = {
    "SendMessage": "message/send",
    "SendStreamingMessage": "message/stream",
    "GetTask": "tasks/get",
    "CancelTask": "tasks/cancel",
    "ListTasks": "tasks/list",
    "SubscribeToTask": "tasks/resubscribe",
    "CreateTaskPushNotificationConfig": "tasks/pushNotificationConfig/set",
    "GetTaskPushNotificationConfig": "tasks/pushNotificationConfig/get",
    "ListTaskPushNotificationConfigs": "tasks/pushNotificationConfig/list",
    "DeleteTaskPushNotificationConfig": "tasks/pushNotificationConfig/delete",
    "GetExtendedAgentCard": "agent/getAuthenticatedExtendedCard",
};

#! v1.0 SSE event discriminator field → v0.3 event type name
public const A2aSseEventMapV10toV03 = {
    "statusUpdate": "TaskStatusUpdateEvent",
    "artifactUpdate": "TaskArtifactUpdateEvent",
    "message": "TaskMessageUpdateEvent",
};

#! v0.3 SSE event type name → v1.0 discriminator field
public const A2aSseEventMapV03toV10 = {
    "TaskStatusUpdateEvent": "statusUpdate",
    "TaskArtifactUpdateEvent": "artifactUpdate",
    "TaskMessageUpdateEvent": "message",
};

#! Translate a task state from v0.3 to v1.0 format
/** @param state v0.3 task state (e.g., \c "submitted")
    @return v1.0 task state (e.g., \c "TASK_STATE_SUBMITTED")
*/
public string sub a2aTranslateTaskStateToV10(string state) {
    *string v10 = A2aStateMapV03toV10{state};
    return v10 ?? state;
}

#! Translate a task state from v1.0 to v0.3 format
/** @param state v1.0 task state (e.g., \c "TASK_STATE_SUBMITTED")
    @return v0.3 task state (e.g., \c "submitted")
*/
public string sub a2aTranslateTaskStateToV03(string state) {
    *string v03 = A2aStateMapV10toV03{state};
    return v03 ?? state;
}

#! Translate a role from v0.3 to v1.0 format
public string sub a2aTranslateRoleToV10(string role) {
    *string v10 = A2aRoleMapV03toV10{role};
    return v10 ?? role;
}

#! Translate a role from v1.0 to v0.3 format
public string sub a2aTranslateRoleToV03(string role) {
    *string v03 = A2aRoleMapV10toV03{role};
    return v03 ?? role;
}

#! Translate a JSON-RPC method name from v0.3 to v1.0 format
public string sub a2aTranslateMethodToV10(string method) {
    *string v10 = A2aMethodMapV03toV10{method};
    return v10 ?? method;
}

#! Translate a JSON-RPC method name from v1.0 to v0.3 format
public string sub a2aTranslateMethodToV03(string method) {
    *string v03 = A2aMethodMapV10toV03{method};
    return v03 ?? method;
}

#! Detect A2A protocol version from a JSON-RPC method name
/** Checks if the method name is a known v1.0 method (e.g., \c "SendMessage", \c "GetTask").
    Unknown methods default to v0.3.
    @return \c "1.0" if the method is a known v1.0 method name, \c "0.3" otherwise
*/
public string sub a2aDetectVersionFromMethod(string method) {
    return A2aMethodMapV10toV03{method} ? A2aVersionV10 : A2aVersionV03;
}

#! Translate message parts from v0.3 to v1.0 format
/** v0.3: \c {"type":"text","text":"Hello"}, \c {"type":"file","file":{"uri":"...","mimeType":"..."}}
    v1.0: \c {"text":"Hello"}, \c {"url":"...","mediaType":"..."}
*/
public list<hash<auto>> sub a2aTranslatePartsToV10(softlist<auto> parts) {
    list<hash<auto>> result = ();
    foreach hash<auto> part in (parts) {
        string part_type = part.type ?? part.kind ?? "";
        hash<auto!> v10_part;
        switch (part_type) {
            case "text": {
                v10_part.text = part.text;
                if (part.metadata) {
                    v10_part.metadata = part.metadata;
                }
                break;
            }
            case "file": {
                if (part.file) {
                    if (part.file.uri) {
                        v10_part.url = part.file.uri;
                    }
                    if (part.file.bytes) {
                        v10_part.raw = part.file.bytes;
                    }
                    if (part.file.name) {
                        v10_part.filename = part.file.name;
                    }
                    if (part.file.mimeType) {
                        v10_part.mediaType = part.file.mimeType;
                    }
                }
                if (part.metadata) {
                    v10_part.metadata = part.metadata;
                }
                break;
            }
            case "data": {
                v10_part.data = part.data;
                if (part.metadata) {
                    v10_part.metadata = part.metadata;
                }
                break;
            }
            default: {
                # Unknown or already v1.0 format — pass through
                v10_part = part;
                break;
            }
        }
        result += v10_part;
    }
    return result;
}

#! Translate message parts from v1.0 to v0.3 format
/** v1.0: \c {"text":"Hello"}, \c {"url":"...","mediaType":"..."}
    v0.3: \c {"type":"text","text":"Hello"}, \c {"type":"file","file":{"uri":"...","mimeType":"..."}}
*/
public list<hash<auto>> sub a2aTranslatePartsToV03(softlist<auto> parts) {
    list<hash<auto>> result = ();
    foreach hash<auto> part in (parts) {
        hash<auto!> v03_part;
        if (part.hasKey("text") && !part.hasKey("type")) {
            # Text part
            v03_part.type = "text";
            v03_part.text = part.text;
            if (part.metadata) {
                v03_part.metadata = part.metadata;
            }
        } else if (part.hasKey("url") || part.hasKey("raw")) {
            # File part
            hash<auto> file_info;
            if (part.url) {
                file_info.uri = part.url;
            }
            if (part.raw) {
                file_info.bytes = part.raw;
            }
            if (part.filename) {
                file_info.name = part.filename;
            }
            if (part.mediaType) {
                file_info.mimeType = part.mediaType;
            }
            v03_part.type = "file";
            v03_part.file = file_info;
            if (part.metadata) {
                v03_part.metadata = part.metadata;
            }
        } else if (part.hasKey("data") && !part.hasKey("type")) {
            # Data part
            v03_part.type = "data";
            v03_part.data = part.data;
            if (part.metadata) {
                v03_part.metadata = part.metadata;
            }
        } else {
            # Already v0.3 or unknown format — pass through
            v03_part = part;
        }
        result += v03_part;
    }
    return result;
}

#! Translate a message from v0.3 to v1.0 format
public hash<auto> sub a2aTranslateMessageToV10(hash<auto> msg) {
    hash<auto> result = msg;
    if (msg.role) {
        result.role = a2aTranslateRoleToV10(msg.role);
    }
    if (msg.parts) {
        result.parts = a2aTranslatePartsToV10(msg.parts);
    }
    # Remove timestamp from message (v1.0 puts it on TaskStatus only)
    delete result.timestamp;
    return result;
}

#! Translate a message from v1.0 to v0.3 format
/** @note \c contextId, \c taskId, and \c extensions are defined identically on the v0.3 \c Message object, so
    they are carried across unchanged; only the role and part encodings differ between versions
*/
public hash<auto> sub a2aTranslateMessageToV03(hash<auto> msg) {
    hash<auto> result = msg;
    if (msg.role) {
        result.role = a2aTranslateRoleToV03(msg.role);
    }
    if (msg.parts) {
        result.parts = a2aTranslatePartsToV03(msg.parts);
    }
    return result;
}

#! Translate a task from v0.3 to v1.0 wire format
public hash<auto> sub a2aTranslateTaskToV10(hash<auto> task) {
    hash<auto> result = task;
    if (task.status) {
        result.status = task.status;
        if (task.status.state) {
            result.status.state = a2aTranslateTaskStateToV10(task.status.state);
        }
        if (task.status.message) {
            result.status.message = a2aTranslateMessageToV10(task.status.message);
        }
    }
    if (task.history) {
        result.history = map a2aTranslateMessageToV10($1), task.history;
    }
    if (task.artifacts) {
        result.artifacts = map a2aTranslateArtifactToV10($1), task.artifacts;
    }
    return result;
}

#! Translate a task from v1.0 to v0.3 wire format
public hash<auto> sub a2aTranslateTaskToV03(hash<auto> task) {
    hash<auto> result = task;
    if (task.status) {
        result.status = task.status;
        if (task.status.state) {
            result.status.state = a2aTranslateTaskStateToV03(task.status.state);
        }
        if (task.status.message) {
            result.status.message = a2aTranslateMessageToV03(task.status.message);
        }
    }
    if (task.history) {
        result.history = map a2aTranslateMessageToV03($1), task.history;
    }
    if (task.artifacts) {
        result.artifacts = map a2aTranslateArtifactToV03($1), task.artifacts;
    }
    return result;
}

#! Translate an artifact from v0.3 to v1.0 format
public hash<auto> sub a2aTranslateArtifactToV10(hash<auto> artifact) {
    hash<auto> result = artifact;
    if (artifact.parts) {
        result.parts = a2aTranslatePartsToV10(artifact.parts);
    }
    return result;
}

#! Translate an artifact from v1.0 to v0.3 format
/** @note \c extensions is defined identically on the v0.3 \c Artifact object, so it is carried across
    unchanged; only the part encoding differs between versions
*/
public hash<auto> sub a2aTranslateArtifactToV03(hash<auto> artifact) {
    hash<auto> result = artifact;
    if (artifact.parts) {
        result.parts = a2aTranslatePartsToV03(artifact.parts);
    }
    return result;
}

#! Translate an agent card from v0.3 to v1.0 format
/** Converts top-level \c url to \c supportedInterfaces array
*/
public hash<auto> sub a2aTranslateAgentCardToV10(hash<auto> card) {
    hash<auto> result = card;
    # Move url into supportedInterfaces
    if (card.url && !card.supportedInterfaces) {
        result.supportedInterfaces = ({
            "url": card.url,
            "protocolBinding": "JSONRPC",
            "protocolVersion": A2aVersionV10,
        },);
        delete result.url;
    }
    # Move supportsAuthenticatedExtendedCard into capabilities.extendedAgentCard
    if (card.supportsAuthenticatedExtendedCard && card.capabilities) {
        result.capabilities.extendedAgentCard = card.supportsAuthenticatedExtendedCard;
        delete result.supportsAuthenticatedExtendedCard;
    }
    # Rename security to securityRequirements
    if (card.security) {
        result.securityRequirements = card.security;
        delete result.security;
    }
    return result;
}

#! Translate an agent card from v1.0 to v0.3 format
/** Extracts \c url from \c supportedInterfaces array
*/
public hash<auto> sub a2aTranslateAgentCardToV03(hash<auto> card) {
    hash<auto> result = card;
    # Extract url from supportedInterfaces
    if (card.supportedInterfaces && !card.url) {
        # Use the first JSON-RPC interface, or the first interface if none match
        *hash<auto> iface;
        foreach hash<auto> si in (card.supportedInterfaces) {
            if (si.protocolBinding == "JSONRPC") {
                iface = si;
                break;
            }
        }
        if (!iface && card.supportedInterfaces.size()) {
            iface = card.supportedInterfaces[0];
        }
        if (iface.url) {
            result.url = iface.url;
        }
        delete result.supportedInterfaces;
    }
    # Move capabilities.extendedAgentCard to supportsAuthenticatedExtendedCard
    if (card.capabilities.extendedAgentCard) {
        result.supportsAuthenticatedExtendedCard = card.capabilities.extendedAgentCard;
        delete result.capabilities.extendedAgentCard;
    }
    # Rename securityRequirements to security
    if (card.securityRequirements) {
        result.security = card.securityRequirements;
        delete result.securityRequirements;
    }
    return result;
}

#! Translate an SSE event from v0.3 format (JSON-RPC wrapped) to v1.0 format (plain discriminated)
/** @param event_type v0.3 event type (e.g., \c "TaskStatusUpdateEvent")
    @param data event data (the params from the JSON-RPC wrapper, including \c "id" for task ID)
    @return v1.0 format event hash with discriminator field
*/
public hash<auto> sub a2aTranslateSseEventToV10(string event_type, hash<auto> data) {
    *string disc_field = A2aSseEventMapV03toV10{event_type};
    if (!disc_field) {
        return data;
    }
    hash<auto> event_data = data;
    # Rename "id" to "taskId" for v1.0
    if (event_data.id) {
        event_data.taskId = event_data.id;
        delete event_data.id;
    }
    # Translate state values if present
    if (event_data.state) {
        event_data.state = a2aTranslateTaskStateToV10(event_data.state);
    }
    if (event_data.role) {
        event_data.role = a2aTranslateRoleToV10(event_data.role);
    }
    if (event_data.parts) {
        event_data.parts = a2aTranslatePartsToV10(event_data.parts);
    }
    return {disc_field: event_data};
}

#! Translate an SSE event from v1.0 format (plain discriminated) to v0.3 format (JSON-RPC wrapped)
/** @param event v1.0 format event hash with discriminator field
    @return a hash with \c event_type (string) and \c data (hash) for the v0.3 JSON-RPC wrapper
*/
public hash<auto> sub a2aTranslateSseEventToV03(hash<auto> event) {
    foreach string disc_field in (keys A2aSseEventMapV10toV03) {
        if (event.hasKey(disc_field)) {
            string v03_event_type = A2aSseEventMapV10toV03{disc_field};
            hash<auto> event_data = event{disc_field};
            # Rename "taskId" back to "id"
            if (event_data.taskId) {
                event_data.id = event_data.taskId;
                delete event_data.taskId;
            }
            # Translate values back to v0.3
            if (event_data.state) {
                event_data.state = a2aTranslateTaskStateToV03(event_data.state);
            }
            if (event_data.role) {
                event_data.role = a2aTranslateRoleToV03(event_data.role);
            }
            if (event_data.parts) {
                event_data.parts = a2aTranslatePartsToV03(event_data.parts);
            }
            return {"event_type": v03_event_type, "data": event_data};
        }
    }
    # Unknown format — return as-is
    return {"event_type": "unknown", "data": event};
}

#! Translate SendMessage params from v0.3 to v1.0 format
/** v0.3: \c {message: {...}, contextId: "...", metadata: {...}}
    v1.0: \c {message: {contextId: "...", ...}, configuration: {...}, metadata: {...}}
*/
public hash<auto> sub a2aTranslateSendMessageParamsToV10(hash<auto> params) {
    hash<auto> result = params;
    hash<auto> msg = params.message ?? {};
    # neither version defines a top-level contextId on the send-message params; accept one anyway and fold it
    # into the message, where both versions define it
    if (params.contextId && !msg.contextId) {
        msg.contextId = params.contextId;
    }
    delete result.contextId;
    # Ensure messageId is set (required in v1.0)
    if (!msg.messageId) {
        msg.messageId = get_random_bytes(16).toHex();
    }
    # Translate message role and parts
    msg = a2aTranslateMessageToV10(msg);
    result.message = msg;
    if (params.configuration) {
        result.configuration = a2aTranslateSendConfigurationToV10(params.configuration);
    }
    return result;
}

#! Translate send-message configuration from v0.3 to v1.0 format
/** The two versions agree on \c acceptedOutputModes and \c historyLength.  The push notification config is
    renamed, and v0.3's \c blocking flag is the logical inverse of v1.0's \c returnImmediately.

    @param config the v0.3 configuration

    @return the v1.0 configuration

    @since A2aClient 1.0
*/
public hash<auto> sub a2aTranslateSendConfigurationToV10(hash<auto> config) {
    hash<auto> result = config;
    if (config.hasKey("pushNotificationConfig")) {
        result.taskPushNotificationConfig = config.pushNotificationConfig;
        delete result.pushNotificationConfig;
    }
    if (config.hasKey("blocking")) {
        result.returnImmediately = !config.blocking;
        delete result.blocking;
    }
    return result;
}

#! Translate send-message configuration from v1.0 to v0.3 format
/** @param config the v1.0 configuration

    @return the v0.3 configuration

    @see a2aTranslateSendConfigurationToV10()

    @since A2aClient 1.0
*/
public hash<auto> sub a2aTranslateSendConfigurationToV03(hash<auto> config) {
    hash<auto> result = config;
    if (config.hasKey("taskPushNotificationConfig")) {
        result.pushNotificationConfig = config.taskPushNotificationConfig;
        delete result.taskPushNotificationConfig;
    }
    if (config.hasKey("returnImmediately")) {
        result.blocking = !config.returnImmediately;
        delete result.returnImmediately;
    }
    return result;
}

#! Translate SendMessage params from v1.0 to v0.3 format
/** v1.0: \c {message: {contextId: "...", ...}, configuration: {...}}
    v0.3: \c {message: {...}, contextId: "..."}
*/
/** @note The v0.3 \c MessageSendParams object defines only \c message, \c configuration, and \c metadata; the
    context ID belongs on the message in both versions and is left there.  Only \c tenant is v1.0-only.
*/
public hash<auto> sub a2aTranslateSendMessageParamsToV03(hash<auto> params) {
    hash<auto> result = params;
    hash<auto> msg = params.message ?? {};
    # Translate message
    msg = a2aTranslateMessageToV03(msg);
    result.message = msg;
    if (params.configuration) {
        result.configuration = a2aTranslateSendConfigurationToV03(params.configuration);
    }
    # Remove v1.0-only fields
    delete result.tenant;
    return result;
}

} # namespace A2aClient
