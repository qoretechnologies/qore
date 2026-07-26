#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# A2A Client Compliance Test
#
# Tests our A2aClient against a known-good Python A2A reference server
# to validate cross-implementation interoperability.
#
# Usage:
#   qore test_a2a_client.q <server_url>
#
# Copyright (C) 2026 Qore Technologies, s.r.o.

%modern

%requires json
%requires A2aClient

sub main() {
    if (!ARGV[0]) {
        stderr.printf("Usage: %s <server_url>\n", get_script_name());
        exit(1);
    }

    string server_url = ARGV[0];
    int passed = 0;
    int failed = 0;
    list<string> failures = ();

    code test = sub (string name, code test_fn) {
        try {
            test_fn();
            stdout.printf("  [\033[92mPASS\033[0m] %s\n", name);
            ++passed;
        } catch (hash<ExceptionInfo> ex) {
            stdout.printf("  [\033[91mFAIL\033[0m] %s: %s: %s\n", name, ex.err, ex.desc);
            ++failed;
            failures += name;
        }
    };

    stdout.printf("\n%s\n", strmul("=", 60));
    stdout.printf("A2A Client Cross-Implementation Test\n");
    stdout.printf("Server: %s\n", server_url);
    stdout.printf("%s\n\n", strmul("=", 60));

    A2aClient::A2aClient client(server_url);
    # Force v0.3 for the base tests; v1.0 auto-detection is tested separately below
    client.setProtocolVersion("0.3");

    # Test 1: Agent Card Discovery
    stdout.printf("[Agent Card Discovery]\n");
    hash<auto> card;
    test("GET /.well-known/agent-card.json", sub () {
        card = client.getAgentCard();
        if (!card.name) {
            throw "ASSERTION-ERROR", "Agent card missing 'name' field";
        }
    });

    test("agent card has skills", sub () {
        if (!card.skills || !card.skills.size()) {
            throw "ASSERTION-ERROR", "Agent card has no skills";
        }
    });

    test("agent card has url", sub () {
        if (!card.url) {
            throw "ASSERTION-ERROR", "Agent card missing 'url' field";
        }
    });

    test("agent card caching", sub () {
        hash<auto> cached = client.getCachedAgentCard();
        if (cached.name != card.name) {
            throw "ASSERTION-ERROR", sprintf("Cached card name mismatch: %y != %y", cached.name, card.name);
        }
    });

    # Test 2: Message Send
    stdout.printf("\n[Message Send]\n");
    hash<auto> task;
    test("message/send basic", sub () {
        hash<auto> msg = {
            "role": "user",
            "parts": ({"type": "text", "text": "Hello, World!"},),
            "messageId": get_random_bytes(16).toHex(),
        };
        task = client.sendMessage(msg);
        if (!task.id) {
            throw "ASSERTION-ERROR", "Task response missing 'id' field";
        }
    });

    test("message/send response has status", sub () {
        if (!task.status) {
            throw "ASSERTION-ERROR", "Task response missing 'status' field";
        }
        if (!task.status.state) {
            throw "ASSERTION-ERROR", "Task status missing 'state' field";
        }
    });

    test("message/send response completed", sub () {
        if (task.status.state != "completed") {
            throw "ASSERTION-ERROR", sprintf("Expected completed state, got: %y", task.status.state);
        }
    });

    test("message/send response has agent message", sub () {
        # Check for agent message in status.message or history
        bool has_agent_msg = False;
        if (task.status.message && task.status.message.role == "agent") {
            has_agent_msg = True;
        }
        if (task.history) {
            foreach hash<auto> h in (task.history) {
                if (h.role == "agent") {
                    has_agent_msg = True;
                    break;
                }
            }
        }
        if (!has_agent_msg) {
            throw "ASSERTION-ERROR", "No agent message found in response";
        }
    });

    test("message/send echo content", sub () {
        # Find the agent's response text
        string agent_text = "";
        if (task.status.message && task.status.message.role == "agent") {
            foreach hash<auto> part in (task.status.message.parts) {
                if ((part.type ?? part.kind) == "text") {
                    agent_text += part.text;
                }
            }
        }
        if (!agent_text && task.history) {
            foreach hash<auto> h in (task.history) {
                if (h.role == "agent") {
                    foreach hash<auto> part in (h.parts) {
                        if ((part.type ?? part.kind) == "text") {
                            agent_text += part.text;
                        }
                    }
                    break;
                }
            }
        }
        if (!agent_text) {
            throw "ASSERTION-ERROR", "No text in agent response";
        }
        if (agent_text !~ /Hello/) {
            throw "ASSERTION-ERROR", sprintf("Expected echo of 'Hello', got: %y", agent_text);
        }
    });

    # Test 3: Task Get
    stdout.printf("\n[Task Retrieval]\n");
    test("tasks/get existing task", sub () {
        hash<auto> retrieved = client.getTask(task.id);
        if (retrieved.id != task.id) {
            throw "ASSERTION-ERROR", sprintf("Task ID mismatch: %y != %y", retrieved.id, task.id);
        }
    });

    test("tasks/get non-existent task", sub () {
        bool got_error = False;
        try {
            client.getTask("nonexistent-task-id-12345");
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "A2A-ERROR" || ex.err == "JSON-RPC-ERROR") {
                got_error = True;
            } else {
                rethrow;
            }
        }
        if (!got_error) {
            throw "ASSERTION-ERROR", "Expected error for non-existent task";
        }
    });

    # Test 4: Multiple Messages
    stdout.printf("\n[Multiple Messages]\n");
    test("send multiple messages", sub () {
        list<string> messages = ("First message", "Second message", "Third message");
        list<string> task_ids = ();
        foreach string msg_text in (messages) {
            hash<auto> msg = {
                "role": "user",
                "parts": ({"type": "text", "text": msg_text},),
                "messageId": get_random_bytes(16).toHex(),
            };
            hash<auto> t = client.sendMessage(msg);
            if (!t.id) {
                throw "ASSERTION-ERROR", sprintf("Missing task ID for message: %y", msg_text);
            }
            task_ids += t.id;
        }
        # Verify all task IDs are unique
        hash<string, bool> seen;
        foreach string tid in (task_ids) {
            if (seen{tid}) {
                throw "ASSERTION-ERROR", sprintf("Duplicate task ID: %y", tid);
            }
            seen{tid} = True;
        }
    });

    # Test 5: Message with Metadata
    stdout.printf("\n[Message with Metadata]\n");
    test("message/send with metadata", sub () {
        hash<auto> msg = {
            "role": "user",
            "parts": ({"type": "text", "text": "Test with metadata"},),
            "messageId": get_random_bytes(16).toHex(),
            "metadata": {"source": "integration-test", "priority": "high"},
        };
        hash<auto> t = client.sendMessage(msg);
        if (!t.id) {
            throw "ASSERTION-ERROR", "Missing task ID";
        }
        if (t.status.state != "completed") {
            throw "ASSERTION-ERROR", sprintf("Expected completed state, got: %y", t.status.state);
        }
    });

    # Test 6: v1.0 Auto-Detection
    # The Python reference server serves both v0.3 and v1.0 agent cards.
    # Our client should auto-detect v1.0 from /.well-known/a2a-agent-card
    stdout.printf("\n[v1.0 Auto-Detection]\n");
    A2aClient::A2aClient v10_client(server_url);
    test("v1.0 version auto-detected", sub () {
        hash<auto> v10_card = v10_client.getAgentCard();
        if (v10_client.getProtocolVersion() != "1.0") {
            throw "ASSERTION-ERROR", sprintf("Expected v1.0 detection, got: %y",
                v10_client.getProtocolVersion());
        }
        # Card should be in v0.3 internal format (translated from v1.0)
        if (!v10_card.url) {
            throw "ASSERTION-ERROR", "Agent card should have url (translated from supportedInterfaces)";
        }
    });

    test("v1.0 message/send round-trip", sub () {
        hash<auto> msg = {
            "role": "user",
            "parts": ({"type": "text", "text": "Hello v1.0!"},),
            "messageId": get_random_bytes(16).toHex(),
        };
        hash<auto> t = v10_client.sendMessage(msg);
        if (!t.id) {
            throw "ASSERTION-ERROR", "Missing task ID";
        }
        # Response should be in v0.3 internal format
        if (t.status.state != "completed") {
            throw "ASSERTION-ERROR", sprintf("Expected completed, got: %y", t.status.state);
        }
    });

    test("v1.0 tasks/get round-trip", sub () {
        hash<auto> msg = {
            "role": "user",
            "parts": ({"type": "text", "text": "get test"},),
            "messageId": get_random_bytes(16).toHex(),
        };
        hash<auto> t = v10_client.sendMessage(msg);
        hash<auto> fetched = v10_client.getTask(t.id);
        if (fetched.id != t.id) {
            throw "ASSERTION-ERROR", sprintf("Task ID mismatch: %y != %y", fetched.id, t.id);
        }
        if (fetched.status.state != "completed") {
            throw "ASSERTION-ERROR", sprintf("Expected completed, got: %y", fetched.status.state);
        }
    });

    test("v1.0 tasks/list round-trip", sub () {
        hash<auto> result = v10_client.listTasks();
        if (!result.hasKey("tasks")) {
            throw "ASSERTION-ERROR", "Missing tasks key in list response";
        }
    });

    test("v1.0 error handling", sub () {
        bool got_error = False;
        try {
            v10_client.getTask("nonexistent-task-v10");
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "A2A-ERROR" || ex.err == "JSON-RPC-ERROR") {
                got_error = True;
            } else {
                rethrow;
            }
        }
        if (!got_error) {
            throw "ASSERTION-ERROR", "Expected error for non-existent task";
        }
    });

    v10_client.close();

    # Test 7: SSE Streaming against Python reference server
    # Uses v0.3 protocol since the reference server is our primary independent server
    stdout.printf("\n[SSE Streaming (v0.3)]\n");
    A2aClient::A2aClient stream_client(server_url, {"timeout": 10000});
    stream_client.setProtocolVersion("0.3");

    test("sendMessageStream receives events", sub () {
        hash<auto> msg = {
            "role": "user",
            "parts": ({"type": "text", "text": "Hello streaming!"},),
            "messageId": get_random_bytes(16).toHex(),
        };
        list<hash<auto>> received_events = ();
        stream_client.sendMessageStream(msg, sub (string event_type, hash<auto> event_data) {
            received_events += {"type": event_type, "data": event_data};
        });
        if (!received_events.size()) {
            throw "ASSERTION-ERROR", "No SSE events received";
        }
    });

    test("sendMessageStream receives message events", sub () {
        hash<auto> msg = {
            "role": "user",
            "parts": ({"type": "text", "text": "Token test"},),
            "messageId": get_random_bytes(16).toHex(),
        };
        list<hash<auto>> received_events = ();
        stream_client.sendMessageStream(msg, sub (string event_type, hash<auto> event_data) {
            received_events += {"type": event_type, "data": event_data};
        });
        # Should have message events and a status event
        bool has_message = False;
        bool has_status = False;
        foreach hash<auto> evt in (received_events) {
            if (evt.type =~ /Message/) {
                has_message = True;
            }
            if (evt.type =~ /Status/) {
                has_status = True;
            }
        }
        if (!has_message) {
            throw "ASSERTION-ERROR", sprintf("No message events in: %y",
                (map $1.type, received_events));
        }
        if (!has_status) {
            throw "ASSERTION-ERROR", sprintf("No status events in: %y",
                (map $1.type, received_events));
        }
    });

    test("sendMessageStream event data has task ID", sub () {
        hash<auto> msg = {
            "role": "user",
            "parts": ({"type": "text", "text": "ID test"},),
            "messageId": get_random_bytes(16).toHex(),
        };
        list<hash<auto>> received_events = ();
        stream_client.sendMessageStream(msg, sub (string event_type, hash<auto> event_data) {
            received_events += {"type": event_type, "data": event_data};
        });
        # v0.3 events should have "id" in params
        foreach hash<auto> evt in (received_events) {
            if (evt.data.params && !evt.data.params.id) {
                throw "ASSERTION-ERROR", sprintf("Event missing task ID in params: %y", evt);
            }
        }
    });

    stream_client.close();

    # Test 8: SSE Streaming with v1.0 auto-detected server
    stdout.printf("\n[SSE Streaming (v1.0)]\n");
    A2aClient::A2aClient v10_stream_client(server_url, {"timeout": 10000});
    # Let it auto-detect v1.0 from the agent card
    v10_stream_client.getAgentCard();

    test("v1.0 sendMessageStream receives events", sub () {
        hash<auto> msg = {
            "role": "user",
            "parts": ({"type": "text", "text": "v1.0 streaming!"},),
            "messageId": get_random_bytes(16).toHex(),
        };
        list<hash<auto>> received_events = ();
        v10_stream_client.sendMessageStream(msg, sub (string event_type, hash<auto> event_data) {
            received_events += {"type": event_type, "data": event_data};
        });
        if (!received_events.size()) {
            throw "ASSERTION-ERROR", "No SSE events received for v1.0 streaming";
        }
        # Events should be translated to v0.3 format by the client
        bool has_message = False;
        foreach hash<auto> evt in (received_events) {
            if (evt.type =~ /Message/) {
                has_message = True;
            }
        }
        if (!has_message) {
            throw "ASSERTION-ERROR", sprintf("No message events in v1.0 stream: %y",
                (map $1.type, received_events));
        }
    });

    v10_stream_client.close();

    # Summary
    stdout.printf("\n%s\n", strmul("=", 60));
    stdout.printf("SUMMARY: %d/%d tests passed\n", passed, passed + failed);
    if (failures.size()) {
        stdout.printf("FAILURES:\n");
        foreach string f in (failures) {
            stdout.printf("  - %s\n", f);
        }
    }
    stdout.printf("%s\n", strmul("=", 60));

    exit(failed > 0 ? 1 : 0);
}

main();
