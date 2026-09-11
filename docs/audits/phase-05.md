# Phase 5 Audit - Transport Abstraction

**Date:** 2026-09-11  
**Branch:** main  
**Commit:** `TBD` (after commit)  
**Auditor:** Automated baseline inspection

---

## 1. Problem Statement

UDP and ESP-NOW were interleaved with application messaging:

- `mesh.cpp` owned raw sockets (`udp`, `mesh_udp`, `cmd_udp`) and `espnow_*`
  calls inline.
- Nine send sites repeated the same `if (transport == TRANSPORT_UDP) { ... } else
  if (espnow_is_enabled()) { ... }` dispatch.
- `mesh.h` leaked `<WiFiUdp.h>` and `espnow_p2p.h` to every consumer.
- App-layer code had no way to reason about the medium without reading sockets.

## 2. Solution Delivered

New medium-agnostic channel `qymera::transport` (`src/transport.h`/`.cpp`).
Sockets, ESP-NOW and the RX guard logic are owned exclusively by `transport`;
`mesh` became pure application messaging (framing, protocol, entity/command
logic).

### 2.1 API

| Member | Role |
|--------|------|
| `begin(bcast_port, cmd_port)` | bind both UDP sockets + `espnow_init()` |
| `setActive(Kind)` / `active()` | backend selection |
| `broadcast(data, len)` | UDP broadcast / ESP-NOW broadcast |
| `unicast(peer, data, len)` | UDP unicast; ESP-NOW → broadcast fallback |
| `beginPoll()` / `poll(&frame)` | drain cycle broadcast→command→ESP-NOW |

Constants: `FRAME_MAX` 1400, `ESP_NOW_FRAME_MAX` 250, `RECV_BUDGET` 8.

## 3. Code Changes

### 3.1 `src/transport.h` (new)
- `enum class Kind { UDP, ESP_NOW }`, `Peer { char address[24] }`, `Frame`.
- `broadcast` / `unicast` / `poll` / `beginPoll` / `setActive` / `active` /
  `begin` declarations.

### 3.2 `src/transport.cpp` (new)
- Owns `broadcast_socket`, `command_socket` (WiFiUDP), ESP-NOW calls
  (`mesh::espnow_*`).
- `broadcast()`: UDP 255.255.255.255 with the `udpTxReady()` ESP32 guard;
  ESP-NOW broadcast.
- `unicast()`: UDP unicast; ESP-NOW broadcast fallback (no unicast peer API).
- `poll()`: per-socket `RECV_BUDGET` drain (broadcast → command → ESP-NOW FIFO);
  oversized-datagram drop-and-drain; unreadable-datagram drain-and-stop;
  full-dequeue guard (ESP32 re-yield).

### 3.3 `src/mesh.h`
- Removed `<WiFiUdp.h>`, `espnow_p2p.h` includes; removed `extern WiFiUDP udp`.
- Kept legacy `mesh::Transport` enum + `setTransport`/`getTransport` (core.cpp
  dependency) as a mirror of `transport::active`.

### 3.4 `src/mesh.cpp`
- Removed `udp`, `mesh_udp`, `cmd_udp`, `udpTxReady()`, `parseUDPPacket()`, and
  every raw-socket branch.
- `init()` → `transport::begin(genset ports)`.
- `tick()` → `beginPoll()` + `poll()` loop feeding `parseBuffer()` (same parser).
- All senders now call `transport::broadcast()` / `transport::unicast()`.
- `sendBinaryReport()` keeps its MTU-aware batch policy, now feeding
  `transport::broadcast()`.

## 4. Tests

### 4.1 Host Tests Extended (`tests/host_sanity.py`)
**New sections: [transport dispatch] + [transport rx drain]** — 11 tests:
- UDP broadcast/unicast routing; ESP-NOW broadcast + unicast fallback;
  `setActive` toggles ESP-NOW enable.
- Drain order broadcast→command→ESP-NOW; per-socket budget (8) cap;
  leftover deferred to next `beginPoll`; empty cycle → None.

**Result:** **149/149 PASS** (138 + 11 new)

## 5. Build Matrix

| Environment | Status | RAM | Flash | Delta vs HEAD (measured) |
|-------------|--------|-----|-------|--------------------------|
| esp8266_generic (PIO) | SUCCESS | 71.5% (58,592 B) | 51.0% (488,660 B / 958,448) | RAM: +932 B; Flash: +1,796 B |
| esp32_devkit (PIO) | SUCCESS | 22.9% (74,932 B) | 77.4% (1,014,649 B / 1,310,720) | RAM: +624 B; Flash: +2,064 B |
| esp32c3_devkit (PIO) | SUCCESS | 21.2% (69,468 B) | 76.2% (999,002 B / 1,310,720) | RAM: +616 B; Flash: −1,792 B |
| arduino-cli Base (`esp8266:esp8266:generic`) | SUCCESS | 73.0% (58,584 B / 80,192) | — | — |
| arduino-cli HardwareDemo | SUCCESS | 73.7% (59,084 B / 80,192) | — | — |

Cost dominated by the ownership move of the two UDP sockets + RX buffers into
`transport.cpp` (BSS), not new features. No dynamic allocation added.

## 6. Compatibility

| Aspect | Status |
|--------|--------|
| Wire format (legacy v1-v5, V2) | **Unchanged** — same framing, parser untouched |
| `mesh::setTransport` / `mesh::TRANSPORT_*` | **Unchanged** — core.cpp API intact |
| Remote device model / callbacks | **Unchanged** |
| RX protection semantics | **Preserved** — guards moved verbatim into `transport::poll()` |
| ESP-NOW unicast | Same documented limitation (broadcast fallback) |
| Host tests | **Extended** — 149/149 PASS |

## 7. Risks / Limitations

| Risk | Assessment |
|------|------------|
| Ports captured at `transport::begin()` | Previously read live each send; runtime port reconfiguration ALREADY required a rebind for RX. Documented contract: ports fixed per begin. |
| Batching policy still branches on `transport::active()` | Correct — batching is MTU-dependent and inherently medium-knowing; it uses the abstraction, not raw sockets. |
| `poll()` data pointer lifetime | Valid until next `poll()`; consumers process synchronously (same pattern as before). |
| ESP-NOW unicast fallback | Inherited limitation (no unicast ESP-NOW peer API); Phase 6/17 may revisit. |

## 8. Files Modified

| File | Change Type |
|------|-------------|
| `src/transport.h` | NEW (medium-agnostic API, constants) |
| `src/transport.cpp` | NEW (UDP/ESP-NOW backends, RX guards) |
| `src/mesh.h` | MODIFIED: dropped WiFiUdp/espnow includes + `extern WiFiUDP udp` |
| `src/mesh.cpp` | MODIFIED: transport-owned sends/RX; removed sockets + `parseUDPPacket` |
| `tests/host_sanity.py` | MODIFIED: +11 transport mirror tests (149/149) |
| `docs/protocol.md` | UPDATED: §2 transport abstraction section |
| `docs/runtime-architecture.md` | UPDATED: §13 Transport Abstraction |

## 9. Phase 5 Acceptance Criteria

| Criterion | Met? | Evidence |
|-----------|------|----------|
| Application logic decoupled from UDP/ESP-NOW | ✅ | mesa talks only to `transport::*`; no `WiFiUDP`/`espnow_*` in mesh |
| transport.send / receive / available / peer concepts | ✅ | `broadcast`/`unicast`/`poll`/`Peer` + documented mapping |
| Change medium without rewriting entity/command/discovery | ✅ | backend swap is `setActive` only; logic identical |
| Behavior preserved | ✅ | 149/149 host tests; 5/5 builds; guards moved verbatim |
| Memory measured | ✅ | +616–932 B RAM, −1.8..+2.1 KB flash (vs HEAD) |
| Documentation updated | ✅ | protocol.md §2, runtime-architecture.md §13 |

## 10. Next Phase (Phase 6)

Refer to `docs/professionalization-roadmap.md` Phase 6 (Mesh Redefinition).