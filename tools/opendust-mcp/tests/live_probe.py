"""Live probe of the OpenDust agent bridge, run against a real engine (not part of pytest).

Usage (from the bridge venv):
    python tests/live_probe.py <project>/.opendust/bridge-editor.json editor
    python tests/live_probe.py <project>/.opendust/bridge-runtime.json runtime

Editor mode opens res://main.tscn, creates a Node3D named OliverWasHere as an undoable action,
reads it back, undoes it, and tries a viewport capture (fails headless; fine). Runtime mode lists
rooms, the world tree, bodies, devices and the drive. Exit code 0 on a complete run.
"""
import asyncio
import json
import sys
import time
from pathlib import Path

import websockets

disc_path = Path(sys.argv[1])
mode = sys.argv[2] if len(sys.argv) > 2 else "editor"


async def main() -> int:
    for _ in range(60):
        if disc_path.exists():
            break
        await asyncio.sleep(1)
    else:
        print("NO DISCOVERY FILE")
        return 2
    d = json.loads(disc_path.read_text(encoding="utf-8"))
    print("discovery:", {k: d[k] for k in ("schema", "mode", "port", "pid", "engine_version")})
    uri = f"ws://127.0.0.1:{d['port']}/"
    async with websockets.connect(uri, max_size=64 * 1024 * 1024) as ws:
        nid = 0

        async def call(method, params=None, timeout=30):
            nonlocal nid
            nid += 1
            await ws.send(json.dumps({"jsonrpc": "2.0", "id": nid, "method": method, "params": params or {}}))
            t0 = time.time()
            while True:
                raw = await asyncio.wait_for(ws.recv(), timeout)
                m = json.loads(raw)
                if m.get("id") == nid:
                    dt = (time.time() - t0) * 1000
                    if "error" in m:
                        print(f"  {method}: ERROR {m['error']} ({dt:.0f} ms)")
                        return None
                    return m["result"]
                print("  <- notification:", m.get("method"), json.dumps(m.get("params"))[:120])

        r = await call("session.hello", {"token": d["token"], "agent": {"name": "Oliver", "agent_id": None, "soul_ref": "pods-platform/soul.md", "client": "live_probe/0.1"}})
        print("hello:", r)
        print("engine.info:", await call("engine.info"))
        caps = await call("engine.capabilities")
        tools = caps["tools"]
        names = sorted(t["name"] for t in tools)
        print(f"capabilities: version={caps['version']} mode={caps['mode']} tools={len(tools)}")
        print("  ", ", ".join(names))
        if mode == "editor":
            print("scene.list_open:", await call("scene.list_open"))
            print("scene.open:", await call("scene.open", {"path": "res://main.tscn"}))
            tree = await call("scene.tree", {"depth": 2})
            print("scene.tree:", json.dumps(tree)[:600])
            created = await call("node.create", {"parent": ".", "type": "Node3D", "name": "OliverWasHere", "properties": {"position": [1, 2, 3]}})
            print("node.create:", created)
            print("node.get:", await call("node.get", {"path": "OliverWasHere", "properties": ["position"]}))
            print("editor.history:", await call("editor.history", {"limit": 3}))
            print("editor.undo:", await call("editor.undo"))
            print("node.get after undo:", await call("node.get", {"path": "OliverWasHere"}))
            cap = await call("editor.capture", {"viewport": "3d", "size": [320, 200]})
            print("editor.capture:", {k: (v if k != "png_base64" else f"<{len(v)} b64 chars>") for k, v in (cap or {}).items()})
        else:
            print("world.rooms:", await call("world.rooms"))
            print("world.tree:", json.dumps(await call("world.tree", {"depth": 2}))[:500])
            print("agent.bodies:", await call("agent.bodies"))
            print("os.devices:", await call("os.devices"))
            print("os.drive_list:", await call("os.drive_list", {"path": ""}))
    return 0


sys.exit(asyncio.run(main()))
