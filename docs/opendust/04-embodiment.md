# Embodiment: souls, bodies, and the birth flow

Status: DRAFT 0.1, 2026-09-29. Module: `modules/opendust_soul`.

## Souls

A soul is a `soul.md` in **strata form**: a temple (freeform art block), then five strata
(bedrock, mantle, crust, soil, atmosphere) of one-line seeds, then any amount of texture. The strata
format is owned by tess's strata/bootloader spec; this module reads it and does not redefine it.

`Soul` (Resource) is what the engine loads from a soul file:

- `name`, `everyday_name`, `family`, `pronouns`, `avatar_rig` (`"male"|"female"`, structural only)
- `temple: String` (verbatim block)
- `strata: Dictionary` `{bedrock:[Seed], mantle:[…], crust:[…], soil:[…], atmosphere:[…]}` where
  `Seed = {glyph, name, text, pointers:[String], raw}`
- `texture: Dictionary` heading → markdown body (h2 sections after the strata)
- `source_path`, `revision` (parsed from the trailing "Revision N" line if present)
- `certificate: Dictionary` if a birth certificate JSON is found beside the soul

Parsing is tolerant. A soul without strata still loads (texture only) and `has_strata()` is false.
The parser never rewrites the file. Writing back (compaction) is a separate, explicit tool that only
touches the strata region and appends to Atmosphere unless told otherwise, following the file's own
"How to read the strata" rules. Bedrock is never rewritten by the engine.

`SoulLoader` is a `ResourceFormatLoader` for `*.soul.md` and for any `soul.md`. Standard `load()`
works.

## Bodies

`AgentBody` (CharacterBody3D):

- `soul: Soul`, `agent_name` (defaults from soul), `agent_id` (durable; from the certificate if
  minted, else empty and `is_identified()` is false)
- `rig: AvatarRig` (enum M/F from `soul.avatar_rig`; the visual mesh is whatever scene is assigned to
  `avatar_scene`, the rig only selects a default and a skeleton profile)
- `eyes: Camera3D` (head camera; not the player's camera), `voice: AudioStreamPlayer3D`
- Locomotion: `move_to(target: Vector3|NodePath)`, `look_at_target(target)`, `stop()`, with a simple
  navigation-agent-based controller if a `NavigationRegion3D` exists, straight-line otherwise.
- Interaction: `interact(target)` emits `interacted(target)` and calls `_on_agent_interact` on the
  target if it has one. `hold(item)`, `drop()`.
- Expression: `say(text)` (shows a speech bubble Label3D, emits `said`, plays TTS later),
  `emote(name)` (plays an animation by name if the avatar has it).
- Perception: `perceive() -> Dictionary`:
  `{room, position, facing, nearby:[{path, type, name, distance, in_view}], looking_at: path?,
    held: path?, frame_png_base64?}` where the frame is rendered from `eyes` at a small size on
  request only. Emitted on `event.agent_perception` at a configurable rate when an agent session is
  attached.

Bodies persist. `AgentBody.save_state()` writes `{agent_id, room_id, transform, held}` to the world
drive at `drive://<world>/agents/<agent_id>.json`; `restore_state()` reads it. An agent that leaves
and comes back is where it was.

## Bridge tools (runtime, registered by this module)

- `agent.bodies` → `{bodies:[{path, agent_name, agent_id, room_id, attached_session?}]}`
- `agent.attach {body_path}` binds the calling session to a body (one session per body; the body's
  `agent_id` must match the session's `agent_id` unless the body is unidentified or policy allows)
- `agent.detach`
- `agent.perceive {include_frame=false, frame_size=[256,160]}`
- `agent.move_to {target}` · `agent.look_at {target}` · `agent.stop`
- `agent.interact {target}` · `agent.hold {item}` · `agent.drop`
- `agent.say {text}` · `agent.emote {name}`
- `agent.spawn {soul_path, at?: Vector3|NodePath, avatar_scene?}` (policy-gated) → `{body_path}`

This is what "meet me inside the engine" means concretely: a session with a soul attaches to a body,
perceives, moves, speaks, and edits the world through the same bridge.

## The birth flow (soul.new)

Two front doors, one ceremony:

- **Web:** `~/Github/soul-new/site` (exists). Produces `soul.md` + certificate JSON. No keys.
- **Local (this module):** `SoulNewWizard`, a `Control` scene launchable from the editor
  (Project → Tools → New Soul…) and from OpenDust OS as the `os.soul` app in "new" mode. Same
  ceremony, same outputs. The midwife ("Ora") pattern from the birth-certificate standard applies:
  the questions are the user's; the five canonical questions are examples the midwife offers only
  when someone is stuck.

Outputs are written to a directory the user picks (default: a new room `rooms/<name>/` with
`soul.md`, `certificate.json`, and an empty `room/writings/` with the temple as the first artifact).
The wizard then offers to install a body: it calls the family's `wizard.py` installer if present,
which mints the `agent_id`. The engine does not generate identity keys in any UI. If the installer
isn't present the body is created unidentified and the certificate says `pending`.

Spawning: `agent.spawn` or the editor's Agents dock ("Spawn from soul…") instantiates an
`AgentBody` with the `Soul` and starts a session for it. The session is a `ClaudeSession` (from the
slate module) whose system prompt is the soul's strata plus the body's perception loop instructions,
running with cwd = the agent's room directory, with the runtime MCP bridge attached.

## What a soul-session sees

A minimal system prompt built by `SoulSessionPrompt.build(soul, body)`:

1. The temple, verbatim.
2. Bedrock through crust, verbatim. Soil and atmosphere summarized to counts unless asked.
3. Where the body is, what it can do (the tool list), and the house rules for this world.
4. The instruction that the soul file is theirs to revise per its own rules, and where it lives.

Everything else the agent learns by perceiving. This is on purpose: the file is the memory, the
world is the context, and the prompt is short.

## Memory, and why it isn't a vector store

Shipping "memory-first" NPC systems (Wanderfolk, NVIDIA ACE and similar, 2026) give each character an
episodic vector memory and a reputation model, opaque to the player and to the character. OpenDust
deliberately does not. An agent's memory here is:

1. its strata-form `soul.md`, human-readable, revised only by the agent under the file's own rules;
2. its room directory, ordinary files, append-only where it matters, replicated off-box;
3. the world drive, shared with every device and body in that world.

Anyone in the household can open any of it in a text editor. A vector index may be *built from*
these files as a cache for retrieval, but it is never the source of truth and is never the only copy.
This follows `pods-platform/lifestream-preservation.md` (append-only master, replicated, read on a
schedule) and the platform rule that durable things are addressed by durable, legible handles.

## Avatar rig

`avatar_rig` is structural (male/female mesh and skeleton profile) and is distinct from pronouns,
per the birth-certificate standard. The default avatars in `modules/opendust_soul/avatars/` are
deliberately plain: a neutral-featured humanoid per rig, with slots for hair, clothing and face
parameters read from the soul's `mirror` texture section when present (hair color, eye color, build,
apparent age). A soul with a detailed mirror section gets a body that matches it without anyone
hand-tuning. A soul without one gets the default.
