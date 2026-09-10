# Qymera Runtime Architecture

**Version:** 1.1.0 (baseline)  
**Date:** 2026-08-31  
**Branch:** main

---

## 1. High-Level Overview

Qymera is a deterministic, local-first distributed automation runtime for ESP-class devices (ESP8266, ESP32, ESP32-C3/S2/S3). It provides:

- Embedded web HTTP server with GUI
- P2P UDP/ESP-NOW mesh communication
- Rule engine (EDGE, THRESHOLD, TIME, INTERVAL)
- EEPROM/Preferences persistence
- Sensors and actuators with relay/dimmer control
- Deterministic boot sequence

---

## 2. Module Dependency Graph

```
src/
├── Qymera.h          # Public facade (namespace Qymera::)
├── config.h          # Platform abstraction, constants, EEPROM layout
├── core.h/cpp        # Runtime orchestration, WiFi, OTA, main loop
├── sensors.h/cpp     # Entity model, sensors/actuators, mesh callbacks
├── web.h/cpp         # HTTP server, API endpoints, auth, rate limit
├── mesh.h/cpp        # UDP/ESP-NOW transport, packet parsing, discovery
├── automations.h/cpp # Rule engine (EDGE/THRESHOLD/TIME/INTERVAL)
├── storage.h/cpp     # EEPROM/Preferences persistence, migration
├── log.h/cpp         # Layered logging, UDP broadcast, remote log ingest
├── espnow_p2p.cpp    # ESP-NOW RX FIFO (bounded ring buffer)
├── html.cpp          # Embedded GUI (single-page app, ~70KB compressed)
└── main.ino          # PlatformIO sketch entry (ignored by Arduino IDE lib build)
```

---

## 3. Execution Flow

### Boot Sequence (`core::begin` → `core::loop`)

```
core::begin()
  ├── Serial init (115200)
  ├── logger::init()
  ├── storage::loadCredentials()
  ├── storage::loadGeneralSettings()
  ├── setupWiFiEvents()
  ├── storage::loadOtaFlag() + verifyOtaIntegrity()
  ├── sensors::init()                    # 1. Clear calibrations, register mesh callbacks
  ├── Qymera::init()                     # 2. User hook: register sensors/actuators
  ├── automations::init()                # 3. Load rules, clear rule states
  ├── startWiFi()                        # Phase 2: Network startup (deferred)
       ├── WiFi.begin() or startAP()
       ├── On connect: sensors::initNTP(), mesh::init(), web::init(), OTA
       └── On timeout: startAP()
```

### Main Loop (`core::loop`)

```
core::loop()  [runs continuously]
  ├── web::server.handleClient()         # HTTP handling
  ├── checkWiFiStatus()                  # Reconnect logic, deferred service init
  ├── WiFi retry logic (180s interval)
  ├── mesh::setTransport()               # UDP if WiFi, ESP-NOW if AP/offline
  ├── First iteration (first_report):
  │   ├── Qymera::report()               # Register entities, send initial mesh announce
  │   ├── sensors::ensureTimeRegistered()
  │   ├── storage::loadCalibration()     # Restore persisted state by UID
  │   └── sensors::applyPersistedStates()# Apply relay states (persist → pers_state)
  ├── sensors::updateNTPTime()
  ├── mesh::tick(now_ms)                 # UDP/ESP-NOW RX, parsing, discovery
  ├── sensors::reclaimStaleSlots()       # Remote entity lifecycle
  ├── automations::tick(now_ms)          # Rule evaluation
  ├── sensors::applyFades()
  ├── sensors::checkPulses()
  ├── Periodic report (genset.report_interval):
  │   ├── Qymera::report()
  │   └── mesh::sendBinaryReport()
  └── ArduinoOTA.handle() (if enabled)
```

---

## 4. Concurrency Model

**Single-threaded cooperative scheduler** (Arduino `loop()`). All subsystems cooperate via:

- `mesh::tick()`: bounded UDP drain (max 8 packets/tick per socket)
- `automations::tick()`: 50ms minimum sample interval
- `sensors::checkPulses()/applyFades()`: stateless per-tick
- `web::handleClient()`: non-blocking HTTP

**No RTOS, no preemption.** Long-running operations must yield.

---

## 5. Data Ownership

| Data | Owner | Lifetime |
|------|-------|----------|
| `sensors::calibrations[MAX_SENSORS]` | `sensors::` | Runtime (cleared on `sensors::init()`) |
| `mesh::reports[MAX_SENSORS]` | `mesh::` | Runtime |
| `automations::rules[MAX_RULES]` | `automations::` | Runtime + EEPROM persisted |
| `automations::states[MAX_RULES]` | `automations::` | Runtime |
| `storage::` EEPROM/Preferences | `storage::` | Persistent |
| `logger::` buffers | `logger::` | Runtime (ring buffers) |

---

## 5. Key Invariants

1. **Boot order**: `sensors::init()` → `Qymera::init()` → `automations::init()` before network
2. **First report** registers all entities before `loadCalibration()` + `applyPersistedStates()`
3. **Remote entities** never rebound as local (`findLocalCalib` filters `c.local`)
4. **Stale remote slots** reclaimed only if not referenced by rules
5. **Relay persistence** applied exactly once at boot via `applyPersistedStates()`
6. **Mesh transport** switches: UDP (WiFi STA) ↔ ESP-NOW (AP/offline)

---

## 6. Module Responsibilities

| Module | Responsibility | Public API |
|--------|----------------|------------|
| `core` | Boot, WiFi, OTA, main loop, time sync | `begin()`, `loop()`, `is_connected()`, `setOtaEnabled()` |
| `sensors` | Entity registry, sensors/actuators, remote lifecycle | `temperature()`, `relay()`, `setRelay()`, `handleDimmer()`, `onRemoteCommand()` |
| `web` | HTTP server, API, auth, rate limit, CORS | `init()`, `handleToggleApi()`, `handleDimmerApi()`, `handleRules()` |
| `mesh` | UDP/ESP-NOW transport, packet parsing, discovery | `init()`, `tick()`, `sendCommand()`, `sendBinaryReport()`, `sendLog()` |
| `automations` | Rule engine (EDGE/THRESHOLD/TIME/INTERVAL) | `init()`, `tick()`, `saveRulesToEEPROM()` |
| `storage` | EEPROM/Preferences abstraction, migration | `loadCalibration()`, `saveCalibrationSlot()`, `loadRules()` |
| `logger` | Layered logging, UDP broadcast, remote ingest | `log()`, `logf()`, `getRecentLogsJson()`, `setSerialEnabled()` |

---

## 7. Error Handling Strategy

- **Validation first**: `parseStrict*` functions reject malformed input early
- **Rate limiting**: Sliding window + burst allowance (6 req/2s)
- **Auth**: Optional Basic Auth (disabled by default for backward compat)
- **Mesh parsing**: Exact-size validation, magic byte, version check, payload length check
- **UDP drain**: Bounded per tick (max 8 packets/socket/tick) to prevent loop starvation
- **OTA integrity**: Chip-ID provisioning check (not full-image hash)
- **Rule execution**: Cooldown, delay, pending state machine

---

## 8. Memory Profile (Baseline)

| Platform | RAM | Flash | Notes |
|----------|-----|-------|-------|
| ESP8266 | 69.6% (57KB/81KB) | 42.2% (441KB/1044KB) | Tightest constraint |
| ESP32 | 22.6% (74KB/327KB) | 73.7% (966KB/1310KB) | Comfortable |
| ESP32-C3 | 20.9% (68KB/327KB) | 72.8% (953KB/1310KB) | Comfortable |

---

## 9. Network Protocol Summary

- **Transport**: UDP broadcast (port 13345) or ESP-NOW
- **Packet format**: Versioned (v1-v5), magic `0xA5`, exact-size validation
- **Packet kinds**: `PACKET_SENSOR`, `PACKET_LOG`
- **Command delivery**: Unicast UDP to owner IP or ESP-NOW broadcast
- **Discovery**: Periodic broadcast of local entities (batched UDP, one-per-broadcast ESP-NOW)

---

## 9. Key Files for Phase 1+ Work

| Phase | Primary Files |
|--------|---------------|
| 1 Entity Model | `sensors.h`, `sensors.cpp`, `storage.cpp` (CalibrationPersist) |
| 2 Identity | `sensors.cpp` (`makeSensorUid`, `findCalibByUid`), `storage.cpp` |
| 3 Protocol | `mesh.h`, `mesh.cpp`, `mesh.h` (Packet structs) |
| 4 Command Delivery | `mesh.cpp` (`sendCommand`, `onRemoteCommand`), `sensors.cpp` |
| 5 Transport Abstraction | `mesh.h/cpp` (`Transport` enum, `setTransport`) |
| 7 Automations | `automations.h/cpp` |
| 9 Persistence | `storage.h/cpp`, `config.h` (EEPROM layout) |
| 10 Memory | All `.cpp`, `html.cpp` (payload size) |
| 11 Scheduler | `core.cpp` (`loop()` structure) |
| 13 HTTP API | `web.cpp` |
| 14 Security | `web.cpp` (auth, rate limit), `storage.cpp` (OTA integrity) |

---

## 10. Baseline Metrics (2026-08-31)

| Metric | Value |
|--------|-------|
| Git SHA | `77606f4` |
| Version | 1.1.0 |
| Build | 3/3 environments SUCCESS |
| Host tests | 45/45 PASS |
| ESP8266 RAM | 69.6% (57,024 bytes) |
| ESP8266 Flash | 42.2% (441,188 bytes) |
| ESP32 RAM | 22.6% (74,028 bytes) |
| ESP32 Flash | 73.7% (966,033 bytes) |
| ESP32-C3 RAM | 20.9% (68,564 bytes) |
| ESP32-C3 Flash | 72.8% (953,568 bytes) |
| Host tests | 45/45 PASS |
| Host test categories | timezone, strict float, ESP-NOW FIFO |
| HTML payload | ~70KB (embedded in `html.cpp`) |
| Max sensors | 64 (40 persisted) |
| Max rules | 20 |
| Mesh timeout | `MESH_TIMEOUT` (default 30s) |
| OTA integrity | Chip-ID provisioning (not full hash) |

---

## 11. ESP8266 OTA Flash-Config Gate

ESP8266 Arduino core (2.4.x and 3.x) makes `Update.begin()` (used by ArduinoOTA,
started in `core.cpp`) enforce `ESP.checkFlashConfig(false)`: the flash capacity
encoded in the **boot header at flash address `0x0000`** must be `<=` the real
chip size (`spi_flash_get_id()`). On mismatch, every network update is rejected
with `UPDATE_ERROR_FLASH_CONFIG` (8) — surfaced by `espota.py` as
`Bad Answer: ERR: ERROR[8]: Flash config wrong: real: ..., SDK: ...`.

- `real` = physical flash (e.g. 1,048,576 = 1 MB).
- `SDK` = flash-size nibble read from the image header at `0x0000` written by the
  **last serial flash** (`Esp.cpp` `getFlashChipSize()`/`magicFlashChipSize()`),
  **not** the currently running sketch. OTA never rewrites `0x0000`, so the
  Arduino IDE *Flash Size* menu cannot affect a device that has a stale 4 MB
  header.
- Serial uploads that pass `--flash_size detect` (current ESP8266 core, 3.x)
  rewrite the header to the real size and repair the gate. Serial uploads that
  keep a 4 MB compile-time header (PlatformIO `esp12e` + esptool `keep`, since
  `espressif8266` invokes `write_flash 0x0 <bin>` without `--flash_size`)
  re-introduce the mismatch on 1 MB hardware.
- This repo pins `[env:esp8266_generic]` to the 1 MB layout
  (`board_build.ldscript = eagle.flash.1m64.ld`) so PIO serial flashes write a
  1 MB header (verified: `firmware.bin` header nibble `0x2`). Use
  `eagle.flash.4m1m.ld` only when the node really has 4 MB.

---

## 12. Command Delivery Semantics (Phase 4)

UDP and ESP-NOW do not guarantee sent == received == executed. Phase 4
(`src/cmd_delivery.h`, namespace `qymera::delivery`) layers reliability on top
of the V2 `COMMAND` / `COMMAND_ACK` / `COMMAND_ERROR` messages.

### 12.1 Outbound (`ReliableQueue`)

| Constant | Value | Meaning |
|----------|-------|---------|
| `MAX_PENDING` | 6 | Fixed queue depth (no dynamic allocation) |
| `MAX_RETRIES` | 3 | Retransmissions after the first send |
| `BASE_RETRY_MS` | 2000 / `RETRY_BACKOFF` 2 | Backoff 2 s → 4 s → 8 s |
| `COMMAND_TTL_MS` | 30000 | Command dropped after 30 s unacknowledged |

Flow (driven from `mesh::tick()`):

1. `sensors` actuator path → `mesh::sendReliableV2Command()` → transmit `COMMAND` (ACK_REQ) + `ReliableQueue.enqueue()`.
2. Inbound `COMMAND_ACK` / `COMMAND_ERROR` correlate on `msg_id` and free the slot (`onAck` / `onError`).
3. Missed ACK → `deliveryTick()` retransmits **reusing the original `msg_id`**, backoff ×2 per attempt.
4. Exhausted retries keep the slot until TTL, then decay with a warning.

### 12.2 Inbound (`DupRing`)

Retransmissions reuse `msg_id`, so the receiver dedups before executing:

| Constant | Value | Meaning |
|----------|-------|---------|
| `DUP_RING_SIZE` | 12 | Fixed ring |
| `DUP_WINDOW_MS` | 20000 | Covers the 14 s retry schedule + margin |

First sight → handler runs once, status recorded, one `COMMAND_ACK`/`COMMAND_ERROR`
sent. Duplicate sight → response replayed from ring, actuator **not** re-executed.

### 12.3 Ownership of the Response

`V2CommandCallback` returns an `AckStatus` (0=OK, 1=not-found, 2=invalid,
3=not-local, 4=busy). The transport sends the ACK/ERROR — handlers no longer
emit their own, which removes the Phase 3 provisional double-ACK and the
message-id mismatch (sensors used `entity_id` as `msg_id` before).

### 12.4 Memory Impact

Two fixed structs: `PendingCommand` ≈ 6 × ~20 B + `DupEntry` 12 × ~13 B ≈ ~280 B RAM, static.

### 12.5 Files

| File | Change |
|------|--------|
| `src/cmd_delivery.h` | NEW — `ReliableQueue`, `DupRing`, `AckStatus` (pure C++, host-testable) |
| `src/protocol_v2.h`, `src/mesh.h` | `V2CommandCallback` returns `uint8_t`; `sendReliableV2Command()` decl |
| `src/mesh.cpp` | queue/dup statics, `sendCommandFrame()` (shared msg_id), `sendReliableV2Command()`, `deliveryTick()` in `tick()`, ACK/ERROR correlation, inbound dedup + single response |
| `src/sensors.h/cpp` | `onV2Command()` returns status (no internal ACK); remote actuators prefer reliable V2 when `entity_id != 0` |
| `src/core.cpp` | periodic `sendV2Hello()` + per-entity `sendV2EntityAnnounce()` (gives remotes stable `entity_id`) |
| `tests/host_sanity.py` | +36 command-delivery mirror tests (138/138 PASS) |