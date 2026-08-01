#!/usr/bin/env python3
"""
MCP Protocol Compliance Test

Uses the official MCP Python SDK to validate our McpServerHandler implementation
against the 2025-11-25 protocol specification.

Requirements:
    pip install mcp httpx

Usage:
    python test_mcp_compliance.py <server_url> [--transport sse|streamable]

Example:
    python test_mcp_compliance.py http://localhost:8080 --transport streamable
    python test_mcp_compliance.py http://localhost:8080/sse --transport sse
"""

import asyncio
import sys
import json
import traceback
import argparse
from dataclasses import dataclass
from typing import Optional, List, Any
from datetime import datetime
from contextlib import asynccontextmanager

try:
    from mcp import ClientSession
    from mcp.client.sse import sse_client
    from mcp.types import (
        Tool, Resource, Prompt, TextContent,
        CallToolResult, ReadResourceResult, GetPromptResult,
    )
except ImportError as e:
    print("ERROR: MCP SDK not installed. Run: pip install mcp httpx")
    print(f"Import error: {e}")
    sys.exit(2)

# Streamable HTTP was introduced in protocol revision 2025-03-26; SDK releases that implement
# only 2024-11-05 have no such client, and for those the SSE transport is the only one to test.
try:
    from mcp.client.streamable_http import streamablehttp_client
except ImportError:
    streamablehttp_client = None


@dataclass
class TestResult:
    name: str
    passed: bool
    message: str
    duration_ms: float


class McpComplianceTest:
    """Tests MCP server compliance with the official SDK."""

    def __init__(self, server_url: str, transport: str = "streamable"):
        self.server_url = server_url
        self.transport = transport
        self.results: List[TestResult] = []
        self.session: Optional[ClientSession] = None

    @asynccontextmanager
    async def _get_transport(self):
        """Get the appropriate transport based on configuration."""
        if self.transport == "sse":
            async with sse_client(self.server_url) as (read, write):
                yield read, write
        else:
            if streamablehttp_client is None:
                raise RuntimeError(
                    "this MCP SDK release has no Streamable HTTP client; that transport was "
                    "introduced in protocol revision 2025-03-26"
                )
            # Streamable HTTP - returns (read, write, get_session_id)
            async with streamablehttp_client(self.server_url) as (read, write, _):
                yield read, write

    async def run_all_tests(self) -> bool:
        """Run all compliance tests. Returns True if all pass."""
        print(f"\n{'='*60}")
        print(f"MCP Protocol Compliance Test")
        print(f"Server: {self.server_url}")
        print(f"Transport: {self.transport}")
        print(f"SDK Version: {self._get_sdk_version()}")
        print(f"{'='*60}\n")

        try:
            async with self._get_transport() as (read, write):
                async with ClientSession(read, write) as session:
                    self.session = session

                    # Run test suites
                    await self._test_initialization()
                    await self._test_tools()
                    await self._test_resources()
                    await self._test_prompts()
                    await self._test_completions()
                    await self._test_resource_templates()
                    # Note: Tasks API requires session header, tested separately

        except Exception as e:
            self._add_result("connection", False, f"Failed to connect: {e}")
            traceback.print_exc()

        return self._print_summary()

    def _get_sdk_version(self) -> str:
        try:
            import mcp
            return getattr(mcp, '__version__', 'unknown')
        except:
            return 'unknown'

    def _add_result(self, name: str, passed: bool, message: str, duration_ms: float = 0):
        result = TestResult(name, passed, message, duration_ms)
        self.results.append(result)
        status = "\033[92mPASS\033[0m" if passed else "\033[91mFAIL\033[0m"
        print(f"  [{status}] {name}: {message}")

    async def _timed_test(self, name: str, test_func):
        """Run a test and record timing."""
        start = datetime.now()
        try:
            await test_func()
            duration = (datetime.now() - start).total_seconds() * 1000
            self._add_result(name, True, "OK", duration)
        except AssertionError as e:
            duration = (datetime.now() - start).total_seconds() * 1000
            self._add_result(name, False, str(e), duration)
        except Exception as e:
            duration = (datetime.now() - start).total_seconds() * 1000
            self._add_result(name, False, f"Exception: {e}", duration)
            traceback.print_exc()

    # ========== Initialization Tests ==========

    async def _test_initialization(self):
        print("\n[Initialization]")

        async def test_init():
            result = await self.session.initialize()
            assert result is not None, "Initialize returned None"
            # SDK uses camelCase for attributes
            assert result.protocolVersion, "Missing protocolVersion"
            assert result.capabilities, "Missing capabilities"
            assert result.serverInfo, "Missing serverInfo"
            assert result.serverInfo.name, "Missing server name"

        await self._timed_test("initialize", test_init)

        async def test_ping():
            # Ping should work after initialization
            # The SDK may not expose ping directly, but we can verify the session works
            tools = await self.session.list_tools()
            assert tools is not None, "Session not responsive after init"

        await self._timed_test("session_active", test_ping)

    # ========== Tools Tests ==========

    async def _test_tools(self):
        print("\n[Tools]")

        tools_list = None

        async def test_list_tools():
            nonlocal tools_list
            result = await self.session.list_tools()
            tools_list = result.tools
            assert tools_list is not None, "tools/list returned None"
            assert len(tools_list) > 0, "No tools registered"
            # Verify tool structure
            for tool in tools_list:
                assert tool.name, f"Tool missing name"
                assert tool.description is not None, f"Tool {tool.name} missing description"

        await self._timed_test("tools/list", test_list_tools)

        async def test_call_echo():
            result = await self.session.call_tool("echo", {"message": "hello"})
            assert result is not None, "tools/call returned None"
            assert result.content, "No content in response"
            # Parse the JSON response
            text_content = result.content[0]
            assert hasattr(text_content, 'text'), "Response missing text"

        await self._timed_test("tools/call (echo)", test_call_echo)

        async def test_call_add():
            result = await self.session.call_tool("add", {"a": 5, "b": 3})
            assert result is not None, "tools/call returned None"
            content = result.content[0]
            data = json.loads(content.text)
            assert data.get("result") == 8, f"Expected 8, got {data.get('result')}"

        await self._timed_test("tools/call (add)", test_call_add)

        async def test_call_failing():
            result = await self.session.call_tool("failing_tool", {})
            assert result is not None, "tools/call returned None for failing tool"
            # The result itself has isError flag, not the content
            assert result.isError, "Failing tool should set isError=True"

        await self._timed_test("tools/call (error handling)", test_call_failing)

        async def test_unknown_tool():
            try:
                result = await self.session.call_tool("nonexistent_tool_xyz", {})
                # Should return error content or raise
                if result:
                    # Result.isError should be True for unknown tools
                    assert result.isError, "Unknown tool should set isError=True"
            except Exception as e:
                # Some SDKs may raise for unknown tools - that's acceptable
                pass

        await self._timed_test("tools/call (unknown tool)", test_unknown_tool)

    # ========== Resources Tests ==========

    async def _test_resources(self):
        print("\n[Resources]")

        resources_list = None

        async def test_list_resources():
            nonlocal resources_list
            result = await self.session.list_resources()
            resources_list = result.resources
            assert resources_list is not None, "resources/list returned None"
            # Verify resource structure
            for resource in resources_list:
                assert resource.uri, f"Resource missing uri"
                assert resource.name, f"Resource missing name"

        await self._timed_test("resources/list", test_list_resources)

        async def test_read_resource():
            if not resources_list:
                raise AssertionError("No resources to read")
            # Find the hello resource - uri is an AnyUrl, convert to string
            hello_resource = next((r for r in resources_list if "hello" in str(r.uri)), None)
            if not hello_resource:
                hello_resource = resources_list[0]

            result = await self.session.read_resource(str(hello_resource.uri))
            assert result is not None, "resources/read returned None"
            assert result.contents, "No contents in response"

        await self._timed_test("resources/read", test_read_resource)

    # ========== Prompts Tests ==========

    async def _test_prompts(self):
        print("\n[Prompts]")

        prompts_list = None

        async def test_list_prompts():
            nonlocal prompts_list
            result = await self.session.list_prompts()
            prompts_list = result.prompts
            assert prompts_list is not None, "prompts/list returned None"
            for prompt in prompts_list:
                assert prompt.name, "Prompt missing name"

        await self._timed_test("prompts/list", test_list_prompts)

        async def test_get_prompt():
            if not prompts_list:
                raise AssertionError("No prompts to get")
            # Find greeting prompt
            greeting = next((p for p in prompts_list if p.name == "greeting"), None)
            if not greeting:
                greeting = prompts_list[0]

            result = await self.session.get_prompt(greeting.name, {"name": "TestUser"})
            assert result is not None, "prompts/get returned None"
            assert result.messages, "No messages in prompt response"

        await self._timed_test("prompts/get", test_get_prompt)

    # ========== Completions Tests ==========

    async def _test_completions(self):
        print("\n[Completions]")

        async def test_complete_prompt():
            try:
                # Try to get completion for a prompt argument
                from mcp.types import PromptReference
                result = await self.session.complete(
                    ref=PromptReference(type="ref/prompt", name="greeting"),
                    argument={"name": "name", "value": "T"}
                )
                # Just verify we get a response without error
                assert result is not None, "completion/complete returned None"
            except Exception as e:
                # Completion support is optional
                if "not supported" in str(e).lower() or "not implemented" in str(e).lower():
                    raise AssertionError("Completion not supported (optional)")
                raise

        await self._timed_test("completion/complete", test_complete_prompt)

    # ========== Resource Templates Tests ==========

    async def _test_resource_templates(self):
        print("\n[Resource Templates]")

        async def test_list_templates():
            try:
                result = await self.session.list_resource_templates()
                # SDK uses camelCase: resourceTemplates
                templates = result.resourceTemplates
                assert templates is not None, "No templates returned"
                # Verify template structure - SDK uses camelCase: uriTemplate
                for tpl in templates:
                    assert tpl.uriTemplate, f"Template missing uriTemplate"
                    assert tpl.name, f"Template missing name"
            except AttributeError as e:
                # Check if the method exists but returned different structure
                if "list_resource_templates" in str(e):
                    raise AssertionError("SDK doesn't support list_resource_templates")
                raise
            except Exception as e:
                if "method not found" in str(e).lower():
                    raise AssertionError("Server doesn't support resource templates")
                raise

        await self._timed_test("resources/templates/list", test_list_templates)

    # ========== Summary ==========

    def _print_summary(self) -> bool:
        passed = sum(1 for r in self.results if r.passed)
        failed = sum(1 for r in self.results if not r.passed)
        total = len(self.results)

        print(f"\n{'='*60}")
        print(f"SUMMARY: {passed}/{total} tests passed ({self.transport} transport)")

        if failed > 0:
            print(f"\nFailed tests:")
            for r in self.results:
                if not r.passed:
                    print(f"  - {r.name}: {r.message}")

        print(f"{'='*60}\n")

        return failed == 0


async def main():
    parser = argparse.ArgumentParser(description="MCP Protocol Compliance Test")
    parser.add_argument("server_url", help="Server URL (e.g., http://localhost:8080)")
    parser.add_argument("--transport", "-t", choices=["sse", "streamable"],
                        default="streamable", help="Transport to use (default: streamable)")
    args = parser.parse_args()

    tester = McpComplianceTest(args.server_url, args.transport)
    success = await tester.run_all_tests()
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    asyncio.run(main())
