# OpenDust OS

Status: DRAFT 0.1, 2026-09-29. Module: `modules/opendust_os`. Spec first; skeleton classes second;
apps third.

## What it is

The operating system that runs on devices inside OpenDust worlds. A tablet on a desk in a room, the
phone in a player's pocket, the wall terminal in the workshop, the slate: these are **devices**. Each
device runs OpenDust OS. All devices in a world share one **world drive**. All devices, in every
world, share one **identity** (pods.global) and one **inventory** (`$SKU`). So a note you write on
the tablet is on the phone. A file the agent in the slate creates is on the wall terminal. A thing
you're holding is the same thing in every world that knows how to render it.

The OS is also what the editor becomes in phase 3 of the architecture (the editor as a world): the
editor's docks are OS apps running on a very large device.

## Concepts

| term | meaning |
|---|---|
| **Device** | A node in a world with a screen (SubViewport) that boots OpenDust OS. `Device` extends `Node3D`; `Device2D` exists for 2D worlds and for HUD use. |
| **Kernel** | The per-world singleton (`OSKernel`, an autoload-style node added by the module) that owns the world drive, the app registry, sessions, and the message bus. One per running world. |
| **World drive** | A filesystem namespace `drive://<world_id>/…` backed by a real directory (`user://opendust/worlds/<world_id>/drive` by default, or a `Room`'s `room_dir`, or a pods.global object-store prefix when online). Every device sees the same tree. |
| **App** | A `PackedScene` registered with the kernel under an app id (`os.notes`, `os.files`, `os.terminal`, `os.messages`, `os.inventory`, `os.soul`). Apps are `Control` scenes that receive an `OSAppContext`. |
| **Session** | A signed-in identity on a device: `{family_id, user_id | agent_id, display_name, scopes}`. Comes from pods.global SSO when online, from a local family cache when offline. Devices can be shared; sessions are per-user. |
| **Message** | `{from, to, body, attachments, ts}` on the kernel bus. `to` is a durable id (`agent_id`, `user_id`, `device_id`), never a node path. Undeliverable messages queue against the id (see routing rules in `pods-platform/routing-resolution.md`). |
| **Inventory item** | A `$SKU` token reference. The OS resolves a `$SKU` to a render (icon, 3D mesh, app that opens it) via the inventory registry; it never invents an item. |

## Rules

1. **Durable ids only.** Messages, ownership, and sessions use `agent_id`/`user_id`/`device_id`.
   Node paths are cached for one frame at most.
2. **The drive is real files.** No virtual FS with its own format. `drive://` maps to a directory.
   Slates, agents, and apps see the same bytes. Sync to pods.global object storage is a background
   job keyed on the world id, never a different source of truth.
3. **Apps are scenes.** No app framework beyond `Control` + `OSAppContext`. A family member can make
   an OS app the same way they make any UI scene.
4. **No login screens in apps.** Apps ask the kernel for the session. The kernel talks to
   pods.global. Same rule as the platform pattern: apps never run their own auth.
5. **Offline is normal.** A world must run with no network. Sessions degrade to the local family
   cache; the drive stays local; sync resumes later. Nothing blocks on the network on the main thread.
6. **Inventory is read-through.** A `$SKU` the local registry doesn't know renders as an "unknown
   item" placeholder with the token visible; it never crashes or gets dropped.

## Classes (skeleton for this phase)

- `OSKernel` (Node, one per world, created by the module when a world has any `Device`):
  `world_id`, `drive_root` (absolute), `register_app(id, scene)`, `get_session(device)`,
  `send(message)`, signals `message_received`, `drive_changed(path)`.
- `Device` (Node3D): `device_id`, `screen_size_px`, `boot_app` (default `os.home`), `session`
  (read-only), `open_app(id, args)`, `is_awake`. Owns a `SubViewport` + `OSShell` root Control.
- `OSShell` (Control): the window manager. Home screen with app grid, one app fullscreen at a time
  on small devices, tiled windows on large ones. Status bar: time, session, world.
- `OSAppContext` (RefCounted): handed to each app: `kernel`, `device`, `session`, `drive`,
  `args`, `open(sku_or_path)`, `notify(text)`.
- `WorldDrive` (RefCounted): `list(path)`, `read(path)`, `write(path, bytes)`, `watch(path)`. Thin
  wrapper on `DirAccess`/`FileAccess` rooted at `drive_root`, with path normalization and
  no escapes above the root (unauthorable, not checked-after).

## First apps

- `os.home`: app grid + clock + who's signed in.
- `os.files`: browse the world drive; open text files in `os.notes`; open `$SKU` items in their app.
- `os.notes`: a text editor. Autosave to the drive.
- `os.messages`: the kernel bus, as a chat. Talk to any agent or user by name.
- `os.terminal`: **later**, a real PTY terminal app. This is where a VT emulator belongs.
- `os.soul`: view a `Soul` resource (strata + texture) for any agent in the world who allows it.

## Tools registered on the bridge (runtime)

- `os.devices` → `{devices:[{device_id, node_path, room_id?, awake, session?}]}`
- `os.open_app {device_id, app_id, args?}`
- `os.drive_list {path}` · `os.drive_read {path}` · `os.drive_write {path, text}` (policy-gated)
- `os.message {to, body}` → `{queued: bool, delivered: bool}`

## Identity and $SKU

See `05-identity.md`. The OS is a consumer of both. It holds no secrets beyond a session token in
memory and a family cache on disk that the platform's rules cover.

## Open questions (need operator)

- Drive sync target: MinIO/R2 prefix per world, or per family with world subprefix?
- Does a `Room`'s `room_dir` mount into the world drive as `drive://<world>/rooms/<room_id>`
  (proposal: yes), or stay separate?
- Which devices are allowed to run `os.terminal` with a real shell? Proposal: only devices whose
  session has the `operator` scope.
