#!/usr/bin/env python3
"""
A2A Protocol Compliance Test

Tests our Qore A2aServerHandler implementation against the A2A v0.3.0 specification
using a pure Python client (no SDK dependency, just httpx for HTTP requests).

This validates JSON-RPC method compliance, agent card discovery, task lifecycle,
error handling, and streaming support.

Usage:
    python test_a2a_compliance.py <server_url>
"""

import sys
import json
import uuid
import time
import traceback
import argparse

try:
    import httpx
except ImportError:
    print("ERROR: httpx not installed. Run: pip install httpx")
    sys.exit(2)


class A2AComplianceTest:
    """Tests A2A server compliance with v0.3.0 and v1.0 specifications."""

    def __init__(self, server_url, version="0.3"):
        self.server_url = server_url.rstrip("/")
        self.client = httpx.Client(timeout=30.0)
        self.results = []
        self.task_ids = []  # Track created tasks for later tests
        self.version = version

    def run_all_tests(self):
        """Run all compliance tests. Returns True if all pass."""
        print(f"\n{'=' * 60}")
        print(f"A2A Protocol Compliance Test (v{self.version})")
        print(f"Server: {self.server_url}")
        print(f"{'=' * 60}\n")

        self._run_agent_card_tests()
        self._run_message_send_tests()
        self._run_task_tests()
        self._run_error_handling_tests()
        self._run_jsonrpc_compliance_tests()
        self._run_streaming_tests()

        if self.version == "1.0":
            self._run_v10_tests()

        # Print summary
        passed = sum(1 for r in self.results if r["passed"])
        total = len(self.results)
        print(f"\n{'=' * 60}")
        print(f"SUMMARY: {passed}/{total} tests passed")
        if passed < total:
            print("FAILURES:")
            for r in self.results:
                if not r["passed"]:
                    print(f"  - {r['name']}: {r['message']}")
        print(f"{'=' * 60}")

        return passed == total

    def _test(self, category, name, test_fn):
        """Run a single test and record the result."""
        try:
            test_fn()
            print(f"  [\033[92mPASS\033[0m] {name}")
            self.results.append({"name": f"{category}/{name}", "passed": True, "message": "OK"})
        except Exception as e:
            print(f"  [\033[91mFAIL\033[0m] {name}: {e}")
            self.results.append({"name": f"{category}/{name}", "passed": False, "message": str(e)})

    def _jsonrpc(self, method, params=None, headers=None):
        """Send a JSON-RPC 2.0 request."""
        request = {
            "jsonrpc": "2.0",
            "id": str(uuid.uuid4()),
            "method": method,
            "params": params or {},
        }
        req_headers = {"Content-Type": "application/json"}
        if headers:
            req_headers.update(headers)
        response = self.client.post(
            self.server_url,
            json=request,
            headers=req_headers,
        )
        return response.json(), response.status_code

    # ================================================================
    # Agent Card Tests
    # ================================================================
    def _run_agent_card_tests(self):
        print("[Agent Card Discovery]")

        def test_agent_card_endpoint():
            resp = self.client.get(f"{self.server_url}/.well-known/agent-card.json")
            assert resp.status_code == 200, f"Expected 200, got {resp.status_code}"
            card = resp.json()
            assert "name" in card, "Agent card missing 'name'"
            assert "url" in card, "Agent card missing 'url'"
            self._agent_card = card

        def test_agent_card_content_type():
            resp = self.client.get(f"{self.server_url}/.well-known/agent-card.json")
            ct = resp.headers.get("content-type", "")
            assert "application/json" in ct, f"Expected application/json, got: {ct}"

        def test_agent_card_has_skills():
            assert "skills" in self._agent_card, "Agent card missing 'skills'"
            assert len(self._agent_card["skills"]) > 0, "Agent card has no skills"
            skill = self._agent_card["skills"][0]
            assert "id" in skill, "Skill missing 'id'"
            assert "name" in skill, "Skill missing 'name'"

        def test_agent_card_has_capabilities():
            assert "capabilities" in self._agent_card, "Agent card missing 'capabilities'"

        def test_agent_card_has_version():
            assert "version" in self._agent_card, "Agent card missing 'version'"

        self._test("agent-card", "GET /.well-known/agent-card.json", test_agent_card_endpoint)
        self._test("agent-card", "content-type is application/json", test_agent_card_content_type)
        self._test("agent-card", "has skills", test_agent_card_has_skills)
        self._test("agent-card", "has capabilities", test_agent_card_has_capabilities)
        self._test("agent-card", "has version", test_agent_card_has_version)

    # ================================================================
    # Message Send Tests
    # ================================================================
    def _run_message_send_tests(self):
        print("\n[Message Send]")

        def test_message_send_basic():
            result, status = self._jsonrpc("message/send", {
                "message": {
                    "role": "user",
                    "parts": [{"type": "text", "text": "Hello, A2A!"}],
                    "messageId": str(uuid.uuid4()),
                },
            })
            assert status == 200, f"Expected 200, got {status}"
            assert "result" in result, f"Response missing 'result': {result}"
            task = result["result"]
            assert "id" in task, "Task missing 'id'"
            assert "status" in task, "Task missing 'status'"
            assert "state" in task["status"], "Status missing 'state'"
            self.task_ids.append(task["id"])
            self._last_task = task

        def test_message_send_completed():
            task = self._last_task
            assert task["status"]["state"] == "completed", \
                f"Expected completed, got: {task['status']['state']}"

        def test_message_send_has_response():
            task = self._last_task
            # Agent response should be in status.message or history
            has_agent = False
            if task.get("status", {}).get("message", {}).get("role") == "agent":
                has_agent = True
            for h in task.get("history", []):
                if h.get("role") == "agent":
                    has_agent = True
                    break
            assert has_agent, "No agent message in response"

        def test_message_send_echo_content():
            task = self._last_task
            agent_text = self._get_agent_text(task)
            assert agent_text, "No text in agent response"
            assert "Hello" in agent_text or "hello" in agent_text.lower(), \
                f"Expected echo of input, got: {agent_text}"

        def test_message_send_multiple():
            ids = set()
            for i in range(3):
                result, _ = self._jsonrpc("message/send", {
                    "message": {
                        "role": "user",
                        "parts": [{"type": "text", "text": f"Message {i}"}],
                        "messageId": str(uuid.uuid4()),
                    },
                })
                task_id = result["result"]["id"]
                assert task_id not in ids, f"Duplicate task ID: {task_id}"
                ids.add(task_id)
                self.task_ids.append(task_id)

        def test_message_send_jsonrpc_format():
            """Verify response follows JSON-RPC 2.0 format."""
            result, _ = self._jsonrpc("message/send", {
                "message": {
                    "role": "user",
                    "parts": [{"type": "text", "text": "format test"}],
                    "messageId": str(uuid.uuid4()),
                },
            })
            assert result.get("jsonrpc") == "2.0", "Response missing jsonrpc: 2.0"
            assert "id" in result, "Response missing request id"
            assert "result" in result or "error" in result, "Response must have result or error"

        self._test("message", "message/send basic", test_message_send_basic)
        self._test("message", "response is completed", test_message_send_completed)
        self._test("message", "has agent response", test_message_send_has_response)
        self._test("message", "echo content correct", test_message_send_echo_content)
        self._test("message", "multiple messages unique IDs", test_message_send_multiple)
        self._test("message", "JSON-RPC 2.0 format", test_message_send_jsonrpc_format)

    # ================================================================
    # Task Tests
    # ================================================================
    def _run_task_tests(self):
        print("\n[Task Management]")

        def test_tasks_get():
            if not self.task_ids:
                raise Exception("No tasks created to test")
            result, status = self._jsonrpc("tasks/get", {"id": self.task_ids[0]})
            assert status == 200, f"Expected 200, got {status}"
            assert "result" in result, f"Missing result: {result}"
            task = result["result"]
            assert task["id"] == self.task_ids[0], "Task ID mismatch"
            assert "status" in task, "Task missing status"

        def test_tasks_get_not_found():
            result, status = self._jsonrpc("tasks/get", {"id": "nonexistent-task-xyz"})
            assert "error" in result, "Expected error for non-existent task"
            assert result["error"]["code"] == -32002, \
                f"Expected error code -32002, got: {result['error']['code']}"

        self._test("tasks", "tasks/get existing", test_tasks_get)
        self._test("tasks", "tasks/get not found returns -32002", test_tasks_get_not_found)

    # ================================================================
    # Error Handling Tests
    # ================================================================
    def _run_error_handling_tests(self):
        print("\n[Error Handling]")

        def test_unknown_method():
            result, status = self._jsonrpc("nonexistent/method", {})
            assert "error" in result, "Expected error for unknown method"
            # JSON-RPC spec: method not found is -32601
            assert result["error"]["code"] == -32601, \
                f"Expected -32601, got: {result['error']['code']}"

        def test_invalid_json():
            resp = self.client.post(
                self.server_url,
                content=b"not valid json {{{",
                headers={"Content-Type": "application/json"},
            )
            # Should return an error (parse error or bad request)
            if resp.status_code == 200:
                result = resp.json()
                assert "error" in result, "Expected error for invalid JSON"

        def test_missing_method():
            resp = self.client.post(
                self.server_url,
                json={"jsonrpc": "2.0", "id": "test-1", "params": {}},
                headers={"Content-Type": "application/json"},
            )
            if resp.status_code == 200:
                result = resp.json()
                # Should be method not found or invalid request
                assert "error" in result, "Expected error for missing method"

        self._test("errors", "unknown method returns -32601", test_unknown_method)
        self._test("errors", "invalid JSON handled", test_invalid_json)
        self._test("errors", "missing method handled", test_missing_method)

    # ================================================================
    # JSON-RPC Compliance Tests
    # ================================================================
    def _run_jsonrpc_compliance_tests(self):
        print("\n[JSON-RPC 2.0 Compliance]")

        def test_jsonrpc_version_in_response():
            result, _ = self._jsonrpc("message/send", {
                "message": {
                    "role": "user",
                    "parts": [{"type": "text", "text": "version test"}],
                    "messageId": str(uuid.uuid4()),
                },
            })
            assert result.get("jsonrpc") == "2.0", \
                f"Expected jsonrpc: 2.0, got: {result.get('jsonrpc')}"

        def test_jsonrpc_id_echoed():
            req_id = f"test-echo-{uuid.uuid4().hex[:8]}"
            request = {
                "jsonrpc": "2.0",
                "id": req_id,
                "method": "message/send",
                "params": {
                    "message": {
                        "role": "user",
                        "parts": [{"type": "text", "text": "id echo test"}],
                        "messageId": str(uuid.uuid4()),
                    },
                },
            }
            resp = self.client.post(
                self.server_url,
                json=request,
                headers={"Content-Type": "application/json"},
            )
            result = resp.json()
            assert result.get("id") == req_id, \
                f"Expected id {req_id}, got: {result.get('id')}"

        def test_http_method_handling():
            """PUT/DELETE should return 405."""
            resp = self.client.put(self.server_url, content=b"{}")
            assert resp.status_code == 405, f"PUT: expected 405, got {resp.status_code}"
            resp = self.client.delete(self.server_url)
            assert resp.status_code == 405, f"DELETE: expected 405, got {resp.status_code}"

        self._test("jsonrpc", "response includes jsonrpc: 2.0", test_jsonrpc_version_in_response)
        self._test("jsonrpc", "request ID echoed in response", test_jsonrpc_id_echoed)
        self._test("jsonrpc", "PUT/DELETE return 405", test_http_method_handling)

    # ================================================================
    # SSE Streaming Tests
    # ================================================================
    def _run_streaming_tests(self):
        print("\n[SSE Streaming]")

        # Use version-appropriate method name and headers
        stream_method = "SendStreamingMessage" if self.version == "1.0" else "message/stream"
        stream_headers = {"Content-Type": "application/json", "Accept": "text/event-stream"}
        if self.version == "1.0":
            stream_headers["A2A-Version"] = "1.0"

        def test_stream_response_type():
            """message/stream should return text/event-stream."""
            request = {
                "jsonrpc": "2.0",
                "id": str(uuid.uuid4()),
                "method": stream_method,
                "params": {
                    "message": {
                        "role": "user",
                        "parts": [{"type": "text", "text": "Stream test"}],
                        "messageId": str(uuid.uuid4()),
                    },
                },
            }
            with self.client.stream("POST", self.server_url,
                    json=request, headers=stream_headers) as resp:
                assert resp.status_code == 200, f"Expected 200, got {resp.status_code}"
                ct = resp.headers.get("content-type", "")
                assert "text/event-stream" in ct, f"Expected SSE content type, got: {ct}"

        def test_stream_receives_events():
            """message/stream should deliver SSE events with task data."""
            request = {
                "jsonrpc": "2.0",
                "id": str(uuid.uuid4()),
                "method": stream_method,
                "params": {
                    "message": {
                        "role": "user",
                        "parts": [{"type": "text", "text": "Hello streaming!"}],
                        "messageId": str(uuid.uuid4()),
                    },
                },
            }
            events = []
            with self.client.stream("POST", self.server_url,
                    json=request, headers=stream_headers) as resp:
                buffer = ""
                for chunk in resp.iter_text():
                    buffer += chunk
                    # Parse SSE events from buffer
                    while "\n\n" in buffer:
                        event_block, buffer = buffer.split("\n\n", 1)
                        data_line = None
                        for line in event_block.strip().split("\n"):
                            if line.startswith("data: "):
                                data_line = line[6:]
                        if data_line:
                            try:
                                events.append(json.loads(data_line))
                            except json.JSONDecodeError:
                                pass

            assert len(events) >= 2, f"Expected at least 2 SSE events, got {len(events)}"
            # At least one should be a message/token event
            # At least one should be a status/completion event
            has_message = False
            has_status = False
            for evt in events:
                if self.version == "1.0":
                    if "message" in evt:
                        has_message = True
                    if "statusUpdate" in evt:
                        has_status = True
                else:
                    method = evt.get("method", "")
                    if "Message" in method:
                        has_message = True
                    if "Status" in method:
                        has_status = True
            assert has_message, f"No message event found in: {events}"
            assert has_status, f"No status event found in: {events}"

        def test_stream_event_format():
            """SSE events should have correct format for the protocol version."""
            request = {
                "jsonrpc": "2.0",
                "id": str(uuid.uuid4()),
                "method": stream_method,
                "params": {
                    "message": {
                        "role": "user",
                        "parts": [{"type": "text", "text": "Format check"}],
                        "messageId": str(uuid.uuid4()),
                    },
                },
            }
            events = []
            with self.client.stream("POST", self.server_url,
                    json=request, headers=stream_headers) as resp:
                buffer = ""
                for chunk in resp.iter_text():
                    buffer += chunk
                    while "\n\n" in buffer:
                        event_block, buffer = buffer.split("\n\n", 1)
                        data_line = None
                        for line in event_block.strip().split("\n"):
                            if line.startswith("data: "):
                                data_line = line[6:]
                        if data_line:
                            try:
                                events.append(json.loads(data_line))
                            except json.JSONDecodeError:
                                pass

            assert len(events) > 0, "No events received"

            if self.version == "1.0":
                # v1.0 events should be plain discriminated objects (no jsonrpc wrapper)
                for evt in events:
                    assert "jsonrpc" not in evt, f"v1.0 event should not have jsonrpc: {evt}"
            else:
                # v0.3 events should be JSON-RPC wrapped
                for evt in events:
                    assert evt.get("jsonrpc") == "2.0", f"v0.3 event should have jsonrpc 2.0: {evt}"
                    assert "method" in evt, f"v0.3 event should have method: {evt}"

        self._test("streaming", "message/stream returns SSE", test_stream_response_type)
        self._test("streaming", "SSE events delivered", test_stream_receives_events)
        self._test("streaming", "SSE event format correct", test_stream_event_format)

    # ================================================================
    # V1.0-Specific Tests
    # ================================================================
    def _run_v10_tests(self):
        print("\n[A2A v1.0 Protocol]")
        v10_headers = {"A2A-Version": "1.0"}

        def test_v10_agent_card_path():
            resp = self.client.get(f"{self.server_url}/.well-known/a2a-agent-card")
            assert resp.status_code == 200, f"Expected 200, got {resp.status_code}"
            card = resp.json()
            assert "supportedInterfaces" in card, "v1.0 card should have supportedInterfaces"
            assert "url" not in card, "v1.0 card should not have top-level url"
            assert len(card["supportedInterfaces"]) > 0, "Should have at least one interface"
            iface = card["supportedInterfaces"][0]
            assert "protocolBinding" in iface, "Interface should have protocolBinding"
            assert "protocolVersion" in iface, "Interface should have protocolVersion"

        def test_v10_method_names():
            """SendMessage should be dispatched correctly with v1.0 wire format."""
            result, status = self._jsonrpc("SendMessage", {
                "message": {
                    "role": "ROLE_USER",
                    "parts": [{"text": "v1.0 method test"}],
                    "messageId": str(uuid.uuid4()),
                },
            }, v10_headers)
            assert status == 200, f"Expected 200, got {status}"
            assert "result" in result, f"Missing result: {result}"
            task = result["result"]
            assert "TASK_STATE_" in task["status"]["state"], \
                f"v1.0 state should have TASK_STATE_ prefix, got: {task['status']['state']}"

        def test_v10_state_values():
            """v1.0 response should use TASK_STATE_* enum values."""
            result, _ = self._jsonrpc("SendMessage", {
                "message": {
                    "role": "ROLE_USER",
                    "parts": [{"text": "state test"}],
                    "messageId": str(uuid.uuid4()),
                },
            }, v10_headers)
            task = result["result"]
            assert task["status"]["state"] == "TASK_STATE_COMPLETED", \
                f"Expected TASK_STATE_COMPLETED, got: {task['status']['state']}"

        def test_v10_role_values():
            """v1.0 response should use ROLE_* enum values."""
            result, _ = self._jsonrpc("SendMessage", {
                "message": {
                    "role": "ROLE_USER",
                    "parts": [{"text": "role test"}],
                    "messageId": str(uuid.uuid4()),
                },
            }, v10_headers)
            task = result["result"]
            # Check history roles
            for msg in task.get("history", []):
                assert msg["role"].startswith("ROLE_"), \
                    f"v1.0 role should have ROLE_ prefix, got: {msg['role']}"
            # Check status message role
            if task.get("status", {}).get("message"):
                assert task["status"]["message"]["role"].startswith("ROLE_"), \
                    f"Status message role should have ROLE_ prefix"

        def test_v10_get_task():
            """GetTask should work with v1.0 method name."""
            # First create a task
            result, _ = self._jsonrpc("SendMessage", {
                "message": {
                    "role": "ROLE_USER",
                    "parts": [{"text": "get task test"}],
                    "messageId": str(uuid.uuid4()),
                },
            }, v10_headers)
            task_id = result["result"]["id"]

            # Get it with v1.0 method
            result, status = self._jsonrpc("GetTask", {"id": task_id}, v10_headers)
            assert status == 200
            assert result["result"]["id"] == task_id
            assert "TASK_STATE_" in result["result"]["status"]["state"]

        def test_v10_list_tasks():
            """ListTasks should work with v1.0 method name."""
            result, status = self._jsonrpc("ListTasks", {}, v10_headers)
            assert status == 200
            assert "tasks" in result["result"]

        def test_v10_version_header_detection():
            """A2A-Version header with v0.3 method name should return v1.0 format."""
            result, _ = self._jsonrpc("message/send", {
                "message": {
                    "role": "user",
                    "parts": [{"type": "text", "text": "header detection test"}],
                    "messageId": str(uuid.uuid4()),
                },
            }, v10_headers)
            task = result["result"]
            assert "TASK_STATE_" in task["status"]["state"], \
                f"Header should trigger v1.0 format, got: {task['status']['state']}"

        def test_v10_contextid_in_message():
            """v1.0 SendMessage with contextId inside message should work."""
            result, _ = self._jsonrpc("SendMessage", {
                "message": {
                    "role": "ROLE_USER",
                    "parts": [{"text": "context test"}],
                    "messageId": str(uuid.uuid4()),
                    "contextId": "test-ctx-v10",
                },
            }, v10_headers)
            task = result["result"]
            assert task.get("contextId") == "test-ctx-v10", \
                f"contextId should be preserved, got: {task.get('contextId')}"

        self._test("v1.0", "agent card at /.well-known/a2a-agent-card", test_v10_agent_card_path)
        self._test("v1.0", "SendMessage dispatched", test_v10_method_names)
        self._test("v1.0", "TASK_STATE_* enum values", test_v10_state_values)
        self._test("v1.0", "ROLE_* enum values", test_v10_role_values)
        self._test("v1.0", "GetTask with v1.0 method", test_v10_get_task)
        self._test("v1.0", "ListTasks with v1.0 method", test_v10_list_tasks)
        self._test("v1.0", "A2A-Version header detection", test_v10_version_header_detection)
        self._test("v1.0", "contextId inside message", test_v10_contextid_in_message)

    # ================================================================
    # Helpers
    # ================================================================
    def _get_agent_text(self, task):
        """Extract agent response text from a task (handles both v0.3 and v1.0 formats)."""
        text = ""
        agent_roles = ("agent", "ROLE_AGENT")
        msg = task.get("status", {}).get("message")
        if msg and msg.get("role") in agent_roles:
            for part in msg.get("parts", []):
                # v0.3: {"type": "text", "text": "..."}, v1.0: {"text": "..."}
                if part.get("type") == "text" or part.get("kind") == "text" or (
                        "text" in part and "type" not in part):
                    text += part.get("text", "")
        if not text:
            for h in task.get("history", []):
                if h.get("role") in agent_roles:
                    for part in h.get("parts", []):
                        if part.get("type") == "text" or part.get("kind") == "text" or (
                                "text" in part and "type" not in part):
                            text += part.get("text", "")
                    break
        return text


def main():
    parser = argparse.ArgumentParser(description="A2A Protocol Compliance Test")
    parser.add_argument("server_url", help="URL of the A2A server")
    parser.add_argument("--version", choices=["0.3", "1.0", "both"], default="both",
                        help="Protocol version to test (default: both)")
    args = parser.parse_args()

    success = True
    if args.version in ("0.3", "both"):
        tester = A2AComplianceTest(args.server_url, version="0.3")
        if not tester.run_all_tests():
            success = False
    if args.version in ("1.0", "both"):
        tester = A2AComplianceTest(args.server_url, version="1.0")
        if not tester.run_all_tests():
            success = False
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
