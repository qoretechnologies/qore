# AsyncAPI Schema Generation

This guide explains how to generate AsyncAPI 3.0.0 schemas from Qore source code using the `AsyncApi` module and the `qore-asyncapi-gen` command-line tool.

## Overview

The AsyncAPI schema generator allows you to document your event-driven APIs directly in your Qore source code using special `@EVENT` and `@WEBSOCKET` comment blocks. The generator parses these blocks and produces a complete AsyncAPI 3.0.0 specification in YAML or JSON format.

AsyncAPI is ideal for documenting:
- WebSocket event streaming endpoints
- Message queue systems (MQTT, Kafka, AMQP)
- Real-time notification systems
- Event-driven microservices

## @EVENT Block Format

Document events using the `@EVENT` block format:

```qore
/** @EVENT WORKFLOW_STATUS_CHANGED
    @EVENTSCHEMA
    @summary Workflow execution status changed
    @desc Fired when a workflow order's execution status changes.
          This event is critical for monitoring workflow progress.
    @class WORKFLOW
    @severity INFO
    @info
    - workflow_instanceid (int): Unique workflow instance ID
    - workflowid (int): Workflow definition ID
    - name (string): Workflow name
    - version (string): Workflow version
    - old_status (string): Previous status
    - new_status (string): New status
    - modified (date): Timestamp of the change
    @example
    {
      "workflow_instanceid": 12345,
      "workflowid": 42,
      "name": "Onboarding",
      "version": "1.0.0",
      "old_status": "PENDING",
      "new_status": "RUNNING",
      "modified": "2026-02-04T12:34:56Z"
    }
    @ENDEVENTSCHEMA
*/
```

### Event Block Structure

| Tag | Description | Required |
|-----|-------------|----------|
| `@EVENT` | Event name (e.g., `WORKFLOW_STATUS_CHANGED`) | Yes |
| `@EVENTSCHEMA` | Marks the start of event schema | Yes |
| `@summary` | Brief one-line description | No |
| `@desc` | Detailed description (can be multi-line) | No |
| `@class` | Event classification (e.g., WORKFLOW, SYSTEM, USER) | No |
| `@severity` | Event severity: INFO, WARNING, MINOR, MAJOR, FATAL | No |
| `@info` | Event payload field definitions | No |
| `@example` | JSON example payload (can appear multiple times) | No |
| `@ENDEVENTSCHEMA` | Marks the end of event schema | Yes |

### Event Severity Levels

| Severity | Description |
|----------|-------------|
| `INFO` | Informational event, normal operation |
| `WARNING` | Warning condition, may require attention |
| `MINOR` | Minor issue, limited impact |
| `MAJOR` | Major issue, significant impact |
| `FATAL` | Critical failure, immediate attention required |

## @WEBSOCKET Block Format

Document WebSocket channels using the `@WEBSOCKET` block format:

```qore
/** @WEBSOCKET /events
    @ASYNCAPI
    @summary Real-time event streaming endpoint
    @desc WebSocket endpoint for subscribing to and receiving real-time events.
          Clients can subscribe to specific event types or receive all events.
    @subscribe SubscribeMessage
    - action (string): Subscription action (subscribe, unsubscribe)
    - events (list<string>): List of event names to subscribe to
    - filter (*hash<auto>): Optional filter criteria
    @publish EventMessage
    - name (string): Event name
    - event_class (string): Event classification
    - severity (string): Event severity level
    - data (hash<auto>): Event payload
    - timestamp (date): Event timestamp
    @ENDASYNCAPI
*/
```

### Multiple Message Types Per Channel

A channel can define multiple `@subscribe` and/or `@publish` message types. This is useful for channels that handle different kinds of requests and responses:

```qore
/** @WEBSOCKET /api
    @ASYNCAPI
    @summary Multi-purpose API endpoint
    @desc WebSocket endpoint supporting multiple message types.

    @subscribe CommandMessage
    - action (string): Command action
    - command (string): Command to execute
    - args (*hash<auto>): Command arguments

    @subscribe QueryMessage
    - action (string): Query action
    - query (string): Query expression
    - limit (*int): Maximum results

    @subscribe PingMessage
    - action (string): Ping action
    - timestamp (date): Client timestamp

    @publish CommandResponse
    - ok (bool): Success status
    - result (*hash<auto>): Command result
    - error (*string): Error message if failed

    @publish QueryResponse
    - ok (bool): Success status
    - data (*list<hash<auto>>): Query results
    - total (int): Total matching records

    @publish PongMessage
    - ok (bool): Success status
    - server_time (date): Server timestamp
    - latency_ms (int): Round-trip latency

    @ENDASYNCAPI
*/
```

When parsing channels with multiple message types:
- `subscribe_schemas` list contains all `@subscribe` message schemas
- `publish_schemas` list contains all `@publish` message schemas
- For backwards compatibility, `subscribe_schema` and `publish_schema` contain the first message of each type
 - The generator emits component message schemas for all message types and includes them in the channel's `messages` map

### WebSocket Block Structure

| Tag | Description | Required |
|-----|-------------|----------|
| `@WEBSOCKET` | Channel path (e.g., `/events`) | Yes |
| `@ASYNCAPI` | Marks the start of AsyncAPI schema | Yes |
| `@summary` | Brief one-line description | No |
| `@desc` | Detailed description | No |
| `@subscribe` | Message schema for client-to-server messages (can appear multiple times) | No |
| `@publish` | Message schema for server-to-client messages (can appear multiple times) | No |
| `@example` | JSON example payload for the preceding message block (can appear multiple times) | No |
| `@ENDASYNCAPI` | Marks the end of AsyncAPI schema | Yes |

### Message Field Definitions

Fields are defined using the format:
```
- name (type): description
```

Nested fields are supported by indentation. Nested fields under arrays become the array's `items` schema, and nested
fields under objects become the object's `properties`:

```
@subscribe ExampleRequest
- action (string): Action name
- args (hash): Arguments
  - limit (int): Max results
  - filters (list): Filter list
    - value (string): Filter value
```

Examples:
```
@subscribe SubscribeRequest
- action (string): Action type (subscribe, unsubscribe, ping)
- channel (*string): Optional channel filter
- events (*list<string>): Optional list of event names
- options (*hash<auto>): Optional subscription options
@example
{
  "action": "subscribe",
  "channel": "events",
  "events": ["WORKFLOW_STATUS_CHANGED"],
  "options": {
    "key": "value"
  }
}

@publish EventNotification
- id (string): Unique event ID
- name (string): Event name
- payload (hash<auto>): Event data
- timestamp (date): Event timestamp
- correlation_id (*string): Optional correlation ID for request tracking
@example
{
  "id": "evt-12345",
  "name": "WORKFLOW_STATUS_CHANGED",
  "payload": {
    "key": "value"
  },
  "timestamp": "2026-02-04T12:34:56Z",
  "correlation_id": "corr-1"
}
```

## Type Specifications

The generator supports all Qore types, mapped to JSON Schema:

| Qore Type | JSON Schema Type | Format |
|-----------|------------------|--------|
| `string` | string | - |
| `int` | integer | - |
| `float` | number | - |
| `bool` | boolean | - |
| `date` | string | date-time |
| `binary` | string | binary |
| `hash` | object | - |
| `hash<auto>` | object (additionalProperties: true) | - |
| `hash<TypeName>` | $ref to schema | - |
| `list` | array | - |
| `list<T>` | array of T | - |
| `*type` | type (nullable: true) | - |

## Doxygen `@ref` Tags

Doxygen `@ref` tags are automatically resolved to plain text in all description fields (`@summary`, `@desc`,
and field descriptions in `@info`, `@subscribe`, and `@publish` blocks).

| Source | Generated Output |
|--------|------------------|
| `@ref OMQ::ES_Fatal "FATAL"` | `FATAL` |
| `@ref OMQ::StatError "ERROR"` | `ERROR` |
| `@ref OMQ::BatchMode` (no display text) | `BatchMode` |

Example:
```
- severity (string): error severity (@ref OMQ::ES_Fatal "FATAL", @ref OMQ::ES_Major "MAJOR")
```
Produces a field with description: `error severity (FATAL, MAJOR)`

## Command-Line Tool

### Basic Usage

```bash
qore-asyncapi-gen [options] <source-files...>
```

### Options

| Option | Description |
|--------|-------------|
| `-o, --output=FILE` | Output file (default: stdout) |
| `-f, --format=FORMAT` | Output format: `yaml` or `json` (default: yaml) |
| `-t, --title=TITLE` | API title (default: "Event API") |
| `-V, --api-version=VER` | API version (default: 1.0.0) |
| `-d, --description=DESC` | API description |
| `-s, --server=URL` | Server URL (can be specified multiple times) |
| `-c, --channel=PATH` | Channel path (can be specified multiple times) |
| `-v, --verbose` | Increase verbosity |
| `-h, --help` | Show help message |

### Examples

Generate schema from event definition files:
```bash
qore-asyncapi-gen -t "Qorus Events" -V "7.2.0" -o events.yaml src/events/*.qc
```

Generate JSON with server URLs:
```bash
qore-asyncapi-gen \
    -t "Real-time Events API" \
    -V "1.0.0" \
    -f json \
    -s "wss://api.example.com/ws" \
    -s "wss://staging.example.com/ws" \
    -o events.json \
    src/*.qclass
```

Specify default channels:
```bash
qore-asyncapi-gen \
    -t "Event Stream" \
    -c "/events" \
    -c "/admin/events" \
    -o stream-api.yaml \
    src/EventManager.qc
```

## Programmatic Usage

### Basic Example

```qore
%requires AsyncApi

# Create generator with API metadata
AsyncApi::AsyncApiSchemaGenerator gen({
    "title": "My Event API",
    "version": "1.0.0",
    "description": "Real-time event streaming API",
});

# Parse source file for events
string content = ReadOnlyFile::readTextFile("src/EventManager.qc");
list<hash<AsyncApi::EventSchemaInfo>> events =
    AsyncApi::AsyncApiSchemaTokenizer::parseEventBlocks(content);

# Add events to generator
foreach hash<AsyncApi::EventSchemaInfo> event in (events) {
    gen.addEvent(event);
}

# Parse channels
list<hash<AsyncApi::ChannelSchemaInfo>> channels =
    AsyncApi::AsyncApiSchemaTokenizer::parseChannelBlocks(content);

foreach hash<AsyncApi::ChannelSchemaInfo> channel in (channels) {
    gen.addChannel(channel);
}

# Add server
gen.addServer("production", {
    "host": "api.example.com:443",
    "protocol": "wss",
    "description": "Production WebSocket server",
});

# Generate YAML output
string yaml = gen.toYaml();
print(yaml);
```

### Processing Multiple Source Files

```qore
%requires AsyncApi

AsyncApi::AsyncApiSchemaGenerator gen({
    "title": "Complete Event API",
    "version": "2.0.0",
});

# Process all event definition files
list<string> files = glob("src/events/*.qc") + glob("src/handlers/*.qclass");

foreach string filepath in (files) {
    string content = ReadOnlyFile::readTextFile(filepath);

    # Parse and add events
    list<hash<AsyncApi::EventSchemaInfo>> events =
        AsyncApi::AsyncApiSchemaTokenizer::parseEventBlocks(content);
    foreach hash<AsyncApi::EventSchemaInfo> event in (events) {
        gen.addEvent(event);
    }

    # Parse and add channels
    list<hash<AsyncApi::ChannelSchemaInfo>> channels =
        AsyncApi::AsyncApiSchemaTokenizer::parseChannelBlocks(content);
    foreach hash<AsyncApi::ChannelSchemaInfo> channel in (channels) {
        gen.addChannel(channel);
    }
}

# Add default channel if none found
gen.addChannel(cast<hash<AsyncApi::ChannelSchemaInfo>>({
    "path": "/events",
    "summary": "Default event channel",
}));

# Write to file
File f();
f.open("asyncapi-spec.yaml", O_CREAT | O_WRONLY | O_TRUNC);
f.write(gen.toYaml());
f.close();
```

### Registering Custom Schemas

```qore
%requires AsyncApi

AsyncApi::AsyncApiSchemaGenerator gen({
    "title": "Custom Schema API",
    "version": "1.0.0",
});

# Register custom component schemas
gen.registerSchema("WorkflowInfo", {
    "type": "object",
    "properties": {
        "id": {"type": "integer"},
        "name": {"type": "string"},
        "version": {"type": "string"},
        "status": {
            "type": "string",
            "enum": ("PENDING", "RUNNING", "COMPLETE", "ERROR"),
        },
    },
    "required": ("id", "name", "status"),
});

gen.registerSchema("ErrorInfo", {
    "type": "object",
    "properties": {
        "code": {"type": "string"},
        "message": {"type": "string"},
        "details": {"type": "object"},
    },
    "required": ("code", "message"),
});
```

## Complete Source Code Example

Here's a complete example of documented event definitions:

```qore
/** @file EventDefinitions.qc
    Event definitions for the workflow management system
*/

#####################################
# Workflow Events
#####################################

/** @EVENT WORKFLOW_STARTED
    @EVENTSCHEMA
    @summary Workflow execution started
    @desc Fired when a new workflow order begins execution.
          Contains initial workflow state and configuration.
    @class WORKFLOW
    @severity INFO
    @info
    - workflow_instanceid (int): Unique instance identifier
    - workflowid (int): Workflow definition ID
    - name (string): Workflow name
    - version (string): Workflow version
    - started (date): Execution start time
    - priority (int): Execution priority
    - staticdata (*hash<auto>): Static workflow data
    @ENDEVENTSCHEMA
*/

/** @EVENT WORKFLOW_STATUS_CHANGED
    @EVENTSCHEMA
    @summary Workflow status changed
    @desc Fired when a workflow order transitions to a new status.
          Includes both old and new status for tracking state changes.
    @class WORKFLOW
    @severity INFO
    @info
    - workflow_instanceid (int): Workflow instance ID
    - workflowid (int): Workflow definition ID
    - name (string): Workflow name
    - old_status (string): Previous status
    - new_status (string): New status
    - modified (date): Status change timestamp
    - operator (*string): Operator who triggered the change
    @ENDEVENTSCHEMA
*/

/** @EVENT WORKFLOW_ERROR
    @EVENTSCHEMA
    @summary Workflow error occurred
    @desc Fired when a workflow encounters an error during execution.
          Contains error details and workflow state at time of failure.
    @class WORKFLOW
    @severity MAJOR
    @info
    - workflow_instanceid (int): Workflow instance ID
    - workflowid (int): Workflow definition ID
    - name (string): Workflow name
    - stepid (int): Step where error occurred
    - error_code (string): Error code
    - error_message (string): Error description
    - error_details (*hash<auto>): Additional error context
    - timestamp (date): Error timestamp
    @ENDEVENTSCHEMA
*/

/** @EVENT WORKFLOW_COMPLETE
    @EVENTSCHEMA
    @summary Workflow completed successfully
    @desc Fired when a workflow order completes all steps successfully.
    @class WORKFLOW
    @severity INFO
    @info
    - workflow_instanceid (int): Workflow instance ID
    - workflowid (int): Workflow definition ID
    - name (string): Workflow name
    - started (date): Start time
    - completed (date): Completion time
    - duration_ms (int): Total duration in milliseconds
    - result (*hash<auto>): Workflow result data
    @ENDEVENTSCHEMA
*/

#####################################
# System Events
#####################################

/** @EVENT SYSTEM_STARTUP
    @EVENTSCHEMA
    @summary System startup complete
    @desc Fired when the system completes initialization.
    @class SYSTEM
    @severity INFO
    @info
    - instance_key (string): Instance identifier
    - version (string): System version
    - startup_time (date): Startup timestamp
    - modules_loaded (list<string>): Loaded module names
    @ENDEVENTSCHEMA
*/

/** @EVENT SYSTEM_SHUTDOWN
    @EVENTSCHEMA
    @summary System shutdown initiated
    @desc Fired when the system begins graceful shutdown.
    @class SYSTEM
    @severity WARNING
    @info
    - instance_key (string): Instance identifier
    - reason (string): Shutdown reason
    - shutdown_time (date): Shutdown initiation time
    - graceful (bool): Whether shutdown is graceful
    @ENDEVENTSCHEMA
*/

#####################################
# WebSocket Channels
#####################################

/** @WEBSOCKET /events
    @ASYNCAPI
    @summary Main event streaming endpoint
    @desc Primary WebSocket endpoint for real-time event streaming.
          Supports subscription to specific event types and filtering.
    @subscribe SubscribeMessage
    - action (string): Action type (subscribe, unsubscribe, ping)
    - events (*list<string>): Event names to subscribe to (empty = all)
    - filter (*hash<auto>): Filter criteria for events
    @publish EventMessage
    - name (string): Event name
    - event_class (string): Event classification
    - severity (string): Event severity
    - info (hash<auto>): Event payload
    - timestamp (date): Event timestamp
    @ENDASYNCAPI
*/

/** @WEBSOCKET /admin/events
    @ASYNCAPI
    @summary Administrative event stream
    @desc Privileged endpoint for system administrators.
          Includes all events plus administrative notifications.
    @subscribe AdminSubscribeMessage
    - action (string): Action type
    - include_system (bool): Include system-level events
    - include_debug (*bool): Include debug events
    @publish AdminEventMessage
    - name (string): Event name
    - event_class (string): Event classification
    - severity (string): Event severity
    - info (hash<auto>): Event payload
    - timestamp (date): Event timestamp
    - internal (*hash<auto>): Internal diagnostic data
    @ENDASYNCAPI
*/
```

Generate the schema:
```bash
qore-asyncapi-gen \
    -t "Workflow Event API" \
    -V "7.2.0" \
    -d "Real-time event streaming for workflow management" \
    -s "wss://qorus.example.com/events" \
    -o workflow-events.yaml \
    EventDefinitions.qc
```

## Generated Output Example

The above definitions produce an AsyncAPI 3.0.0 specification like:

```yaml
asyncapi: "3.0.0"
info:
  title: "Workflow Event API"
  version: "7.2.0"
  description: "Real-time event streaming for workflow management"
defaultContentType: "application/json"

servers:
  production:
    host: "qorus.example.com"
    protocol: "wss"

channels:
  events:
    address: "/events"
    description: "Primary WebSocket endpoint for real-time event streaming."
    messages:
      subscribe:
        $ref: "#/components/messages/SubscribeMessage"
      event:
        $ref: "#/components/messages/EventMessage"

operations:
  subscribeToEvents:
    action: send
    channel:
      $ref: "#/channels/events"
    summary: "Subscribe to event stream"
    messages:
      - $ref: "#/channels/events/messages/subscribe"
  receiveEvents:
    action: receive
    channel:
      $ref: "#/channels/events"
    summary: "Receive event notifications"
    messages:
      - $ref: "#/channels/events/messages/event"

components:
  messages:
    SubscribeMessage:
      name: subscribe
      title: "Subscribe to Events"
      contentType: "application/json"
      payload:
        $ref: "#/components/schemas/SubscribePayload"
    EventMessage:
      name: event
      title: "Event Notification"
      contentType: "application/json"
      payload:
        $ref: "#/components/schemas/EventPayload"

  schemas:
    SubscribePayload:
      type: object
      properties:
        action:
          type: string
          description: "Subscription action"
        events:
          type: array
          items:
            type: string
          description: "Event names to subscribe to"

    EventPayload:
      type: object
      properties:
        name:
          type: string
          description: "Event name"
        event_class:
          type: string
          enum: ["WORKFLOW", "SYSTEM", "SERVICE", "JOB", "USER"]
          description: "Event classification"
        severity:
          type: string
          enum: ["INFO", "WARNING", "MINOR", "MAJOR", "FATAL"]
          description: "Event severity"
        info:
          type: object
          description: "Event-specific payload"

    WORKFLOW_STARTEDInfo:
      type: object
      description: "Info payload for WORKFLOW_STARTED event"
      properties:
        workflow_instanceid:
          type: integer
          description: "Unique instance identifier"
        workflowid:
          type: integer
          description: "Workflow definition ID"
        name:
          type: string
          description: "Workflow name"
        # ... additional properties
```

## Best Practices

1. **Categorize events** - Use `@class` to group related events (WORKFLOW, SYSTEM, USER, etc.).

2. **Set appropriate severity** - Use severity levels consistently:
   - `INFO` for normal operations
   - `WARNING` for potential issues
   - `MAJOR`/`FATAL` for errors requiring attention

3. **Document all payload fields** - Include descriptions for every field in `@info` sections.

4. **Use consistent naming** - Follow a naming convention for events (e.g., `ENTITY_ACTION` pattern).

5. **Include timestamps** - Always include a timestamp field in event payloads.

6. **Document channels** - Describe the purpose and security requirements of each WebSocket endpoint.

7. **Version your API** - Keep the API version updated as you add new events or channels.

8. **Keep events focused** - Each event should represent a single, atomic occurrence.
