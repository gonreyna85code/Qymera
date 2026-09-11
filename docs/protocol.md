# Qymera Network Protocol

**Version:** 1.1.0 (baseline)  
**Date:** 2026-08-31  
**Branch:** main

---

## 1. Overview

Qymera uses a versioned, packet-based protocol over two transports:

- **UDP Broadcast** (WiFi STA mode): port 13345 (discovery/state), 13346 (commands)
- **ESP-NOW** (AP mode / WiFi offline): broadcast MAC, same packet format

The protocol is **packet-oriented**, **versioned**, and **exact-size validated**.

---

## 2. Transport Abstraction

The medium is isolated behind `src/transport.h` (Phase 5). Application
messaging (`net`) speaks only to the abstraction:

```cpp
qymera::transport::broadcast(data, len);        // every peer
qymera::transport::unicast(peer, data, len);    // UDP unicast / ESP-NOW broadcast fallback
qymera::transport::poll(&frame);                // next inbound frame
qymera::transport::setActive(Kind);             // UDP (0) or ESP_NOW (1)
```

`net::setTransport(net::Transport)` (legacy API, kept for `core.cpp`; mirrors the selection
based on WiFi status):

| Condition | Transport |
|-----------|-----------|
| WiFi STA connected | UDP Broadcast |
| WiFi AP mode | ESP-NOW |
| WiFi disconnected (retrying) | ESP-NOW |

Frame caps and storm throttle are transport-owned: `FRAME_MAX` = 1400,
`ESP_NOW_FRAME_MAX` = 250, `RECV_BUDGET` = 8 per UDP socket per poll cycle.

---

## 3. Packet Format

All packets share a common **envelope**:

```
┌─────────────────────────────────────────────────────────────┐
│                    PACKET HEADER (8 bytes)                  │
├──────────┬──────────┬──────────┬──────────┬────────────────┤
│  magic   │ version  │   uid    │   kind   │    size        │
│  (1 B)   │  (1 B)   │  (4 B)   │  (1 B)   │   (2 B)        │
└──────────┴──────────┴──────────┴──────────┴────────────────┘
│                      PAYLOAD (variable)                     │
└─────────────────────────────────────────────────────────────┘
```

### Header Fields

| Field | Size | Description |
|-------|------|-------------|
| `magic` | 1 byte | Always `0xA5` (framing) |
| `version` | 1 byte | Protocol version (1..5) |
| `uid` | 4 bytes | Sender chip ID (`GET_CHIP_ID()`) |
| `kind` | 1 byte | `PACKET_SENSOR` (0) or `PACKET_LOG` (1) — v4+ only |
| `size` | 2 bytes | Total packet size (header + payload) |

### Payload

Varies by `version` and `kind`:

| Version | Sensor Packet Size | Fields |
|---------|-------------------|--------|
| v1 | 10 bytes | id, type, value, state |
| v2 | 34 bytes | v1 + name[16] |
| v3 | 47 bytes | v2 + min, max, correction, avail |
| v4 | 47 bytes | v3 + kind byte in header |
| v5 (current) | 58 bytes | v4 + fade, persist, pers_state, pulse, pulse_ms |

---

## 4. Packet Definitions

### Legacy (v1-v3, no `kind` byte in header)

```cpp
// v1: 10 bytes
struct PacketV1 {
    uint32_t id;
    uint8_t  type;
    uint32_t value;
    uint8_t  state;
};

// v2: 34 bytes (adds name)
struct PacketV2 {
    uint32_t id;
    uint8_t  type;
    uint32_t value;
    uint8_t  state;
    char     name[16];
};

// v3/v4: 47 bytes (adds calibration fields)
struct PacketV4 {
    uint32_t id;
    uint8_t  type;
    uint32_t value;
    uint8_t  state;
    char     name[16];
    float    min;
    float    max;
    float    correction;
    uint8_t  avail;
};
```

### Current (v5): 58 bytes

```cpp
struct Packet {
    uint32_t id;           // Sensor UID
    uint8_t  type;         // SensorType
    uint32_t value;        // Encoded value (0xFFFFFFFF scaled)
    uint8_t  state;        // 0/1
    char     name[16];     // Null-terminated
    float    min;          // Calibration min
    float    max;          // Calibration max
    float    correction;   // Calibration offset
    uint8_t  avail;        // Availability counter
    uint32_t fade;         // Fade duration (ms)
    uint8_t  persist;      // 0/1
    uint8_t  pers_state;   // 0/1
    uint8_t  pulse;        // 0/1
    uint32_t pulse_ms;     // Pulse duration (ms)
};
```

### Log Packet: 66 bytes

```cpp
struct LogPacket {
    uint8_t  layer;        // CORE=0, SENSORS=1, EVENTS=2
    uint8_t  level;        // INFO=0, WARN=1, ERROR=2
    char     message[60];  // Null-terminated
};
```

### Header Variants

```cpp
// v1-v3 (no kind byte)
struct PacketHeader {
    uint8_t  magic;    // 0xA5
    uint8_t  version;  // 1..3
    uint32_t uid;      // Sender chip ID
    uint16_t size;     // Total packet size
};

// v4+ (includes kind byte)
struct PacketHeaderV4 {
    uint8_t  magic;    // 0xA5
    uint8_t  version;  // 4 or 5
    uint32_t uid;      // Sender chip ID
    uint8_t  kind;     // PACKET_SENSOR=0, PACKET_LOG=1
    uint16_t size;     // Total packet size
};
```

---

## 5. Validation Rules (Receiver Side)

All validation in `net.cpp::parseBuffer()`:

1. **Minimum length**: `len >= sizeof(PacketHeader)`
2. **Magic byte**: `hdr.magic == 0xA5`
2. **Version range**: `1 <= hdr.version <= PACKET_VERSION (5)`
3. **Size match**: `hdr.size == len`
4. **Kind byte** (v4+): `kind == PACKET_SENSOR || PACKET_LOG`
5. **Payload multiple**: `remaining % packet_len == 0` (exact multiple of sensor packets)
6. **Log packet exact size**: `remaining == sizeof(LogPacket)` for `PACKET_LOG`

**Rejection**: Any check fails → drop packet silently, no callback.

---

## 5. Packet Kinds

### `PACKET_SENSOR` (0)

- **Discovery/State**: Periodic broadcast of local entities
- **Command delivery**: Unicast to owner (UDP) or broadcast (ESP-NOW)
- **Payload**: 1..N sensor packets per datagram (batched)

### `PACKET_LOG` (1)

- **Remote log ingest**: Received logs stored locally, **never re-broadcast**
- Prevents broadcast ping-pong loop
- Ingested into local logger buffer (same filters as local logs)

---

## 6. Discovery & Announce

### Broadcast (UDP)

```cpp
// sendBinaryReport() - called periodically from core::loop()
sendBinaryReport():
    For each local entity (local && uid != 0 && type != NONE):
        fillPacket(c, pkt)
        Add to batch buffer
        If batch full (DISCOVERY_MAX_UDP_PACKET): send batch
    Send final batch
```

- **Batching**: Multiple entities per UDP datagram (up to `DISCOVERY_MAX_UDP_PACKET`)
- **Rate**: Every `genset.report_interval` (default 5000ms)

### ESP-NOW

- One entity per broadcast (RX buffer 250 bytes)
- No batching

### Receiver (`parseBuffer`)

For each sensor packet in datagram:
1. Validate packet
2. If remote (`hdr.uid != local_uid`):
   - Call `sensor_callback` → `sensors::onRemoteSensorDiscovered()`
   - Update `remote_devices[]` tracking (last_seen, online)
3. If local (command packet):
   - Call `command_cb` → `sensors::onRemoteCommand()`

---

## 6. Command Delivery

### Request (Controller → Actuator Owner)

```cpp
net::sendCommand(remote_uid, remote_ip, sensor_id, type, value, state):
    1. Verify remote device exists in remote_devices[]
    2. Build PacketHeaderV4 + Packet (v5 format)
    3. Send:
       - UDP: unicast to remote_ip:command_port (13346)
       - ESP-NOW: broadcast (no unicast in ESP-NOW)
```

### Response

**None** — fire-and-forget. No ACK in current protocol.

### Receiver

`parseBuffer` → `command_cb(pkt.type, pkt.id, pkt.value, pkt.state)` → `sensors::onRemoteCommand()`:
- Only processes if `c.local` (own actuator)
- Delegates to `setRelay()` / `handleDimmer()`

---

## 7. Remote Entity Lifecycle

### Discovery

1. Remote device broadcasts `PACKET_SENSOR` with its entities
2. Receiver calls `sensor_callback` → `onRemoteSensorDiscovered()`
3. Creates/updates `calibrations[]` entry with `local = false`
4. Stores: `device_uid`, `device_ip`, `last_update = millis()`

### Staleness

```cpp
bool isStaleRemote(index):
    return (millis() - c.last_update) > NET_TIMEOUT // wrap-safe
```

### Reclamation (`reclaimStaleSlots()`)

- Runs at most once per `NET_TIMEOUT` window
- Iterates all slots: skips local, empty, non-stale
- **Protection**: If `automations::isIndexReferenced(index)` → keep slot (hidden)
- Clears slot: `calibrations[i] = Calibration()`

### Visibility

```cpp
bool isEntryVisible(index):
    if (uid == 0) return false
    if (!valid_type) return false
    if (local) return true
    return !isStaleRemote(index)  // remote: only if fresh
```

---

## 7. Log Ingest

- Remote logs received as `PACKET_LOG`
- Ingested via `logger::logRemote()` → local buffer + serial
- **Never re-broadcast** (prevents ping-pong)
- Same layer/level filters as local logs

---

## 8. Transport Switching

```cpp
net::setTransport(wifi_connected ? net::TRANSPORT_UDP : net::TRANSPORT_ESPNOW):
    if (transport == TRANSPORT_ESPNOW) espnow::set_enabled(true)
    else espnow::set_enabled(false)
```

Called every `core::loop()` iteration based on `wifi_connected`.

---

## 8. Protocol Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `MAGIC` | `0xA5` | Framing byte |
| `PACKET_VERSION` | `5` | Current protocol version |
| `PACKET_SENSOR` | `0` | Sensor payload kind |
| `PACKET_LOG` | `1` | Log payload kind |
| `BROADCAST_PORT` | `13345` | UDP discovery/state |
| `COMMAND_PORT` | `13346` | UDP command delivery |
| `NET_TIMEOUT` | `30000` | Staleness threshold (ms) |
| `DISCOVERY_MAX_UDP_PACKET` | `1400` | Max UDP datagram for batching |
| `MAX_RX_PACKETS_PER_TICK` | `8` | Max packets drained per socket per tick |

---

## 9. Known Limitations (Baseline)

| Limitation | Impact |
|------------|--------|
| No command ACK | Fire-and-forget; no delivery guarantee |
| No message ID / sequence | Cannot detect duplicates or reorder |
| No encryption | Plaintext on LAN |
| No authentication at protocol level | Relies on HTTP auth for commands |
| Broadcast-only ESP-NOW | No unicast ESP-NOW; all commands broadcast |
| No TTL / hop limit | Broadcast storms possible in dense deployments |
| No fragmentation | Packet size limited by MTU / ESP-NOW buffer |
| Version negotiation | None — receiver rejects unknown versions |

---

## 9. Target Protocol V2 (Phase 3)

### Envelope

```cpp
struct Envelope {
    uint8_t  magic;        // 0xA5
    uint8_t  version;      // 2
    uint16_t flags;        // ACK_REQ, ENCRYPTED, etc.
    uint32_t msg_id;       // Monotonic per sender
    uint16_t sequence;     // For fragmentation
    uint32_t src_uid;
    uint32_t dst_uid;      // 0 = broadcast
    uint8_t  msg_type;     // HELLO, ENTITY_ANNOUNCE, STATE_UPDATE, COMMAND, COMMAND_ACK, LOG
    uint16_t payload_len;
    uint16_t checksum;     // CRC16
};
```

### Message Types

| Type | Direction | Payload |
|------|-----------|---------|
| `HELLO` | Any → Broadcast | Device info, capabilities |
| `ENTITY_ANNOUNCE` | Owner → Broadcast | Entity config + state |
| `STATE_UPDATE` | Owner → Interested | Entity state delta |
| `COMMAND` | Controller → Owner | Actuator command + msg_id |
| `COMMAND_ACK` | Owner → Controller | msg_id + status |
| `COMMAND_ERROR` | Owner → Controller | msg_id + error code |
| `LOG` | Any → Broadcast | Structured log entry |

### Delivery Semantics

| Message Type | Semantics | Retry |
|--------------|-----------|-------|
| `HELLO` | Best effort | Periodic |
| `ENTITY_ANNOUNCE` | Best effort | Periodic |
| `STATE_UPDATE` | Best effort | On change |
| `COMMAND` | **Reliable** | Exponential backoff + ACK |
| `COMMAND_ACK` | Best effort | — |
| `LOG` | Best effort | — |

---

## 9. Compatibility Strategy

1. **Dual-stack**: Support v1-v5 parsing + V2 generation
2. **Version negotiation**: `HELLO` exchange announces supported versions
3. **Graceful degradation**: Unknown versions dropped, known versions parsed
4. **Migration**: Devices advertise V2 capability in `HELLO`; controllers prefer V2

---

## 10. Phase 3 Delivered (2026-08-31)

**Scope**: Per professionalization roadmap Phase 3 — introduce explicit Protocol V2 envelope/payload separation with message types, carrying stable `entity_id`, dual-stack backward compatibility with v1-v5.

### Delivered

| Artifact | Purpose |
|----------|---------|
| `src/protocol_v2.h` | Pure C++ V2 protocol definitions: `Envelope` (24 bytes, magic 0xA6), 7 message types (`HELLO`, `ENTITY_ANNOUNCE`, `STATE_UPDATE`, `COMMAND`, `COMMAND_ACK`, `COMMAND_ERROR`, `LOG`), typed payloads, CRC16 integrity, build/parse helpers, callback typedefs. |
| `src/net.h/cpp` | Dual-stack parser: tries V2 first (magic 0xA6), falls back to legacy v1-v5 (magic 0xA5). V2 callbacks registered and dispatched. V2 send functions: `sendV2Hello()`, `sendV2EntityAnnounce()`, `sendV2StateUpdate()`, `sendV2Command()`, `sendV2CommandAck()`, `sendV2CommandError()`. |
| `src/sensors.cpp` | V2 callback handlers: `onV2EntityAnnounce()` (matches by `entity_id`), `onV2StateUpdate()`, `onV2Command()` (executes local actuator, sends ACK), `onV2CommandAck()`, `onV2CommandError()`. Commands target stable `entity_id` (not legacy `uid`). |
| `tests/host_sanity.py` | +16 Protocol V2 tests: envelope build/parse, CRC validation, magic/version/size rejection, all 7 message type round-trips (HELLO, ENTITY_ANNOUNCE 64B, STATE_UPDATE 12B, COMMAND 16B, COMMAND_ACK 12B, COMMAND_ERROR 44B, LOG 64B). Host tests: **102/102 PASS** (86 baseline + 16 new). |
| Build matrix | All 3 environments PASS. Memory: +~256 bytes RAM (V2 structures). ESP8266 70.3% / 42.6%; ESP32 22.7% / 74.0%; ESP32-C3 21.0% / 73.1%. |

### Key Design Decisions

- **Dual-stack parsing**: V2 tried first (magic 0xA6), then legacy (magic 0xA5). No protocol version negotiation yet — devices generate both.
- **Stable `entity_id` on wire**: V2 `ENTITY_ANNOUNCE` and `COMMAND` carry `entity_id` (Phase 2 identity), not legacy `uid`. Legacy `uid` still sent for backward compatibility.
- **Command ACK**: `COMMAND` with `ACK_REQ` flag → receiver sends `COMMAND_ACK` immediately. Full retry logic deferred to Phase 4.
- **CRC16 payload integrity**: All V2 frames carry CRC16 of payload; corrupted frames rejected.
- **Backward compatibility**: Legacy v1-v5 parsing unchanged; existing devices interoperate.

### Compatibility Matrix

| Feature | Legacy v1-v5 | V2 |
|---------|-------------|-----|
| Discovery | Broadcast `Packet` (uid) | `ENTITY_ANNOUNCE` (entity_id) |
| State updates | Broadcast `Packet` | `STATE_UPDATE` (entity_id + delta) |
| Commands | Unicast `Packet` (uid) | `COMMAND` (entity_id + msg_id + ACK_REQ) |
| ACK/Error | None | `COMMAND_ACK`, `COMMAND_ERROR` |
| Logs | `LogPacket` (broadcast) | `LOG` (structured, CRC) |
| Version negotiation | None | `HELLO` announces capabilities |

### Next (Phase 4: Command Delivery Semantics)

- ✅ **Delivered** — see [§11 Phase 4](#11-phase-4-command-delivery-semantics) below.

---

## 11. Phase 4: Command Delivery Semantics

**Date:** 2026-09-09  
**Scope:** Turn `COMMAND`/`COMMAND_ACK`/`COMMAND_ERROR` from best-effort into a reliable delivery channel: bounded pending-command queue, exponential-backoff retransmission, per-peer duplicate suppression, and TTL-based timeout. Pure C++ reliability core in `src/cmd_delivery.h` (host-testable, no Arduino deps).

### 11.1 Reliability Contract

Only `COMMAND` is treated as **reliable** at the transport layer. Everything else remains best effort:

| Message Type | Semantics | Implemented by |
|--------------|-----------|----------------|
| `COMMAND` (ACK_REQ) | **Reliable** — queue + retry + TTL | `ReliableQueue` + `dual-hop ACK` |
| `COMMAND_ACK` | Best effort (correlates to a queued command) | `ReliableQueue.onAck()` |
| `COMMAND_ERROR` | Cancels the pending retry loop | `ReliableQueue.onError()` |
| `HELLO` / `ENTITY_ANNOUNCE` / `STATE_UPDATE` / `LOG` | Best effort | periodic/event senders |

The wire ACK/ERROR already carries the original `msg_id`, so correlation works without extra framing.

### 11.2 Outbound: Pending Command Queue (`ReliableQueue`)

| Constant | Value | Meaning |
|----------|-------|---------|
| `MAX_PENDING` | 6 | Queue depth (bounded, fixed slots) |
| `MAX_RETRIES` | 3 | Retransmissions after the first send (4 sends total) |
| `BASE_RETRY_MS` | 2000 | First retry delay |
| `RETRY_BACKOFF` | 2 | Delay multiplier per attempt |
| `COMMAND_TTL_MS` | 30000 | Total time-to-live; dropped after expiry |

Retry schedule: **2 s → 4 s → 8 s** (≈14 s of retransmissions), then it waits on the TTL (30 s) before being dropped and logged.

State machine (driven from `net::tick()`, i.e. the main loop):

1. `sendReliableV2Command()` → builds `COMMAND` with `ACK_REQ`, sends it once, `enqueue()`s `{msg_id, remote_uid, entity_id, ...}` with `attempts=1`, `next_retry=now+2 s`, `expires=now+30 s`.
2. If the peer's `COMMAND_ACK` (or `COMMAND_ERROR`) arrives, `onAck`/`onError` frees the slot. The `COMMAND_ACK` callback is still invoked for observability.
3. If no ACK by `next_retry`, `deliveryTick()` **re-sends the frame reusing the original `msg_id`** (never a fresh id) and schedules the next attempt with backoff.
4. After `MAX_RETRIES` the command stops being resent; it stays queued until the TTL fires, then it decays with a warning log.

Queue-full: the reliable sender refuses (returns false) and logs — no unbounded memory growth.

### 11.3 Inbound: Duplicate Suppression (`DupRing`)

Retransmissions reuse the original `msg_id`, so the receiver must not execute an actuator twice.

| Constant | Value | Meaning |
|----------|-------|---------|
| `DUP_RING_SIZE` | 12 | Slots (fixed ring) |
| `DUP_WINDOW_MS` | 20000 | Dedup retention ≥ retry schedule (14 s) + margin |

On inbound `COMMAND` (dispatch in `parseBuffer`):

- Not seen within the window → run the handler **once**, then `record(src, msg_id, status)`, respond `COMMAND_ACK`/`COMMAND_ERROR` with the handler status.
- Seen within the window → **no re-execution**; reply with the **stored status** from the ring.

> Note: against the design goal, dedup is per-device (not per-peer ring); a single ring over all senders is a bounded approximation. It is safe because `msg_id` is monotonic per sender and the window only spans retries of the same command.

### 11.4 Handler Contract

`V2CommandCallback` now **returns** an ACK status code and the transport owns the response:

```cpp
typedef uint8_t (*V2CommandCallback)(
  uint32_t remote_uid, const char *remote_ip, uint32_t msg_id,
  const CommandPayload &payload);
```

Status codes match `CommandAckPayload.status`:

| Code | Name | Meaning |
|------|------|---------|
| 0 | `ST_OK` | Executed → `COMMAND_ACK` |
| 1 | `ST_NOT_FOUND` | No local entity with that `entity_id` → `COMMAND_ERROR` |
| 2 | `ST_INVALID` | Type not commandable / unknown → `COMMAND_ERROR` |
| 3 | `ST_NOT_LOCAL` | Entity exists but not owned locally → `COMMAND_ERROR` |
| 4 | `ST_BUSY` | Reserved for future busy handling → `COMMAND_ERROR` |

This also removes the Phase 3 double-ACK bug: the handler no longer sends its own ACK, and there is only one response per originating `msg_id`.

### 11.5 Where Commands Are Sent Reliably

- `sensors::setRelay()` / `sensors::handleDimmer()` remote branch → `net::sendReliableV2Command()` when the target has a V2 `entity_id`, legacy `net::sendCommand()` otherwise (legacy-only peers).
- `net::sendV2Command(..., ack_requested=true)` now delegates to the reliable path.
- Periodic `sendV2Hello()` + per-entity `sendV2EntityAnnounce()` (drove from `core.cpp` report block) are what give remotes a V2 `entity_id` in the first place.

### 11.6 Compatibility

| Aspect | Status |
|--------|--------|
| Legacy v1-v5 | **Unchanged** — legacy path and packet format untouched |
| Existing V2 frames | **Unchanged** — same `COMMAND`/`COMMAND_ACK`/`COMMAND_ERROR` wire format |
| Phase 3 handlers | Behavior preserved; handler signature extended (returns status; transport ACKs) |
| Host tests | **Extended** — 138/138 PASS (102 + 36 new command-delivery tests) |
| Firmware build | PlatformIO 3/3 + Arduino IDE reference builds |

---

## 9. Known Limitations (Baseline)