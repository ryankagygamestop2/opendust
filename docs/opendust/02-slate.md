# The slate

Status: DRAFT 0.1, 2026-09-29. Module: `modules/opendust_slate`.

## What it is

A held object in the game world, in the spirit of the Sheikah Slate: you raise it, its screen comes
up, you talk to it. What's on the other side is a Claude Code session whose working directory is the
room you're standing in. It can read and write that room's files, and, because it's launched with the
OpenDust MCP bridge attached, it can also change the world around you through the same tools the
editor agents use.

## What it is not

Not a VT100. A terminal emulator is a large, finicky piece of software and its output is the wrong
shape for a hand-held object in a game. Claude Code has a structured streaming mode and the slate
speaks that. A real PTY shell will be an OpenDust OS app later, running in a proper window on a
device. The slate stays a conversation surface.

## Classes

### `ClaudeSession` (RefCounted)

Owns one `claude` child process.

- `start(cwd: String, options: Dictionary) -> Error`
  Launches `claude -p --input-format stream-json --output-format stream-json --verbose
  --include-partial-messages [--permission-mode X] [--mcp-config path] [--resume id]` via
  `OS::execute_with_pipe(path, args, /*blocking=*/false)`. `cwd` is set by launching through a
  small platform shim, since `execute_with_pipe` does not take a cwd: on Windows
  `cmd /d /s /c "cd /d <cwd> && claude …"`, on Unix `sh -c 'cd <cwd> && exec claude …'`. The claude
  executable path is a project setting `opendust/slate/claude_path` (default: found on PATH).
- `send_user_message(text: String, attachments: Array = [])` writes one JSON line to stdin:
  `{"type":"user","message":{"role":"user","content":[{"type":"text","text":…}]}}`.
- A reader `Thread` blocks on the stdout pipe, splits lines, parses JSON, and pushes events onto a
  mutex-guarded queue. `poll()` (called from the owning node's `_process`) drains the queue on the
  main thread and emits signals. Nothing but `poll()` ever touches the scene tree.
- Signals: `started(session_id)`, `text_delta(text)`, `message_complete(message: Dictionary)`,
  `tool_use(id, name, input: Dictionary)`, `tool_result(id, content, is_error)`,
  `result(summary: Dictionary)`, `permission_request(request: Dictionary)`, `stderr_line(text)`,
  `exited(code)`.
- `respond_permission(request_id, allow: bool, updated_input?)` for interactive permission mode.
- `stop()` sends EOF / kills the process; `is_running()`.
- `session_id` is captured from the `system.init` event and exposed so a slate can `--resume` after
  the player puts it down and picks it up again. That id is *not* the durable identity of the agent
  in the slate; the `agent_id` is. The session id is a cache.

Event parsing follows Claude Code's stream-json shapes: `system` (init), `assistant` (message with
content blocks; partial deltas when `--include-partial-messages`), `user` (tool results), `result`.
The parser is permissive: unknown event types are emitted raw on `raw_event(dict)`.

### `SlatePanel` (Control)

The 2D UI drawn on the slate's screen. Plain Godot controls, themed dark, monospace where it
matters. Regions: transcript (RichTextLabel, streaming), tool cards (collapsed by default: tool
name, one-line input summary, expand to see result), input line, status strip (room name, cwd,
session state, agent name). It binds to a `ClaudeSession` and to a `Room`.

Keyboard focus works when the slate is raised. A project can replace the panel with its own scene
(`AgentSlate3D.panel_scene`).

### `AgentSlate3D` (Node3D)

The held object. A `MeshInstance3D` quad (or any mesh you assign) with a `SubViewport` rendering
the `SlatePanel` into a `ViewportTexture`. Properties: `panel_scene`, `screen_size_px`
(default 1024×768), `raised` (bool, animates up/down), `permission_mode`
(`default|acceptEdits|plan|bypassPermissions`, default `default`), `attach_bridge` (bool, default
true: passes `--mcp-config` pointing at the runtime bridge's generated MCP config), `agent_name`,
`soul_path`, `auto_start`, `resume_last_session`.

It finds its `Room` by walking up the tree. If there is none, the project directory is the room.

When `raised` it captures input, forwards typed text to the panel, and routes `ui_cancel` to lower
the slate. Interaction with the world (pointing the slate at a node and asking about it) is done by
the session calling `world.*` tools, not by special-casing in the slate.

### `Room` (Node3D)

- `room_id: String` (stable, human-legible, e.g. `"ollie-room"`)
- `room_dir: String` (absolute path or `res://…`; resolved to absolute for the session)
- `display_name: String`
- Registers itself with `OpenDustToolRegistry`'s world room list on enter/exit tree.

A `Room` creates `room_dir` if it doesn't exist and writes a `.opendust-room.json` marker with the
`room_id` and the scene path, so a Claude session started there can tell which room it is in by
reading the file. That's the whole "room memory" mechanism for now: a directory and a marker.

## MCP config for the slate's session

The runtime `OpenDustAgentServer` writes `<project>/.opendust/mcp-runtime.json`:

```json
{"mcpServers":{"opendust":{"command":"python","args":["<repo>/tools/opendust-mcp/opendust_mcp.py",
  "--discovery","<project>/.opendust/bridge-runtime.json"]}}}
```

The slate passes that with `--mcp-config`. The agent in the slate therefore has `world.*`,
`agent.*`, and `os.*` tools and can act on the world it is being held in, within the runtime policy.

## Demo

`demo/slate_room/`: one room, one floor, a player with a slate, a Room node pointing at the demo
directory itself. Press E to raise the slate, type, watch it answer. This is the first thing a
family member should be able to open and run after building.

## Later

- Voice in and out (the session's text is already structured; TTS/STT are attachments).
- A "point and ask" ray from the slate that pre-fills `world.node_get`.
- Multiple sessions per slate (tabs), each with its own cwd.
- Handoff: put the slate down, another player picks it up, same session, new `agent.name`.
