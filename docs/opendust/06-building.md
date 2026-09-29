# Building OpenDust

OpenDust builds exactly like Godot. The only differences are the binary name (`opendust.*`) and the
extra modules, which are on by default.

## Windows (what the first build machine uses)

Toolchain:

- Python 3.11+ and `pip install scons` (SCons ≥ 4.8; ≥ 4.10.1 if you have Visual Studio 2026).
- Visual Studio 2022 Build Tools with the "Desktop development with C++" workload and a
  Windows 11 SDK. Installable non-interactively:

  ```
  winget install --id Microsoft.VisualStudio.2022.BuildTools --exact --silent ^
    --override "--quiet --wait --norestart --add Microsoft.VisualStudio.Workload.VCTools ^
    --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 ^
    --add Microsoft.VisualStudio.Component.Windows11SDK.22621 --includeRecommended"
  ```

Build the editor:

```
cd opendust
scons platform=windows target=editor -j12
```

Faster iteration on our modules only:

```
scons platform=windows target=editor dev_build=yes -j12
```

Output: `bin/opendust.windows.editor.x86_64.exe`. Export templates: `target=template_release` /
`target=template_debug`.

Build times on a 12-thread machine: first full editor build is on the order of 30–60 minutes.
Incremental builds after touching one OpenDust module are a minute or two.

## Linux / macOS

Same as Godot: `scons platform=linuxbsd target=editor` / `scons platform=macos target=editor`.
See Godot's docs for the toolchains.

## Module switches

Each OpenDust module can be turned off like any Godot module:

```
scons platform=windows target=editor module_opendust_slate_enabled=no
```

`opendust_soul` and `opendust_os` depend on `opendust_agent`; `opendust_slate` depends on
`websocket` being present (it is, by default).

## Tests

- C++ unit tests: `scons tests=yes` then `bin/opendust.* --test`. OpenDust tests live in each
  module's `tests/` directory and are picked up like Godot's.
- MCP bridge: `cd tools/opendust-mcp && python -m pytest`.

## Keeping up with upstream

```
git fetch upstream
git merge upstream/4.7          # or the next stable branch when we move
```

Conflicts should only occur in `version.py`, `README.md`, `.gitignore`, and files under
`modules/opendust_*`, `docs/opendust/`, `tools/`. Anything else is a core patch and must be listed
in `CORE-PATCHES.md`.
