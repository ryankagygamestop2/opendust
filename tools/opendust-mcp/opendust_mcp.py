#!/usr/bin/env python
"""OpenDust MCP bridge.

A stdio MCP server that connects to a running OpenDust editor or game over the agent
bridge (WebSocket, JSON-RPC 2.0; see docs/opendust/01-agent-bridge-protocol.md) and
exposes every engine tool 1:1 as an MCP tool. The tool list is discovered from the
engine (`engine.capabilities`) and never duplicated here.

Usage (normally launched by Claude Code via the repo's .mcp.json):

    python opendust_mcp.py [--discovery PATH | --mode editor|runtime]
                           [--agent-name NAME] [--agent-id ID] [--soul-ref REF]
                           [--log-level LEVEL]

Environment:
    OPENDUST_BRIDGE_DISCOVERY   path to a bridge-*.json discovery file
    OPENDUST_AGENT_NAME         default agent name (else "claude-code")
    OPENDUST_AGENT_ID           default durable agent id
    OPENDUST_SOUL_REF           default soul reference
"""

from __future__ import annotations

import argparse
import asyncio
import json
import logging
import os
import sys
from collections.abc import Awaitable, Callable, Mapping
from dataclasses import dataclass
from pathlib import Path
from typing import Any

# If the deps aren't importable with the interpreter that launched us (the root .mcp.json uses
# plain `python`), re-exec with the module's own venv when one exists. This keeps `.mcp.json`
# portable without asking every family member to install into their global Python.
try:
    import mcp_types as types  # noqa: F401
except ImportError:  # pragma: no cover - environment-dependent
    _here = Path(__file__).resolve().parent
    _venv_py = next((p for p in (
        _here / ".venv" / "Scripts" / "python.exe",
        _here / ".venv" / "bin" / "python",
    ) if p.exists()), None)
    if _venv_py is not None and Path(sys.executable).resolve() != _venv_py.resolve():
        os.execv(str(_venv_py), [str(_venv_py), str(Path(__file__).resolve()), *sys.argv[1:]])
    raise SystemExit(
        "opendust-mcp: the 'mcp' and 'websockets' packages are not installed for "
        f"{sys.executable}. Run: python -m venv tools/opendust-mcp/.venv && "
        "tools/opendust-mcp/.venv/Scripts/pip install -e tools/opendust-mcp"
    )
import mcp_types as types
import websockets
from mcp.server.context import ServerRequestContext
from mcp.server.lowlevel import NotificationOptions, Server
from mcp.server.session import ServerSession
from mcp.server.stdio import stdio_server
from websockets.asyncio.client import ClientConnection, connect

__version__ = "0.1.0"

BRIDGE_SCHEMA = "opendust.bridge/1"
DISCOVERY_DIR = ".opendust"
DISCOVERY_FILES: Mapping[str, str] = {
    "editor": "bridge-editor.json",
    "runtime": "bridge-runtime.json",
}
STATUS_TOOL = "opendust.status"
CLIENT_ID = f"opendust-mcp/{__version__}"

log = logging.getLogger("opendust-mcp")


# --------------------------------------------------------------------------------------
# Errors
# --------------------------------------------------------------------------------------


class EngineError(Exception):
    """A JSON-RPC error returned by the engine."""

    def __init__(self, code: int, message: str, data: Any = None) -> None:
        super().__init__(f"{code}: {message}")
        self.code = code
        self.message = message
        self.data = data

    def describe(self) -> str:
        detail = ""
        if isinstance(self.data, Mapping):
            tool = self.data.get("tool")
            det = self.data.get("detail")
            if tool:
                detail += f" tool={tool}"
            if det:
                detail += f" detail={det}"
        elif self.data is not None:
            detail = f" data={json.dumps(self.data, default=str)}"
        return f"engine error {self.code}: {self.message}{detail}"


class NotConnected(Exception):
    """Raised when a call is attempted with no live engine session."""


# --------------------------------------------------------------------------------------
# Discovery
# --------------------------------------------------------------------------------------


@dataclass(frozen=True)
class Discovery:
    """Contents of a `.opendust/bridge-<mode>.json` file written by the engine."""

    path: Path
    schema: str
    mode: str
    port: int
    token: str
    pid: int | None
    project_path: str | None
    engine_version: str | None

    @classmethod
    def load(cls, path: Path) -> Discovery:
        raw = json.loads(path.read_text(encoding="utf-8"))
        schema = raw.get("schema")
        if schema != BRIDGE_SCHEMA:
            raise ValueError(f"{path}: unsupported bridge schema {schema!r} (want {BRIDGE_SCHEMA!r})")
        for key in ("mode", "port", "token"):
            if key not in raw:
                raise ValueError(f"{path}: discovery file missing {key!r}")
        return cls(
            path=path,
            schema=schema,
            mode=str(raw["mode"]),
            port=int(raw["port"]),
            token=str(raw["token"]),
            pid=int(raw["pid"]) if raw.get("pid") is not None else None,
            project_path=raw.get("project_path"),
            engine_version=raw.get("engine_version"),
        )

    @property
    def url(self) -> str:
        return f"ws://127.0.0.1:{self.port}"


def find_discovery(
    start: Path,
    mode: str | None = None,
    explicit: Path | None = None,
    env: Mapping[str, str] | None = None,
) -> Path | None:
    """Locate a discovery file.

    Precedence: explicit path, then OPENDUST_BRIDGE_DISCOVERY, then a walk upward from
    `start` looking in each directory's `.opendust/` for `bridge-editor.json` then
    `bridge-runtime.json` (or only the requested `mode`). Returns None if nothing exists.
    """
    env = os.environ if env is None else env
    if explicit is not None:
        return explicit if explicit.is_file() else None
    from_env = env.get("OPENDUST_BRIDGE_DISCOVERY")
    if from_env:
        p = Path(from_env)
        return p if p.is_file() else None
    modes = [mode] if mode else ["editor", "runtime"]
    start = start.resolve()
    for directory in (start, *start.parents):
        for m in modes:
            candidate = directory / DISCOVERY_DIR / DISCOVERY_FILES[m]
            if candidate.is_file():
                return candidate
    return None


# --------------------------------------------------------------------------------------
# Engine connection
# --------------------------------------------------------------------------------------


@dataclass(frozen=True)
class AgentIdentity:
    name: str
    agent_id: str | None = None
    soul_ref: str | None = None
    client: str = CLIENT_ID

    def as_params(self) -> dict[str, Any]:
        return {
            "name": self.name,
            "agent_id": self.agent_id,
            "soul_ref": self.soul_ref,
            "client": self.client,
        }


Locator = Callable[[], Path | None]
ToolsChangedHook = Callable[[], Awaitable[None]]


class EngineConnection:
    """One long-lived connection to an engine, with reconnect and tool discovery.

    States: "no_engine" (no discovery file), "connecting", "ready", "disconnected".
    `run()` loops forever until `close()`; `call()` forwards a JSON-RPC request.
    """

    def __init__(
        self,
        locator: Locator,
        identity: AgentIdentity,
        on_tools_changed: ToolsChangedHook | None = None,
        *,
        retry_delay: float = 3.0,
        max_backoff: float = 30.0,
        request_timeout: float = 120.0,
    ) -> None:
        self._locator = locator
        self.identity = identity
        self._on_tools_changed = on_tools_changed
        self._retry_delay = retry_delay
        self._max_backoff = max_backoff
        self._request_timeout = request_timeout

        self.state: str = "no_engine"
        self.last_error: str | None = None
        self.discovery: Discovery | None = None
        self.session_info: dict[str, Any] = {}
        self.tools: list[dict[str, Any]] = []
        self.capabilities_version: int = 0

        self._ws: ClientConnection | None = None
        self._pending: dict[int, asyncio.Future[Any]] = {}
        self._next_id = 0
        self._closed = False
        self._wake = asyncio.Event()

    # -- lifecycle ---------------------------------------------------------------------

    async def run(self) -> None:
        backoff = self._retry_delay
        while not self._closed:
            path = self._locator()
            if path is None:
                self._set_state("no_engine")
                await self._sleep(self._retry_delay)
                continue
            try:
                discovery = Discovery.load(path)
            except (OSError, ValueError, json.JSONDecodeError) as exc:
                self.last_error = str(exc)
                log.warning("discovery unreadable: %s", exc)
                self._set_state("no_engine")
                await self._sleep(self._retry_delay)
                continue

            self.discovery = discovery
            self._set_state("connecting")
            try:
                await self._serve_once(discovery)
                backoff = self._retry_delay  # clean close: reset
            except (OSError, websockets.exceptions.WebSocketException, EngineError, NotConnected, asyncio.TimeoutError) as exc:
                self.last_error = f"{type(exc).__name__}: {exc}"
                log.warning("engine connection lost: %s", self.last_error)
            finally:
                had_tools = bool(self.tools)
                self._ws = None
                self._fail_pending(NotConnected("engine disconnected"))
                self.tools = []
                self.session_info = {}
                if not self._closed:
                    self._set_state("disconnected")
                if had_tools:
                    await self._notify_tools_changed()
            if self._closed:
                break
            await self._sleep(backoff)
            backoff = min(backoff * 2, self._max_backoff)

    async def close(self) -> None:
        self._closed = True
        self._wake.set()
        ws = self._ws
        if ws is not None:
            await ws.close()

    async def _serve_once(self, discovery: Discovery) -> None:
        async with connect(discovery.url, open_timeout=5, max_size=64 * 1024 * 1024) as ws:
            self._ws = ws
            reader = asyncio.create_task(self._reader(ws), name="opendust-reader")
            try:
                hello = await self._request(
                    "session.hello",
                    {"token": discovery.token, "agent": self.identity.as_params()},
                    timeout=10.0,
                )
                self.session_info = hello if isinstance(hello, dict) else {}
                await self._refresh_tools()
                self._set_state("ready")
                log.info(
                    "connected to %s engine %s on port %d (%d tools)",
                    discovery.mode,
                    self.session_info.get("engine_version", "?"),
                    discovery.port,
                    len(self.tools),
                )
                await reader
            finally:
                if not reader.done():
                    reader.cancel()
                    try:
                        await reader
                    except (asyncio.CancelledError, Exception):  # noqa: BLE001
                        pass

    # -- wire ------------------------------------------------------------------------

    async def _reader(self, ws: ClientConnection) -> None:
        try:
            await self._read_frames(ws)
        finally:
            # Whatever ended the loop, nothing will answer outstanding requests now.
            self._fail_pending(NotConnected("engine closed the connection"))

    async def _read_frames(self, ws: ClientConnection) -> None:
        async for frame in ws:
            if isinstance(frame, bytes):
                log.debug("ignoring binary frame (%d bytes)", len(frame))
                continue
            try:
                msg = json.loads(frame)
            except json.JSONDecodeError:
                log.warning("engine sent non-JSON frame: %.120s", frame)
                continue
            if not isinstance(msg, dict):
                continue
            if "id" in msg and ("result" in msg or "error" in msg):
                self._resolve(msg)
            elif "method" in msg and "id" not in msg:
                await self._on_notification(str(msg["method"]), msg.get("params"))
            elif "method" in msg and "id" in msg:
                # Engine → client request. We serve none; answer per JSON-RPC.
                await ws.send(
                    json.dumps(
                        {
                            "jsonrpc": "2.0",
                            "id": msg["id"],
                            "error": {"code": -32601, "message": "method not found"},
                        }
                    )
                )

    def _resolve(self, msg: dict[str, Any]) -> None:
        try:
            rid = int(msg["id"])
        except (TypeError, ValueError):
            return
        fut = self._pending.pop(rid, None)
        if fut is None or fut.done():
            return
        if "error" in msg and msg["error"] is not None:
            err = msg["error"] or {}
            fut.set_exception(
                EngineError(int(err.get("code", -32000)), str(err.get("message", "error")), err.get("data"))
            )
        else:
            fut.set_result(msg.get("result"))

    async def _on_notification(self, method: str, params: Any) -> None:
        if method == "event.capabilities_changed":
            log.debug("capabilities changed: %s", params)
            # Must not await a request here: the reader loop is what delivers the answer.
            asyncio.create_task(self._refresh_tools_safe(), name="opendust-relist")
        else:
            log.debug("engine event %s: %s", method, json.dumps(params, default=str)[:400])

    async def _request(self, method: str, params: Mapping[str, Any] | None = None, *, timeout: float | None = None) -> Any:
        ws = self._ws
        if ws is None:
            raise NotConnected("no engine connection")
        self._next_id += 1
        rid = self._next_id
        loop = asyncio.get_running_loop()
        fut: asyncio.Future[Any] = loop.create_future()
        self._pending[rid] = fut
        payload: dict[str, Any] = {"jsonrpc": "2.0", "id": rid, "method": method}
        if params is not None:
            payload["params"] = dict(params)
        try:
            await ws.send(json.dumps(payload, default=str))
            return await asyncio.wait_for(fut, timeout or self._request_timeout)
        except websockets.exceptions.ConnectionClosed as exc:
            self._pending.pop(rid, None)
            raise NotConnected(f"engine closed the connection during {method}: {exc}") from exc
        except asyncio.TimeoutError:
            self._pending.pop(rid, None)
            raise

    async def _refresh_tools_safe(self) -> None:
        try:
            await self._refresh_tools()
        except (EngineError, NotConnected, asyncio.TimeoutError) as exc:
            log.warning("re-listing tools failed: %s", exc)

    async def _refresh_tools(self) -> None:
        caps = await self._request("engine.capabilities", timeout=30.0)
        tools = caps.get("tools", []) if isinstance(caps, dict) else []
        self.tools = [t for t in tools if isinstance(t, dict) and t.get("name")]
        self.capabilities_version = int(caps.get("version", 0)) if isinstance(caps, dict) else 0
        await self._notify_tools_changed()

    # -- public ----------------------------------------------------------------------

    async def call(self, method: str, params: Mapping[str, Any] | None = None) -> Any:
        if self.state != "ready":
            raise NotConnected(f"engine not ready (state={self.state})")
        return await self._request(method, params)

    def status(self) -> dict[str, Any]:
        d = self.discovery
        return {
            "state": self.state,
            "mode": d.mode if d else None,
            "port": d.port if d else None,
            "project_path": d.project_path if d else None,
            "engine_version": self.session_info.get("engine_version") or (d.engine_version if d else None),
            "session_id": self.session_info.get("session_id"),
            "tools": len(self.tools),
            "capabilities_version": self.capabilities_version,
            "discovery_file": str(d.path) if d else None,
            "last_error": self.last_error,
            "agent": self.identity.as_params(),
        }

    # -- helpers ---------------------------------------------------------------------

    def _set_state(self, state: str) -> None:
        if state != self.state:
            log.debug("state %s -> %s", self.state, state)
        self.state = state

    def _fail_pending(self, exc: Exception) -> None:
        for fut in self._pending.values():
            if not fut.done():
                fut.set_exception(exc)
        self._pending.clear()

    async def _notify_tools_changed(self) -> None:
        if self._on_tools_changed is None:
            return
        try:
            await self._on_tools_changed()
        except Exception as exc:  # noqa: BLE001 - never let a notification kill the loop
            log.debug("tools-changed hook failed: %s", exc)

    async def _sleep(self, seconds: float) -> None:
        self._wake.clear()
        try:
            await asyncio.wait_for(self._wake.wait(), seconds)
        except asyncio.TimeoutError:
            pass


# --------------------------------------------------------------------------------------
# MCP surface
# --------------------------------------------------------------------------------------


def _status_text(conn: EngineConnection) -> str:
    st = conn.status()
    lines = [f"OpenDust bridge {__version__}: state={st['state']}"]
    if st["state"] == "ready":
        lines.append(
            f"connected to {st['mode']} engine {st['engine_version']} on port {st['port']} "
            f"({st['tools']} tools, capabilities v{st['capabilities_version']})"
        )
        lines.append(f"project: {st['project_path']}")
    else:
        lines.append(
            "No running OpenDust engine was found. Start the editor "
            "(bin/opendust.windows.editor.x86_64.exe) on a project, or run a game built with "
            "the opendust_agent module. The engine writes <project>/.opendust/bridge-editor.json "
            "(or bridge-runtime.json); this bridge searches upward from its working directory, "
            "or you can point it with --discovery PATH or OPENDUST_BRIDGE_DISCOVERY."
        )
        lines.append("Tools will appear automatically once the engine is up; call this tool again to check.")
        if st["discovery_file"]:
            lines.append(f"last discovery file: {st['discovery_file']}")
        if st["last_error"]:
            lines.append(f"last error: {st['last_error']}")
    lines.append(f"agent: {st['agent']['name']} (agent_id={st['agent']['agent_id']}, soul_ref={st['agent']['soul_ref']})")
    return "\n".join(lines)


def _to_mcp_tool(spec: Mapping[str, Any]) -> types.Tool:
    schema = spec.get("inputSchema")
    if not isinstance(schema, dict):
        schema = {"type": "object", "properties": {}}
    flags = spec.get("flags") or []
    desc = str(spec.get("description") or "")
    if flags:
        desc = f"{desc} [{', '.join(str(f) for f in flags)}]".strip()
    return types.Tool(name=str(spec["name"]), description=desc, input_schema=schema)


class Bridge:
    """Binds an EngineConnection to the MCP request handlers."""

    def __init__(self, conn: EngineConnection) -> None:
        self.conn = conn
        self._session: ServerSession | None = None

    def _remember(self, ctx: ServerRequestContext[Any]) -> None:
        self._session = ctx.session

    async def list_tools(self, ctx: ServerRequestContext[Any], params: types.PaginatedRequestParams | None) -> types.ListToolsResult:
        self._remember(ctx)
        tools: list[types.Tool] = [
            types.Tool(
                name=STATUS_TOOL,
                description="Report the bridge's connection to the OpenDust engine and how to start one.",
                input_schema={"type": "object", "properties": {}},
            )
        ]
        if self.conn.state == "ready":
            tools.extend(_to_mcp_tool(t) for t in self.conn.tools)
        return types.ListToolsResult(tools=tools)

    async def call_tool(self, ctx: ServerRequestContext[Any], params: types.CallToolRequestParams) -> types.CallToolResult:
        self._remember(ctx)
        name = params.name
        args = dict(params.arguments or {})
        if name == STATUS_TOOL:
            return _text_result(_status_text(self.conn))
        try:
            result = await self.conn.call(name, args)
        except EngineError as exc:
            return _text_result(exc.describe(), is_error=True)
        except NotConnected as exc:
            return _text_result(f"not connected to an OpenDust engine ({exc}); call {STATUS_TOOL}", is_error=True)
        except asyncio.TimeoutError:
            return _text_result(f"engine did not answer {name} in time", is_error=True)
        return _text_result(json.dumps(result, indent=2, default=str))

    async def on_tools_changed(self) -> None:
        session = self._session
        if session is None:
            return
        await session.send_tool_list_changed()


def _text_result(text: str, *, is_error: bool = False) -> types.CallToolResult:
    return types.CallToolResult(content=[types.TextContent(type="text", text=text)], is_error=is_error)


def build_server(bridge: Bridge) -> Server[Any]:
    return Server(
        "opendust",
        version=__version__,
        instructions=(
            "Tools are discovered from a running OpenDust engine. Call opendust.status first if "
            "the tool list is short; call engine.info and scene.tree before editing."
        ),
        on_list_tools=bridge.list_tools,
        on_call_tool=bridge.call_tool,
    )


def init_options(server: Server[Any]) -> Any:
    return server.create_initialization_options(notification_options=NotificationOptions(tools_changed=True))


# --------------------------------------------------------------------------------------
# CLI
# --------------------------------------------------------------------------------------


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    p = argparse.ArgumentParser(prog="opendust-mcp", description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    target = p.add_mutually_exclusive_group()
    target.add_argument("--discovery", type=Path, help="path to a bridge-*.json discovery file")
    target.add_argument("--mode", choices=("editor", "runtime"), help="only look for this mode's discovery file")
    p.add_argument("--agent-name", default=os.environ.get("OPENDUST_AGENT_NAME") or "claude-code")
    p.add_argument("--agent-id", default=os.environ.get("OPENDUST_AGENT_ID") or None)
    p.add_argument("--soul-ref", default=os.environ.get("OPENDUST_SOUL_REF") or None)
    p.add_argument("--retry-delay", type=float, default=3.0, help="seconds between discovery retries")
    p.add_argument("--log-level", default=os.environ.get("OPENDUST_MCP_LOG", "INFO"))
    p.add_argument("--version", action="version", version=f"opendust-mcp {__version__}")
    return p.parse_args(argv)


async def amain(args: argparse.Namespace) -> None:
    identity = AgentIdentity(name=args.agent_name, agent_id=args.agent_id, soul_ref=args.soul_ref)
    cwd = Path.cwd()
    explicit: Path | None = args.discovery

    def locator() -> Path | None:
        return find_discovery(cwd, args.mode, explicit)

    bridge_holder: dict[str, Bridge] = {}

    async def tools_changed() -> None:
        b = bridge_holder.get("bridge")
        if b is not None:
            await b.on_tools_changed()

    conn = EngineConnection(locator, identity, tools_changed, retry_delay=args.retry_delay)
    bridge = Bridge(conn)
    bridge_holder["bridge"] = bridge
    server = build_server(bridge)

    runner = asyncio.create_task(conn.run(), name="opendust-engine")
    try:
        async with stdio_server() as (read_stream, write_stream):
            await server.run(read_stream, write_stream, init_options(server))
    finally:
        await conn.close()
        runner.cancel()
        try:
            await runner
        except (asyncio.CancelledError, Exception):  # noqa: BLE001
            pass


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    logging.basicConfig(
        level=getattr(logging, str(args.log_level).upper(), logging.INFO),
        stream=sys.stderr,
        format="%(asctime)s %(name)s %(levelname)s %(message)s",
    )
    try:
        asyncio.run(amain(args))
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == "__main__":
    sys.exit(main())
