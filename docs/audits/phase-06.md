# Phase 6 Audit - Mesh Redefinition

**Date:** 2026-09-11  
**Branch:** main  
**Commit:** `d90d92d` (pushed to origin/main `67fa8e8..d90d92d`)  
**Supersedes terminology in:** audits/phase-04, audits/phase-05 (historical records kept)

---

## 1. Problem Statement

The word "mesh" was applied to code that is not a mesh. Two problems:

1. **Conceptual:** `mesh.cpp` was called "mesh" only because multiple nodes
   broadcast to each other. No routing, no multi-hop, no topology repair exists.
2. **Structural:** two *identically named* `namespace mesh` blocks lived in the
   same binary — the main networking module (`mesh.h`) and the ESP-NOW P2P
   helper (`espnow_p2p.h`). C++ merges them into one namespace, hiding which
   symbol lives where.

The roadmap (Phase 6) demanded: define what Qymera networking *is*, distinguish
Discovery / Messaging / Transport / Topology / Ownership / Routing, and if the
system is broadcast-only, say so honestly. No complex routing was to be
implemented.

## 2. Decision: single broadcast-domain "net" layer

`docs/networking.md` now defines the six axes explicitly:

| Axis | Answer |
|------|--------|
| Discovery | periodic legacy broadcast + V2 HELLO / ENTITY_ANNOUNCE |
| Messaging | versioned packets, reliable COMMAND delivery |
| Transport | medium-agnostic channel (`qymera::transport`) |
| Topology | flat single broadcast domain, no hops |
| Ownership | static `device_uid` → `entity_id`, authoritative publisher model |
| Routing | **none** — explicitly not implemented (no real requirement) |

## 3. Code Changes

### 3.1 Source renames (no logic changes)

| Before | After |
|--------|-------|
| `src/mesh.h` / `src/mesh.cpp` | `src/net.h` / `src/net.cpp` (git mv, namespace `mesh` → `net`) |
| `MESH_TIMEOUT` | `NET_TIMEOUT` |
| espnow_p2p `namespace mesh` | `namespace qymera::espnow` |
| `espnow_init/send_broadcast/recv/get_rx_overflow/get_rx_queue_depth/add_peer/clear_peers/get_peer_count/set_enabled/is_enabled` | `init/send_broadcast/recv/get_rx_overflow/get_rx_queue_depth/add_peer/clear_peers/get_peer_count/set_enabled/is_enabled` |
| call sites `mesh::*` (core, log, storage, web, sensors, Qymera.h) | `net::*` |
| transport.cpp `mesh::espnow_*` | `espnow::*` (short names) |
| comments in src (CLEAN grep) | `net` / `esp-now` terminology |

Wire-format constants, packet kinds, magic bytes and the legacy `net::Transport`
mirror API (`TRANSPORT_UDP`/`TRANSPORT_ESPNOW` + `setTransport`) are **unchanged**
for compatibility (`core.cpp`,`transport.cpp` contract).

### 3.2 Documentation

- **NEW `docs/networking.md`** — the model: six axes, module map, naming table,
  honest limitations (no routing, no ESP-NOW unicast, no fragmentation), facts
  for implementers (slots, `NET_TIMEOUT`, cadence).
- `docs/protocol.md`, `docs/runtime-architecture.md`, `docs/architecture-baseline.md`,
  `docs/security-model.md`, `docs/testing-strategy.md`: terminology updated to
  `net`/transport/peer-group; historical phrases kept only as explicit "(formerly mesh)".
- `docs/professionalization-roadmap.md`: operational references updated; Phase 6
  definition text left as the spec it is.

## 4. Verification

| Check | Result |
|-------|--------|
| `grep -rn "mesh" src --include=*.h/cpp` | CLEAN (only `espnow_p2p` module, which is its own file) |
| No `#include "mesh.h"` anywhere | ✅ |
| `MESH_TIMEOUT` in src | 0 |
| Host tests | **149/149 PASS** (unchanged suite; logic untouched) |
| Build matrix | see below |

### Memory deltas vs HEAD (Phase 5, commit `503c728`)

Pure rename ⇒ effectively zero:

| Env | RAM | Flash |
|-----|-----|-------|
| esp8266_generic | 58,580 B (71.5%) — «match 58,592» (−12 B) | 488,656 (−4 B) |
| esp32_devkit | 74,932 (22.9%) — «=» | 1,014,633 (−16 B) |
| esp32c3_devkit | 69,468 (21.2%) — «=» | 999,002 (0 B) |

`«match»`/`«=»` denote byte-for-byte equality with the Phase 5 build image.

| Build | Status |
|-------|--------|
| PlatformIO esp8266_generic / esp32_devkit / esp32c3_devkit | SUCCESS ×3 |
| arduino-cli `esp8266:esp8266:generic` Base | SUCCESS (58,584 B RAM) |
| arduino-cli `esp8266:esp8266:generic` HardwareDemo | SUCCESS (59,084 B RAM) |

## 5. Risk Assessment

| Risk | Assessment |
|------|------------|
| External code referencing `mesh::` / `mesh.h` | No consumers outside src hit it (grep: none). Arduino examples use the `Qymera::` facade only. |
| Two `namespace mesh` merge removed | Now each module has one unambiguous namespace. |
| Compatibility via `net::Transport` mirror | Kept exactly as before (`TRANSPORT_UDP`/`TRANSPORT_ESPNOW`, `setTransport`). |
| Protocol ABI | Untouched — renames are compile-time only. |

## 6. Files Modified

| File | Change |
|------|--------|
| `src/mesh.h` → `src/net.h` | rename + namespace + macro |
| `src/mesh.cpp` → `src/net.cpp` | rename + namespace + macro |
| `src/espnow_p2p.h/.cpp` | namespace `qymera::espnow`, short function names |
| `src/core.cpp`, `src/log.cpp`, `src/storage.cpp`, `src/web.cpp`, `src/sensors.cpp`, `src/Qymera.h`, `src/sensors.h`, `src/log.h`, `src/model.h`, `src/transport.h/.cpp` | call sites + comments |
| `tests/host_sanity.py` | comments only (suite unchanged, 149/149) |
| `docs/networking.md` | NEW — networking model definition |
| `docs/protocol.md`, `docs/runtime-architecture.md`, `docs/architecture-baseline.md`, `docs/security-model.md`, `docs/testing-strategy.md`, `docs/professionalization-roadmap.md` | terminology update |

## 7. Acceptance Criteria

| Criterion | Met? | Evidence |
|-----------|------|----------|
| Six axes distinguished | ✅ | `docs/networking.md §2` |
| Honest, documented broadcast-only topology | ✅ | `docs/networking.md §1, §5` |
| No complex routing implemented | ✅ | explicitly documented as a non-feature; no code added |
| "mesh" misnomer removed from code | ✅ | src CLEAN grep |
| Ambiguity removed (two `namespace mesh`) | ✅ | now `net` + `qymera::espnow` |
| Behavior preserved | ✅ | 149/149 tests, memory byte-identical outside rename |
| Build matrix green | ✅ | 5/5 |

## 8. Next Phase

Phase 7 — Automation Engine 2.0. (Entered, as-is.)