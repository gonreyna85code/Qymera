# Qymera Networking Model

**Status:** Phase 6 (Mesh Redefinition) — 2026-09-11
**Supersedes:** the loose use of the word "mesh".
**See also:** `docs/protocol.md` (wire format), `docs/runtime-architecture.md` (modules).

---

## 1. What Qymera networking is NOT

Qymera is **not** a mesh in the routing sense. It has no multi-hop forwarding, no
path computation and no topology repair. It was historically called "mesh"
(module `mesh.h/cpp`, logs, docs) only because *several nodes broadcast to each
other*. That single word implied a capability the system never had and cluttered
the codebase with two identically named `namespace mesh` blocks (the main
networking module and the ESP-NOW P2P helper).

Phase 6 fixed this in code and docs and states the honest model below.

## 2. The six axes

| Axis | Qymera answer | Owner |
|------|---------------|-------|
| Discovery | Nodes announce themselves and their entities periodically (legacy v1 broadcast + V2 HELLO / ENTITY_ANNOUNCE). Peers learn addresses and `entity_id`s from these broadcasts. | `qymera::net` |
| Messaging | Versioned packets (legacy v1-v5 + V2) for state, reports, logs and commands. Commands may be reliable (ACK). | `qymera::net` |
| Transport | Two interchangeable mediums behind one agnostic channel. UDP broadcast/unicast when connected, ESP-NOW broadcast when AP/offline. | `qymera::transport` |
| Topology | A single flat broadcast domain. Any two reachable nodes see each other directly; there is no intermediate hop. Peer lifetime `NET_TIMEOUT` (30 s). | `qymera::net` (state) |
| Ownership | Entity ownership is static and identity-based: `device_uid` → `entity_id` (V2). The authoritative node publishes its own entities; every other node holds a remote mirror. | `qymera::net` + `sensors` |
| Routing | None. Sending is broadcast, or unicast as an optimization to a known peer address *inside the same broadcast domain*. | — (explicit non-feature) |

### Roadmap rule

> "No llamar 'mesh' a algo simplemente porque varios nodos se mandan broadcast."
> "No implementar routing complejo salvo que exista un requisito real."

There is **no real requirement** for routing today (fleet is LAN-sized, all nodes
in one domain), so routing is **deliberately not implemented**. If a future
deployment needs it, it becomes a distinct, intentionally-designed subsystem —
never a rename.

## 3. Module map

| Module | Role | Namespace |
|--------|------|-----------|
| `src/net.h/.cpp` | Peer discovery + messaging + remote peer model (`RemoteDevice`, `reports[]`), V2 send/recv, command delivery | `net` |
| `src/transport.h/.cpp` | Medium-agnostic channel: UDP + ESP-NOW behind `broadcast()/unicast()/poll()` | `qymera::transport` |
| `src/espnow_p2p.h/.cpp` | ESP-NOW P2P backend: RX FIFO, broadcast send, peer table | `qymera::espnow` |
| `src/core.cpp` | Runtime orchestration; selects the active medium via `net::setTransport()` | `core` |

`transport` and `espnow` are dumb carriers. All protocol knowledge lives in `net`.

## 4. Naming changes (Phase 6)

| Before | After | Reason |
|--------|-------|--------|
| `src/mesh.h/cpp`, `namespace mesh` | `src/net.h/cpp`, `namespace net` | "mesh" implied routing the module never had |
| `MESH_TIMEOUT` | `NET_TIMEOUT` | consistent with `net` |
| `namespace mesh` (in `espnow_p2p.h/.cpp`) | `namespace qymera::espnow` | removed the ambiguous merged `mesh` namespace |
| `espnow_*` function names | `init()`, `send_broadcast()`, `recv()`, `set_enabled()`, … | no longer needed a prefix; redundant inside the namespace |
| Wire constants / packet kinds | **unchanged** | protocol compatibility is untouchable |

## 5. Honest capabilities & limitations

| Capability | Status |
|------------|--------|
| UDP broadcast + unicast | Yes — unicast is a same-domain optimization |
| ESP-NOW broadcast | Yes |
| ESP-NOW unicast | **No** — broadcast fallback, documented at `transport::unicast` |
| Fragmentation / large frames | No — `FRAME_MAX` 1400, `ESP_NOW_FRAME_MAX` 250 |
| Multi-hop routing, topology repair | **No** — deliberately out of scope (no requirement) |
| Encrypted signing | No — LAN assumption (see `docs/security-model.md`) |
| Reliable COMMAND delivery | Yes (`net::sendReliableV2Command`, see `docs/protocol.md` Phase 4) |

## 6. Facts for implementers

- Peer slots: `net::RemoteDevice remote_devices[MAX_SENSORS]` (64 slots max).
- Peer staleness: > `NET_TIMEOUT` (30 s) → declared offline.
- Discovery cadence: V2 HELLO + per-entity ENTITY_ANNOUNCE every
  `genset.report_interval` (co-located with the periodic report); legacy
  batched UDP uses `DISCOVERY_MAX_UDP_PACKET` (1400) with the same periodic
  report. So discovery is *piggy-backed on the report cadence*, not a separate timer.
- UDP ports: broadcast + command (from `genset`), bound by `transport::begin()`.
- Common repeated phrasing to avoid going forward: *"peer group"* /
  *"broadcast domain"* / *"net layer"* — not "mesh".