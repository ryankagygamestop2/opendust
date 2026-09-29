# Identity: pods.global SSO, agent_id, $SKU

Status: DRAFT 0.1, 2026-09-29. OpenDust **consumes** these; it does not define them. The
definitions live in `pods-platform/` (platform pattern, birth-certificate standard, routing
resolution) and in the SSO work under way. This doc records how the engine uses them so that nothing
here drifts from the platform.

## pods.global SSO

- One family logs into pods.global once. Every OpenDust world, device, and editor session authorizes
  against that session. OpenDust never stores passwords or runs its own login.
- The engine speaks standard OIDC to whatever IdP backs pods.global (Zitadel is recommended and
  pending ratification; the engine doesn't care which).
- Where it happens: `OpenDustIdentity` (singleton in `modules/opendust_os`): `sign_in()` opens the
  system browser to the pods.global authorize URL with PKCE, listens on a loopback port for the
  redirect, exchanges the code, and holds the tokens in memory. Refresh tokens are stored via the
  OS keychain when available, else in `user://opendust/identity/` encrypted with a machine key. This
  is the same shape every native app uses; nothing bespoke.
- Offline: the last known `{family_id, user_id, display_name, org_memberships}` is cached so worlds
  run without network. Anything that needs a fresh assertion (drive sync, cross-world messages)
  waits for one.
- Agents are subjects too: a body's `agent_id` maps to an OIDC service user in the family's org, so
  an agent in a world has a real session with real scopes, not a shared operator token.

## agent_id

- Minted at body install by the family's `wizard.py`, per the birth-certificate standard §5. Genesis
  seed → keypair → `agent_id`. Never in a browser, never in an engine UI.
- The engine reads it from `certificate.json` next to the soul, or from the installer's output. If
  absent, the body is `pending` and the engine says so everywhere the id would appear.
- Persisted references (drive files, saved body state, messages, inventory ownership) use
  `agent_id`. Session ids, bridge session ids, node paths and sockets are addresses and are never
  persisted. See `pods-platform/routing-resolution.md`.

## $SKU

- A `$SKU` is a cryptographic token that identifies one inventory item. Every item in every world
  that can be held, traded, or owned has one. The token format and issuance are specified in the
  platform; the engine treats a `$SKU` as an opaque string with a known prefix.
- `InventoryRegistry` (in `modules/opendust_os`) resolves `$SKU` → `{class, display_name, icon,
  mesh_scene?, app_id?, owner: agent_id|user_id, metadata}` from (a) a local registry file the
  project ships, (b) the family's pods.global inventory service when online. Cache with TTL; unknown
  tokens render as placeholders and are never dropped.
- `Holdable` (Node3D component): any world object with a `sku` property is an inventory item. The
  slate is a `Holdable` with its own `$SKU`. `AgentBody.hold(item)` requires the item's `sku` and
  records the hold against the agent's id.
- Ownership changes are platform transactions, not engine state. The engine requests a transfer and
  renders the result. Offline transfers queue.

## What the engine will never do

- Generate identity keys in a UI.
- Store a password.
- Persist a routing address as if it were an identity.
- Invent a `$SKU`.
