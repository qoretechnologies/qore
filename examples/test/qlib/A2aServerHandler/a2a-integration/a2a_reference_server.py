#!/usr/bin/env python3
"""
Minimal A2A Reference Server using the official a2a-sdk.

This server implements a simple echo agent for testing our Qore A2A client
against a known-good Python implementation.

Usage:
    python a2a_reference_server.py <port>
"""

import sys
import signal
import json
import uuid
from http.server import HTTPServer, BaseHTTPRequestHandler
from datetime import datetime, timezone

# v0.3 agent card served at /.well-known/agent-card.json
AGENT_CARD_V03 = {
    "name": "Python A2A Reference Agent",
    "description": "A reference A2A agent for cross-implementation testing",
    "url": "",  # Set dynamically
    "version": "1.0",
    "capabilities": {
        "streaming": True,
        "pushNotifications": False,
    },
    "defaultInputModes": ["text", "text/plain"],
    "defaultOutputModes": ["text", "text/plain"],
    "skills": [
        {
            "id": "echo",
            "name": "Echo",
            "description": "Echoes messages back to the sender",
            "tags": ["test", "echo"],
        },
        {
            "id": "hello",
            "name": "Hello World",
            "description": "Returns a greeting message",
            "tags": ["test", "greeting"],
        },
    ],
}

# v1.0 agent card served at /.well-known/a2a-agent-card
AGENT_CARD_V10 = {
    "name": "Python A2A Reference Agent",
    "description": "A reference A2A agent for cross-implementation testing",
    "version": "1.0",
    "supportedInterfaces": [
        {
            "url": "",  # Set dynamically
            "protocolBinding": "JSONRPC",
            "protocolVersion": "1.0",
        },
    ],
    "capabilities": {
        "streaming": True,
        "pushNotifications": False,
    },
    "defaultInputModes": ["text", "text/plain"],
    "defaultOutputModes": ["text", "text/plain"],
    "skills": [
        {
            "id": "echo",
            "name": "Echo",
            "description": "Echoes messages back to the sender",
            "tags": ["test", "echo"],
        },
        {
            "id": "hello",
            "name": "Hello World",
            "description": "Returns a greeting message",
            "tags": ["test", "greeting"],
        },
    ],
}

# State/role/method mappings for v1.0
STATE_V03_TO_V10 = {
    "pending": "TASK_STATE_SUBMITTED", "active": "TASK_STATE_WORKING",
    "input-required": "TASK_STATE_INPUT_REQUIRED", "auth-required": "TASK_STATE_AUTH_REQUIRED",
    "completed": "TASK_STATE_COMPLETED", "failed": "TASK_STATE_FAILED",
    "canceled": "TASK_STATE_CANCELED", "rejected": "TASK_STATE_REJECTED",
}
ROLE_V03_TO_V10 = {"user": "ROLE_USER", "agent": "ROLE_AGENT"}
METHOD_V10_TO_V03 = {
    "SendMessage": "message/send", "SendStreamingMessage": "message/stream",
    "GetTask": "tasks/get", "CancelTask": "tasks/cancel", "ListTasks": "tasks/list",
}


def translate_task_to_v10(task):
    """Translate a v0.3 task to v1.0 wire format."""
    import copy
    t = copy.deepcopy(task)
    if "status" in t and "state" in t["status"]:
        t["status"]["state"] = STATE_V03_TO_V10.get(t["status"]["state"], t["status"]["state"])
    if "status" in t and "message" in t.get("status", {}):
        msg = t["status"]["message"]
        if msg and "role" in msg:
            msg["role"] = ROLE_V03_TO_V10.get(msg["role"], msg["role"])
    for msg in t.get("history", []):
        if "role" in msg:
            msg["role"] = ROLE_V03_TO_V10.get(msg["role"], msg["role"])
    return t


def translate_send_params_from_v10(params):
    """Translate v1.0 SendMessage params to v0.3 internal format."""
    import copy
    p = copy.deepcopy(params)
    msg = p.get("message", {})
    # Move contextId from message to top level
    if "contextId" in msg:
        p["contextId"] = msg.pop("contextId")
    # Remove v1.0-specific fields
    p.pop("configuration", None)
    p.pop("tenant", None)
    return p

# In-memory task store
tasks = {}


def create_task(context_id=None, metadata=None):
    """Create a new task."""
    task_id = f"task-{uuid.uuid4().hex[:8]}"
    now = datetime.now(timezone.utc).isoformat()
    task = {
        "id": task_id,
        "contextId": context_id,
        "status": {
            "state": "completed",
            "timestamp": now,
        },
        "history": [],
        "artifacts": [],
        "metadata": metadata,
    }
    tasks[task_id] = task
    return task


def handle_message_send(params):
    """Handle message/send JSON-RPC method."""
    message = params.get("message", {})
    config = params.get("configuration", {})

    # Extract text from parts (handles both v0.3 and v1.0 formats)
    text_parts = []
    for part in message.get("parts", []):
        kind = part.get("type") or part.get("kind", "")
        if kind == "text":
            text_parts.append(part.get("text", ""))
        elif "text" in part and "type" not in part:
            # v1.0 format: {"text": "..."} without type discriminator
            text_parts.append(part.get("text", ""))

    input_text = "".join(text_parts) if text_parts else ""

    # Create task
    task = create_task(
        context_id=config.get("contextId") or params.get("contextId"),
        metadata=message.get("metadata"),
    )

    # Add user message to history
    task["history"] = [message]

    # Create agent response
    agent_message = {
        "role": "agent",
        "parts": [{"type": "text", "text": f"Echo: {input_text}"}],
        "messageId": str(uuid.uuid4()),
    }
    task["history"].append(agent_message)
    task["status"]["message"] = agent_message

    return task


def handle_message_stream_events(params, is_v10=False):
    """Generate SSE events for message/stream. Returns (task, events_list)."""
    message = params.get("message", {})

    # Extract text from parts (both v0.3 and v1.0 formats)
    text_parts = []
    for part in message.get("parts", []):
        kind = part.get("type") or part.get("kind", "")
        if kind == "text":
            text_parts.append(part.get("text", ""))
        elif "text" in part and "type" not in part:
            text_parts.append(part.get("text", ""))

    input_text = "".join(text_parts) if text_parts else ""

    # Create task
    task = create_task(
        context_id=params.get("contextId"),
        metadata=message.get("metadata"),
    )
    task_id = task["id"]
    task["status"]["state"] = "active" if not is_v10 else "TASK_STATE_WORKING"
    task["history"] = [message]

    # Generate streaming tokens
    tokens = ["Echo", ": ", input_text]
    events = []

    for token in tokens:
        if is_v10:
            # v1.0: plain discriminated object
            events.append(json.dumps({
                "message": {
                    "role": "ROLE_AGENT",
                    "parts": [{"text": token}],
                    "messageId": str(uuid.uuid4()),
                    "taskId": task_id,
                },
            }))
        else:
            # v0.3: JSON-RPC wrapped
            events.append(json.dumps({
                "jsonrpc": "2.0",
                "method": "TaskMessageUpdateEvent",
                "params": {
                    "id": task_id,
                    "role": "agent",
                    "parts": [{"type": "text", "text": token}],
                },
            }))

    # Final status event
    now = datetime.now(timezone.utc).isoformat()
    agent_message = {
        "role": "ROLE_AGENT" if is_v10 else "agent",
        "parts": [{"text": f"Echo: {input_text}"} if is_v10
                  else {"type": "text", "text": f"Echo: {input_text}"}],
        "messageId": str(uuid.uuid4()),
    }
    task["status"]["state"] = "TASK_STATE_COMPLETED" if is_v10 else "completed"
    task["status"]["timestamp"] = now
    task["status"]["message"] = agent_message
    task["history"].append(agent_message)

    if is_v10:
        events.append(json.dumps({
            "statusUpdate": {
                "taskId": task_id,
                "state": "TASK_STATE_COMPLETED",
                "timestamp": now,
            },
        }))
    else:
        events.append(json.dumps({
            "jsonrpc": "2.0",
            "method": "TaskStatusUpdateEvent",
            "params": {
                "id": task_id,
                "state": "completed",
                "timestamp": now,
            },
        }))

    return task, events


def handle_tasks_get(params):
    """Handle tasks/get JSON-RPC method."""
    task_id = params.get("id")
    if not task_id or task_id not in tasks:
        return None, {"code": -32002, "message": f"Task not found: {task_id}"}
    return tasks[task_id], None


def handle_tasks_cancel(params):
    """Handle tasks/cancel JSON-RPC method."""
    task_id = params.get("id")
    if not task_id or task_id not in tasks:
        return None, {"code": -32002, "message": f"Task not found: {task_id}"}

    task = tasks[task_id]
    state = task["status"]["state"]
    if state in ("completed", "failed", "canceled", "rejected"):
        return None, {"code": -32003, "message": f"Cannot cancel task in state: {state}"}

    task["status"]["state"] = "canceled"
    task["status"]["timestamp"] = datetime.now(timezone.utc).isoformat()
    return task, None


def handle_tasks_list(params):
    """Handle tasks/list JSON-RPC method."""
    context_id = params.get("contextId")
    result = list(tasks.values())
    if context_id:
        result = [t for t in result if t.get("contextId") == context_id]
    return {"tasks": result}, None


class A2ARequestHandler(BaseHTTPRequestHandler):
    """HTTP request handler for A2A protocol."""

    def log_message(self, format, *args):
        """Suppress default logging."""
        pass

    def do_GET(self):
        """Handle GET requests - agent card discovery."""
        if self.path == "/.well-known/a2a-agent-card":
            # v1.0 agent card path
            body = json.dumps(AGENT_CARD_V10).encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        elif self.path in ("/.well-known/agent-card.json", "/.well-known/agent.json"):
            # v0.3 agent card path
            body = json.dumps(AGENT_CARD_V03).encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        else:
            self.send_error(404)

    def do_POST(self):
        """Handle POST requests - JSON-RPC."""
        content_length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(content_length)

        try:
            request = json.loads(body)
        except json.JSONDecodeError:
            self._send_jsonrpc_error(None, -32700, "Parse error")
            return

        req_id = request.get("id")
        method = request.get("method")
        params = request.get("params", {})

        # Detect v1.0: from A2A-Version header or PascalCase method name
        a2a_version = self.headers.get("A2A-Version", "")
        is_v10 = a2a_version == "1.0" or method in METHOD_V10_TO_V03

        # Normalize v1.0 method names to v0.3 for dispatch
        dispatch_method = METHOD_V10_TO_V03.get(method, method)

        # Translate v1.0 params to v0.3 internal format
        if is_v10 and dispatch_method in ("message/send", "message/stream"):
            params = translate_send_params_from_v10(params)

        # Handle streaming separately — returns SSE instead of JSON-RPC
        if dispatch_method == "message/stream":
            self._handle_stream(req_id, params, is_v10)
            return

        result = None
        error = None

        if dispatch_method == "message/send":
            result = handle_message_send(params)
        elif dispatch_method == "tasks/get":
            result, error = handle_tasks_get(params)
        elif dispatch_method == "tasks/cancel":
            result, error = handle_tasks_cancel(params)
        elif dispatch_method == "tasks/list":
            result, error = handle_tasks_list(params)
        else:
            self._send_jsonrpc_error(req_id, -32601, f"Method not found: {method}")
            return

        if error:
            self._send_jsonrpc_error(req_id, error["code"], error["message"])
        else:
            # Translate response to v1.0 format if needed
            if is_v10 and isinstance(result, dict) and "status" in result:
                result = translate_task_to_v10(result)
            elif is_v10 and isinstance(result, dict) and "tasks" in result:
                result["tasks"] = [translate_task_to_v10(t) if isinstance(t, dict) and "status" in t
                                   else t for t in result["tasks"]]
            self._send_jsonrpc_result(req_id, result)

    def _send_jsonrpc_result(self, req_id, result):
        """Send a successful JSON-RPC response."""
        response = {
            "jsonrpc": "2.0",
            "id": req_id,
            "result": result,
        }
        body = json.dumps(response).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _handle_stream(self, req_id, params, is_v10):
        """Handle message/stream by returning SSE events."""
        import time as _time

        task, events = handle_message_stream_events(params, is_v10)

        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Cache-Control", "no-cache")
        self.send_header("Transfer-Encoding", "chunked")
        self.end_headers()

        def send_chunk(data_bytes):
            """Send a chunk in HTTP chunked transfer encoding."""
            self.wfile.write(f"{len(data_bytes):x}\r\n".encode("ascii"))
            self.wfile.write(data_bytes)
            self.wfile.write(b"\r\n")
            self.wfile.flush()

        for event_data in events:
            if is_v10:
                chunk = f"data: {event_data}\n\n".encode("utf-8")
            else:
                parsed = json.loads(event_data)
                event_type = parsed.get("method", "message")
                chunk = f"event: {event_type}\ndata: {event_data}\n\n".encode("utf-8")
            send_chunk(chunk)
            _time.sleep(0.05)  # 50ms between events

        # Send terminating chunk
        self.wfile.write(b"0\r\n\r\n")
        self.wfile.flush()

    def _send_jsonrpc_error(self, req_id, code, message):
        """Send a JSON-RPC error response."""
        response = {
            "jsonrpc": "2.0",
            "id": req_id,
            "error": {
                "code": code,
                "message": message,
            },
        }
        body = json.dumps(response).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


def main():
    if len(sys.argv) < 2:
        print("Usage: python a2a_reference_server.py <port>", file=sys.stderr)
        sys.exit(1)

    port = int(sys.argv[1])
    AGENT_CARD_V03["url"] = f"http://localhost:{port}"
    AGENT_CARD_V10["supportedInterfaces"][0]["url"] = f"http://localhost:{port}"

    server = HTTPServer(("127.0.0.1", port), A2ARequestHandler)
    signal.signal(signal.SIGTERM, lambda *_: sys.exit(0))

    # Write port file if specified
    import os
    port_file = os.environ.get("A2A_REF_PORT_FILE")
    if port_file:
        with open(port_file, "w") as f:
            f.write(f"{port}\n")

    print(f"A2A reference server started on port {port}", file=sys.stderr)
    print(f"PORT={port}")
    sys.stdout.flush()

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
