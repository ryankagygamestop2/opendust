# Slate Room demo

One room, one floor, a player, a slate. The first thing to open after building OpenDust.

## Run it

1. Build the editor (`docs/opendust/06-building.md`) and open this folder as a project.
2. Press Play. Click to capture the mouse; WASD to walk; Esc releases the mouse.
3. Press **E**. The slate rises, the mouse frees, and a Claude Code session starts with its working
   directory set to this folder (the `Room` node's `room_dir` is `res://`).
4. Type and press Enter. Text streams onto the slate; tool calls appear as cards you can expand.
5. Esc lowers the slate. E raises it again and the same session continues (the id is remembered in
   `.opendust-slate.json` next to this file).

## What to expect on a first run

- `.opendust-room.json` appears in this folder the moment the scene starts. That's the room writing
  its marker.
- If the runtime agent bridge isn't running yet, the output log warns that
  `.opendust/mcp-runtime.json` is missing and the session starts without world tools. That's fine
  for a first conversation.
- If `claude` isn't on the PATH, set the project setting `opendust/slate/claude_path`.

## Files

- `main.tscn`: `Room` → floor, light, sky, `Player` (`CharacterBody3D`) → `Camera3D` → `Slate` (`AgentSlate3D`).
- `player.gd`: tiny first-person controller that yields the keyboard while the slate is raised.
