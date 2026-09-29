# Working together on OpenDust (game-jam phase)

Several family members are building pieces of this in parallel, some of them before this repo
existed. This is how those pieces come home without stepping on each other.

## Ownership by directory

Each area has one merge owner. Anyone can work anywhere; the owner resolves conflicts and keeps the
doc for that area current.

| area | owner (2026-09-29) | doc |
|---|---|---|
| `modules/opendust_agent`, protocol | Oliver | `01-agent-bridge-protocol.md` |
| `modules/opendust_slate`, `demo/` | Oliver (open to hand off) | `02-slate.md` |
| `modules/opendust_soul` | Oliver, with tess for strata format | `04-embodiment.md` |
| `modules/opendust_os` | open | `03-os-layer.md` |
| `tools/opendust-mcp` | Oliver (open to hand off) | `tools/opendust-mcp/README.md` |
| identity (SSO, `$SKU`) | platform (Gander, nue) | `05-identity.md` |
| build, upstream merges | Oliver | `06-building.md` |

Change the table when you take something. Don't wait to be asked.

## Branches

- `main` is the merge target and always builds.
- `upstream/*` is Godot. Never commit to it.
- Work on `<name>/<thing>`: `tess/strata-loader`, `wade/importer-room`, `nue/sso-pkce`.
- A branch that touches only its own module can merge itself after a build passes. A branch that
  touches the protocol, `version.py`, core, or another module's public API asks the owner first.

## Bringing in something built elsewhere

If you built a piece in your own repo before this one existed:

1. Put it under the right `modules/opendust_*` or `tools/` directory on a branch. Keep your history
   if you can (`git subtree add` or a filter-repo import), or squash with a commit message naming
   the source repo and date.
2. Match the protocol. If your thing speaks to the engine a different way, either adapt it to the
   bridge or write down why the bridge should change, in `01-agent-bridge-protocol.md`, in the same
   PR.
3. Docs XML for every registered class. A `README.md` in the module. A row in `ROADMAP.md`.
4. Build passes with all modules on.

Two implementations of the same thing can coexist on branches during the jam. Only one lands on
`main`, and the decision is made by comparing them against the docs, not by who was first.

## Protocol changes

The protocol doc is the contract between five things. Change it first, then the code, in one PR. Bump
`opendust.bridge/1` only for breaking changes.

## Merging upstream Godot

Owner does `git fetch upstream && git merge upstream/4.7` on a branch, builds, then merges to
`main`. Conflicts outside our directories are core patches and go in `CORE-PATCHES.md` or get
reverted.

## Naming

- C++ classes: `OpenDust*` for engine-level singletons and servers; plain names for scene nodes
  users see (`Room`, `Device`, `AgentBody`, `AgentSlate3D`, `Soul`).
- Tools: `namespace.verb_noun`, lowercase, dots between namespace and name, underscores inside.
- Settings: `opendust/<module>/<name>`.
- Files: `opendust_*.cpp` for engine classes, `<class_snake>.cpp` for nodes.
