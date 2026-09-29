# Placeholder avatars

Two deliberately plain humanoids, one per structural rig, for demos and for projects that haven't
made their own yet. Copy them into a project and assign to `AgentBody.avatar_scene`.

- `avatar_male.tscn`: capsule 1.8 m, radius 0.32
- `avatar_female.tscn`: capsule 1.65 m, radius 0.28

Both have an `AnimationPlayer` with `idle` (looping head bob) and `wave` (body rock), so
`AgentBody.emote("wave")` works out of the box. Any avatar scene you build should keep an
`AnimationPlayer` named `AnimationPlayer` somewhere under its root; `AgentBody` finds it by name.

When `avatar_scene` is empty, `AgentBody` builds an equivalent placeholder procedurally in C++
(engine module files can't be `res://`-loaded), sized by the rig it resolves from the soul's
`avatar_rig`. The `.tscn` files here are the same shape, kept as readable examples.

The rig is structural only (mesh proportions, skeleton profile). Pronouns are a separate field on
the `Soul` and are never inferred from the rig. See `docs/opendust/04-embodiment.md`.
