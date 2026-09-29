# opendust_os

OpenDust OS skeleton. Spec: `docs/opendust/03-os-layer.md`. Identity notes: `05-identity.md`.

## What's here

| file | class | role |
|---|---|---|
| `world_drive.*` | `WorldDrive` | shared per-world filesystem on a real directory; root escape is unauthorable (`_resolve()` fails closed) |
| `os_kernel.*` | `OSKernel` | one per world; drive, app registry, sessions, message bus with per-id queues |
| `opendust_identity.*` | `OpenDustIdentity` | pods.global SSO consumer, **stub**: local session only, `sign_in()` → `ERR_UNAVAILABLE` |
| `os_app_context.*` | `OSAppContext` | handed to apps: kernel, device, shell, session, drive, args, `open()`, `notify()` |
| `os_shell.*` | `OSShell` | status bar + one fullscreen app; procedural |
| `os_apps.*` | `OSApp`, `OSHomeApp`, `OSFilesApp`, `OSNotesApp`, `OSMessagesApp` | base class and the four built-in apps |
| `device.*` | `Device`, `Device2D` | 3D screen quad / 2D control that boots the OS and registers with the kernel |
| `os_tools.*` | `OSTools` | registers `os.devices`, `os.open_app`, `os.drive_list`, `os.drive_read`, `os.drive_write`, `os.message` on `OpenDustToolRegistry` (guarded by `MODULE_OPENDUST_AGENT_ENABLED`) |

## Project settings

- `opendust/world_id` (default `default`)
- `opendust/os/drive_root` (default empty → `user://opendust/worlds/<world_id>/drive`)

## Try it

Add a `Device` to any 3D scene and run. The kernel appears under the root, the home screen shows
Files / Notes / Messages. Add a second `Device`: same drive, same bus. From GDScript:

```gdscript
var k := OSKernel.get_singleton()
k.get_drive().write_text("notes/hello.md", "hi from the world")
k.send({"to": "dev-tablet", "body": "ping"})   # queues if no such device yet
```

## Not here yet (by design, see the spec's open questions)

- pods.global OIDC/PKCE flow (`OpenDustIdentity` stub)
- `$SKU` inventory registry and `Holdable` (`OSAppContext.open("$SKU…")` shows a placeholder)
- `os.terminal` (a real PTY app) and `os.soul`
- drive sync to pods.global object storage
- mounting a `Room`'s `room_dir` into the drive
- large-device tiled windows (the shell is single-app)
- input routing from the 3D world to the screen quad (use `Device.push_input`)
