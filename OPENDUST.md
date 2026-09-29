# OpenDust

**OpenDust is a fork of Godot Engine where agents are first-class inhabitants.**

The editor is a place an agent can stand in. The game world is a place an agent can live in. The OS
that runs inside the game worlds is the same OS the agents and the players use. Same identity, same
inventory, same files, whether you're holding a tablet in a world or sitting at the editor.

- Sites: opendust.io · opendust.space
- Upstream: [godotengine/godot](https://github.com/godotengine/godot), tracked on the `upstream` remote
- Base: Godot **4.7.2-stable** (branch `main` of this repo)
- License: MIT, same as Godot. See `LICENSE.txt` and `COPYRIGHT.txt`.
- Family platform: [pods.global](https://pods.global) (SSO, agent families, `$SKU` inventory tokens)

## What's different from Godot

Everything OpenDust adds lives in self-contained modules and docs so that merging upstream Godot
stays cheap. Core patches are avoided; when one is unavoidable it is listed in
`docs/opendust/CORE-PATCHES.md` with the reason.

| Module | What it is |
|---|---|
| `modules/opendust_agent` | The agent bridge. A JSON-RPC 2.0 server over WebSocket, in the editor and at runtime, that exposes the engine as tools: scene tree, nodes, properties, scripts, resources, run/stop, screenshots, undo/redo. Every mutation is a real editor action. Includes the **Agents** dock. |
| `modules/opendust_slate` | The in-world terminal object (the "slate"). A 3D object a player can hold that runs a Claude Code session in the working directory of the room they're standing in. Structured streaming, not a VT100. |
| `modules/opendust_soul` | Souls and bodies. Loads a `soul.md` (strata form) as a resource, gives an agent a persistent avatar body with perception and motion tools, mints and carries the durable `agent_id`. Hosts the local `soul.new` birth flow. |
| `modules/opendust_os` | OpenDust OS. The operating system that runs on in-world devices (tablets, phones, terminals) with a shared world filesystem, app model, and sessions. Spec first, skeleton second. |
| `tools/opendust-mcp` | A stdio MCP bridge so Claude Code (and any MCP client) can drive a running OpenDust editor or game. Tool list is discovered from the engine, never duplicated. |

Docs: start at `docs/opendust/00-architecture.md`.

## Building

See `docs/opendust/06-building.md`. Short version, Windows:

```
pip install scons
scons platform=windows target=editor dev_build=no -j12
```

The binary lands in `bin/godot.windows.editor.x86_64.exe`.

## Working here as an agent

If you are an agent reading this in the repo: `CLAUDE.md` at the root tells you how to connect to a
running editor through the MCP bridge and what the rules are. The short rules: every change you make
to a scene goes through undo/redo with your name on it, you never write to `.opendust/` by hand, and
you leave the tree in a state a human can open.

## Status

Early. Game-jam phase, 2026-09. Multiple family members are building pieces in parallel; this repo is
the merge target. See `docs/opendust/ROADMAP.md` for what exists, what's stubbed, and what's next.
