#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# MCP Test Server for integration testing with official MCP clients

%modern

%requires json
%requires HttpServer
%requires Logger
%requires Mime
%requires McpServerHandler

class TestMcpServerHandler inherits McpServerHandler {
    constructor(LoggerInterface logger) : McpServerHandler(logger, new PermissiveAuthenticator()) {
        # Register test tools
        registerTool("echo", "Echoes the input back", NOTHING, \echoTool());
        registerTool("add", "Adds two numbers", NOTHING, \addTool());
        registerTool("slow_task", "A slow task for testing async/tasks", NOTHING, \slowTask());
        registerTool("failing_tool", "A tool that always fails", NOTHING, \failingTool());
        # tools that exercise the Multi Round-Trip Request pattern; with a handshake-based
        # client these become server-initiated requests on the event stream instead
        registerTool("confirm", "Asks the client to confirm an action", NOTHING, \confirmTool());
        registerTool("sampler", "Asks the client for an LLM completion", NOTHING, \samplerTool());

        # Register test resources
        registerResource("test://static/hello", "hello", "A static hello resource", "text/plain",
            \readHello());
        registerResource("test://static/data", "data", "A JSON data resource", "application/json",
            \readData());

        # Register test resource templates
        registerResourceTemplate("test://dynamic/{id}", "dynamic-resource",
            "Dynamic resource by ID", "application/json", \readDynamic());

        # Register test prompts
        registerPrompt("greeting", "A greeting prompt", (
            {"name": "name", "description": "Name to greet", "required": True},
        ), \greetingPrompt());
        registerPrompt("summarize", "Summarize content", (
            {"name": "content", "description": "Content to summarize", "required": True},
            {"name": "style", "description": "Summary style", "required": False},
        ), \summarizePrompt());

        setInstructions("Qore MCP integration test server");
    }

    private hash<auto> confirmTool(*hash<auto> args) {
        hash<auto> ctx = McpServerHandler::getToolCallContext();
        hash<auto> answers = ctx.handler.requestInput({
            "confirm": {
                "method": "elicitation/create",
                "params": {
                    "mode": "form",
                    "message": sprintf("Deploy %s?", args.build ?? "the build"),
                    "requestedSchema": {"type": "object", "properties": {}},
                },
            },
        }, {"build": args.build});
        return {
            "content": ({
                "type": "text",
                "text": sprintf("%s", answers.confirm.action),
            },),
        };
    }

    private hash<auto> samplerTool(*hash<auto> args) {
        hash<auto> ctx = McpServerHandler::getToolCallContext();
        hash<auto> answers = ctx.handler.requestInput({
            "llm": {
                "method": "sampling/createMessage",
                "params": {
                    "maxTokens": 32,
                    "messages": ({
                        "role": "user",
                        "content": {"type": "text", "text": "say something"},
                    },),
                },
            },
        }, {"asked": True});
        return {
            "content": ({
                "type": "text",
                "text": answers.llm.content.text ?? "",
            },),
        };
    }

    private hash<auto> echoTool(*hash<auto> args) {
        return {"echoed": args};
    }

    private hash<auto> addTool(*hash<auto> args) {
        return {"result": (args.a ?? 0) + (args.b ?? 0)};
    }

    private hash<auto> slowTask(*hash<auto> args) {
        int delay_ms = args.delay_ms ?? 500;
        usleep(delay_ms * 1000);
        return {"status": "completed", "delay_ms": delay_ms};
    }

    private hash<auto> failingTool(*hash<auto> args) {
        throw "TOOL-ERROR", "This tool always fails";
    }

    private string readHello(hash<auto> cx, string uri) {
        return "Hello from MCP test server!";
    }

    private hash<auto> readData(hash<auto> cx, string uri) {
        return {
            "version": "1.0",
            "features": ("tools", "resources", "prompts", "tasks"),
            "timestamp": now_us().format("YYYY-MM-DD HH:mm:SS"),
        };
    }

    private hash<auto> readDynamic(hash<auto> cx, string uri) {
        # Extract ID from URI
        *string id = (uri =~ x/test:\/\/dynamic\/(.+)/)[0];
        return {
            "id": id,
            "uri": uri,
            "generated": True,
        };
    }

    private list<hash<auto>> greetingPrompt(hash<auto> cx, *hash<auto> args) {
        string name = args.name ?? "World";
        return ({
            "role": "user",
            "content": {
                "type": "text",
                "text": sprintf("Hello, %s! How can I help you today?", name),
            },
        },);
    }

    private list<hash<auto>> summarizePrompt(hash<auto> cx, *hash<auto> args) {
        string style = args.style ?? "concise";
        return ({
            "role": "user",
            "content": {
                "type": "text",
                "text": sprintf("Please summarize the following in a %s style:\n\n%s", style, args.content),
            },
        },);
    }
}

sub main() {
    # Parse command line for port
    int port = 0;  # 0 means auto-assign
    if (ARGV[0]) {
        port = ARGV[0].toInt();
    }

    # Setup logging
    Logger logger("mcp-test-server", LoggerLevel::getLevelInfo());
    if (ENV.MCP_DEBUG) {
        logger.addAppender(new StdoutAppender());
    }

    # Create handler and server
    TestMcpServerHandler handler(logger);
    hash<HttpServerOptionInfo> http_opts = <HttpServerOptionInfo>{
        "logger": logger,
        "debug": True,
    };
    HttpServer server(http_opts);
    server.setHandler("mcp", "", MimeTypeJson, handler);
    server.setDefaultHandler("mcp", handler);

    # Add listener
    hash<auto> listener_info = server.addListener(<HttpListenerOptionInfo>{"service": port});
    int actual_port = listener_info.port;

    # Write port to a file if specified (primary method for test harness)
    if (ENV.MCP_PORT_FILE) {
        File f();
        f.open2(ENV.MCP_PORT_FILE, O_CREAT | O_WRONLY | O_TRUNC);
        f.printf("%d\n", actual_port);
        f.close();
    }

    # Also write port to stdout
    stderr.printf("MCP test server started on port %d\n", actual_port);
    stdout.printf("PORT=%d\n", actual_port);
    flush();

    # Run until terminated
    while (True) {
        sleep(1);
    }
}

# Call main
main();
