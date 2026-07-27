#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# A2A Client test against official a2a-sdk server
#
# Tests our A2aClient against the official a2a-sdk Python server,
# validating cross-implementation interoperability.
#
# The SDK server returns Message (not Task) from SendMessage —
# this tests our client's ability to handle both response types.
#
# Usage:
#   qore test_a2a_client_sdk.q <sdk_server_url>

%modern

%requires json
%requires A2aClient

sub main() {
    if (!ARGV[0]) {
        stderr.printf("Usage: %s <sdk_server_url>\n", get_script_name());
        exit(1);
    }

    string server_url = ARGV[0];
    int passed = 0;
    int failed = 0;

    code test = sub (string name, code test_fn) {
        try {
            test_fn();
            stdout.printf("  [PASS] %s\n", name);
            ++passed;
        } catch (hash<ExceptionInfo> ex) {
            stdout.printf("  [FAIL] %s: %s: %s\n", name, ex.err, ex.desc);
            ++failed;
        }
    };

    stdout.printf("Qore A2aClient → SDK Server (%s)\n", server_url);

    A2aClient::A2aClient client(server_url, {"timeout": 10000, "connect_timeout": 5000});

    # Agent card discovery — SDK serves at /.well-known/agent-card.json
    test("agent card discovery", sub () {
        hash<auto> card = client.getAgentCard();
        if (!card.name) {
            throw "ASSERTION-ERROR", "Agent card missing name";
        }
    });

    # SendMessage — SDK returns Message (not Task)
    # Our client should handle this via the v1.0 SendMessageResponse oneof
    test("sendMessage returns response", sub () {
        hash<auto> msg = {
            "role": "user",
            "parts": ({"type": "text", "text": "Hello SDK!"},),
            "messageId": get_random_bytes(16).toHex(),
        };
        hash<auto> result = client.sendMessage(msg);
        # SDK returns a Message, which our client translates
        # Result should have either "role" (Message) or "id"+"status" (Task)
        if (!result.role && !result.id) {
            throw "ASSERTION-ERROR", sprintf("Unexpected response format: %y", keys result);
        }
    });

    # Verify the response content
    test("sendMessage echo content", sub () {
        hash<auto> msg = {
            "role": "user",
            "parts": ({"type": "text", "text": "Echo test!"},),
            "messageId": get_random_bytes(16).toHex(),
        };
        hash<auto> result = client.sendMessage(msg);
        # Find the response text (in Message or Task format)
        string response_text = "";
        if (result.parts) {
            foreach hash<auto> part in (result.parts) {
                if ((part.type ?? "") == "text" || part.hasKey("text")) {
                    response_text += part.text ?? "";
                }
            }
        }
        if (!response_text) {
            throw "ASSERTION-ERROR", sprintf("No text in response: %y", result);
        }
        if (response_text !~ /Echo/) {
            throw "ASSERTION-ERROR", sprintf("Expected echo, got: %y", response_text);
        }
    });

    # Multiple messages
    test("multiple sendMessage calls", sub () {
        for (int i = 0; i < 3; ++i) {
            hash<auto> msg = {
                "role": "user",
                "parts": ({"type": "text", "text": sprintf("Message %d", i)},),
                "messageId": get_random_bytes(16).toHex(),
            };
            hash<auto> result = client.sendMessage(msg);
            if (!result.role && !result.id) {
                throw "ASSERTION-ERROR", sprintf("Message %d failed: %y", i, keys result);
            }
        }
    });

    client.close();

    stdout.printf("Result: %d/%d passed\n", passed, passed + failed);
    exit(failed > 0 ? 1 : 0);
}

main();
