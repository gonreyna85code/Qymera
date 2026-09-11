# Qymeras 1.1 Architecture Baseline

## System Overview
Qymeras is an ESP8266/ESP32 firmware for IoT sensor/actuation networks with:
- Web-based configuration UI
- ESP-NOW + WiFi peer communication (single broadcast domain)
- Automation rules engine
- EEPROM/Preferences persistence
- OTA updates via Arduino framework

## Core Runtime Lifecycle

### Initialization Order
1. **`setup()`** (user sketch) → calls `Qymera::begin()`
2. **`Qymera::begin()`** — two-phase init:
   - **Phase 1 (local, no WiFi dependency):** Serial, EEPROM/Preferences storage, credentials/settings load, OTA identity check, sensor subsystem
   - **Phase 1b (device registration, correct order):**
1. `sensors::init()` — zero sensor/calibration tables
      2. `Qymera::init()` (user) — register/discover local sensors and actuators
      3. `automations::init()` — rules (index-based, resolved at eval time)
      - **Config load deferred to first `Qymera::report()`** (in `loop()`): entities are only
        registered when the sketch calls `Qymera::xxx()` inside `Qymera::report()`, so
        `loadCalibration()`/`applyPersistedStates()` run right after the first
        `Qymera::report()` instead of in `begin()`. `sensors::ensureTimeRegistered()` binds
       the TIME entity before `loadCalibration()` so its persisted
       correction/timezone restores. See *Persistence Fixes* below.
   - **Phase 2 (network startup):** `startWiFi()` (calls `esp_netif_init()` on ESP32 before WiFi ops, non-blocking STA connect or AP mode)
   - **Deferred services** (initialized once WiFi is operational, in `checkWiFiStatus()` for STA or `startAP()` for AP mode): web server, net layer, OTA module — guarded by `web_initialized`/`net_initialized`/`ota_initialized` flags; ArduinoOTA only starts when `ota_enabled` is true
3. **`loop()`** (user sketch) → calls `Qymera::loop()`
4. **`Qymera::loop()`** main state machine:
   - Process WiFi/ESP-NOW events
   - Tick automation rules
   - Handle web server requests (only when web initialized)
   - Manage OTA if enabled (runtime flag, `ArduinoOTA.handle()` every loop)
   - First iteration: initial `report()` → `ensureTimeRegistered()` → `loadCalibration()` → `applyPersistedStates()` (persisted relay states applied before any net announce)
   - Report sensor states

### Runtime States
- **BOOT**: Initial hardware and stack setup
- **STA_CONNECTED**: WiFi station mode connected to network
- **AP_MODE**: Access point mode (no external WiFi required)
- **OTA_ENABLED**: OTA updates active (reboots after toggle)
- **FACTORY_RESET**: EEPROM cleared, AP mode active

## Sensors/Device Model

### Sensor Types (MAX_SENSORS=64)
Enum values (`src/sensors.h`), 1..12 on `main` — **no AIDIG/AIANA** (those
virtual types exist only on `feature/ai-experiments`):

| Value | Type | Description |
|-------|------|-------------|
| 1 | `SENSOR_LUMI` | Light |
| 2 | `SENSOR_HUMI` | Humidity |
| 3 | `SENSOR_TEMP` | Temperature |
| 4 | `SENSOR_PRESS` | Pressure |
| 5 | `SENSOR_LEVEL` | Level/flow |
| 6 | `SENSOR_AIRQ` | Air quality |
| 7 | `SENSOR_RAIN` | Rainfall |
| 8 | `TYPE_DIMMER` | Dimmer (actuator, 0-100%) |
| 9 | `TYPE_RELAY` | Relay (actuator) |
| 10 | `SENSOR_TIME` | Time (UTC clock; `correction` = timezone offset) |
| 11 | `SENSOR_GENERIC` | Generic sensor |
| 12 | `SENSOR_CONTACT` | Contact/relay state |

`isValidSensorType()` accepts `1..12`; 0 (`SENSOR_NONE`) marks a free slot.

### Calibration Model
- Each sensor has individual calibration persisted per-slot
- Persisted slot: `CalibrationPersist` — magic + version + **device uid** + pers_state/min/max/correction/avail/persist/pulse/pulse_ms/fade (34 bytes, 40 slots)
- Loaded **by UID match** to the registered device (survives index reordering); slots with invalid magic/version, missing keys, or non-finite floats are ignored → defaults
- **ESP32 root cause fix**: `prefs.getBytes()` leaves the buffer untouched on missing keys → `storage::get()` now zero-fills first, eliminating the "every sensor shows the same random fade value from boot" corruption
- Calibration values survive factory reset of relay states

### Actuators
- `TYPE_RELAY`: On/off control with pulse modes; **persisted state restored at boot** (UID-matched, single GPIO write before first report — no OFF→ON glitch); non-persistent relays boot OFF
- `TYPE_DIMMER`: PWM value control (0-100%)
- Pulse mode: Non-blocking duration control
- Fade mode: Smooth transition between values

## Automations/Rules

### Rule Types (MAX_RULES=20)
| Type | Description |
|------|-------------|
| `EDGE` | Trigger on state change (RISING/FALLING) |
| `THRESHOLD` | Trigger when value crosses threshold |
| `TIME` | Trigger at specific time of day |
| `INTERVAL` | Trigger at regular intervals |

### Rule Logic
- **AND/OR** composite conditions
- **Max 5 sensors** per rule condition
- **Max 5 actuators** per rule action
- Rules evaluated in `loop()` tick cycle
- Persistence: `saveRulesToEEPROM()` with diff check

### Rule Storage
- Stored in EEPROM at dedicated offsets
- Auto-loaded on boot
- Validate sensor/actuator counts on load
- Diff check before writing to EEPROM

## Persistence

### EEPROM Layout (4 KB)
| Offset | Size | Content |
|--------|------|---------|
| 0 | 10 | Relay state (reserved) |
| 10 | 100 | WiFi credentials |
| 110 | 12 | General settings (ports/interval) |
| 122 | 1360 | Sensor calibrations (40 × 34B UID slots) |
| 1482 | 1600 | Rule definitions |
| 3082 | 4 | OTA device identity token |
| 3086 | 1 | OTA enable flag |
| 3087 | - | Reserved (to 4096) |

ESP32 maps each EEPROM address to a Preferences key (`String(addr)`) in namespace `eeprom`. Missing/corrupt keys are handled by zero-fill + validation, never assumed valid.

### Flag Persistence
- `ota_enabled`: Stored at `EEPROM_OTA_FLAG_ADDR`, **normalized** (only `1` = enabled; `0xFF`/unprovisioned → disabled), loaded once into a runtime flag
- survives reboot, cleared on factory reset
- `transport_mode`: UDP (STA) or ESP-NOW (AP)

### Persistence Fixes
- **Root cause**: `loadCalibration()` ran in `begin()`, but entities are registered by
  the sketch inside `report()` (first `loop()` iteration) — and TIME only after NTP
  sync. So persisted `min/max/correction/persist/pers_state/fade/pulse` never
  matched any registered UID and were silently dropped on reboot.
- **Fix 1 (config load after registration)**: `loadCalibration()` +
  `applyPersistedStates()` moved from `begin()` into the first-iteration block of
  `loop()`, immediately after the first `report()` (which registers all local
  entities) and before any net announce.
- **Fix 2 (TIME pre-registration)**: `sensors::ensureTimeRegistered()` binds the
  TIME entity before `loadCalibration()`, so its persisted correction/timezone is
  restored even though NTP sync happens later.
- **Fix 3 (pers_state snapshot)**: enabling persist via `/calib/set` snapshots the
  live relay state into `pers_state` before saving, so a reboot right after enabling
  persistence restores the current state.
- Stability note: entity UIDs derive from registration index (`chip_id + idx + 1`);
  a stable registration order (same sketch + TIME binding at the first free slot) is
  required for persisted UIDs to keep matching across boots.

## Web/API

### HTTP Endpoints

| Method | Route | Action |
|--------|-------|--------|
| GET | `/` | Root — sends embbeded HTML |
| POST | `/save` | Heat WiFi credentials → 303 redirect + reboot |
| GET/POST | `/calib` | Read calibration in JSON |
| POST | `/calib/set` | Set: TIME / ref / min / max / fad / pulse / persist / avail / res / timezone |
| POST | `/genset/save` | Save broadcast/command/interval set |
| GET | `/rules` | List rules JSON (filters sensor_count/actuator_count == 0) |
| POST | `/rules/set` | Create/validate rule, type, comparator/threshold |
| POST | `/rules/delete` | Delete rule by id (validate bounds before delete) |
| POST | `/factory` | Factory reset — clear credentials, relay state, config → reboot in AP |
| POST | `/toggle` | API toggle for actuators (`id` = entity uid) |
| POST | `/dimmer` | Value control dimmer (`id` = entity uid, `value` 0-100) |
| GET | `/logs` | Recent logs JSON |
| GET | `/ota/status` | OTA state `{"ota":1}` or `{"ota":0}` |
| GET | `/ota/toggle` | Enable/disable OTA |

State-changing endpoints (`/calib/set`, `/toggle`, `/dimmer`) also answer
`HTTP_OPTIONS` with CORS headers so the browser UI can cross-post to remote
nodes; every response (including 400/401/429) carries
`Access-Control-Allow-Origin: *`.

### Web Interface
- **Tabs**: WiFi, Calibration, Rules, Actuators, Factory, Logs
- **Status panels**: Connection, OTA, Memory, Last reset
- **Log viewer**: 3 layers (CORE, EVENTS, SENSORS)
- **Calibration editor**: Per-sensor min/max/offset
- **Devices tab**: renders local devices + active (recent) remote devices only.
  Stale remotes, `SENSOR_NONE`, and invalid types are filtered by
  `isDeviceVisible()` before rendering; one card per active entry. Remote
  recency uses the server-computed `age_ms` field (same `millis()` timebase as
  `NET_TIMEOUT`); `id` carries the sensor uid (the JSON does not duplicate it
  as `uid`).
- **Settings tab**: renders cards for ANY valid/configurable entity — local or
  remote — since `local` indicates provenance, not configurability. Remote
  configuration is routed to the owning device over HTTP via `isVirtual()`.
  `isVirtual()` now: (1) verifies the owner's HTTP `response.ok` before
  reporting success — a 4xx/5xx shows an alert with the owner IP and never
  falls back to the local node; (2) enforces a 5s `AbortController` timeout;
  (3) detects network errors with a clear alert. Optimistic UI toggles
  (relay button, persist/pulse checkboxes) roll back on failure.
  Renderers are resolved by sensor type via `TYPE_RENDERERS` (all 12 valid
  types on `main`, 1..12); unknown types are skipped with a `console.warn` —
  GENERAL SETTINGS (node-level config: WiFi/ports/interval/OTA/factory reset)
  is never used as a fallback and is rendered exactly once, explicitly.

## UDP Transport

### Default Configuration
- **Port**: 8888 (broadcast)
- **Address**: 255.255.255.255
- **Packet format**: `PacketHeader` + `Packet` (max 250 bytes)
- **Broadcast interval**: Configurable (default 2s)

### STA Mode Behavior
- UDP broadcast on `255.255.255.255:8888`
- Requires WiFi connected to external network
- Peers discovered via broadcast

### Known Issues
- Port < 1024 or > 65500: reset to default
- Interval < 5000ms or > 600000ms: reset to default

### Wire Protocol (v4 / v5) - Packet Kind Dispatch
- Legacy protocols v1/v2/v3 use the 8-byte `PacketHeader` (magic, version, size,
  uid) with an always-sensor payload.
- **v4** adds an explicit `kind` byte (`PacketHeaderV4`, 9 bytes) after the base
  header so the receiver can dispatch the payload before parsing it:
  - `PACKET_SENSOR = 1` → one or more `Packet` structs.
  - `PACKET_LOG = 2` → one `LogPacket` (66 bytes: layer, level, message[64]).
- **v5** grows the sensor `Packet` from 47 → 58 bytes by mirroring the actuator
  config the announcing node holds: `fade`, `persist`, `pers_state`, `pulse`,
  `pulse_ms`. Remote nodes thus see the owner's relay persistence / dimmer fade
  state without a second round-trip. Legacy v3/v4 sensor packets (47 bytes) are
  still parsed for backwards compatibility; the exact-size alignment check in
  `parseBuffer()` accepts both lengths.
- `parseBuffer()` rejects unknown kinds and requires the sensor payload size to
  be an exact multiple of the packet length. This guarantees a `LogPacket` is
  NEVER parsed as a sensor `Packet` (they share the broadcast transport).
- Root-cause fix: previously the same 8-byte `PacketHeader` was used for sensor
  reports, commands AND logs, so `LogPacket` payloads were interpreted as
  `Packet` payloads and generated phantom remote sensors with garbage
  type/name bytes (e.g. types 108/109/115/119) and phantom UI cards.
- Logs received over the net are ingested into the local log buffer via
  `logger::logRemote()` (no re-broadcast, preventing a broadcast ping-pong).
- Compatibility: v1/v2 sensor packets and v3 sensor packets are still accepted
  (no kind byte → sensor payload). Legacy v3 log packets fail the exact-size
  validation and are dropped instead of being fragmented into fake sensors.
- Wire-format sizes are enforced by `static_assert` in `net.cpp`.

## Remote Sensor Lifecycle

- Remote sensors discovered over the net live in `sensors::calibrations[]`
  with `local = false`, `device_uid`, `uid` and `last_update`.
- A remote entry is **active** while `millis() - last_update <= NET_TIMEOUT`
  (30000 ms); afterwards it is **stale**.
- Stale remotes are excluded from `/calib` (and therefore from Devices/Settings
  UIs) even while their slot still exists internally.
- Slots are reclaimed only when the entry is stale AND no automation rule
  references the index (`automations::isIndexReferenced()`). Rules address
  sensors/actuators by calibration index, so referenced slots are kept (hidden)
  until the rule is deleted; local sensor indexes never change and nothing is
  shifted.
- `/calib` only exposes entries with `uid != 0`, a valid `SensorType` (1..12)
  and, for remote entries, a non-stale `last_update`.

### Discovery Redistribution Loop (fixed)

- Discovery announces ONLY local entities: `net::sendBinaryReport()` skips
  `!c.local`. The loop source was on the RECEIVING side: the sensor read
  functions (`temperature`, `humidity`, `luminosity`, `level`, `pressure`,
  `airQ`, `rain`, `custom`, `contact`, `aidig`, `aiana`) looked up slots by name
  via `findCalib()` (ANY entity) and then called `bindLocalSensor()`
  unconditionally. A name collision with a discovered REMOTE sensor rebound it:
  `local = true`, `uid = makeSensorUid()` (a NEW local uid), `device_uid`/IP =
  this node → the remote was re-announced as a local entity of this node →
  the peer registered it back → cross-node duplicates.
- Fix: the read/registration functions now use `findLocalCalib()` (matches only
  `local == true && uid != 0`). A remote entity is never rebound as local; its
  `uid`/`device_uid`/`device_ip` stay owned by the originating node. `relay()`
  and `dimmer()` were already safe (they only bind when `is_new`).
- Architectural rule enforced: LOCAL entity → may announce; REMOTE entity → may
  be visualized/configured/used, never re-announced as this node's own. A remote
  entity's uid must be stable and owned by its originating node.

### UDP Discovery Batching (fixed)

- Root cause of partial discovery (remote entities missing on ESP32): the sender
  transmitted **one UDP datagram per local sensor** while the receiver processed
  **one datagram per socket per tick**. Under bursts the queue overflowed on
  ESP32; ESP8266 only survived thanks to stack buffering/timing differences.
- Fix (`net.cpp`):
  - `sendBinaryReport()` now batches up to
    `floor((DISCOVERY_MAX_UDP_PACKET-9)/47) = 29` `Packet`s per UDP datagram
    (`DISCOVERY_MAX_UDP_PACKET = 1400`, below the Ethernet MTU → no IP
    fragmentation). `hdr.size = sizeof(PacketHeaderV4) + N*sizeof(Packet)` is
    exact; the existing parser already accepts N packets per datagram
    (`remaining % packet_len == 0`).
  - ESP-NOW keeps one entity per broadcast (RX buffer is 250 bytes); the
    `[DISC TX]`/`[DISC RX]` batch logs are TEMP-DEBUG only. RX is a bounded FIFO
    (8 slots x 250 bytes) — the callback never blocks and only bumps an
    overflow counter; dropped messages are logged from `loop()` (see
    *ESP-NOW Transport*).
  - `parseUDPPacket()` drains up to `MAX_RX_PACKETS_PER_TICK` (8) datagrams per
    socket per tick (both historical `mesh_udp` and `udp`), bounded so a UDP storm cannot
    starve `loop()`; the RX buffer was raised from 512 to 1400 bytes.
  - All validations are preserved (magic, version, kind, `hdr.size == len`,
    packet alignment, valid types). Discovery remains idempotent
    (`device_uid + sensor_id`) and only local entities are announced.

## ESP-NOW Transport

### Configuration
- **Mode**: AP mode only (WiFi disabled for ESP-NOW)
- **Broadcast**: FF:FF:FF:FF:FF:FF
- **Unicast**: To registered peers (max 25)
- **Payload**: `PacketHeader` + `Packet` (max 250 bytes)

### AP Mode Behavior
- ESP-NOW broadcast on FF:FF:FF:FF:FF:FF
- Works without WiFi network connection
- Peer management: `addPeer()`, `clearPeers()`, `getPeerCount()`
- Fallback when WiFi disconnected

### Transport Auto-detection
- **STA mode**: UDP broadcast
- **AP mode**: ESP-NOW broadcast
- Auto-switched based on WiFi mode at boot

## ESP-NOW P2P (formerly "mesh")

### Peer Management
- **Max peers**: 25
- **Registration**: Via `addPeer(mac_addr)`
- **Discovery**: Automatic via broadcast
- **Cleanup**: `clearPeers()` on factory reset

### Message Format
- **Header**: `PacketHeader` {src, dst, type, length}
- **Payload**: Up to 250 bytes
- **Types**: SENSOR_DATA, RULE_EXEC, COMMAND, ACK

### Reliability
- Send callback: `espnow_send_cb`
- Receive callback: `espnow_recv_cb`
- **Bounded RX FIFO** (8 entries x 250 bytes): the interrupt/IRAM callback
  copies payload+len+src MAC into the ring via `rx_enqueue()` under an ESP32
  portMUX critical section and never blocks, allocates, or logs. When full the
  new message is dropped and a `rx_overflow` counter is bumped; `net::tick()`
  logs a warning with the delta. `espnow_recv()` consumes the FIFO from
  `loop()`. This removes the old single-slot buffer that could drop traffic
  under bursts.
- No automatic retransmission (application level)

## Logging

### 3-Layer System
| Layer | Purpose | Entries |
|-------|---------|---------|
| **CORE** | Boot, WiFi, OTA, errors | 12 |
| **EVENTS** | Rule triggers, automations | 12 |
| **SENSORS** | Sensor readings, calibrations | 12 |

### Buffer Management
- Circular buffer: oldest overwrites new
- Per-layer `getRecentLogs()` API
- JSON format for web endpoint `/logs`
- Cleared on factory reset

### Log Entry Format
```json
{"layer":"CORE","msg":"Boot complete","ts":12345}
{"layer":"EVENTS","msg":"Rule #3 triggered","ts":12346}
{"layer":"SENSORS","msg":"TEMP calibration updated","ts":12347}
```

## OTA Behavior

### Default State
- **OTA disabled** at first boot
- Enable flag persisted at `EEPROM_OTA_FLAG_ADDR` (reserved region after the rules block)
- Can be enabled via `/ota/toggle?enabled=1`

### Toggle Flow
1. `GET /ota/toggle?enabled=1` → `core::setOtaEnabled(true)`
2. Runtime flag updated + flag persisted (`storage::saveOtaFlag(1)`) + identity token re-verified
3. Device reboots
4. On boot: runtime `ota_enabled` loaded from normalized flag
5. If enabled + WiFi/AP ready: `startOtaService()` → single `ArduinoOTA.begin()` (guarded by `ota_initialized`)
6. `ArduinoOTA.handle()` runs every loop only when `ota_enabled && ota_initialized` (no per-loop Preferences reads)
7. If disabled: ArduinoOTA never bound, no stale listener

### OTA Lifecycle Separation
- **storage** owns only persistent config: `saveOtaFlag()`, `loadOtaFlag()`, `isOtaEnabled()`, `verifyOtaIntegrity()`
- **core/network** owns the lifecycle: `ArduinoOTA.begin()`, `ArduinoOTA.handle()`, diagnostic callbacks (`onStart`/`onProgress`/`onEnd`/`onError`), single `ota_initialized` guard
- Storage never calls `ArduinoOTA.begin()` (removed) — exactly one controlled OTA initialization path exists

### OTA Status Endpoint
- `GET /ota/status` → `{"ota":1}` or `{"ota":0}`
- Reflects current flag state

### Integrity (device identity / provisioning check — NOT a firmware hash)
- The "integrity" mechanism stores a **chip-unique device token** (`GET_CHIP_ID()`)
  in a 4-byte slot at `EEPROM_OTA_HASH_ADDR`. Provisioned on first enable / empty slot.
- On each boot/toggle it compares the stored token against the current device token;
  a mismatch disables OTA.
- **Accurate naming**: this is a **device identity / provisioning check only**. It
  detects a device with a different identity (e.g. a swapped module), **not** firmware
  corruption, authenticity, or tampering. It is not cryptographic and does not hash
  the firmware image.
- Known limitation: no SHA-256/signature verification (Phase 3+ candidate per AGENTS.md).
- The enable flag (`EEPROM_OTA_FLAG_ADDR`) and the baseline no longer alias the
  relay-state region (0..9) or the WiFi credentials block — this fixes the
  reported "saving the OTA flag corrupts memory" defect (the old flag/token
  lived at offsets 9/10, overlapping relay state and the SSID length byte).

### Security
- **No authentication** on OTA endpoint
- **Recommended**: Only on local network
- **Known limitation**: Integrity is detection-only, not cryptographic authenticity

## Configuration

### WiFi Credentials
- Stored in EEPROM offset 0-511
- POST /save → validates and stores
- 303 redirect to root + reboot
- Invalid credentials: retry loop

### Timezone
- The runtime clock always stays **UTC** (NTP syncs UTC; `time()` is never
  shifted). The SENSOR_TIME calibration `correction` field IS the timezone
  offset in **minutes from UTC** (persisted).
- UTC→local is an explicit portable conversion
  (`local = utc + offset_minutes*60` decomposed with `gmtime()`), avoiding
  libc timezone globals whose semantics differ between ESP8266 and ESP32.
- POST `/calib/set` with `type=TIME` (or `timezone`) and `ref` = integer
  minutes, range **-720..840** (UTC-12..UTC+14).
- Used for TIME rule triggers (`getMinutesOfDay()`) and displayed time
  (`getTime()`).

### Broadcast/Command Intervals
- Default: 2s broadcast, 1s command
- POST /genset/save → validates and stores
- Interval < 5000ms or > 600000ms: reset to default

### Security Limitations
- **HTTP Basic Auth available** but **dormant by default** (`auth_enabled=false`);
  hardcoded credentials remain in the binary as a placeholder (deferred to Phase 3+)
- **Recommended**: Local network only
- **Known**: Credentials transmitted in plain text
- **Rate limiting active**: burst-tolerant 6 requests / 2s window on all
  state-changing endpoints (400 on malformed input, 429 on burst overflow)

## Platform Differences

### ESP8266 (generic ESP-12E / NodeMCU)
- `WiFiUdp` for UDP transport
- `ESP8266HTTPClient` for OTA
- `ESP8266HTTPUpdateServer` for web OTA
- `Preferences` library not available (uses EEPROM)
- `raw_address()` framework issue (patched)

### ESP32 (devkit)
- `WiFi` class with native UDP
- `HTTPClient` with `SECURITY_*` constants
- `Preferences` library for storage
- `ESPNow` class for ESP-NOW transport
- `Qymera::setSerialEnabled()` for pin reuse (GPIO 0-16)

### Framework Pinning
- **ESP8266**: espressif8266@3.30102.0 (avoids `raw_address()` bug)
- **ESP32**: espressif32@6.5.0 (avoids EEPROM bugs)
- Pinning prevents breaking changes from framework updates

## Known Bugs & Fragile Areas

### Framework Bugs (Pinned Versions Fix These)
1. `raw_address()` private method (ESP8266 core)
2. `IPAddress::IPAddress()` deleted constructor
3. `WiFiEventStationModeGotIP::WiFiEventStationModeGotIP()` deleted function
4. ESP32 EEPROM corruption bugs

### Code Fragilities
1. **Blocking delays** in `loop()` during WiFi reconnect
2. **EEPROM write endurance**: 4KB limited writes
3. **Memory fragmentation**: String objects in web handlers
4. **Race conditions**: WiFi/Ethernet event callbacks
5. **Calibration loss**: Values lost on factory reset (EEPROM cleared), but survive power cycle (loaded from EEPROM on boot)

### Memory Risks
- **Buffer overflow**: Log buffers (30→12 entries fixed)
- **Stack overflow**: Deep recursion in rule evaluation
- **Heap fragmentation**: Repeated HTTP server restarts
- **EEPROM saturation**: Max 512 bytes WiFi credentials

## Security Limitations

### Current State
- **HTTP Basic Auth available** (disabled by default, can be enabled via `AUTH_USERNAME`/`AUTH_PASSWORD` constants in `web.cpp`)
- **OTA device identity check** (chip token, not a firmware hash; verified on boot and toggle)
- **No encryption** on web UI or API (HTTP only)
- **Rate limiting active** on state-changing POST endpoints (6 req/2s burst-tolerant)
- **Strict input validation** on `/calib/set` (and ID parsing): rejects empty
  strings, trailing junk, overflow, NaN/Inf; type-specific ranges
  (timezone -720..840 integer minutes, fade/pulse 0..3600000 ms, persist/avail
  strict 0/1). No silent coercion of garbage to 0.

### Recommended Hardening
1. Enable HTTP basic auth by setting `AUTH_USERNAME` and `AUTH_PASSWORD` in `web.cpp`
2. Use HTTPS for OTA transfers
3. ~~Implement rate limiting on `/save`, `/rules/set`~~ ✅ done (6 req/2s burst)
4. Validate all JSON payloads sizes
5. Add CSRF tokens on web forms
6. Signed OTA payloads (SHA-256, Phase 3+)

### Known Acceptable Limitations
- Open network recommended for local IoT
- No auth = simpler setup for trusted networks
- Physical access required for most attacks
- Debug/OTA ports physically accessible

## Testability

### Unit Testable Components
- Rule evaluation logic (pure functions)
- Calibration math (input/output verification)
- Packet parsing (fixed format validation)
- EEPROM offset calculations

### Integration Test Challenges
- WiFi dependency (requires mock or AP mode)
- EEPROM/Persistence (emulated in tests)
- Web server (requires HTTP client mock)
- OTA (requires network stack mock)

### Recommended Test Approach
1. **Host tests** for rule logic and calibration math
2. **Integration tests** with ESP8266-ESP32 mock
3. **Property tests**: invariant checks on state transitions
4. **Fuzz testing**: malformed JSON payloads

## Production Readiness Checklist

### ✅ Completed
- Framework pinned to stable versions
- `raw_address()` issue patched
- EEPROM/Preferences fixes applied
- All source files compile
- Build verification on ESP8266 and ESP32
- Web authentication (HTTP Basic Auth) ✅
- OTA device identity check (chip token, not a firmware hash) ✅

### ⚠️ Requires Attention
- [ ] Web authentication implementation ✅ (completed - HTTP Basic Auth added)
- [ ] OTA device identity check ✅ (completed - chip-token verification added)
- [ ] Memory leak testing under load (NOT TESTED on hardware)
- [ ] Long-term EEPROM write endurance (NOT TESTED)
- [ ] Network partition recovery (code fix applied; hardware test pending)
- [ ] Factory reset reliability (hardware test pending)

### ❌ Production Blockers
- [ ] None critical - all compile and run
- [ ] Authentication must be added before public deployment
- [ ] OTA security hardening recommended

### ✅ Acceptable Limitations (for 1.1)
- No authentication (local network only; auth infra dormant)
- No HTTPS for OTA (HTTP only)
- ~~No rate limiting on API~~ ✅ rate limiting active (6 req/2s burst)
- ~~No input sanitization beyond bounds~~ ✅ strict `/calib/set` validation added
- Framework-level bugs pinned/fixed

## Recommended Implementation Order (Phase 2)

1. **Authentication layer** on web/API endpoints
2. **OTA device identity check** (chip token) + optional SHA-256 authenticity (Phase 3+)
3. **Memory profiling** and leak fixes
4. **Long-term endurance** testing
5. **Network resilience** recovery tests
6. **Factory reset** reliability validation
7. **Documentation** updates for new features
8. **Test suite** development for core logic

## Optional AI Subsystem (authorized, NOT on main)

An **optional external AI assistant** is authorized for the next release and is
developed exclusively on `feature/ai-experiments`. It is fully opt-in: the
deterministic core above is untouched when disabled. On `main` (HEAD
`5e46e12`) there is no AI code — no `ai.cpp`/`ai.h`, no AIDIG/AIANA types, no
QMAI EEPROM block. LLM tool-loop probe payloads (`qwen3.5:2b` against the
device HTTP API) were archived out of the repo root on 2026-08-27; they are not
part of the 1.1 tree. See `AGENTS.md` "Scope Change Authorization (2026-08)".