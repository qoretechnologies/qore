#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# A2A Test Server for integration testing with the official A2A TCK and Python SDK clients
#
# Copyright (C) 2026 Qore Technologies, s.r.o.

%modern

%requires json
%requires HttpServer
%requires Logger
%requires Mime
%requires A2aServerHandler

class TestA2aServerHandler inherits A2aServerHandler {
    constructor(LoggerInterface logger, int port) : A2aServerHandler(logger, {
        "name": "Qore A2A Test Agent",
        "description": "A test agent for A2A protocol compliance testing",
        "url": sprintf("http://localhost:%d", port),
        "version": "1.0",
        "capabilities": <A2aAgentCapabilities>{
            "streaming": True,
            "pushNotifications": True,
        },
        "defaultInputModes": ("text", "text/plain"),
        "defaultOutputModes": ("text", "text/plain"),
        "skills": (<A2aAgentSkill>{
            "id": "echo",
            "name": "Echo",
            "description": "Echoes messages back to the sender",
            "tags": ("test", "echo"),
        }, <A2aAgentSkill>{
            "id": "hello",
            "name": "Hello World",
            "description": "Returns a greeting message",
            "tags": ("test", "greeting"),
        },),
    }) {
        setMessageHandler(\handleMessage());
        setStreamMessageHandler(\handleStreamMessage());
    }

    private auto handleMessage(hash<A2aTaskInfo> task_info, hash<auto> msg) {
        # Extract text from message parts
        string text = "";
        foreach hash<auto> part in (msg.parts) {
            if (part.type == "text" || part.kind == "text") {
                text += (part.text ?? "");
            }
        }
        return {
            "role": "agent",
            "parts": ({"type": "text", "text": "Echo: " + text},),
        };
    }

    private handleStreamMessage(hash<A2aTaskInfo> task_info, hash<auto> msg, code token_callback) {
        # Extract text from message parts
        string text = "";
        foreach hash<auto> part in (msg.parts) {
            if (part.type == "text" || part.kind == "text") {
                text += (part.text ?? "");
            }
        }

        # Stream the response token by token
        list<string> tokens = ("Echo", ": ", text);
        foreach string token in (tokens) {
            token_callback({
                "role": "agent",
                "parts": ({"type": "text", "text": token},),
            });
            usleep(50000);  # 50ms between tokens
        }
    }
}

sub main() {
    # Parse command line for port
    int port = 0;  # 0 means auto-assign
    if (ARGV[0]) {
        port = ARGV[0].toInt();
    }

    # Setup logging
    Logger logger("a2a-test-server", LoggerLevel::getLevelInfo());
    if (ENV.A2A_DEBUG) {
        logger.addAppender(new StdoutAppender());
    }

    # Create server first to get the port, then create handler with correct URL
    hash<HttpServerOptionInfo> http_opts = <HttpServerOptionInfo>{
        "logger": logger,
        "debug": True,
    };
    HttpServer server(http_opts);

    # Add listener to get actual port
    hash<auto> listener_info = server.addListener(<HttpListenerOptionInfo>{"service": port});
    int actual_port = listener_info.port;

    # Create handler with correct URL
    TestA2aServerHandler handler(logger, actual_port);
    server.setHandler("a2a", "", MimeTypeJson, handler);
    server.setDefaultHandler("a2a", handler);

    # Write port to a file if specified (primary method for test harness)
    if (ENV.A2A_PORT_FILE) {
        File f();
        f.open2(ENV.A2A_PORT_FILE, O_CREAT | O_WRONLY | O_TRUNC);
        f.printf("%d\n", actual_port);
        f.close();
    }

    # Also write port to stderr/stdout
    stderr.printf("A2A test server started on port %d\n", actual_port);
    stdout.printf("PORT=%d\n", actual_port);
    flush();

    # Run until terminated
    while (True) {
        sleep(1);
    }
}

# Call main
main();
