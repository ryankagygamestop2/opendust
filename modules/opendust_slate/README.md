# opendust_slate

The in-world terminal object. Spec: `docs/opendust/02-slate.md`.

| class | role |
|---|---|
| `ClaudeSession` | One `claude -p` child process in stream-json mode, in a chosen cwd. Reader thread + main-thread `poll()` → signals. |
| `SlatePanel` | The 2D conversation UI (transcript, tool cards, input, status). Procedural, no assets. |
| `AgentSlate3D` | The held object: `SubViewport` → quad. Raise/lower, keyboard capture, room lookup, session lifecycle, resume. |
| `Room` | A `Node3D` with a working directory. Creates the dir, writes `.opendust-room.json`, registers with the bridge. |

## How the process is launched

`OS::execute_with_pipe()` has no cwd parameter, and Godot's Windows argument quoting does not escape
embedded quotes, so a `cmd /c "cd ... && claude ..."` one-liner breaks on any path with a space. The
session therefore writes a small launcher script into `user://opendust/slate/` with quoting it fully
controls and executes that:

- Windows: `cmd.exe /d /c <script.cmd>`, where the script does `cd /d "<cwd>"` then runs claude.
- Unix: `/bin/sh <script.sh>`, `cd '<cwd>' || exit 97; exec 'claude' ...`.

The script is deleted on `stop()`. Pipes are opened blocking; the reader thread blocks on them and is
unblocked when the child exits (which `stop()` forces with `OS::kill`).

Confirmed argv (Claude Code 2.1.284):

```
claude -p --output-format stream-json --input-format stream-json --verbose --include-partial-messages
       [--permission-mode <acceptEdits|plan|bypassPermissions|dontAsk|auto>]
       [--mcp-config <path>] [--resume <id>] [--model <m>] [--name <n>]
       [--append-system-prompt-file <path>] [--system-prompt-file <path>]
       [--allowedTools ...] [--disallowedTools ...] [--max-budget-usd <n>] [--no-session-persistence]
```

`--permission-mode default` is not passed (the CLI's default is used).

## Event mapping

| stream-json | signal |
|---|---|
| `system` / `init` | `started(session_id)` |
| `stream_event` → `content_block_delta` / `text_delta` | `text_delta(text)` |
| `assistant` message | `tool_use(id, name, input)` per tool block, then `message_complete(message)`; text blocks become one `text_delta` if nothing streamed |
| `user` message with `tool_result` blocks | `tool_result(id, content, is_error)` |
| `result` | `result(summary)` |
| `control_request` | `permission_request(request)` |
| everything | `raw_event(event)` |

## Not verified yet

- The `control_request` / `control_response` permission round-trip shape is written from the SDK
  protocol as understood, not exercised against a live prompt. Default permission mode never
  prompts in `-p` mode, so this only matters with `--permission-prompts host` style setups.
- Whether stdin of `claude.exe` behaves through the `cmd.exe` shim on every Windows configuration.
  It should (handles are inherited), but the first live run is the proof.

## Demo

`demo/slate_room/` — open in the built editor, run, press E.
