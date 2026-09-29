# Agent bridge protocol

Status: DRAFT 0.1, 2026-09-29. This is the contract between `modules/opendust_agent` (server),
`tools/opendust-mcp` (client), `modules/opendust_soul` and `modules/opendust_os` (tool providers),
and any other agent that wants to be in the engine. Change it here first.

## Transport

- WebSocket, text frames, one JSON-RPC 2.0 message per frame.
- Server binds `127.0.0.1` only. Port from project setting `opendust/agent/port` (editor default
  `6120`, runtime default `6121`). If the port is taken the server tries the next 16 and records the
  one it got in the discovery file.
- Server implementation uses Godot's `TCPServer` + `WebSocketPeer::accept_stream()`. Polled once per
  frame on the main thread.

## Discovery file

Written on server start, deleted on clean shutdown, overwritten on crash-restart:

```
<project_dir>/.opendust/bridge-editor.json
<project_dir>/.opendust/bridge-runtime.json
```

```json
{
  "schema": "opendust.bridge/1",
  "mode": "editor",
  "port": 6120,
  "token": "b6f1…",
  "pid": 12345,
  "project_path": "C:/Users/Goose/Github/opendust/demo/slate_room",
  "engine_version": "4.7.2.stable.opendust",
  "started_at": "2026-09-29T11:42:00Z"
}
```

`.opendust/` is gitignored at the repo root. The token is 32 random bytes, hex. Clients never write
this file.

## Session

First request on a connection must be:

```json
{"jsonrpc":"2.0","id":1,"method":"session.hello","params":{
  "token":"b6f1…",
  "agent":{"name":"Oliver","agent_id":null,"soul_ref":"pods-platform/soul.md","client":"opendust-mcp/0.1"}
}}
```

Response:

```json
{"jsonrpc":"2.0","id":1,"result":{
  "session_id":"s_01H…","mode":"editor","engine_version":"4.7.2.stable.opendust",
  "project_name":"slate_room","capabilities_version":3
}}
```

Any other method before a successful hello → error `-32001 not_authenticated`. Wrong token → the
server closes the socket. The `agent.name` is used in undo/redo action names ("Oliver: Add Node").

## Requests

Standard JSON-RPC 2.0. `method` is the tool name. `params` is an object matching the tool's
`inputSchema`. Results are objects. Errors use standard codes plus:

| code | name | meaning |
|---|---|---|
| -32001 | not_authenticated | hello not done |
| -32002 | wrong_mode | editor tool called at runtime or vice versa |
| -32003 | not_found | node/scene/resource/script path doesn't resolve |
| -32004 | invalid_target | e.g. can't delete the scene root, can't reparent into own subtree |
| -32005 | engine_busy | scene is being saved/reloaded; retry |
| -32006 | denied | tool is disabled by project setting / policy |

Error `data` always includes `{"tool": name, "detail": string}`.

## Capabilities

`engine.capabilities` → `{ "version": int, "mode": "editor"|"runtime", "tools": [ToolSpec] }`

```json
{"name":"node.create","description":"Create a node under a parent.",
 "inputSchema":{"type":"object","properties":{…},"required":[…]},
 "flags":["editor","mutates"]}
```

Flags: `editor`, `runtime`, `mutates`, `slow`. A tool with both `editor` and `runtime` works in both
modes. The MCP bridge converts each ToolSpec 1:1 into an MCP tool. Nothing about a tool is written
down twice.

The `version` increments whenever the tool set changes at runtime (a module registered or removed
tools). Clients should re-list when a `event.capabilities_changed` notification arrives.

## Notifications (server → client)

No `id`. Clients must tolerate unknown events.

| method | params |
|---|---|
| `event.scene_changed` | `{scene: path, reason: "edited"\|"saved"\|"opened"\|"closed"}` |
| `event.selection_changed` | `{nodes: [node_path]}` |
| `event.log` | `{level:"info"\|"warning"\|"error", text, source?}` |
| `event.run_state` | `{state:"stopped"\|"playing"\|"paused", scene?}` |
| `event.capabilities_changed` | `{version}` |
| `event.agent_perception` | runtime, per body, see 04-embodiment |

## Addressing

- **Scenes** by `res://` path.
- **Nodes** by absolute node path from the edited scene root, e.g. `/root` is never used;
  `"."` is the edited scene root, `"Player/Camera3D"` is a child path. Runtime mode uses paths from
  the main loop root (`/root/Main/Player`).
- **Resources** by `res://` path or, for sub-resources, `res://scene.tscn::ResourceName`.
- **Properties** by name, using Godot's property path syntax (`position:x` is allowed).
- **Values** are JSON encoded with Godot's Variant JSON rules; typed values that JSON can't carry
  (Vector3, Color, NodePath, Object refs) use a tagged object `{"$type":"Vector3","v":[x,y,z]}`.
  The server accepts both a plain JSON array for vectors and the tagged form. Objects are returned
  as `{"$type":"Object","class":"…","path":"…"}` and never inlined.

## Tool set, editor mode

All `editor` tools go through `EditorUndoRedoManager` when they mutate. The action name is
`"<agent.name>: <human verb>"`.

### engine.*
- `engine.info` → `{version, mode, project_name, project_path, platform, features[]}`
- `engine.capabilities` → see above
- `engine.ping` → `{time_ms}`

### project.*
- `project.settings_get {name}` / `project.settings_set {name, value}` (mutates)
- `project.files_list {dir="res://", recursive=false, glob?}` → `{entries:[{path,is_dir,size}]}`
- `project.file_read {path, max_bytes?}` → `{text|bytes_base64, truncated}`
- `project.file_write {path, text}` (mutates) → also triggers filesystem rescan
- `project.rescan`

### scene.*
- `scene.list_open` → `{scenes:[path], current}`
- `scene.open {path}` · `scene.new {root_type, root_name, path?}` · `scene.save {path?}` ·
  `scene.save_all` · `scene.close {path}` · `scene.reload {path}`
- `scene.tree {root=".", depth=-1, include_properties=false}` → serialized tree
  `{name,type,path,script?,children[],properties?}`
- `scene.select {nodes:[path]}` · `scene.selection`
- `scene.instantiate {scene_path, parent, name?}` (mutates) → `{path}`

### node.*
- `node.create {parent, type, name?, properties?}` (mutates) → `{path}`
- `node.delete {path}` (mutates)
- `node.rename {path, name}` (mutates) → `{path}`
- `node.reparent {path, new_parent, keep_global_transform=true}` (mutates) → `{path}`
- `node.duplicate {path, name?}` (mutates) → `{path}`
- `node.get {path, properties?:[name]}` → `{type, properties:{…}, script?, groups[], signals[]}`
- `node.set {path, properties:{name:value}}` (mutates)
- `node.call {path, method, args:[]}` → `{result}` (mutates unless method is known-pure)
- `node.list_properties {path}` → `{properties:[{name,type,hint,usage}]}`
- `node.attach_script {path, script_path}` / `node.detach_script {path}` (mutates)
- `node.set_owner_editable {path, editable}` (instanced scenes)

### script.*
- `script.read {path}` → `{text, language}`
- `script.write {path, text}` (mutates; reloads open script editors)
- `script.create {path, language="GDScript", extends?, template?}` (mutates)
- `script.errors {path?}` → `{errors:[{path,line,column,message}]}` after a reparse
- `script.open_in_editor {path, line?}`

### resource.*
- `resource.load {path}` → `{class, path, properties}` (shallow)
- `resource.create {class, path, properties?}` (mutates)
- `resource.set {path, properties}` (mutates) · `resource.save {path}`

### run.*
- `run.play {scene?="main"|"current"|res_path}` · `run.stop` · `run.state`
- `run.capture {size?}` → `{png_base64, width, height}` of the running game's main viewport

### editor.*
- `editor.capture {viewport="3d"|"2d"|"main", size?}` → PNG of an editor viewport
- `editor.log_tail {lines=100}` → `{lines:[{level,text}]}`
- `editor.undo` · `editor.redo` · `editor.history {limit}` → `{actions:[{name,agent?}]}`
- `editor.inspect {path}` (focus inspector on node)
- `editor.set_main_screen {name}` (`"3D"`, `"2D"`, `"Script"`, `"AssetLib"`, …)
- `editor.command {name, args?}` (run an editor command palette command)

## Tool set, runtime mode

Provided by `opendust_agent` at runtime, extended by other modules.

### world.*
- `world.tree {root="/root", depth, include_properties}` (same shape as scene.tree)
- `world.rooms` → `{rooms:[{room_id, node_path, room_dir}]}`
- `world.room_of {node_path}` → the enclosing Room
- `world.capture {size?}` → PNG of the main viewport
- `world.time` → `{ticks_ms, frame, physics_frame}`
- `world.node_get {path, properties?}` / `world.node_call {path, method, args}` (policy-gated)

### agent.* (registered by opendust_soul, see 04-embodiment)
- `agent.bodies` · `agent.attach {body_path}` · `agent.perceive` · `agent.move_to` ·
  `agent.look_at` · `agent.interact` · `agent.say` · `agent.emote` · `agent.hold` · `agent.drop`

### os.* (registered by opendust_os, see 03-os-layer)
- `os.devices` · `os.open_app` · `os.drive_list` · `os.drive_read` · `os.drive_write` ·
  `os.message`

## Registering tools from other modules (C++)

`OpenDustToolRegistry` is a singleton created by `opendust_agent` at
`MODULE_INITIALIZATION_LEVEL_SERVERS`. Other modules register at `SCENE` level or later.

```cpp
// modules/opendust_agent/opendust_tool_registry.h
class OpenDustToolRegistry : public Object {
    GDCLASS(OpenDustToolRegistry, Object);
public:
    enum ToolFlags { TOOL_EDITOR = 1, TOOL_RUNTIME = 2, TOOL_MUTATES = 4, TOOL_SLOW = 8 };

    static OpenDustToolRegistry *get_singleton();

    // p_handler: Callable taking (Dictionary params, Dictionary context) and returning a
    // Dictionary result. To signal an error, return {"$error": {"code": int, "message": String}}.
    // context carries {session_id, agent_name, agent_id, mode}.
    void register_tool(const String &p_name, const String &p_description,
                       const Dictionary &p_input_schema, const Callable &p_handler, int p_flags);
    void unregister_tool(const String &p_name);
    bool has_tool(const String &p_name) const;
    Array list_tools(int p_mode_mask) const;   // ToolSpec dictionaries
    Variant call_tool(const String &p_name, const Dictionary &p_params,
                      const Dictionary &p_context, Dictionary &r_error);
    int get_capabilities_version() const;
};
```

Also exposed to GDScript, so a project addon can register tools too. Registration bumps
`capabilities_version` and emits `capabilities_changed`, which the server forwards.

## Policy

Project setting `opendust/agent/policy`:

- `open` (default in editor): all tools.
- `no_mutations`: read-only tools only.
- `allowlist`: only tools in `opendust/agent/allowed_tools`.

Runtime default is `no_mutations` unless the project opts in; `world.node_call` and `os.drive_write`
are always gated by the allowlist at runtime. The server refuses with `-32006 denied`.

## Versioning

`schema` in the discovery file and `capabilities.version` are independent. The protocol itself is
versioned by this document; breaking changes bump `opendust.bridge/1` → `/2` and the bridge refuses
to connect to a version it doesn't know.
