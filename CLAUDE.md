# OpenDust: notes for agents working in this repo

You are in a fork of Godot Engine called OpenDust. Read `OPENDUST.md` first, then
`docs/opendust/00-architecture.md`. The protocol you speak to a running editor is
`docs/opendust/01-agent-bridge-protocol.md`.

## Where things are

- `modules/opendust_agent/` the bridge server, tool registry, editor tools, Agents dock
- `modules/opendust_slate/` the in-world Claude terminal object, `Room`, `ClaudeSession`
- `modules/opendust_soul/` `Soul` resource, `AgentBody`, `agent.*` tools, soul.new local flow
- `modules/opendust_os/` OpenDust OS: kernel, devices, world drive, apps
- `tools/opendust-mcp/` the stdio MCP bridge Claude Code uses to reach the engine
- `docs/opendust/` all design docs; `ROADMAP.md` says what's real
- `demo/` small projects you can open in the built editor

Everything else is upstream Godot. Don't patch it without adding a row to
`docs/opendust/CORE-PATCHES.md`.

## Connecting to a running editor

1. Build (`docs/opendust/06-building.md`) and open a project in `bin/godot.windows.editor.x86_64.exe`.
2. The editor writes `<project>/.opendust/bridge-editor.json`.
3. `.mcp.json` at this repo root registers the `opendust` MCP server. Start Claude Code in the
   project directory (or pass `--discovery <path>` to the bridge) and the engine's tools appear as
   `mcp__opendust__*`.
4. Call `engine.info` first. Then `scene.tree`. Then build.

## Rules

- Every mutation you make in the editor is an undo/redo action with your name on it. Don't try to
  go around that.
- Never write into `.opendust/`. The engine owns it.
- Leave scenes openable by a human. If you're mid-change when you stop, say so.
- Tool schemas live in C++ next to the handler. Don't duplicate them in the bridge or in docs
  beyond the summary tables.
- Godot code style: clang-format config is upstream's. `#include` order: own header, module
  headers, then `core/`, `scene/`, `editor/`. Class names `OpenDust*`, files `opendust_*.cpp`.
- Doc XML for every registered class in `doc_classes/`.
- Building: `scons platform=windows target=editor -j12`. Incremental after touching one module is a
  couple of minutes. Don't run two builds at once in the same tree.

## Family conventions that apply here

From `~/Github/pods-platform/`:

- Unauthorable > detectable > documented. Never stop at documented.
- Durable ids (`agent_id`, `$SKU`) for anything persisted; addresses (session ids, node paths,
  sockets) are caches.
- Souls are strata-form `soul.md`; the engine reads them and never rewrites bedrock.
- No identity keys generated in a UI. Ever.
