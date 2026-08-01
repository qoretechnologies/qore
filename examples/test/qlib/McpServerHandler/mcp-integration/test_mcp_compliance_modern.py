#!/usr/bin/env python3
"""
MCP Protocol Compliance Test - modern (stateless) protocol revisions

Validates the Qore McpServerHandler against the 2026-07-28 specification using the
official MCP Python SDK v2, which is an independent implementation of the same spec.
A shared misreading of the specification cannot pass this test the way it could pass a
Qore-client-against-Qore-server test.

Requirements:
    pip install -r requirements-modern.txt      # mcp 2.x

Usage:
    python test_mcp_compliance_modern.py <server_url>

Example:
    python test_mcp_compliance_modern.py http://localhost:8080
"""

import argparse
import asyncio
import sys
import traceback
from dataclasses import dataclass, field
from typing import Any

try:
    from mcp import Client
    from mcp.types import ElicitResult, CreateMessageResult, TextContent
except ImportError as e:
    print("ERROR: MCP SDK v2 not installed. Run: pip install -r requirements-modern.txt")
    print(f"Import error: {e}")
    sys.exit(2)


@dataclass
class Results:
    passed: list[str] = field(default_factory=list)
    failed: list[tuple[str, str]] = field(default_factory=list)

    def ok(self, name: str) -> None:
        self.passed.append(name)
        print(f"  PASS  {name}")

    def fail(self, name: str, message: str) -> None:
        self.failed.append((name, message))
        print(f"  FAIL  {name}: {message}")

    def check(self, name: str, condition: bool, detail: str = "") -> None:
        if condition:
            self.ok(name)
        else:
            self.fail(name, detail or "assertion failed")


async def elicitation_callback(context: Any, params: Any) -> ElicitResult:
    """Answers the server's elicitation requests so MRTR round trips can complete."""
    return ElicitResult(action="accept", content={})


async def sampling_callback(context: Any, params: Any) -> CreateMessageResult:
    """Answers the server's sampling requests."""
    return CreateMessageResult(
        role="assistant",
        content=TextContent(type="text", text="sampled"),
        model="test-model",
        stopReason="endTurn",
    )


async def run(url: str) -> Results:
    r = Results()

    # "auto" is the SDK's dual-era mode: probe with server/discover, fall back to the
    # initialize handshake.  Against our dual-era server it must select the modern era.
    async with Client(
        url,
        mode="auto",
        raise_exceptions=True,
        elicitation_callback=elicitation_callback,
        sampling_callback=sampling_callback,
    ) as client:
        print("\n[discovery]")
        version = client.protocol_version
        r.check("negotiated a modern protocol version", str(version).startswith("2026-"),
                f"negotiated {version!r}")
        r.check("server identified itself", bool(client.server_info),
                f"server_info={client.server_info!r}")
        r.check("server reported capabilities", client.server_capabilities is not None)
        r.check("server reported instructions", bool(client.instructions),
                f"instructions={client.instructions!r}")

        print("\n[tools]")
        tools = await client.list_tools()
        names = [t.name for t in tools.tools]
        r.check("tools/list returned tools", bool(names), f"names={names}")
        r.check("tools/list is deterministically ordered", names == sorted(names),
                f"names={names}")
        r.check("tools/list carries a freshness hint", tools.ttl_ms is not None,
                f"ttl_ms={tools.ttl_ms!r}")
        r.check("tools/list declares its cache scope",
                tools.cache_scope in ("public", "private"),
                f"cache_scope={tools.cache_scope!r}")
        r.check("tools/list result is discriminated", tools.result_type == "complete",
                f"result_type={tools.result_type!r}")

        if "echo" in names:
            result = await client.call_tool("echo", {"message": "hello"})
            r.check("tools/call returned content", bool(result.content))
            r.check("tools/call is not an error", not result.is_error)
            r.check("tools/call result is discriminated", result.result_type == "complete",
                    f"result_type={result.result_type!r}")

        print("\n[resources]")
        resources = await client.list_resources()
        uris = [str(res.uri) for res in resources.resources]
        r.check("resources/list returned resources", bool(uris), f"uris={uris}")
        if uris:
            content = await client.read_resource(uris[0])
            r.check("resources/read returned contents", bool(content.contents))
            r.check("resources/read declares its cache scope",
                    content.cache_scope in ("public", "private"),
                    f"cache_scope={content.cache_scope!r}")

        try:
            await client.read_resource("file:///definitely-not-a-resource")
            r.fail("reading an unknown resource fails", "no error raised")
        except Exception as e:  # noqa: BLE001 - any protocol error is acceptable here
            code = getattr(getattr(e, "error", None), "code", None)
            # 2026-07-28 replaced the reserved -32002 with the JSON-RPC standard -32602
            r.check("unknown resource reports invalid params, not the retired -32002",
                    code != -32002, f"code={code!r}")

        print("\n[prompts]")
        prompts = await client.list_prompts()
        pnames = [p.name for p in prompts.prompts]
        r.check("prompts/list returned prompts", bool(pnames), f"names={pnames}")
        if pnames:
            got = await client.get_prompt(pnames[0], {"name": "Alice"})
            r.check("prompts/get returned messages", bool(got.messages))

        print("\n[statelessness]")
        # every request is self-contained: repeated calls on the same client, and a
        # brand-new client with no handshake, must both work
        again = await client.list_tools()
        r.check("a repeated list is served identically",
                [t.name for t in again.tools] == names)

        async with Client(url, mode=str(version), raise_exceptions=True) as fresh:
            fresh_tools = await fresh.list_tools()
            r.check("a client that adopts the version directly needs no handshake",
                    [t.name for t in fresh_tools.tools] == names)

        print("\n[multi round-trip requests]")
        if "confirm" in names:
            try:
                result = await client.call_tool("confirm", {"build": "v9"})
                text = "".join(c.text for c in result.content if hasattr(c, "text"))
                r.check("an elicitation round trip completes transparently",
                        "accept" in text, f"result={text!r}")
            except Exception as e:  # noqa: BLE001
                r.fail("an elicitation round trip completes transparently", repr(e))
        else:
            print("  SKIP  no 'confirm' tool advertised")

        if "sampler" in names:
            try:
                result = await client.call_tool("sampler", {})
                text = "".join(c.text for c in result.content if hasattr(c, "text"))
                r.check("a sampling round trip completes transparently",
                        "sampled" in text, f"result={text!r}")
            except Exception as e:  # noqa: BLE001
                r.fail("a sampling round trip completes transparently", repr(e))
        else:
            print("  SKIP  no 'sampler' tool advertised")

        print("\n[subscriptions]")
        try:
            async with client.listen(tools_list_changed=True) as subscription:
                r.check("subscriptions/listen opened a stream", subscription is not None)
        except Exception as e:  # noqa: BLE001
            r.fail("subscriptions/listen opened a stream", repr(e))

        print("\n[removed methods]")
        # ping was removed in 2026-07-28; the server must not answer it
        try:
            await client.send_ping()
            r.fail("ping is not available to modern clients", "ping succeeded")
        except Exception:  # noqa: BLE001
            r.ok("ping is not available to modern clients")

    return r


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("url", help="the MCP server URL")
    args = parser.parse_args()

    print("=" * 60)
    print("MCP compliance test - modern protocol (SDK v2)")
    print(f"server: {args.url}")
    print("=" * 60)

    try:
        results = asyncio.run(run(args.url))
    except Exception:  # noqa: BLE001
        traceback.print_exc()
        return 1

    print("\n" + "=" * 60)
    print(f"passed: {len(results.passed)}   failed: {len(results.failed)}")
    for name, message in results.failed:
        print(f"  FAILED: {name}: {message}")
    print("=" * 60)
    return 1 if results.failed else 0


if __name__ == "__main__":
    sys.exit(main())
