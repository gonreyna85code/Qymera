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

```cpp
enum Transport {
    TRANSPORT_UDP,      // WiFi STA connected
    TRANSPORT_ESPNOW    // AP mode or WiFi disconnected
};

mesh::setTransport(Transport);  // Called from core::loop() based on WiFi status
```

| Condition | Transport |
|-----------|-----------|
| WiFi STA connected | UDP Broadcast |
| WiFi AP mode | ESP-NOW |
| WiFi disconnected (retrying) | ESP-NOW |

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

All validation in `mesh.cpp::parseBuffer()`:

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
mesh::sendCommand(remote_uid, remote_ip, sensor_id, type, value, state):
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
    return (millis() - c.last_update) > MESH_TIMEOUT  // wrap-safe
```

### Reclamation (`reclaimStaleSlots()`)

- Runs at most once per `MESH_TIMEOUT` window
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
mesh::setTransport(wifi_connected ? TRANSPORT_UDP : TRANSPORT_ESPNOW):
    if (transport == TRANSPORT_ESPNOW) espnow_set_enabled(true)
    else espnow_set_enabled(false)
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
| `MESH_TIMEOUT` | `30000` | Staleness threshold (ms) |
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