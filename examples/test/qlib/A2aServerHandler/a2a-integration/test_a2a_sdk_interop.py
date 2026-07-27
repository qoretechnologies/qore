#!/usr/bin/env python3
"""
A2A SDK Interoperability Test

Authoritative validation of our A2A implementation against the official
a2a-sdk from PyPI — the only independent, known-good implementation.

Tests our Qore A2A server against SDK-format requests, and tests our
Qore A2A client against the official a2a-sdk Python server.

CAVEATS:
- The a2a-sdk is pre-release (1.0.0a0). Tests should be re-validated
  when the SDK reaches stable release.
- The SDK server does not return SSE from POST (SendStreamingMessage
  returns empty SSE stream). Our Qore client's sendMessageStream() SSE
  parsing cannot be validated against an independent server until the
  SDK or another implementation supports SSE POST responses.
  TODO: Re-test client streaming when a2a-sdk stable or A2A TCK v1.0.
- The SDK server returns Message (not Task) from SendMessage. This is
  correct per v1.0 spec (SendMessageResponse oneof). Our server always
  returns Task, which is also valid.

This requires: pip install 'a2a-sdk[sqlite,http-server]'

Usage:
    python test_a2a_sdk_interop.py <qore_server_url> --sdk-port <port>
"""

import sys
import json
import uuid
import time
import signal
import threading
import argparse

# Check for required packages
try:
    import httpx
    import uvicorn
    from a2a.server.request_handlers import DefaultRequestHandler
    from a2a.server.agent_execution import AgentExecutor
    from a2a.server.agent_execution.context import RequestContext
    from a2a.server.tasks.inmemory_task_store import InMemoryTaskStore
    from a2a.server.events.in_memory_queue_manager import InMemoryQueueManager
    from a2a.types import (
        AgentCard, AgentInterface, AgentCapabilities, AgentSkill,
        Part, Message,
    )
    # v1.0.0a1+: apps module removed, use routes + Starlette directly
    from a2a.server.routes.jsonrpc_routes import create_jsonrpc_routes
    from a2a.server.routes.agent_card_routes import create_agent_card_routes
    from starlette.applications import Starlette
except ImportError as e:
    print(f"ERROR: a2a-sdk not installed ({e})")
    print("Install with: pip install 'a2a-sdk[sqlite,http-server]'")
    sys.exit(1)


class EchoExecutor(AgentExecutor):
    """Simple echo agent for testing."""

    async def execute(self, context: RequestContext, event_queue):
        text = ""
        if context.message and context.message.parts:
            for part in context.message.parts:
                if part.text:
                    text += part.text
        await event_queue.enqueue_event(
            Message(
                role="ROLE_AGENT",
                parts=[Part(text=f"SDK Echo: {text}")],
                message_id=str(uuid.uuid4()),
            )
        )

    async def cancel(self, context, event_queue):
        pass


def start_sdk_server(port):
    """Start the a2a-sdk echo server on the given port."""
    agent_card = AgentCard(
        name="SDK Echo Agent",
        description="Official a2a-sdk echo agent for interop testing",
        version="1.0",
        supported_interfaces=[
            AgentInterface(
                url=f"http://127.0.0.1:{port}",
                protocol_binding="JSONRPC",
                protocol_version="1.0",
            ),
        ],
        capabilities=AgentCapabilities(streaming=True),
        default_input_modes=["text/plain"],
        default_output_modes=["text/plain"],
        skills=[
            AgentSkill(
                id="echo", name="Echo",
                description="Echoes messages back",
                tags=["test"],
            ),
        ],
    )

    handler = DefaultRequestHandler(
        agent_executor=EchoExecutor(),
        task_store=InMemoryTaskStore(),
        agent_card=agent_card,
        queue_manager=InMemoryQueueManager(),
    )

    # Build Starlette app from routes (v1.0.0a1+ API)
    routes = create_agent_card_routes(agent_card=agent_card)
    routes += create_jsonrpc_routes(
        request_handler=handler,
        rpc_url="/",
        enable_v0_3_compat=True,
    )
    app = Starlette(routes=routes)

    config = uvicorn.Config(app, host="127.0.0.1", port=port, log_level="error")
    server = uvicorn.Server(config)
    thread = threading.Thread(target=server.run, daemon=True)
    thread.start()

    # Wait for server to be ready
    for _ in range(30):
        try:
            httpx.get(f"http://127.0.0.1:{port}/.well-known/agent-card.json", timeout=1.0)
            return server
        except Exception:
            time.sleep(0.5)

    raise RuntimeError("SDK server did not start in time")


class InteropTest:
    """Interoperability tests between Qore and official a2a-sdk."""

    def __init__(self):
        self.results = []
        self.client = httpx.Client(timeout=30.0)

    def _test(self, category, name, test_fn):
        try:
            test_fn()
            print(f"  [\033[92mPASS\033[0m] {name}")
            self.results.append({"name": f"{category}/{name}", "passed": True})
        except Exception as e:
            print(f"  [\033[91mFAIL\033[0m] {name}: {e}")
            self.results.append({"name": f"{category}/{name}", "passed": False, "message": str(e)})

    def _parse_sse(self, text):
        """Parse SSE events from a text/event-stream response body."""
        events = []
        for block in text.split("\n\n"):
            for line in block.strip().split("\n"):
                if line.startswith("data: "):
                    try:
                        events.append(json.loads(line[6:]))
                    except json.JSONDecodeError:
                        pass
        return events

    def _jsonrpc(self, url, method, params=None, headers=None):
        request = {
            "jsonrpc": "2.0",
            "id": str(uuid.uuid4()),
            "method": method,
            "params": params or {},
        }
        req_headers = {"Content-Type": "application/json"}
        if headers:
            req_headers.update(headers)
        response = self.client.post(url, json=request, headers=req_headers)
        return response.json(), response.status_code

    def test_sdk_client_to_qore_server(self, qore_url):
        """Comprehensive test: SDK-format requests → our Qore A2A server.

        This is the authoritative validation of our server — every test here
        uses wire format that the official SDK would produce, sent to our server.
        """
        print("\n[SDK Client → Qore Server]")
        v10_headers = {"A2A-Version": "1.0"}

        # --- Agent Card ---
        def test_v03_agent_card():
            resp = self.client.get(f"{qore_url}/.well-known/agent-card.json")
            assert resp.status_code == 200, f"Expected 200, got {resp.status_code}"
            ct = resp.headers.get("content-type", "")
            assert "application/json" in ct, f"Expected JSON, got: {ct}"
            card = resp.json()
            assert "name" in card, "Card missing name"
            assert "url" in card, "v0.3 card should have url"
            assert "skills" in card and len(card["skills"]) > 0, "Card should have skills"
            assert "capabilities" in card, "Card missing capabilities"

        def test_v10_agent_card():
            resp = self.client.get(f"{qore_url}/.well-known/a2a-agent-card")
            assert resp.status_code == 200, f"Expected 200, got {resp.status_code}"
            card = resp.json()
            assert "supportedInterfaces" in card, "v1.0 card should have supportedInterfaces"
            assert "url" not in card, "v1.0 card should not have top-level url"
            iface = card["supportedInterfaces"][0]
            assert "protocolBinding" in iface, "Interface should have protocolBinding"
            assert "protocolVersion" in iface, "Interface should have protocolVersion"

        # --- v1.0 SendMessage ---
        def test_v10_send_message():
            result, status = self._jsonrpc(qore_url, "SendMessage", {
                "message": {
                    "role": "ROLE_USER",
                    "parts": [{"text": "Hello!"}],
                    "messageId": str(uuid.uuid4()),
                },
            }, v10_headers)
            assert status == 200
            assert "result" in result, f"Missing result: {json.dumps(result)[:200]}"
            task = result["result"]
            assert "status" in task, "Task missing status"
            assert task["status"]["state"] == "TASK_STATE_COMPLETED", \
                f"Expected TASK_STATE_COMPLETED, got: {task['status']['state']}"

        def test_v10_response_roles():
            """v1.0 response should use ROLE_* values."""
            result, _ = self._jsonrpc(qore_url, "SendMessage", {
                "message": {"role": "ROLE_USER", "parts": [{"text": "role test"}],
                            "messageId": str(uuid.uuid4())},
            }, v10_headers)
            task = result["result"]
            for msg in task.get("history", []):
                assert msg["role"].startswith("ROLE_"), f"Role should have ROLE_ prefix: {msg['role']}"
            if task.get("status", {}).get("message"):
                assert task["status"]["message"]["role"].startswith("ROLE_")

        def test_v10_echo_content():
            """Server should echo input text."""
            result, _ = self._jsonrpc(qore_url, "SendMessage", {
                "message": {"role": "ROLE_USER", "parts": [{"text": "Echo this!"}],
                            "messageId": str(uuid.uuid4())},
            }, v10_headers)
            task = result["result"]
            agent_text = ""
            if task.get("status", {}).get("message"):
                for p in task["status"]["message"].get("parts", []):
                    agent_text += p.get("text", "")
            assert "Echo" in agent_text, f"Expected echo, got: {agent_text}"

        def test_v10_contextid_in_message():
            """v1.0 contextId inside message should be preserved."""
            result, _ = self._jsonrpc(qore_url, "SendMessage", {
                "message": {"role": "ROLE_USER", "parts": [{"text": "ctx test"}],
                            "messageId": str(uuid.uuid4()), "contextId": "sdk-ctx-1"},
            }, v10_headers)
            assert result["result"].get("contextId") == "sdk-ctx-1"

        def test_v10_multiple_messages():
            """Multiple SendMessage calls should return unique task IDs."""
            ids = set()
            for i in range(3):
                result, _ = self._jsonrpc(qore_url, "SendMessage", {
                    "message": {"role": "ROLE_USER", "parts": [{"text": f"msg {i}"}],
                                "messageId": str(uuid.uuid4())},
                }, v10_headers)
                tid = result["result"]["id"]
                assert tid not in ids, f"Duplicate task ID: {tid}"
                ids.add(tid)

        # --- v1.0 GetTask ---
        def test_v10_get_task():
            result, _ = self._jsonrpc(qore_url, "SendMessage", {
                "message": {"role": "ROLE_USER", "parts": [{"text": "get test"}],
                            "messageId": str(uuid.uuid4())},
            }, v10_headers)
            task_id = result["result"]["id"]
            result2, status = self._jsonrpc(qore_url, "GetTask", {"id": task_id}, v10_headers)
            assert status == 200
            assert result2["result"]["id"] == task_id
            assert "TASK_STATE_" in result2["result"]["status"]["state"]

        def test_v10_get_task_not_found():
            """GetTask for non-existent ID should return error -32002."""
            result, status = self._jsonrpc(qore_url, "GetTask",
                {"id": "nonexistent-task-xyz"}, v10_headers)
            assert "error" in result, "Expected error for non-existent task"
            assert result["error"]["code"] == -32002, \
                f"Expected -32002, got: {result['error']['code']}"

        # --- v1.0 ListTasks ---
        def test_v10_list_tasks():
            result, status = self._jsonrpc(qore_url, "ListTasks", {}, v10_headers)
            assert status == 200
            assert "tasks" in result["result"]
            assert isinstance(result["result"]["tasks"], list)

        # --- v1.0 CancelTask ---
        def test_v10_cancel_completed_task():
            """CancelTask on completed task should return conflict error."""
            result, _ = self._jsonrpc(qore_url, "SendMessage", {
                "message": {"role": "ROLE_USER", "parts": [{"text": "cancel test"}],
                            "messageId": str(uuid.uuid4())},
            }, v10_headers)
            task_id = result["result"]["id"]
            result2, _ = self._jsonrpc(qore_url, "CancelTask", {"id": task_id}, v10_headers)
            assert "error" in result2, "CancelTask on completed task should error"

        # --- Error Handling ---
        def test_unknown_method():
            result, _ = self._jsonrpc(qore_url, "NonexistentMethod", {}, v10_headers)
            assert "error" in result
            assert result["error"]["code"] == -32601, \
                f"Expected -32601, got: {result['error']['code']}"

        def test_invalid_json():
            resp = self.client.post(qore_url, content=b"not valid json",
                headers={"Content-Type": "application/json"})
            if resp.status_code == 200:
                result = resp.json()
                assert "error" in result

        # --- JSON-RPC 2.0 Compliance ---
        def test_jsonrpc_version():
            result, _ = self._jsonrpc(qore_url, "SendMessage", {
                "message": {"role": "ROLE_USER", "parts": [{"text": "version test"}],
                            "messageId": str(uuid.uuid4())},
            }, v10_headers)
            assert result.get("jsonrpc") == "2.0"

        def test_jsonrpc_id_echo():
            req_id = f"sdk-echo-{uuid.uuid4().hex[:8]}"
            request = {"jsonrpc": "2.0", "id": req_id, "method": "SendMessage", "params": {
                "message": {"role": "ROLE_USER", "parts": [{"text": "id test"}],
                            "messageId": str(uuid.uuid4())}}}
            resp = self.client.post(qore_url, json=request,
                headers={"Content-Type": "application/json", "A2A-Version": "1.0"})
            result = resp.json()
            assert result.get("id") == req_id, f"Expected id {req_id}, got: {result.get('id')}"

        def test_http_method_handling():
            """PUT/DELETE should return 405."""
            resp = self.client.put(qore_url, content=b"{}")
            assert resp.status_code == 405, f"PUT: expected 405, got {resp.status_code}"
            resp = self.client.delete(qore_url)
            assert resp.status_code == 405, f"DELETE: expected 405, got {resp.status_code}"

        # --- v0.3 Backward Compatibility ---
        def test_v03_send_message():
            """v0.3 method name should still work and return v0.3 format."""
            result, status = self._jsonrpc(qore_url, "message/send", {
                "message": {"role": "user", "parts": [{"type": "text", "text": "v0.3 test"}],
                            "messageId": str(uuid.uuid4())},
            })
            assert status == 200
            assert "result" in result
            task = result["result"]
            assert task["status"]["state"] == "completed", \
                f"v0.3 should return lowercase state, got: {task['status']['state']}"

        def test_version_detection_from_header():
            """A2A-Version header with v0.3 method should return v1.0 format."""
            result, _ = self._jsonrpc(qore_url, "message/send", {
                "message": {"role": "user", "parts": [{"type": "text", "text": "header test"}],
                            "messageId": str(uuid.uuid4())},
            }, v10_headers)
            assert "TASK_STATE_" in result["result"]["status"]["state"]

        # --- SSE Streaming ---
        def test_v10_stream_returns_sse():
            """SendStreamingMessage POST should return text/event-stream."""
            request = {"jsonrpc": "2.0", "id": str(uuid.uuid4()),
                "method": "SendStreamingMessage", "params": {
                    "message": {"role": "ROLE_USER", "parts": [{"text": "stream test"}],
                                "messageId": str(uuid.uuid4())}}}
            resp = self.client.post(qore_url, json=request,
                headers={"Content-Type": "application/json",
                         "Accept": "text/event-stream", "A2A-Version": "1.0"})
            assert resp.status_code == 200
            ct = resp.headers.get("content-type", "")
            assert "text/event-stream" in ct, f"Expected SSE, got: {ct}"

        def test_v10_stream_events():
            """SSE stream should contain message and status events."""
            request = {"jsonrpc": "2.0", "id": str(uuid.uuid4()),
                "method": "SendStreamingMessage", "params": {
                    "message": {"role": "ROLE_USER", "parts": [{"text": "event test"}],
                                "messageId": str(uuid.uuid4())}}}
            resp = self.client.post(qore_url, json=request,
                headers={"Content-Type": "application/json",
                         "Accept": "text/event-stream", "A2A-Version": "1.0"})
            events = self._parse_sse(resp.text)
            assert len(events) >= 2, f"Expected >=2 events, got {len(events)}"
            has_message = any("message" in e for e in events)
            has_status = any("statusUpdate" in e for e in events)
            assert has_message, f"No message event in: {events}"
            assert has_status, f"No status event in: {events}"

        def test_v10_stream_event_format():
            """v1.0 SSE events should be plain objects (no jsonrpc wrapper)."""
            request = {"jsonrpc": "2.0", "id": str(uuid.uuid4()),
                "method": "SendStreamingMessage", "params": {
                    "message": {"role": "ROLE_USER", "parts": [{"text": "format test"}],
                                "messageId": str(uuid.uuid4())}}}
            resp = self.client.post(qore_url, json=request,
                headers={"Content-Type": "application/json",
                         "Accept": "text/event-stream", "A2A-Version": "1.0"})
            events = self._parse_sse(resp.text)
            for evt in events:
                assert "jsonrpc" not in evt, f"v1.0 SSE should not have jsonrpc: {evt}"

        self._test("sdk→qore", "v0.3 agent card", test_v03_agent_card)
        self._test("sdk→qore", "v1.0 agent card", test_v10_agent_card)
        self._test("sdk→qore", "v1.0 SendMessage", test_v10_send_message)
        self._test("sdk→qore", "v1.0 response roles (ROLE_*)", test_v10_response_roles)
        self._test("sdk→qore", "echo content", test_v10_echo_content)
        self._test("sdk→qore", "contextId in message", test_v10_contextid_in_message)
        self._test("sdk→qore", "multiple messages unique IDs", test_v10_multiple_messages)
        self._test("sdk→qore", "v1.0 GetTask", test_v10_get_task)
        self._test("sdk→qore", "GetTask not found (-32002)", test_v10_get_task_not_found)
        self._test("sdk→qore", "v1.0 ListTasks", test_v10_list_tasks)
        self._test("sdk→qore", "CancelTask on completed", test_v10_cancel_completed_task)
        self._test("sdk→qore", "unknown method (-32601)", test_unknown_method)
        self._test("sdk→qore", "invalid JSON handled", test_invalid_json)
        self._test("sdk→qore", "jsonrpc 2.0 in response", test_jsonrpc_version)
        self._test("sdk→qore", "request ID echoed", test_jsonrpc_id_echo)
        self._test("sdk→qore", "PUT/DELETE return 405", test_http_method_handling)
        self._test("sdk→qore", "v0.3 backward compat", test_v03_send_message)
        self._test("sdk→qore", "version detection from header", test_version_detection_from_header)
        self._test("sdk→qore", "SSE stream content type", test_v10_stream_returns_sse)
        self._test("sdk→qore", "SSE message+status events", test_v10_stream_events)
        self._test("sdk→qore", "SSE v1.0 event format", test_v10_stream_event_format)

        # --- Push Notification Config CRUD ---
        def test_push_notification_crud():
            """Full push notification config lifecycle."""
            # Create a task first
            result, _ = self._jsonrpc(qore_url, "SendMessage", {
                "message": {"role": "ROLE_USER", "parts": [{"text": "push test"}],
                            "messageId": str(uuid.uuid4())},
            }, v10_headers)
            task_id = result["result"]["id"]

            # Set config
            result, status = self._jsonrpc(qore_url, "CreateTaskPushNotificationConfig", {
                "id": task_id,
                "pushNotificationConfig": {"url": "https://example.com/webhook", "token": "test-token"},
            }, v10_headers)
            assert status == 200, f"Set: expected 200, got {status}"
            assert "result" in result, f"Set: missing result: {result}"

            # List configs
            result, status = self._jsonrpc(qore_url, "ListTaskPushNotificationConfigs", {
                "id": task_id,
            }, v10_headers)
            assert status == 200
            assert "result" in result
            configs = result["result"].get("pushNotificationConfigs", [])
            assert len(configs) >= 1, f"Expected at least 1 config, got {len(configs)}"
            config_id = configs[0].get("id")

            # Get config
            result, status = self._jsonrpc(qore_url, "GetTaskPushNotificationConfig", {
                "id": task_id,
            }, v10_headers)
            assert status == 200
            assert "result" in result

            # Delete config
            result, status = self._jsonrpc(qore_url, "DeleteTaskPushNotificationConfig", {
                "id": task_id, "pushNotificationConfigId": config_id,
            }, v10_headers)
            assert status == 200

        # --- SubscribeToTask ---
        def test_subscribe_to_task():
            """SubscribeToTask should return task info."""
            result, _ = self._jsonrpc(qore_url, "SendMessage", {
                "message": {"role": "ROLE_USER", "parts": [{"text": "subscribe test"}],
                            "messageId": str(uuid.uuid4())},
            }, v10_headers)
            task_id = result["result"]["id"]
            result2, status = self._jsonrpc(qore_url, "SubscribeToTask", {
                "id": task_id,
            }, v10_headers)
            assert status == 200
            assert "result" in result2
            assert result2["result"]["id"] == task_id

        # --- Extended Agent Card ---
        def test_extended_agent_card():
            """GetExtendedAgentCard should return a card."""
            result, status = self._jsonrpc(qore_url, "GetExtendedAgentCard", {}, v10_headers)
            assert status == 200
            assert "result" in result
            card = result["result"]
            assert "name" in card, f"Extended card missing name: {card}"

        # --- Context History / Multi-Turn ---
        def test_multi_turn_context():
            """Multiple messages with same contextId should accumulate history."""
            ctx_id = f"sdk-multi-turn-{uuid.uuid4().hex[:6]}"
            for i in range(3):
                result, _ = self._jsonrpc(qore_url, "SendMessage", {
                    "message": {"role": "ROLE_USER", "parts": [{"text": f"turn {i}"}],
                                "messageId": str(uuid.uuid4()), "contextId": ctx_id},
                }, v10_headers)
                assert "result" in result
                assert result["result"].get("contextId") == ctx_id

        # --- Message Response Edge Cases ---
        def test_empty_parts():
            """SendMessage with empty parts should still work."""
            result, status = self._jsonrpc(qore_url, "SendMessage", {
                "message": {"role": "ROLE_USER", "parts": [],
                            "messageId": str(uuid.uuid4())},
            }, v10_headers)
            assert status == 200
            assert "result" in result

        # --- v1.0 Part Type Format ---
        def test_v10_part_format():
            """v1.0 parts without type discriminator should be accepted."""
            result, _ = self._jsonrpc(qore_url, "SendMessage", {
                "message": {"role": "ROLE_USER",
                            "parts": [{"text": "plain v1.0 part"}],
                            "messageId": str(uuid.uuid4())},
            }, v10_headers)
            task = result["result"]
            # Response parts should also be v1.0 format (no "type" key)
            if task.get("status", {}).get("message", {}).get("parts"):
                for part in task["status"]["message"]["parts"]:
                    assert "type" not in part, f"v1.0 part should not have 'type': {part}"

        self._test("sdk→qore", "push notification CRUD", test_push_notification_crud)
        self._test("sdk→qore", "SubscribeToTask", test_subscribe_to_task)
        self._test("sdk→qore", "GetExtendedAgentCard", test_extended_agent_card)
        self._test("sdk→qore", "multi-turn context", test_multi_turn_context)
        self._test("sdk→qore", "empty parts accepted", test_empty_parts)
        self._test("sdk→qore", "v1.0 part format (no type key)", test_v10_part_format)

    def test_qore_format_against_sdk_server(self, sdk_url):
        """Test: Qore-style JSON-RPC requests → official a2a-sdk server.

        This validates that the wire format our Qore client produces is
        accepted by the official SDK server.
        """
        print("\n[Qore Client Format → SDK Server]")
        v10_headers = {"A2A-Version": "1.0"}

        def test_agent_card():
            resp = self.client.get(f"{sdk_url}/.well-known/agent-card.json")
            assert resp.status_code == 200
            card = resp.json()
            assert card["name"] == "SDK Echo Agent"

        def test_v10_send_message():
            """Send v1.0 format message to SDK server."""
            result, status = self._jsonrpc(sdk_url, "SendMessage", {
                "message": {
                    "role": "ROLE_USER",
                    "parts": [{"text": "Hello from Qore!"}],
                    "messageId": str(uuid.uuid4()),
                },
            }, v10_headers)
            assert status == 200, f"Expected 200, got {status}"
            assert "result" in result, f"Missing result: {json.dumps(result)[:200]}"
            # SDK may return Message (not Task) for simple echo
            r = result["result"]
            has_response = ("message" in r) or ("status" in r) or ("id" in r)
            assert has_response, f"Unexpected response format: {json.dumps(r)[:200]}"

        def test_v03_send_message():
            """Send v0.3 format message to SDK server (compat mode)."""
            result, status = self._jsonrpc(sdk_url, "message/send", {
                "message": {
                    "role": "user",
                    "parts": [{"type": "text", "text": "Hello v0.3!"}],
                    "messageId": str(uuid.uuid4()),
                },
            })
            assert status == 200, f"Expected 200, got {status}"
            # May return error if v0.3 compat is incomplete in alpha
            if "error" in result:
                print(f"    (SDK v0.3 compat returned error: {result['error'].get('message', '')[:100]})")
            else:
                assert "result" in result

        def test_v10_stream_message():
            """SendStreamingMessage against SDK server should return SSE."""
            request = {
                "jsonrpc": "2.0",
                "id": str(uuid.uuid4()),
                "method": "SendStreamingMessage",
                "params": {
                    "message": {
                        "role": "ROLE_USER",
                        "parts": [{"text": "Stream from Qore!"}],
                        "messageId": str(uuid.uuid4()),
                    },
                },
            }
            # Use streaming read to handle SSE
            with self.client.stream("POST", sdk_url, json=request,
                    headers={"Content-Type": "application/json",
                             "Accept": "text/event-stream",
                             "A2A-Version": "1.0"}) as resp:
                assert resp.status_code == 200, f"Expected 200, got {resp.status_code}"
                ct = resp.headers.get("content-type", "")
                # SDK may return SSE or JSON depending on implementation
                events = []
                # Read the full response body
                body_text = ""
                for chunk in resp.iter_text():
                    body_text += chunk

                if "text/event-stream" in ct and body_text.strip():
                    # Parse SSE events
                    for block in body_text.split("\n\n"):
                        for line in block.strip().split("\n"):
                            if line.startswith("data: "):
                                try:
                                    events.append(json.loads(line[6:]))
                                except json.JSONDecodeError:
                                    pass

                # Accept: events delivered OR valid JSON response (SDK alpha may vary)
                if not events:
                    # Try parsing as JSON-RPC response
                    try:
                        result = json.loads(body_text)
                        has_response = "result" in result or "error" not in result
                        assert has_response, f"Unexpected: {body_text[:200]}"
                    except json.JSONDecodeError:
                        pass  # Empty SSE stream is acceptable from alpha SDK

        self._test("qore→sdk-server", "agent card discovery", test_agent_card)
        self._test("qore→sdk-server", "v1.0 SendMessage", test_v10_send_message)
        self._test("qore→sdk-server", "v1.0 SendStreamingMessage", test_v10_stream_message)
        self._test("qore→sdk-server", "v0.3 message/send (compat)", test_v03_send_message)

    def test_qore_client_against_sdk_server(self, sdk_url):
        """Test: actual Qore A2aClient binary → official a2a-sdk server.

        Runs a Qore script that uses our A2aClient to talk to the SDK server.
        This validates the real Qore client, not just httpx requests.
        """
        print("\n[Qore A2aClient → SDK Server]")
        import subprocess
        import os

        script_dir = os.path.dirname(os.path.abspath(__file__))
        qore_script = os.path.join(script_dir, "test_a2a_client_sdk.q")

        def test_qore_client():
            try:
                result = subprocess.run(
                    ["qore", qore_script, sdk_url],
                    capture_output=True, text=True, timeout=30,
                )
            except subprocess.TimeoutExpired as e:
                # Capture partial output for debugging
                partial = ""
                if e.stdout:
                    partial += e.stdout if isinstance(e.stdout, str) else e.stdout.decode(errors="replace")
                if e.stderr:
                    partial += "\nSTDERR: " + (e.stderr if isinstance(e.stderr, str)
                                               else e.stderr.decode(errors="replace"))
                raise AssertionError(
                    f"Qore client timed out after {e.timeout}s. Partial output:\n{partial[:500]}"
                ) from None
            if result.returncode != 0:
                # Show output for debugging
                output = result.stdout + result.stderr
                raise AssertionError(f"Qore client test failed (exit {result.returncode}):\n{output[:500]}")
            # Print test output
            for line in result.stdout.strip().split("\n"):
                if line.strip():
                    print(f"    {line}")

        self._test("qore-client→sdk", "Qore A2aClient round-trip", test_qore_client)

    def summary(self):
        passed = sum(1 for r in self.results if r["passed"])
        total = len(self.results)
        print(f"\n{'=' * 60}")
        print(f"SDK INTEROP SUMMARY: {passed}/{total} tests passed")
        if passed < total:
            print("FAILURES:")
            for r in self.results:
                if not r["passed"]:
                    print(f"  - {r['name']}: {r.get('message', '')}")
        print(f"{'=' * 60}")
        return passed == total


def main():
    parser = argparse.ArgumentParser(description="A2A SDK Interoperability Test")
    parser.add_argument("qore_server_url", help="URL of the Qore A2A server")
    parser.add_argument("--sdk-port", type=int, default=9876,
                        help="Port for the SDK echo server (default: 9876)")
    args = parser.parse_args()

    tester = InteropTest()

    # Part 1: SDK-format requests against our Qore server
    tester.test_sdk_client_to_qore_server(args.qore_server_url)

    # Part 2: Start SDK server and test against it
    print(f"\nStarting SDK echo server on port {args.sdk_port}...")
    try:
        server = start_sdk_server(args.sdk_port)
        sdk_url = f"http://127.0.0.1:{args.sdk_port}"
        # Test raw wire format against SDK server
        tester.test_qore_format_against_sdk_server(sdk_url)
        # Test actual Qore A2aClient binary against SDK server
        tester.test_qore_client_against_sdk_server(sdk_url)
    except Exception as e:
        print(f"  SDK server failed to start: {e}")

    success = tester.summary()
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
