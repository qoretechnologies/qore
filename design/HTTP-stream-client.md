# HTTP Streaming Client APIs for MCP

This document describes the new HTTP streaming APIs added to Qore 2.3 for supporting the Model Context Protocol (MCP) and other streaming HTTP use cases like Server-Sent Events (SSE).

## Overview

MCP uses HTTP with Server-Sent Events (SSE) for streaming responses. The new APIs provide:

1. **`HTTPClient::sendAndStream()`** - Core C++ method that sends an HTTP request and returns headers without consuming the response body
2. **`HttpStreamClient` module** - Qore user module providing a convenient streaming API wrapper

## Core API: HTTPClient::sendAndStream()

### Method Signature

```qore
hash<auto> HTTPClient::sendAndStream(string method, *string path, *data body,
    *hash<auto> headers, *reference<hash<auto>> info)
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `method` | `string` | HTTP method (GET, POST, etc.) |
| `path` | `*string` | Request path (optional) |
| `body` | `*data` | Request body - string or binary (optional) |
| `headers` | `*hash<auto>` | Additional request headers (optional) |
| `info` | `*reference<hash<auto>>` | Reference to receive extended response info |

### Return Value

Returns a hash containing response metadata:
- `status_code`: HTTP status code (int)
- `status_message`: HTTP status message (string)
- `http_version`: HTTP version (string)
- `content-type`: Content-Type header value
- `content-encoding`: Content-Encoding header value
- `transfer-encoding`: Transfer-Encoding header value
- `content-length`: Content-Length header value

### Key Behavior

Unlike `HTTPClient::send()` with `getbody=False`, `sendAndStream()` **never** reads the response body regardless of content type. This enables streaming for any response, not just SSE.

### Low-Level Usage Example

```qore
%modern
%requires json

HTTPClient client({"url": "https://api.example.com"});

hash<auto> info;
hash<auto> resp = client.sendAndStream("POST", "/mcp/messages",
    make_json({"jsonrpc": "2.0", "method": "tools/list", "id": 1}),
    {"Content-Type": "application/json", "Accept": "text/event-stream"},
    \info);

if (resp.status_code != 200) {
    throw "HTTP-ERROR", sprintf("HTTP %d: %s", resp.status_code, resp.status_message);
}

# Check if SSE response
if (resp."content-type" =~ /text\/event-stream/) {
    # Read SSE events
    while (*hash<SseMessageInfo> evt = client.readServerSentEvent(resp."content-encoding", 30s)) {
        printf("Event: %s, Data: %s\n", evt.event, evt.data);
    }
} else {
    # Read chunked response
    while (hash<auto> chunk = client.readHTTPChunk(30s)) {
        if (!chunk.body) break;
        process(chunk.body);
    }
}

client.disconnect();
```

## HttpStreamClient Module

The `HttpStreamClient` module provides a higher-level API that wraps `sendAndStream()` with automatic decompression, timeout management, and lifecycle handling.

### Key Classes

#### HTTPResponseStreamInfo (hashdecl)

```qore
public hashdecl HTTPResponseStreamInfo {
    int status_code;           # HTTP status code
    string status_message;     # HTTP status message
    string http_version;       # HTTP version (e.g., "HTTP/1.1")
    hash<auto> headers;        # Response headers (lowercase keys)
    hash<auto> headers_raw;    # Raw headers (original case)
    *string content_type;      # Content-Type without parameters
    *string charset;           # Charset from Content-Type
    *string content_encoding;  # Content-Encoding value
    bool chunked;              # True if Transfer-Encoding is "chunked"
    *int content_length;       # Content-Length if known
}
```

#### HTTPResponseStream Class

The main class for streaming HTTP responses.

##### Constructor

```qore
constructor(HTTPClient http_client, hash<auto> response_info, timeout default_timeout = 30s)
```

##### Methods

| Method | Description |
|--------|-------------|
| `getInfo()` | Returns `HTTPResponseStreamInfo` with response metadata |
| `readServerSentEvent(*timeout)` | Reads next SSE event, returns `*hash<SseMessageInfo>` |
| `readChunk(*timeout, bool decompress = True)` | Reads next chunk with optional decompression |
| `readRawChunk(*timeout)` | Reads next chunk without decompression |
| `isEof()` | Returns True if stream is at end |
| `isClosed()` | Returns True if stream has been closed |
| `setReadTimeout(timeout)` | Sets default read timeout |
| `getReadTimeout()` | Gets current default timeout |
| `close()` | Closes stream and disconnects client |

#### sendAndStream() Function

Convenience function to send request and create stream:

```qore
public HTTPResponseStream sub sendAndStream(HTTPClient client, string method, *string path,
    *data body, *hash<auto> headers, *reference<hash<auto>> info, timeout default_timeout = 30s)
```

### Supported Compression

Automatic decompression is supported for:
- `gzip`
- `deflate`
- `bzip2`
- `br` (Brotli)
- `zstd`
- `lz4`

## MCP Client Usage Examples

### Basic MCP Request with SSE Response

```qore
%modern
%requires HttpStreamClient
%requires json

sub sendMcpRequest(HTTPClient client, hash<auto> request) {
    hash<auto> info;
    HTTPResponseStream stream = HttpStreamClient::sendAndStream(
        client, "POST", "/mcp/messages",
        make_json(request),
        {
            "Content-Type": "application/json",
            "Accept": "text/event-stream",
        },
        \info
    );

    hash<HttpStreamClient::HTTPResponseStreamInfo> resp = stream.getInfo();
    if (resp.status_code != 200) {
        stream.close();
        throw "MCP-ERROR", sprintf("HTTP %d: %s", resp.status_code, resp.status_message);
    }

    # Handle based on content type
    if (resp.content_type == "text/event-stream") {
        return readSseResponse(stream, request.id);
    } else {
        return readJsonResponse(stream);
    }
}

hash<auto> sub readSseResponse(HTTPResponseStream stream, auto request_id) {
    while (!stream.isEof()) {
        *hash<SseMessageInfo> evt = stream.readServerSentEvent(30s);
        if (!evt) {
            break;
        }

        hash<auto> msg = parse_json(evt.data);

        # Check for our response
        if (msg.id == request_id) {
            stream.close();
            return msg;
        }

        # Handle notifications
        if (!msg.hasKey("id")) {
            handleNotification(msg);
        }
    }
    stream.close();
    throw "MCP-ERROR", "No response received";
}

hash<auto> sub readJsonResponse(HTTPResponseStream stream) {
    binary body;
    while (*binary chunk = stream.readChunk(30s)) {
        body += chunk;
    }
    stream.close();
    return parse_json(body.toString());
}
```

### MCP Client Class Pattern

```qore
%modern
%requires HttpStreamClient
%requires json

public class McpClient {
    private {
        HTTPClient client;
        int next_id = 1;
    }

    constructor(string url) {
        client = new HTTPClient({
            "url": url,
            "headers": {
                "Accept": "text/event-stream",
            },
        });
    }

    # Initialize MCP session
    hash<auto> initialize(hash<auto> client_info) {
        return sendRequest("initialize", {
            "protocolVersion": "2024-11-05",
            "capabilities": {},
            "clientInfo": client_info,
        });
    }

    # List available tools
    hash<auto> listTools() {
        return sendRequest("tools/list");
    }

    # Call a tool
    hash<auto> callTool(string name, hash<auto> arguments) {
        return sendRequest("tools/call", {
            "name": name,
            "arguments": arguments,
        });
    }

    # List available prompts
    hash<auto> listPrompts() {
        return sendRequest("prompts/list");
    }

    # Get a prompt
    hash<auto> getPrompt(string name, *hash<auto> arguments) {
        return sendRequest("prompts/get", {
            "name": name,
            "arguments": arguments ?? {},
        });
    }

    # List available resources
    hash<auto> listResources() {
        return sendRequest("resources/list");
    }

    # Read a resource
    hash<auto> readResource(string uri) {
        return sendRequest("resources/read", {
            "uri": uri,
        });
    }

    private hash<auto> sendRequest(string method, *hash<auto> params) {
        int id = next_id++;
        hash<auto> request = {
            "jsonrpc": "2.0",
            "method": method,
            "id": id,
        };
        if (params) {
            request.params = params;
        }

        hash<auto> info;
        HTTPResponseStream stream = HttpStreamClient::sendAndStream(
            client, "POST", "/",
            make_json(request),
            {"Content-Type": "application/json"},
            \info
        );

        on_exit stream.close();

        hash<HttpStreamClient::HTTPResponseStreamInfo> resp = stream.getInfo();
        if (resp.status_code != 200) {
            throw "MCP-HTTP-ERROR", sprintf("HTTP %d: %s", resp.status_code, resp.status_message);
        }

        if (resp.content_type == "text/event-stream") {
            return readSseResponse(stream, id);
        }
        return readJsonResponse(stream);
    }

    private hash<auto> readSseResponse(HTTPResponseStream stream, int request_id) {
        while (!stream.isEof()) {
            *hash<SseMessageInfo> evt = stream.readServerSentEvent(30s);
            if (!evt) {
                break;
            }

            hash<auto> msg = parse_json(evt.data);

            # Return response matching our request ID
            if (msg.id == request_id) {
                if (msg.hasKey("error")) {
                    throw "MCP-ERROR", msg.error.message, msg.error;
                }
                return msg.result;
            }
        }
        throw "MCP-TIMEOUT", "No response received for request";
    }

    private hash<auto> readJsonResponse(HTTPResponseStream stream) {
        binary body;
        while (*binary chunk = stream.readChunk(30s)) {
            body += chunk;
        }
        hash<auto> msg = parse_json(body.toString());
        if (msg.hasKey("error")) {
            throw "MCP-ERROR", msg.error.message, msg.error;
        }
        return msg.result;
    }
}
```

### Usage Example

```qore
%modern
%requires McpClient

McpClient mcp("https://mcp-server.example.com");

# Initialize session
hash<auto> init_result = mcp.initialize({
    "name": "my-client",
    "version": "1.0.0",
});
printf("Server capabilities: %N\n", init_result.capabilities);

# List and call tools
hash<auto> tools = mcp.listTools();
for (hash<auto> tool in tools.tools) {
    printf("Tool: %s - %s\n", tool.name, tool.description);
}

# Call a tool
hash<auto> result = mcp.callTool("search", {"query": "Qore programming"});
printf("Result: %N\n", result);
```

## Thread Safety Considerations

The `sendAndStream()` function temporarily modifies the HTTPClient's `error_passthru` setting. If you're sharing an HTTPClient across threads, consider:

1. Using a dedicated HTTPClient instance per stream
2. Synchronizing access to the client
3. Using a connection pool pattern

## Error Handling

### HTTP Errors

Non-2xx responses are passed through (not thrown as exceptions) when using `sendAndStream()`. Check `status_code` in the response info:

```qore
hash<HttpStreamClient::HTTPResponseStreamInfo> resp = stream.getInfo();
if (resp.status_code >= 400) {
    # Read error body
    *binary body;
    while (*binary chunk = stream.readChunk(5s)) {
        body += chunk;
    }
    throw "HTTP-ERROR", sprintf("HTTP %d: %s", resp.status_code, body.toString());
}
```

### Timeout Handling

Read operations accept an optional timeout. On timeout, a `SOCKET-TIMEOUT` exception is thrown:

```qore
try {
    *hash<SseMessageInfo> evt = stream.readServerSentEvent(5s);
} catch (hash<ExceptionInfo> ex) {
    if (ex.err == "SOCKET-TIMEOUT") {
        # Handle timeout
    }
    rethrow;
}
```

### EOF Detection

Use `isEof()` to check for end of stream:

```qore
while (!stream.isEof()) {
    *binary chunk = stream.readChunk(30s);
    if (!chunk) {
        break;  # EOF or timeout
    }
    process(chunk);
}
```

## Comparison with Existing APIs

| Feature | `send(getbody=False)` | `sendAndStream()` |
|---------|----------------------|-------------------|
| SSE streaming | Yes | Yes |
| Non-SSE streaming | No (body auto-consumed) | Yes |
| Chunked non-SSE | No | Yes |
| Content-Length body | No | Yes |
| Error responses | Thrown as exceptions | Passed through |

## Files

| File | Description |
|------|-------------|
| `include/qore/QoreHttpClientObject.h` | C++ header with `sendAndStream()` declarations |
| `lib/QoreHttpClientObject.cpp` | C++ implementation |
| `lib/QC_HTTPClient.qpp` | Qore bindings for `HTTPClient::sendAndStream()` |
| `qlib/HttpStreamClient.qm` | User module with `HTTPResponseStream` class |
| `examples/test/qlib/HttpStreamClient/HttpStreamClient.qtest` | Unit tests |
