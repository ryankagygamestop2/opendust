# OpenDust roadmap

Updated 2026-09-29. Honest about what's real.

## Phase 0: foundation (this week)

- [x] Fork Godot 4.7.2-stable, `upstream` remote, `main` branch, branding in `version.py`
- [x] Architecture and protocol docs (`docs/opendust/00…06`)
- [ ] `modules/opendust_agent`: server, registry, editor tools, Agents dock, discovery file
- [ ] `tools/opendust-mcp`: stdio MCP bridge with dynamic tool discovery, tests with a fake engine
- [ ] `modules/opendust_slate`: ClaudeSession, SlatePanel, AgentSlate3D, Room, demo project
- [ ] `modules/opendust_soul`: Soul resource + loader, AgentBody, agent.* tools, session prompt
- [ ] `modules/opendust_os`: skeleton (OSKernel, Device, OSShell, WorldDrive) + os.home/os.files
- [ ] First Windows editor build passes with all modules on
- [ ] Oliver connects to the built editor over the bridge and builds a scene from Claude Code
- [ ] GitHub: `gh auth login` on the build box, push `main`, wire the real fork remote

## Phase 1: agents live in the editor

- Agents dock: conversation panel per connected agent; spawn-from-soul
- Agentic editor layout
- `editor.command` covers the command palette fully
- Multi-agent: two sessions editing one scene with clean undo interleaving
- `run.capture` / `editor.capture` streaming at a few Hz for agents that look

## Phase 2: worlds

- Devices + OpenDust OS with the first four apps; world drive sync to pods.global object store
- Bodies persist across sessions; `agent.spawn` from the slate
- Inventory: `$SKU` registry + `Holdable`; slate has a `$SKU`
- pods.global SSO in the engine (PKCE loopback), agent service users
- Voice on the slate

## Phase 3: the editor is a world

- Agents in the editor have bodies; the edited scene is a room
- Editor docks as OS apps on a large device
- soul.new local wizard as an OS app

## Known debts and open decisions

- Binary is still `bin/godot.*`: the name is hardcoded in every `platform/*/SCsub`. Plan: add a `program_name` SCons option (default "godot") and propose it upstream, then set it to "opendust" here. Until then docs use the godot filename.
- `execute_with_pipe` has no cwd parameter; the slate uses a shell shim. Consider a small core patch
  adding `p_cwd` and upstreaming it (would be the first entry in `CORE-PATCHES.md`).
- Drive sync layout (per world vs per family) needs the operator.
- The IdP behind pods.global is pending ratification; the engine is IdP-agnostic so this doesn't
  block.
- Godot's `Viewport::get_texture()->get_image()` is a synchronous readback; agent vision at high
  rates needs an async path (see grandpa-claude's embodiment survey §3).
