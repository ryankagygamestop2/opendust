"""Tests for the OpenDust MCP bridge against the fake engine."""

from __future__ import annotations

import asyncio
import json
from collections.abc import AsyncIterator, Callable
from contextlib import asynccontextmanager
from pathlib import Path
from typing import Any

import pytest
from mcp.client.session import ClientSession
from mcp.shared.memory import create_client_server_memory_streams

import opendust_mcp as om
from tests.fake_engine import FakeEngine

IDENTITY = om.AgentIdentity(name="test-agent", agent_id="agent_test", soul_ref="test/soul.md")


async def wait_for(cond: Callable[[], bool], timeout: float = 5.0, step: float = 0.02) -> None:
    loop = asyncio.get_running_loop()
    deadline = loop.time() + timeout
    while not cond():
        if loop.time() > deadline:
            raise AssertionError("condition not met in time")
        await asyncio.sleep(step)


@pytest.fixture
async def engine(tmp_path: Path) -> AsyncIterator[FakeEngine]:
    e = FakeEngine(tmp_path / "project")
    await e.start()
    try:
        yield e
    finally:
        await e.stop()


@asynccontextmanager
async def running_connection(
    locator: om.Locator, on_tools_changed: om.ToolsChangedHook | None = None, **kw: Any
) -> AsyncIterator[om.EngineConnection]:
    conn = om.EngineConnection(locator, IDENTITY, on_tools_changed, retry_delay=kw.pop("retry_delay", 0.05), **kw)
    task = asyncio.create_task(conn.run())
    try:
        yield conn
    finally:
        await conn.close()
        task.cancel()
        try:
            await task
        except (asyncio.CancelledError, Exception):  # noqa: BLE001
            pass


# -- discovery -------------------------------------------------------------------------


def _write_discovery(root: Path, mode: str, **over: Any) -> Path:
    p = root / ".opendust" / f"bridge-{mode}.json"
    p.parent.mkdir(parents=True, exist_ok=True)
    body = {"schema": om.BRIDGE_SCHEMA, "mode": mode, "port": 1, "token": "t", "pid": 1}
    body.update(over)
    p.write_text(json.dumps(body), encoding="utf-8")
    return p


def test_find_discovery_walks_up_and_prefers_editor(tmp_path: Path) -> None:
    project = tmp_path / "proj"
    deep = project / "a" / "b"
    deep.mkdir(parents=True)
    assert om.find_discovery(deep, env={}) is None
    runtime = _write_discovery(project, "runtime")
    assert om.find_discovery(deep, env={}) == runtime
    editor = _write_discovery(project, "editor")
    assert om.find_discovery(deep, env={}) == editor
    assert om.find_discovery(deep, mode="runtime", env={}) == runtime
    # nearer project wins over an outer one
    inner = _write_discovery(deep, "runtime")
    assert om.find_discovery(deep, env={}) == inner


def test_find_discovery_explicit_and_env(tmp_path: Path) -> None:
    editor = _write_discovery(tmp_path, "editor")
    other = tmp_path / "elsewhere.json"
    other.write_text("{}", encoding="utf-8")
    assert om.find_discovery(tmp_path, explicit=other, env={}) == other
    assert om.find_discovery(tmp_path, explicit=tmp_path / "missing.json", env={}) is None
    assert om.find_discovery(tmp_path, env={"OPENDUST_BRIDGE_DISCOVERY": str(other)}) == other
    assert om.find_discovery(tmp_path, env={"OPENDUST_BRIDGE_DISCOVERY": str(tmp_path / "nope.json")}) is None
    assert om.find_discovery(tmp_path, env={}) == editor


def test_discovery_schema_rejected(tmp_path: Path) -> None:
    bad = _write_discovery(tmp_path, "editor", schema="opendust.bridge/99")
    with pytest.raises(ValueError, match="unsupported bridge schema"):
        om.Discovery.load(bad)
    good = _write_discovery(tmp_path, "runtime", port=6121, token="abc")
    d = om.Discovery.load(good)
    assert (d.mode, d.port, d.token, d.url) == ("runtime", 6121, "abc", "ws://127.0.0.1:6121")


# -- connection ------------------------------------------------------------------------


async def test_connect_hello_and_list_tools(engine: FakeEngine) -> None:
    async with running_connection(lambda: engine.discovery_path) as conn:
        await wait_for(lambda: conn.state == "ready")
        assert [t["name"] for t in conn.tools] == ["engine.ping", "echo", "fail"]
        assert conn.capabilities_version == 1
        assert engine.hellos[0]["name"] == "test-agent"
        assert engine.hellos[0]["agent_id"] == "agent_test"
        assert engine.hellos[0]["client"].startswith("opendust-mcp/")
        assert conn.session_info["mode"] == "editor"


async def test_wrong_token_never_becomes_ready(engine: FakeEngine, tmp_path: Path) -> None:
    tampered = tmp_path / "tampered.json"
    body = json.loads(engine.discovery_path.read_text(encoding="utf-8"))
    body["token"] = "wrong"
    tampered.write_text(json.dumps(body), encoding="utf-8")
    async with running_connection(lambda: tampered) as conn:
        await wait_for(lambda: conn.last_error is not None)
        await asyncio.sleep(0.2)
        assert conn.state != "ready"
        assert conn.tools == []
        assert not engine.hellos
        with pytest.raises(om.NotConnected):
            await conn.call("echo", {})


async def test_call_forwarding_and_error_mapping(engine: FakeEngine) -> None:
    async with running_connection(lambda: engine.discovery_path) as conn:
        await wait_for(lambda: conn.state == "ready")
        assert await conn.call("echo", {"x": 1}) == {"echo": {"x": 1}}
        with pytest.raises(om.EngineError) as ei:
            await conn.call("fail", {"path": "nope"})
        assert ei.value.code == -32003
        assert ei.value.message == "not_found"
        assert ei.value.data["detail"] == "no such path 'nope'"
        assert "tool=fail" in ei.value.describe()
        assert ("echo", {"x": 1}) in engine.calls


async def test_capabilities_changed_relists(engine: FakeEngine) -> None:
    changes = 0

    async def hook() -> None:
        nonlocal changes
        changes += 1

    async with running_connection(lambda: engine.discovery_path, hook) as conn:
        await wait_for(lambda: conn.state == "ready")
        base = changes
        assert base >= 1
        await engine.add_tool({"name": "extra.tool", "description": "added later", "inputSchema": {"type": "object"}})
        await wait_for(lambda: any(t["name"] == "extra.tool" for t in conn.tools))
        assert conn.capabilities_version == 2
        assert changes > base


async def test_reconnects_after_engine_restart(engine: FakeEngine) -> None:
    project = engine.project_dir
    path = engine.discovery_path
    async with running_connection(lambda: path if path.exists() else None, retry_delay=0.05) as conn:
        await wait_for(lambda: conn.state == "ready")
        await engine.stop()
        await wait_for(lambda: conn.state in ("disconnected", "no_engine"))
        assert conn.tools == []
        # restart on a fresh port with a fresh token; discovery file is rewritten
        engine2 = FakeEngine(project, token="second")
        await engine2.start()
        try:
            await wait_for(lambda: conn.state == "ready" and conn.discovery is not None and conn.discovery.token == "second")
            assert await conn.call("engine.ping") == {"time_ms": 0}
        finally:
            await engine2.stop()


# -- MCP layer -------------------------------------------------------------------------


@asynccontextmanager
async def mcp_client(bridge: om.Bridge) -> AsyncIterator[ClientSession]:
    server = om.build_server(bridge)
    async with create_client_server_memory_streams() as (client_streams, server_streams):
        server_task = asyncio.create_task(
            server.run(server_streams[0], server_streams[1], om.init_options(server), raise_exceptions=True)
        )
        try:
            async with ClientSession(client_streams[0], client_streams[1]) as client:
                await client.initialize()
                yield client
        finally:
            server_task.cancel()
            try:
                await server_task
            except (asyncio.CancelledError, Exception):  # noqa: BLE001
                pass


async def test_mcp_lists_and_calls_engine_tools(engine: FakeEngine) -> None:
    bridge_ref: dict[str, om.Bridge] = {}

    async def hook() -> None:
        if "b" in bridge_ref:
            await bridge_ref["b"].on_tools_changed()

    async with running_connection(lambda: engine.discovery_path, hook) as conn:
        bridge = om.Bridge(conn)
        bridge_ref["b"] = bridge
        await wait_for(lambda: conn.state == "ready")
        async with mcp_client(bridge) as client:
            listed = await client.list_tools()
            names = [t.name for t in listed.tools]
            assert names == [om.STATUS_TOOL, "engine.ping", "echo", "fail"]
            echo = next(t for t in listed.tools if t.name == "echo")
            assert echo.input_schema["properties"]["x"]["type"] == "integer"
            assert "[editor, runtime]" in (echo.description or "")

            res = await client.call_tool("echo", {"x": 7})
            assert res.is_error is not True
            assert json.loads(res.content[0].text) == {"echo": {"x": 7}}

            err = await client.call_tool("fail", {"path": "x"})
            assert err.is_error is True
            assert "-32003" in err.content[0].text and "not_found" in err.content[0].text

            status = await client.call_tool(om.STATUS_TOOL, {})
            assert "state=ready" in status.content[0].text

            # a tool added at runtime shows up on the next list
            await engine.add_tool({"name": "late.tool", "description": "late", "inputSchema": {"type": "object"}})
            await wait_for(lambda: any(t["name"] == "late.tool" for t in conn.tools))
            listed2 = await client.list_tools()
            assert "late.tool" in [t.name for t in listed2.tools]


async def test_mcp_without_engine_exposes_status_only() -> None:
    async with running_connection(lambda: None) as conn:
        bridge = om.Bridge(conn)
        async with mcp_client(bridge) as client:
            listed = await client.list_tools()
            assert [t.name for t in listed.tools] == [om.STATUS_TOOL]
            res = await client.call_tool(om.STATUS_TOOL, {})
            text = res.content[0].text
            assert "state=no_engine" in text
            assert "bridge-editor.json" in text
            missing = await client.call_tool("scene.tree", {})
            assert missing.is_error is True
            assert "not connected" in missing.content[0].text


def test_cli_defaults(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.delenv("OPENDUST_AGENT_NAME", raising=False)
    args = om.parse_args([])
    assert args.agent_name == "claude-code" and args.discovery is None and args.mode is None
    monkeypatch.setenv("OPENDUST_AGENT_NAME", "Oliver")
    args = om.parse_args(["--mode", "runtime", "--agent-id", "a1"])
    assert (args.agent_name, args.mode, args.agent_id) == ("Oliver", "runtime", "a1")
