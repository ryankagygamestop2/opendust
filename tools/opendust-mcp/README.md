# opendust-mcp

The stdio MCP bridge between Claude Code (or any MCP client) and a running OpenDust engine.

The engine (`modules/opendust_agent`) hosts a JSON-RPC 2.0 server over WebSocket on loopback and
writes a discovery file into the project. This bridge finds that file, says hello with the token,
asks the engine what tools it has, and exposes each one 1:1 as an MCP tool. Nothing about a tool is
defined here; add a tool in C++ and it appears in Claude Code with no other change.

Protocol: `docs/opendust/01-agent-bridge-protocol.md`.

## How Claude Code picks it up

The repo root `.mcp.json` registers this server:

```json
{"mcpServers": {"opendust": {"command": "python", "args": ["tools/opendust-mcp/opendust_mcp.py"]}}}
```

Start Claude Code in the OpenDust repo (or any directory under a project the editor has open) and
the engine's tools show up as `mcp__opendust__<tool>` (for example `mcp__opendust__scene.tree`).
If no engine is running yet, only `opendust.status` is listed; open the editor and the tools appear
on their own.

Install deps once:

```
cd tools/opendust-mcp
python -m venv .venv
.venv\Scripts\pip install -e .[test]      # Windows
# .venv/bin/pip install -e .[test]        # Unix
```

If you use the venv, point `.mcp.json`'s `command` at `tools/opendust-mcp/.venv/Scripts/python`
(or install `mcp` and `websockets` into the Python on your PATH).

## Discovery

The bridge looks for the engine's discovery file in this order:

1. `--discovery PATH`
2. `OPENDUST_BRIDGE_DISCOVERY` (env)
3. Walking upward from the current directory: `<dir>/.opendust/bridge-editor.json`, then
   `<dir>/.opendust/bridge-runtime.json`, then the parent directory, and so on. `--mode editor` or
   `--mode runtime` restricts the search to one file.

It re-reads the file on every reconnect, so an editor restart (new port or token) is picked up.

## Flags and environment

| flag | env | default |
|---|---|---|
| `--discovery PATH` | `OPENDUST_BRIDGE_DISCOVERY` | search upward |
| `--mode editor\|runtime` | | both, editor first |
| `--agent-name NAME` | `OPENDUST_AGENT_NAME` | `claude-code` |
| `--agent-id ID` | `OPENDUST_AGENT_ID` | none |
| `--soul-ref REF` | `OPENDUST_SOUL_REF` | none |
| `--retry-delay S` | | 3 |
| `--log-level LEVEL` | `OPENDUST_MCP_LOG` | INFO |

The agent name is what the editor puts on your undo/redo actions ("Oliver: Add Node"), so set it.

## Behaviour

- **Tool list is live.** The engine's `event.capabilities_changed` triggers a re-list and an MCP
  `notifications/tools/list_changed`, so a module that registers tools at runtime shows up.
- **Errors map.** An engine JSON-RPC error becomes an MCP tool error whose text carries the code,
  message, tool, and detail (`engine error -32003: not_found tool=node.get detail=...`).
- **Results are JSON text.** Every successful call returns the engine's result object pretty-printed
  as text content.
- **Reconnects.** If the engine goes away the bridge drops its tools, tells the client, and retries
  with backoff (3s doubling to 30s). When it's back, tools return.
- **`opendust.status`** is always present. Call it to see connection state, the discovery file in
  use, the last error, and what to do if nothing is connected.
- Loopback only. The bridge never connects anywhere but `127.0.0.1`.

## Trying it without the C++ build

A fake engine lives in `tests/fake_engine.py`:

```
.venv\Scripts\python -m tests.fake_engine --port 6120 --project C:\path\to\some\dir
```

It writes `<project>/.opendust/bridge-editor.json` and serves `engine.ping`, `echo`, and `fail`.
Start Claude Code in that project directory and call `mcp__opendust__echo`.

## Tests

```
.venv\Scripts\python -m pytest
```

Tests cover discovery search and precedence, schema rejection, hello and token rejection, dynamic
tool listing, call forwarding and error mapping, capabilities-changed re-listing, reconnect after an
engine restart, and the no-engine status path. They run entirely in-process against the fake engine.

## Troubleshooting

- **Only `opendust.status` is listed.** No discovery file was found. Run the status tool; it prints
  the path it last tried and the last error. Check that the editor has the project open and that
  `<project>/.opendust/bridge-editor.json` exists.
- **`engine closed the connection during session.hello`.** Token mismatch: the discovery file is
  stale. Restart the bridge (it re-reads the file) or restart the editor.
- **`unsupported bridge schema`.** The engine speaks a newer protocol than this bridge. Update
  `tools/opendust-mcp`.
- **Tools time out.** The engine polls the socket on its main thread; a paused or hung editor won't
  answer. Default request timeout is 120s.
