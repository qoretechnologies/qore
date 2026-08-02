#!/usr/bin/env python3
"""
Reference MCP server built with the official MCP Python SDK.

Used to validate the *Qore client* against an independent implementation: the Qore client
talks to this server exactly as it would to any third-party MCP server, so a misreading of
the specification on the Qore side cannot be masked by the same misreading on the other end.

The protocol revision spoken is whatever the installed SDK implements, so running this from
each of the era-specific virtual environments covers each supported revision in turn.

Usage:
    python sdk_reference_server.py --port-file <path> [--host 127.0.0.1] [--port 0]
"""

import argparse
import contextlib
import socket
import sys
import threading

# The SDK renamed its high-level server between majors: FastMCP in 1.x, MCPServer in 2.x.
# Both expose the same decorator surface used below.
try:
    from mcp.server.mcpserver import MCPServer as ServerClass
    SDK_MAJOR = 2
except ImportError:
    try:
        from mcp.server.fastmcp import FastMCP as ServerClass
        SDK_MAJOR = 1
    except ImportError as e:
        print(f"ERROR: MCP SDK not installed: {e}", file=sys.stderr)
        sys.exit(2)


def pick_port(host: str, port: int) -> int:
    """Reserves a port so the port file can be written before the server binds."""
    if port:
        return port
    with contextlib.closing(socket.socket(socket.AF_INET, socket.SOCK_STREAM)) as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind((host, 0))
        return s.getsockname()[1]


def build_server(host: str, port: int):
    # 1.x takes the bind address at construction; 2.x takes it at run() time
    if SDK_MAJOR == 1:
        mcp = ServerClass("SdkReferenceServer", host=host, port=port,
                          instructions="SDK reference server for Qore MCP client tests")
    else:
        mcp = ServerClass("SdkReferenceServer",
                          instructions="SDK reference server for Qore MCP client tests")

    @mcp.tool(description="Echoes the input message back")
    def echo(message: str) -> str:
        return message

    @mcp.tool(description="Adds two numbers")
    def add(a: int, b: int) -> int:
        return a + b

    @mcp.tool(description="Always fails")
    def failing_tool() -> str:
        raise ValueError("this tool always fails")

    @mcp.resource("test://static/hello", description="A static hello resource")
    def hello() -> str:
        return "Hello from the SDK reference server"

    @mcp.prompt(description="A greeting prompt")
    def greeting(name: str = "world") -> str:
        return f"Hello, {name}!"

    return mcp


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=0)
    parser.add_argument("--port-file", required=True,
                        help="file to write the bound port to once listening")
    parser.add_argument("--transport", default="streamable-http",
                        choices=("streamable-http", "sse"))
    args = parser.parse_args()

    port = pick_port(args.host, args.port)
    server = build_server(args.host, port)

    # publish the port only once the listener is actually up, so the harness never races
    def publish() -> None:
        deadline = threading.Event()
        while not deadline.wait(0.05):
            with contextlib.closing(socket.socket(socket.AF_INET, socket.SOCK_STREAM)) as s:
                if s.connect_ex((args.host, port)) == 0:
                    with open(args.port_file, "w") as f:
                        f.write(str(port))
                    return

    threading.Thread(target=publish, daemon=True).start()
    if SDK_MAJOR == 1:
        server.run(transport=args.transport)
    elif args.transport == "sse":
        server.run(transport="sse", host=args.host, port=port)
    else:
        server.run(transport="streamable-http", host=args.host, port=port)
    return 0


if __name__ == "__main__":
    sys.exit(main())
