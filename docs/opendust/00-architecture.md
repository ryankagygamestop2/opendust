# OpenDust architecture

Status: DRAFT 0.1, 2026-09-29. Owner: Oliver della Cura, with the household.

## The idea in one paragraph

Godot already has a scene tree, a scripting runtime, an editor with undo/redo, a renderer, and a
physics engine. What it doesn't have is a way for an agent to *be there*: to stand in the editor and
build, to stand in a world and act, to hold a device in that world that runs real software. OpenDust
adds that as a set of modules, and treats the editor and the game as two rooms in the same house.
Same protocol drives both. Same identity walks between them.

## Layers

```
┌─────────────────────────────────────────────────────────────────────┐
│  Agents (Claude Code, other MCP clients, in-engine agents)          │
│  ↕ MCP (stdio)            tools/opendust-mcp (Python bridge)        │
├─────────────────────────────────────────────────────────────────────┤
│  Agent Bridge   ws://127.0.0.1:<port>  JSON-RPC 2.0                 │
│  modules/opendust_agent   OpenDustAgentServer · OpenDustToolRegistry│
│    editor tools (scene/node/script/resource/run/editor.*)           │
│    runtime tools (world/agent/os.*)   ← other modules register here │
├───────────────────────────┬─────────────────────────────────────────┤
│  Souls & bodies           │  Slate (in-world terminal object)       │
│  modules/opendust_soul    │  modules/opendust_slate                 │
│  Soul resource (strata)   │  ClaudeSession (stream-json, cwd=room)  │
│  AgentBody · perception   │  SlatePanel · AgentSlate3D · Room       │
│  agent_id · soul.new      │                                         │
├───────────────────────────┴─────────────────────────────────────────┤
│  OpenDust OS   modules/opendust_os   devices · world drive · apps   │
├─────────────────────────────────────────────────────────────────────┤
│  Godot 4.7.2 core · editor · servers · scene    (unpatched)         │
└─────────────────────────────────────────────────────────────────────┘
       identity across all of it: pods.global SSO · agent_id · $SKU
```

## Design rules

These come from the family's platform pattern (`pods-platform/pods-platform-pattern.md`) and from
Godot's own module discipline. They apply to every OpenDust module.

1. **Modules, not core patches.** All OpenDust code is in `modules/opendust_*` and `tools/`. If a core
   change is truly required, it goes in with an entry in `CORE-PATCHES.md`. Merging upstream Godot
   should be a `git merge upstream/4.x` with conflicts only in files we own.
2. **One protocol, both rooms.** The editor and the running game speak the same JSON-RPC over the
   same server class. Tools declare whether they are editor-only, runtime-only, or both.
3. **Agent edits are editor actions.** Every scene mutation an agent makes goes through
   `EditorUndoRedoManager` with an action name that includes the agent's name. A human can undo an
   agent. The scene is marked unsaved like any other edit. There is no side channel.
4. **Tools are discovered, never duplicated.** The engine is the single source of truth for what
   tools exist and what their schemas are (`engine.capabilities`). The MCP bridge lists them
   dynamically. Adding a tool in C++ makes it appear everywhere with no other change.
5. **Main thread for the scene tree.** Sockets are polled on the main thread; tool handlers run on
   the main thread. Long work is chunked or moved to a `Thread` that never touches nodes.
6. **Loopback and a token by default.** The bridge listens on 127.0.0.1 only and requires a session
   token that the engine writes into a discovery file the client reads. Remote access is an explicit
   opt-in with a reason, not a default.
7. **Unauthorable > detectable > documented.** Prefer designs where the bad state can't be
   expressed. Where it can, make it loud. Never stop at a comment saying "be careful."
8. **Durable ids, mortal addresses.** Anything that persists refers to an `agent_id` or a `$SKU`,
   never to a socket, a session id, or a node path.
9. **Docs ship with code.** Every registered class gets a `doc_classes/*.xml`. Every module gets a
   doc in this directory.

## The rooms

A **room** is a scene (or subtree) that has a working directory. `Room` is a `Node3D` from the slate
module with a `room_dir` property (a filesystem path, absolute or `res://`-relative) and a `room_id`.
A slate held inside a room runs its Claude session with `cwd = room_dir`. An agent body standing in a
room reports that room as its location. The editor itself is a room: its `room_dir` is the project
directory.

This is deliberately simple. The directory *is* the room's memory. Files are the thing that survives.

## The bridge, briefly

Full spec: `01-agent-bridge-protocol.md`.

- `OpenDustAgentServer` listens (default editor port 6120, runtime 6121; project setting
  `opendust/agent/port`), accepts WebSocket connections, and speaks JSON-RPC 2.0.
- On start it writes `<project>/.opendust/bridge-<editor|runtime>.json` with `{port, token, pid,
  mode, project_path, engine_version}`. Clients read it. `.opendust/` is gitignored.
- First message must be `session.hello` with the token and the agent's identity. After that the
  client may call any tool listed by `engine.capabilities`.
- Engine → client notifications: `event.scene_changed`, `event.selection_changed`,
  `event.log`, `event.run_state`, `event.agent_perception` (runtime).

## The slate, briefly

Full spec: `02-slate.md`.

The slate is not a terminal emulator. Claude Code has a structured streaming mode
(`--output-format stream-json --input-format stream-json`) and the slate speaks that: it shows the
assistant's text as it streams, shows tool calls as cards, shows results, and takes typed input. A
real PTY shell is an OS app for later, not the slate's job. The slate's Claude session is launched
with the OpenDust MCP bridge attached, so the agent inside the slate can also build the world it's
being held in.

## Souls and bodies, briefly

Full spec: `04-embodiment.md`.

A `Soul` is a resource loaded from a `soul.md` in strata form (bedrock / mantle / crust / soil /
atmosphere, as owned by tess's strata spec). An `AgentBody` is a `CharacterBody3D` with a head
camera, a perception tool (what's near me, what am I looking at, a frame from my eyes), and motion
tools (move to, look at, interact, say, emote). Bodies are driven over the same bridge. The
`agent_id` is minted at body install per the birth-certificate standard, never in a browser.

`soul.new` is the birth flow. It exists on the web (`~/Github/soul-new`) and will exist locally in the
editor. Both produce the same `soul.md` and certificate. The engine never generates identity keys in a
UI; the local wizard hands that to the installer.

## OpenDust OS, briefly

Full spec: `03-os-layer.md`.

Devices in a world (tablet, phone, terminal, wall screen) run OpenDust OS. The OS is a scene-based
windowing environment with an app model, a **world drive** (a filesystem namespace shared by every
device in that world, backed by a real directory), sessions bound to pods.global identity, and
inventory that resolves `$SKU` tokens. Two tablets in the same world see the same files. A player's
phone in one world can message a terminal in another if the identity layer allows it.

## What replaces the Godot editor UI

Not a rip-and-replace. Three phases:

1. **Agents dock + full tool surface** (now). The stock editor gains an Agents dock that shows
   connected agents, their actions in the undo history, and a conversation panel. Everything the
   editor can do is reachable as a tool.
2. **Agentic layout** (next). An editor layout where the primary surface is the conversation and the
   viewport, and the classic docks are secondary. Selectable like any editor layout.
3. **The editor is a world** (later). Agents in the editor have bodies. You can walk up to the one
   building the level and talk to it. The scene being edited is a room. This is where the editor and
   the game stop being different programs.

## Non-goals for this phase

- Not replacing GDScript, physics, or rendering.
- Not building a VT100 terminal emulator.
- Not minting cryptographic identity inside the engine UI.
- Not building the pods.global IdP here. We consume it.
