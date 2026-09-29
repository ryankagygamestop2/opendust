"""A fake OpenDust engine for tests and for trying the bridge before the C++ build lands.

Implements enough of docs/opendust/01-agent-bridge-protocol.md to exercise the bridge:
session.hello (token check), engine.capabilities, engine.ping, and a few sample tools.

Standalone:

    python -m tests.fake_engine --port 6120 --project <dir> [--mode editor]

writes <dir>/.opendust/bridge-<mode>.json and serves until Ctrl-C.
"""

from __future__ import annotations

import argparse
import asyncio
import json
import os
import secrets
from pathlib import Path
from typing import Any

import websockets
from websockets.asyncio.server import Server, ServerConnection, serve

BRIDGE_SCHEMA = "opendust.bridge/1"

SAMPLE_TOOLS: list[dict[str, Any]] = [
    {
        "name": "engine.ping",
        "description": "Round-trip check.",
        "inputSchema": {"type": "object", "properties": {}},
        "flags": ["editor", "runtime"],
    },
    {
        "name": "echo",
        "description": "Return the params you sent.",
        "inputSchema": {"type": "object", "properties": {"x": {"type": "integer"}}},
        "flags": ["editor", "runtime"],
    },
    {
        "name": "fail",
        "description": "Always errors with -32003 not_found.",
        "inputSchema": {"type": "object", "properties": {"path": {"type": "string"}}, "required": ["path"]},
        "flags": ["editor"],
    },
]


class FakeEngine:
    def __init__(self, project_dir: Path, *, mode: str = "editor", port: int = 0, token: str | None = None) -> None:
        self.project_dir = Path(project_dir)
        self.mode = mode
        self.port = port
        self.token = token or secrets.token_hex(32)
        self.tools: list[dict[str, Any]] = [dict(t) for t in SAMPLE_TOOLS]
        self.capabilities_version = 1
        self.calls: list[tuple[str, dict[str, Any]]] = []
        self.hellos: list[dict[str, Any]] = []
        self._server: Server | None = None
        self._clients: set[ServerConnection] = set()
        self._authed: set[ServerConnection] = set()

    # -- lifecycle ------------------------------------------------------------------

    @property
    def discovery_path(self) -> Path:
        return self.project_dir / ".opendust" / f"bridge-{self.mode}.json"

    async def start(self) -> None:
        self._server = await serve(self._handle, "127.0.0.1", self.port)
        sock = next(iter(self._server.sockets))
        self.port = sock.getsockname()[1]
        self.write_discovery()

    def write_discovery(self, token: str | None = None) -> None:
        if token is not None:
            self.token = token
        self.discovery_path.parent.mkdir(parents=True, exist_ok=True)
        self.discovery_path.write_text(
            json.dumps(
                {
                    "schema": BRIDGE_SCHEMA,
                    "mode": self.mode,
                    "port": self.port,
                    "token": self.token,
                    "pid": os.getpid(),
                    "project_path": str(self.project_dir),
                    "engine_version": "4.7.2.stable.opendust-fake",
                    "started_at": "2026-09-29T00:00:00Z",
                },
                indent=2,
            ),
            encoding="utf-8",
        )

    async def stop(self) -> None:
        if self._server is not None:
            self._server.close()
            await self._server.wait_closed()
            self._server = None
        for ws in list(self._clients):
            await ws.close()
        self._clients.clear()
        self._authed.clear()
        if self.discovery_path.exists():
            self.discovery_path.unlink()

    async def add_tool(self, spec: dict[str, Any]) -> None:
        self.tools.append(dict(spec))
        self.capabilities_version += 1
        await self.broadcast("event.capabilities_changed", {"version": self.capabilities_version})

    async def broadcast(self, method: str, params: dict[str, Any]) -> None:
        msg = json.dumps({"jsonrpc": "2.0", "method": method, "params": params})
        for ws in list(self._authed):
            try:
                await ws.send(msg)
            except websockets.exceptions.ConnectionClosed:
                self._authed.discard(ws)

    # -- protocol -------------------------------------------------------------------

    async def _handle(self, ws: ServerConnection) -> None:
        self._clients.add(ws)
        try:
            async for frame in ws:
                try:
                    msg = json.loads(frame)
                except json.JSONDecodeError:
                    await ws.send(_error(None, -32700, "parse error"))
                    continue
                rid = msg.get("id")
                method = msg.get("method")
                params = msg.get("params") or {}
                if ws not in self._authed:
                    if method != "session.hello":
                        await ws.send(_error(rid, -32001, "not_authenticated", {"tool": method, "detail": "hello first"}))
                        continue
                    if params.get("token") != self.token:
                        await ws.close(code=1008, reason="bad token")
                        return
                    self._authed.add(ws)
                    self.hellos.append(params.get("agent") or {})
                    await ws.send(
                        _result(
                            rid,
                            {
                                "session_id": f"s_{secrets.token_hex(4)}",
                                "mode": self.mode,
                                "engine_version": "4.7.2.stable.opendust-fake",
                                "project_name": self.project_dir.name,
                                "capabilities_version": self.capabilities_version,
                            },
                        )
                    )
                    continue
                await ws.send(await self._dispatch(rid, method, params))
        except websockets.exceptions.ConnectionClosed:
            pass
        finally:
            self._clients.discard(ws)
            self._authed.discard(ws)

    async def _dispatch(self, rid: Any, method: str, params: dict[str, Any]) -> str:
        self.calls.append((method, params))
        if method == "engine.capabilities":
            return _result(rid, {"version": self.capabilities_version, "mode": self.mode, "tools": self.tools})
        if method == "engine.ping":
            return _result(rid, {"time_ms": 0})
        if method == "engine.info":
            return _result(
                rid,
                {"version": "4.7.2.stable.opendust-fake", "mode": self.mode, "project_name": self.project_dir.name,
                 "project_path": str(self.project_dir), "platform": "fake", "features": []},
            )
        if method == "echo":
            return _result(rid, {"echo": params})
        if method == "fail":
            return _error(rid, -32003, "not_found", {"tool": "fail", "detail": f"no such path {params.get('path')!r}"})
        if any(t["name"] == method for t in self.tools):
            return _result(rid, {"ok": True, "tool": method, "params": params})
        return _error(rid, -32601, "method not found", {"tool": method, "detail": "unknown tool"})


def _result(rid: Any, result: Any) -> str:
    return json.dumps({"jsonrpc": "2.0", "id": rid, "result": result})


def _error(rid: Any, code: int, message: str, data: Any = None) -> str:
    err: dict[str, Any] = {"code": code, "message": message}
    if data is not None:
        err["data"] = data
    return json.dumps({"jsonrpc": "2.0", "id": rid, "error": err})


async def _amain(args: argparse.Namespace) -> None:
    engine = FakeEngine(args.project, mode=args.mode, port=args.port)
    await engine.start()
    print(f"fake OpenDust engine ({engine.mode}) on ws://127.0.0.1:{engine.port}")
    print(f"discovery: {engine.discovery_path}")
    print("Ctrl-C to stop")
    try:
        await asyncio.Event().wait()
    finally:
        await engine.stop()


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(description="Fake OpenDust engine for trying the MCP bridge.")
    p.add_argument("--port", type=int, default=6120)
    p.add_argument("--project", type=Path, default=Path.cwd())
    p.add_argument("--mode", choices=("editor", "runtime"), default="editor")
    args = p.parse_args(argv)
    try:
        asyncio.run(_amain(args))
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
