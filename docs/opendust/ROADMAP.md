# OpenDust roadmap

Updated 2026-09-29. Honest about what's real.

## Phase 0: foundation (this week)

- [x] Fork Godot 4.7.2-stable, `upstream` remote, `main` branch, branding in `version.py`
- [x] Architecture and protocol docs (`docs/opendust/00…06`)
- [x] `modules/opendust_agent`: server, registry, editor tools, Agents dock, discovery file
- [x] `tools/opendust-mcp`: stdio MCP bridge with dynamic tool discovery, tests with a fake engine
- [x] `modules/opendust_slate`: ClaudeSession, SlatePanel, AgentSlate3D, Room, demo project
- [x] `modules/opendust_soul`: Soul resource + loader, AgentBody, agent.* tools, session prompt
- [x] `modules/opendust_os`: skeleton (OSKernel, Device, OSShell, WorldDrive) + os.home/os.files
- [x] First Windows editor build passes with all modules on
- [x] Oliver connects to the built editor over the bridge and builds a scene from Claude Code (2026-09-29: headless editor, `node.create` recorded as "Oliver: Create Node3D", undo works; runtime bridge + MCP path verified; slate and UI not yet exercised in a windowed session)
- [x] GitHub: `gh auth login` on the build box, push `main`, wire the real fork remote

### Not yet verified (phase 0 leftovers)

- A windowed (non-headless) editor session: Agents dock visible, `editor.capture` returning pixels
- The slate actually spawning `claude` and streaming into the panel in the demo
- `AgentBody` spawn + `agent.perceive` with a real body in a scene
- OS shell rendering on the tablet's screen, apps opening
- C++ unit tests build (`tests=yes`) and the soul parser tests pass
- Linux/macOS builds

## Phase 1: agents live in the editor

- Tool groups gated by default (`engine.capabilities` returns groups; a session enables the ones it
  needs) so 53+ tools don't flood an agent's context. Prior art: "Godot MCP Pro" toolsets.
- Editor tools missing vs. community plugins: `node.connect_signal`, `node.disconnect_signal`,
  `node.add_to_group` / `node.remove_from_group` (AI Assistant Hub v2 already has these).
- Learned-world note: SIMA 2 trains inside Genie 3 worlds and the gains transfer (arXiv 2512.04797),
  so "engines are truth, learned worlds only generate" is too strong. The claim that survives:
  a world that has to be the same tomorrow, with files, items and other people in it, needs an
  engine. That's the world OpenDust is for.
- Coverage borrowed from the addon-based bridges (Sciumo/godot-mcp, hi-godot/godot-ai): signal
  wiring, input simulation for play-testing, material/animation/particle helpers. Study their tool
  lists before writing ours; see journal 2026-09-29 12:43.
- Evaluate the editor agent on GameDevBench (arXiv 2602.11103) once a windowed session works.
- Two seams for pixel-in/action-out agents (NitroGen, SIMA 2, Lumine all play through the human
  interface): (1) `world.capture` streaming at a fixed rate with an async readback path instead of
  a per-call synchronous `get_image()`; (2) `world.input {block:[{t_ms, action|key|mouse, ...}]}` to inject a short timed
  sequence of `InputEvent`s and return a capture at the end, policy-gated. Block-cycle control
  (GameWAM, arXiv 2608.26200): the agent sends an action block, the engine applies it over N
  frames, the agent replans from the new frame. One event at a time is the wrong granularity. See journal
  2026-09-29 13:13.
- godot-proposals #12409 turned out to be a title with no body, closed unread (May 2025). Upstream
  has never been asked properly. If the tool registry is ever worth upstreaming, write the proposal
  with `01-agent-bridge-protocol.md` attached.
- 4.7.2 vendors Jolt 5.5.0; Jolt 5.6 (Jul 2026) adds a faster friction model and determinism fixes.
  Track whether Godot 4.8 bumps it; determinism matters for replaying agent actions.

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
