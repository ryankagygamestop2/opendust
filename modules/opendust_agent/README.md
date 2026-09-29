# modules/opendust_agent

The agent bridge. Implements `docs/opendust/01-agent-bridge-protocol.md`.

## Files

| file | what |
|---|---|
| `opendust_tool_registry.*` | `OpenDustToolRegistry` singleton: tools, flags, JSON-RPC error codes, room registry. Bound to GDScript. |
| `opendust_agent_server.*` | `OpenDustAgentServer` (Node): loopback `TCPServer` + `WebSocketPeer`, JSON-RPC 2.0, `session.hello` token auth, discovery + MCP config files, policy, log capture, `engine.*` tools. |
| `opendust_json.*` | Wire encoding: tagged `{"$type": …}` values ↔ `Variant`, coercion to property types. |
| `opendust_node_utils.*` | Tree/node serialization, path resolution, viewport capture (PNG base64). |
| `opendust_runtime_tools.*` | `world.*` tools for running games. |
| `editor/opendust_editor_plugin.*` | Starts the editor bridge, registers editor tools, forwards editor events. |
| `editor/opendust_agents_dock.*` | The **Agents** dock (right-bottom-left slot). |
| `editor/opendust_editor_tools*.cpp` | `project.* scene.* node.* script.* resource.* run.* editor.*` against `EditorInterface` / `EditorUndoRedoManager`. |
| `register_types.*` | Registration; runtime autostart. |

## Settings

| setting | default | meaning |
|---|---|---|
| `opendust/agent/port` | `0` | 0 = 6120 (editor) / 6121 (runtime). Next 16 ports tried if busy. |
| `opendust/agent/policy` | `""` | `open` (editor default), `no_mutations` (runtime default), `allowlist`. |
| `opendust/agent/allowed_tools` | `[]` | Tools allowed under `allowlist`, or exempted under `no_mutations`. `world.node_call` and `os.drive_write` always need an entry at runtime. |
| `opendust/agent/runtime_autostart` | `true` | Start the runtime bridge when a game starts. |
| `opendust/agent/mcp_bridge_script` | `""` | Path to `opendust_mcp.py` for the generated MCP config; empty = `<exe dir>/../tools/opendust-mcp/opendust_mcp.py`. |

## Tools registered here

Editor: `engine.info engine.capabilities engine.ping editor.log_tail`
`project.settings_get project.settings_set project.files_list project.file_read project.file_write project.rescan`
`scene.list_open scene.open scene.new scene.save scene.save_all scene.close scene.reload scene.tree scene.select scene.selection scene.instantiate`
`node.create node.delete node.rename node.reparent node.duplicate node.get node.set node.call node.list_properties node.attach_script node.detach_script node.set_owner_editable`
`script.read script.write script.create script.errors script.open_in_editor`
`resource.load resource.create resource.set resource.save`
`run.play run.stop run.state run.capture`
`editor.capture editor.undo editor.redo editor.history editor.inspect editor.set_main_screen editor.command`

Runtime: `engine.* editor.log_tail world.tree world.rooms world.room_of world.capture world.time world.node_get world.node_call`

`agent.*` and `os.*` come from `opendust_soul` and `opendust_os`.

## Events

`event.scene_changed {scene, reason: opened|edited|saved|closed}`, `event.selection_changed {nodes}`,
`event.run_state {state, scene?}`, `event.log {level, text, source, time_ms}`,
`event.capabilities_changed {version}`.

## Notes and known gaps

- `run.capture` in editor mode returns `-32002`: the running game is another process. Use the runtime bridge's `world.capture`.
- `scene.close` goes through the command palette (`editor/close_scene`), so an unsaved scene shows the editor's confirmation dialog.
- `scene.new` requires a `path`; the scene is created by packing a root and saving, then opening.
- `node.call` is not undoable; it marks the scene unsaved.
- Only `event.log` is buffered across threads; everything else runs on the main thread in `NOTIFICATION_INTERNAL_PROCESS`.
- The bridge is loopback-only and unauthenticated connections are closed on a bad token.
