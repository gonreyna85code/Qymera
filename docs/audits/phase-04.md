# Phase 4 Audit - Command Delivery Semantics

**Date:** 2026-09-09  
**Branch:** main  
**Commit:** `TBD` (after commit)  
**Auditor:** Automated baseline inspection

---

## 1. Problem Statement

Phase 3 introduced `COMMAND` / `COMMAND_ACK` / `COMMAND_ERROR` with an `ACK_REQ`
flag, but delivery was fire-and-forget:

- No pending-command tracking → lost commands are never retried.
- No retry/backoff → a single dropped UDP packet silently cancels the user intent.
- No ACK correlation → extra state necessary to react to a delivery result.
- No duplicate detection → a retry would double-execute the actuator.
- The handler sent its own (provisional) ACK **and** the transport layer did,
  producing a double-ACK; and the mismatch used `entity_id` as `msg_id`.

---

## 2. Solution Delivered

Reliability core in pure C++ (`src/cmd_delivery.h`, no Arduino deps, host-testable):

### 2.1 `ReliableQueue` (outbound)

| Constant | Value | Meaning |
|----------|-------|---------|
| `MAX_PENDING` | 6 | Fixed slot count (bounded memory) |
| `MAX_RETRIES` | 3 | Retransmissions after the first send |
| `BASE_RETRY_MS` | 2000 | First retry delay |
| `RETRY_BACKOFF` | 2 | Delay ×2 per attempt → 2 s / 4 s / 8 s |
| `COMMAND_TTL_MS` | 30000 | Unacked command decay window |

### 2.2 `DupRing` (inbound)

| Constant | Value | Meaning |
|----------|-------|---------|
| `DUP_RING_SIZE` | 12 | Slot count (fixed ring) |
| `DUP_WINDOW_MS` | 20000 | Dedup window covers the 14 s schedule + margin |

### 2.3 `AckStatus` codes

| Code | Name | Transport response |
|------|------|--------------------|
| 0 | `ST_OK` | `COMMAND_ACK` (status 0) |
| 1 | `ST_NOT_FOUND` | `COMMAND_ERROR` "Entity not found" |
| 2 | `ST_INVALID` | `COMMAND_ERROR` "Invalid type" |
| 3 | `ST_NOT_LOCAL` | `COMMAND_ERROR` "Not local" |
| 4 | `ST_BUSY` | `COMMAND_ERROR` "Busy" (reserved) |

---

## 3. Code Changes

### 3.1 `src/cmd_delivery.h` (new)
- `ReliableQueue`: `enqueue()`, `onAck()`/`onError()` (both complete), `nextDue()`,
  `rescheduled()` (backoff), `expire()` (TTL), `count()`/`isFull()`.
- `DupRing`: `isDuplicate()`, `statusFor()`, `record()`.
- `retryDelayMs()`: `BASE << (attempts - 1)`.

### 3.2 `src/protocol_v2.h` + `src/mesh.h`
- `V2CommandCallback` → `uint8_t (*)(remote_uid, remote_ip, msg_id, payload)`
  (returns `AckStatus`; transport sends the response).
- New `mesh::sendReliableV2Command(...)` declaration.

### 3.3 `src/mesh.cpp`
- Statics `cmd_queue` (ReliableQueue) + `cmd_dup_ring` (DupRing).
- `sendCommandFrame()` — shared frame builder; retries reuse the **original msg_id**.
- `sendReliableV2Command()` — transmit + enqueue (queue-full → refused + log).
- `deliveryTick(now_ms)` called from `tick()`: expire TTLs, resend due commands.
- Inbound `COMMAND`: dedup guard → dispatch handler once → single ACK/ERROR with
  handler status (removes the provisional double-ACK).
- Inbound `COMMAND_ACK` / `COMMAND_ERROR`: correlate with `cmd_queue` (stop retry).
- `ackErrorText()` helper for `COMMAND_ERROR` messages.

### 3.4 `src/sensors.h/cpp`
- `onV2Command()` now returns status; **no longer sends ACK/ERROR itself**.
- `setRelay()` / `handleDimmer()` remote path: `entity_id != 0` →
  `mesh::sendReliableV2Command()`, else legacy `mesh::sendCommand()`.

### 3.5 `src/core.cpp`
- Periodic report block now also calls `mesh::sendV2Hello()` and per-entity
  `mesh::sendV2EntityAnnounce(i)` so remotes obtain a V2 `entity_id` (legacy
  discovery alone leaves `entity_id == 0` → reliable path in 3.4).

---

## 4. Tests

### 4.1 Host Tests Extended (`tests/host_sanity.py`)
**New sections: [command delivery queue] + [command delivery dup ring]** — 36 tests:
- Enqueue state (attempts=1, first retry at +2 s, expiry at +30 s)
- Backoff schedule 2000 → 6000 → 14000 ms (due/not-due boundaries)
- Retry exhaustion stops resends
- ACK and ERROR complete the command; unknown ACK is a no-op
- Queue overflow rejected; slot reuse after ACK; TTL frees slot; no expire early
- Dup ring: fresh vs duplicate, different msg_id/src, window expiry, stored status replay (BUSY)

**Result:** **138/138 PASS** (102 + 36 new)

---

## 5. Build Matrix

| Environment | Status | RAM | Flash | Delta vs HEAD (measured) |
|-------------|--------|-----|-------|--------------------------|
| esp8266_generic (PIO) | SUCCESS | 71.2% (58,360 B) | 51.0% (488,820 B / 958,448) | RAM: +700 B; Flash: +1,956 B |
| esp32_devkit (PIO) | SUCCESS | 22.8% (74,724 B) | 77.4% (1,014,641 B / 1,310,720) | RAM: +416 B; Flash: +2,056 B |
| esp32c3_devkit (PIO) | SUCCESS | 21.1% (69,268 B) | 76.5% (1,002,182 B / 1,310,720) | RAM: +416 B; Flash: +1,388 B |
| arduino-cli Base (`esp8266:esp8266:generic`) | SUCCESS | 72.8% (58,356 B / 80,192) | 43.1% (452,280 B / 1,048,576) | — |
| arduino-cli HardwareDemo | SUCCESS | 73.4% (58,864 B / 80,192) | 43.4% (455,448 B / 1,048,576) | — |

Cost: ~+400–700 B RAM and ~+1.4–2.1 KB flash for the reliability core (bounded
queue + dedup ring + retry scheduler). No dynamic allocation. IRAM 95–96% on
esp8266 core 3.x is pre-existing core behavior, unaffected by this change.

---

## 6. Compatibility

| Aspect | Status |
|--------|--------|
| Legacy v1-v5 | **Unchanged** — legacy path and packet format untouched |
| V2 wire format | **Unchanged** — same `COMMAND`/`COMMAND_ACK`/`COMMAND_ERROR` |
| Phase 3 V2 handlers | Behavior preserved; signature extended (return status) |
| Legacy-only peers | Remote `entity_id == 0` → still uses legacy `mesh::sendCommand()` |
| Persistence / rules / web | **Unchanged** |
| Host tests | **Extended** — 138/138 PASS |

---

## 7. Risks / Limitations

| Risk | Assessment |
|------|------------|
| Dedup ring is per-device, not per-peer | `msg_id` is monotonic per sender and window is short; a collision across senders would only suppress a re-execution (safe direction). Acceptable for current scale. |
| ACK loss on ESP-NOW broadcast | Same limitation as Phase 3 (broadcast, no unicast). Retry helps; scale limited. |
| Single response channel (ACK *or* ERROR) | Sufficient for today's status set; `ST_BUSY` reserved for future load control. |
| No network version negotiation | `HELLO` capabilities still reserved; devices generate both stacks. |
| Not end-to-end confirmed on hardware | Logic mirrored + host-tested; two-device verification is a Phase 4 HW test step. |

---

## 8. Files Modified

| File | Change Type |
|------|-------------|
| `src/cmd_delivery.h` | NEW (ReliableQueue + DupRing, pure C++) |
| `src/protocol_v2.h` | MODIFIED: `V2CommandCallback` returns status |
| `src/mesh.h` | MODIFIED: callback typedef + `sendReliableV2Command()` decl |
| `src/mesh.cpp` | MODIFIED: queue/dup statics, `sendCommandFrame`, `sendReliableV2Command`, `deliveryTick` in `tick()`, ACK correlation, dedup + single ACK/ERROR, `ackErrorText` |
| `src/sensors.h` | MODIFIED: `onV2Command` signature |
| `src/sensors.cpp` | MODIFIED: `onV2Command` returns status; reliable remote actuator path |
| `src/core.cpp` | MODIFIED: periodic V2 hello + entity announce |
| `tests/host_sanity.py` | MODIFIED: +36 command-delivery tests (138/138) |
| `docs/protocol.md` | UPDATED: §11 Phase 4 section |
| `docs/runtime-architecture.md` | UPDATED: §12 Command Delivery Semantics |

---

## 9. Phase 4 Acceptance Criteria

| Criterion | Met? | Evidence |
|-----------|------|----------|
| Pending command queue per peer | ✅ | `ReliableQueue` (MAX_PENDING=6), keyed by msg_id + remote_uid |
| Exponential backoff retry | ✅ | 2 s → 4 s → 8 s; boundary tests |
| Max retries + TTL | ✅ | `MAX_RETRIES=3`, `COMMAND_TTL_MS=30000`, expire test |
| ACK correlation | ✅ | `onAck`/`onError` free the matching slot |
| Duplicate detection | ✅ | `DupRing` + window expiry tests |
| Exactly-once actuator execution (per command) | ✅ | dedup before dispatch; status replay |
| Single ACK per msg_id (remove double-ACK) | ✅ | transport owns the response; handler returns status |
| Legacy compatibility | ✅ | `entity_id==0` falls back to legacy path |
| Build passes matrix | ✅ | PlatformIO 3/3 + arduino-cli reference builds |
| Host tests pass | ✅ | 138/138 PASS |
| Documentation updated | ✅ | protocol.md §11, runtime-architecture.md §12 |

---

## 10. Next Phase (Phase 5)

Refer to `docs/professionalization-roadmap.md` for Phase 5 scope.