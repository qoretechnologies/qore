# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore A2A request and response data types

/*  Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the A2aClientDataProvider module
public namespace A2aClientDataProvider {

#! A2A message send request data type
public class A2aMessageSendRequestDataType inherits HashDataType {
    public {
        #! Field descriptions
        const Fields = {
            "role": {
                "type": StringType,
                "display_name": "Role",
                "short_desc": "Message role",
                "desc": "The role of the message sender: `user` or `agent`",
                "example_value": "user",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "display_name": "User",
                        "short_desc": "Message from user",
                        "desc": "A message from the user",
                        "value": A2aMessageRole::User,
                    },
                    <AllowedValueInfo>{
                        "display_name": "Agent",
                        "short_desc": "Message from agent",
                        "desc": "A message from the agent",
                        "value": A2aMessageRole::Agent,
                    },
                ),
            },
            "parts": {
                "type": AutoListType,
                "display_name": "Parts",
                "short_desc": "Message parts",
                "desc": "List of message parts (text, file, or data).\n\n"
                    "Each part is a hash with a `type` key:\n"
                    "- `text`: `{\"type\": \"text\", \"text\": \"Hello\"}`\n"
                    "- `file`: `{\"type\": \"file\", \"file\": {\"name\": \"doc.pdf\", \"uri\": \"...\"}}`\n"
                    "- `data`: `{\"type\": \"data\", \"data\": {\"key\": \"value\"}}`",
                "example_value": ({"type": "text", "text": "Hello, agent!"},),
            },
            "messageId": {
                "type": StringOrNothingType,
                "display_name": "Message ID",
                "short_desc": "Optional message identifier",
                "desc": "Optional unique identifier for the message",
            },
            "contextId": {
                "type": StringOrNothingType,
                "display_name": "Context ID",
                "short_desc": "Context identifier",
                "desc": "Optional context ID for grouping related tasks into a conversation",
            },
            "metadata": {
                "type": AutoHashOrNothingType,
                "display_name": "Metadata",
                "short_desc": "Message metadata",
                "desc": "Optional metadata for the message",
            },
            "configuration": {
                "type": AutoHashOrNothingType,
                "display_name": "Configuration",
                "short_desc": "Send message configuration (v1.0)",
                "desc": "Optional v1.0 configuration including `acceptedOutputModes`, "
                    "`historyLength`, `returnImmediately`",
            },
            "tenant": {
                "type": StringOrNothingType,
                "display_name": "Tenant",
                "short_desc": "Tenant identifier (v1.0)",
                "desc": "Optional tenant identifier for multi-tenancy support",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! A2A task get request data type
public class A2aTaskGetRequestDataType inherits HashDataType {
    public {
        #! Field descriptions
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "Task ID",
                "short_desc": "Task identifier",
                "desc": "The unique identifier of the task to retrieve",
                "example_value": "task-123",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! A2A task cancel request data type
public class A2aTaskCancelRequestDataType inherits HashDataType {
    public {
        #! Field descriptions
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "Task ID",
                "short_desc": "Task identifier",
                "desc": "The unique identifier of the task to cancel",
                "example_value": "task-123",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! A2A agent card result data type
public class A2aAgentCardResultDataType inherits HashDataType {
    public {
        #! Field descriptions
        const Fields = {
            "name": {
                "type": StringType,
                "display_name": "Name",
                "short_desc": "Agent name",
                "desc": "The name of the A2A agent",
            },
            "description": {
                "type": StringOrNothingType,
                "display_name": "Description",
                "short_desc": "Agent description",
                "desc": "A description of the A2A agent",
            },
            "url": {
                "type": UriType,
                "display_name": "URL",
                "short_desc": "Agent URL",
                "desc": "The URL of the A2A agent",
            },
            "version": {
                "type": StringOrNothingType,
                "display_name": "Version",
                "short_desc": "Agent version",
                "desc": "The version of the A2A agent",
            },
            "skills": {
                "type": AutoListOrNothingType,
                "display_name": "Skills",
                "short_desc": "Agent skills",
                "desc": "List of skills the agent supports",
            },
            "capabilities": {
                "type": AutoHashOrNothingType,
                "display_name": "Capabilities",
                "short_desc": "Agent capabilities",
                "desc": "The capabilities of the A2A agent (`streaming`, `pushNotifications`, etc.)",
            },
            "supportedInterfaces": {
                "type": AutoListOrNothingType,
                "display_name": "Supported Interfaces",
                "short_desc": "Protocol interfaces (v1.0)",
                "desc": "List of supported protocol interfaces with URL, binding, and version",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
        setDefaultOtherFieldType(AbstractDataProviderTypeMap."any");
    }
}

#! A2A task result data type
public class A2aTaskResultDataType inherits HashDataType {
    public {
        #! Field descriptions
        const Fields = {
            "id": {
                "type": StringType,
                "display_name": "Task ID",
                "short_desc": "Task identifier",
                "desc": "The unique identifier of the task",
            },
            "contextId": {
                "type": StringOrNothingType,
                "display_name": "Context ID",
                "short_desc": "Context identifier",
                "desc": "Context ID for grouping related tasks",
            },
            "status": {
                "type": AutoHashType,
                "display_name": "Status",
                "short_desc": "Task status",
                "desc": "The current status of the task including `state` and `timestamp`",
            },
            "history": {
                "type": AutoListOrNothingType,
                "display_name": "History",
                "short_desc": "Message history",
                "desc": "The message history of the task",
            },
            "artifacts": {
                "type": AutoListOrNothingType,
                "display_name": "Artifacts",
                "short_desc": "Task artifacts",
                "desc": "Artifacts produced by the task",
            },
            "metadata": {
                "type": AutoHashOrNothingType,
                "display_name": "Metadata",
                "short_desc": "Task metadata",
                "desc": "Optional metadata associated with the task",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
        setDefaultOtherFieldType(AbstractDataProviderTypeMap."any");
    }
}

#! A2A message send request data type constant
public const A2aMessageSendRequestDataType = new A2aMessageSendRequestDataType();

#! A2A task get request data type constant
public const A2aTaskGetRequestDataType = new A2aTaskGetRequestDataType();

#! A2A task cancel request data type constant
public const A2aTaskCancelRequestDataType = new A2aTaskCancelRequestDataType();

#! A2A agent card result data type constant
public const A2aAgentCardResultDataType = new A2aAgentCardResultDataType();

#! A2A task result data type constant
public const A2aTaskResultDataType = new A2aTaskResultDataType();
}
