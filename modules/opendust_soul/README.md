# opendust_soul

Souls and bodies. Spec: `docs/opendust/04-embodiment.md`.

| file | what |
|---|---|
| `soul.h/.cpp` | `Soul` resource: name, pronouns, rig, temple, strata (seed dictionaries), texture, certificate |
| `soul_parser.h/.cpp` | Tolerant Markdown parser for strata-form `soul.md`; reads `certificate.json` beside it |
| `soul_loader.h/.cpp` | `ResourceFormatLoaderSoul`: `load("res://rooms/ollie/soul.md")` returns a `Soul` |
| `agent_body.h/.cpp` | `AgentBody` (CharacterBody3D): eyes, bubble, nav, avatar, perceive/move/say/hold, state save/restore |
| `soul_session_prompt.h/.cpp` | `SoulSessionPrompt.build(soul, body, tools, house_rules)`: the short system prompt a soul-session runs with |
| `agent_tools.h/.cpp` | Registers `agent.*` on `OpenDustToolRegistry` when `opendust_agent` is present; session→body attachment |
| `avatars/` | Placeholder male/female avatar scenes with `idle` and `wave` |
| `tests/` | Parser unit tests (`scons tests=yes`, then `bin/opendust… --test --test-case="*Soul*"`) |
| `doc_classes/` | `Soul.xml`, `AgentBody.xml` |

## Project settings

- `opendust/world_id` (default `"default"`): namespace for saved body state.
- `opendust/agent/allow_spawn` (default `false`): lets `agent.spawn` instantiate bodies over the bridge.
- `opendust/agent/allow_attach_any_body` (default `false`): lets a session attach to an identified body whose `agent_id` differs from the session's.

## Quick use from GDScript

```gdscript
var soul: Soul = load("res://rooms/ollie/soul.md")
var body := AgentBody.new()
body.soul = soul
add_child(body)
body.say("morning")
body.move_to($Workbench)
print(body.perceive())
print(SoulSessionPrompt.build(soul, body))
```

## What's not here yet

- The local `soul.new` wizard (`SoulNewWizard`), the editor "New Soul…" menu item and the "Spawn from soul…" dock action. These need the Agents dock from `opendust_agent` and the OS app shell from `opendust_os` to have somewhere to live.
- Hooking a spawned body to a `ClaudeSession` (slate module) so it actually talks. `SoulSessionPrompt` is the piece this module contributes to that.
- Reading the mirror texture section to set hair/eye/build parameters on a richer avatar.
- TTS on `say`.
- Compaction (writing seeds between strata). Deliberately absent; see the spec.
