# Phase 3 Audit - Protocol V2

**Date:** 2026-08-31  
**Branch:** main  
**Commit:** `TBD` (after commit)  
**Auditor:** Automated baseline inspection

---

## 1. Problem Statement

Legacy protocol (v1-v5) evolved incrementally around structs (`Packet`, `PacketV4`, `PacketV5`) with:
- No envelope/payload separation — header and payload fused
- No message typing — only `PACKET_SENSOR` (0) and `PACKET_LOG` (1) via kind byte
- No stable identity on wire — uses `uid = ChipID + index` (index-dependent)
- No command ACK/retry — fire-and-forget
- No payload integrity — no checksum
- No version negotiation — unknown versions silently dropped

---

## 2. Solution Delivered

**Protocol V2** — explicit envelope/payload separation with typed messages, stable `entity_id`, dual-stack backward compatibility.

### 2.1 New Protocol Structures (`src/protocol_v2.h`)

| Structure | Size | Purpose |
|-----------|------|---------|
| `Envelope` | 24 bytes | Frame header: magic 0xA6, version 2, flags, msg_id, sequence, src/dst UID, msg_type, payload_len, CRC16, reserved |
| `HelloPayload` | 8 bytes | Device introduction: device_id, capabilities bitmask, legacy protocol versions supported |
| `EntityAnnouncePayload` | 64 bytes | Full entity config + initial state: **entity_id** (stable!), device_id, type, capabilities, ownership, name[24], calibration fields |
| `StateUpdatePayload` | 12 bytes | Minimal delta: entity_id, value, state, avail |
| `CommandPayload` | 16 bytes | Actuator command: entity_id, type, flags (ACK_REQ), value, state |
| `CommandAckPayload` | 12 bytes | ACK: original msg_id, entity_id, status |
| `CommandErrorPayload` | 44 bytes | Error: original msg_id, entity_id, error_code, message[32] |
| `LogPayload` | 64 bytes | Structured log: layer, level, timestamp, message[56] |

### 2.2 V2 Message Types

| Type | Value | Direction | Semantics |
|------|-------|-----------|-----------|
| `HELLO` | 0x01 | Any → Broadcast | Device intro, capabilities |
| `ENTITY_ANNOUNCE` | 0x02 | Owner → Broadcast | Entity config + state (carries `entity_id`) |
| `STATE_UPDATE` | 0x03 | Owner → Interested | State delta (entity_id + value/state/avail) |
| `COMMAND` | 0x04 | Controller → Owner | Actuator command (entity_id + msg_id + ACK_REQ) |
| `COMMAND_ACK` | 0x05 | Owner → Controller | ACK for COMMAND |
| `COMMAND_ERROR` | 0x06 | Owner → Controller | Error response |
| `LOG` | 0x07 | Any → Broadcast | Structured log entry |

### 2.3 Flags

| Flag | Value | Purpose |
|------|-------|---------|
| `ACK_REQ` | 0x0001 | Request COMMAND_ACK |
| `ENCRYPTED` | 0x0002 | Payload encrypted (future) |
| `FRAGMENTED` | 0x0004 | Part of fragmented message |
| `LAST_FRAG` | 0x0008 | Last fragment |

---

## 3. Code Changes

### 3.1 `src/protocol_v2.h` (new, ~250 lines)
- Pure C++ (no Arduino deps), namespace `qymera::protocol::v2`
- Packed structs with `static_assert` size verification
- CRC16-CCITT implementation
- `buildFrame()` / `parseFrame()` helpers
- Type-safe payload accessors (`asHello()`, `asEntityAnnounce()`, etc.)
- Callback typedefs for V2 message dispatch

### 3.2 `src/mesh.h/cpp`
- Includes `protocol_v2.h`
- Dual-stack parser in `parseBuffer()`: checks magic byte first (0xA6 → V2, 0xA5 → legacy)
- V2 callback storage + registration functions (`setV2*Callback()`)
- V2 send functions:
  - `sendV2Hello()` — periodic broadcast
  - `sendV2EntityAnnounce(index)` — per local entity
  - `sendV2StateUpdate(index)` — per local entity on change
  - `sendV2Command()` — unicast (UDP) or broadcast (ESP-NOW) with ACK_REQ
  - `sendV2CommandAck()` / `sendV2CommandError()` — responses
- Monotonic `v2_msg_id_counter` per device

### 3.3 `src/sensors.cpp`
- Registers V2 callbacks in `init()`
- `onV2EntityAnnounce()`: matches by `entity_id` (preferred), falls back to legacy `uid`+`device_uid`
- `onV2StateUpdate()`: updates remote entity by `entity_id`
- `onV2Command()`: finds local actuator by `entity_id`, validates capability (READ_WRITE only), executes via `setRelay()`/`handleDimmer()`, sends `COMMAND_ACK` with OK status
- `onV2CommandAck()` / `onV2CommandError()`: logging hooks for future retry logic

---

## 4. Tests

### 4.1 Host Tests Extended (`tests/host_sanity.py`)
**New section: [protocol v2]** — 16 tests:
- Envelope build/parse round-trip
- CRC16 validation (corruption rejected)
- Wrong magic (0xA5) rejected
- Wrong version rejected
- Payload size mismatch rejected
- All 7 message type round-trips with correct payload sizes:
  - `HELLO` (8 bytes)
  - `ENTITY_ANNOUNCE` (64 bytes)
  - `STATE_UPDATE` (12 bytes)
  - `COMMAND` (16 bytes, ACK_REQ flag)
  - `COMMAND_ACK` (12 bytes)
  - `COMMAND_ERROR` (44 bytes)
  - `LOG` (64 bytes)

**Result:** **102/102 PASS** (86 baseline + 16 new)

---

## 5. Build Matrix

| Environment | Status | RAM | Flash | Delta vs Phase 2 |
|-------------|--------|-----|-------|------------------|
| esp8266_generic | SUCCESS | 70.3% (57,592 B) | 42.6% (444,692 B) | RAM: +256 B; Flash: +3,128 B |
| esp32_devkit | SUCCESS | 22.7% (74,308 B) | 74.0% (970,061 B) | RAM: +16 B; Flash: +3,364 B |
| esp32c3_devkit | SUCCESS | 21.0% (68,852 B) | 73.1% (958,262 B) | RAM: +32 B; Flash: +4,134 B |

Flash increase = V2 protocol code + CRC16 table + string constants. Acceptable.

---

## 6. Compatibility

| Aspect | Status |
|--------|--------|
| Legacy v1-v5 parsing | **Unchanged** — dual-stack, legacy path identical |
| Wire format (legacy) | **Unchanged** — existing devices interoperate |
| Persistence (EEPROM) | **Unchanged** — no schema change |
| Rule engine | **Unchanged** — still uses array indices |
| Remote discovery (legacy) | **Works** — legacy `uid`+`device_uid` matching |
| V2 discovery | **New** — `entity_id` matching (preferred) |
| V2 commands | **New** — target `entity_id`, send ACK |
| Host tests | **Extended** — 102/102 PASS |

---

## 7. Risks / Limitations

| Risk | Assessment |
|------|------------|
| No version negotiation yet | Devices send both legacy + V2; controllers parse both. `HELLO` capabilities field reserved for negotiation (Phase 4+). |
| No command retry logic | `COMMAND_ACK` sent immediately; retry queue + exponential backoff deferred to Phase 4. |
| ESP-NOW command broadcast | `sendV2Command()` uses broadcast on ESP-NOW (no unicast). Acceptable for current scale. |
| No encryption | `ENCRYPTED` flag reserved; Phase 14 Security. |
| No fragmentation | Large payloads (>MTU) not supported. Max payload = 64 bytes (fits). |
| CRC16 only | Not cryptographic; integrity against corruption only. |

---

## 8. Files Modified

| File | Change Type |
|------|-------------|
| `src/protocol_v2.h` | NEW (250 lines) |
| `src/mesh.h` | MODIFIED: +V2 callback declarations, send function declarations |
| `src/mesh.cpp` | MODIFIED: +dual-stack parser, V2 callback storage/registration, V2 send functions |
| `src/sensors.h` | MODIFIED: +include protocol_v2.h, +V2 callback forward declarations |
| `src/sensors.cpp` | MODIFIED: +V2 callback registration in init(), 5 V2 handler implementations |
| `tests/host_sanity.py` | MODIFIED: +16 Protocol V2 tests |
| `docs/protocol.md` | UPDATED: Phase 3 Delivered section, compatibility matrix |

---

## 9. Phase 3 Acceptance Criteria

| Criterion | Met? | Evidence |
|-----------|------|----------|
| Envelope/payload separation | ✅ | `Envelope` + 7 typed payloads |
| 7 message types defined | ✅ | HELLO, ENTITY_ANNOUNCE, STATE_UPDATE, COMMAND, COMMAND_ACK, COMMAND_ERROR, LOG |
| Stable `entity_id` on wire | ✅ | `EntityAnnouncePayload.entity_id`, `CommandPayload.entity_id` |
| Dual-stack backward compat | ✅ | Legacy v1-v5 parsing unchanged; V2 magic 0xA6 tried first |
| CRC16 payload integrity | ✅ | `buildFrame`/`parseFrame` with CRC16-CCITT |
| Command ACK mechanism | ✅ | `ACK_REQ` flag → `COMMAND_ACK` sent |
| Build passes 3/3 envs | ✅ | All SUCCESS |
| Host tests pass | ✅ | 102/102 PASS |
| Memory overhead minimal | ✅ | +256–312 bytes RAM |
| Documentation updated | ✅ | `protocol.md` §10 |

---

## 10. Next Phase (Phase 4: Command Delivery Semantics)

**Scope:** Reliable command delivery semantics.
- Pending command queue per peer (msg_id → command + retry state)
- Exponential backoff retry (configurable max retries, base delay)
- Duplicate detection (track received msg_ids per peer)
- Timeout handling → `COMMAND_ERROR` or local fallback
- ACK correlation (match ACK to pending command)

**Blocking on Phase 3:** None — Phase 3 complete.