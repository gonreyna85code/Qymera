# Qymera Structure & Ownership

Updated 2026-09-15 (Qymera 1.0.0 STABLE, official repo `gonreyna85code/Qymera`).
`main` is the **1.0.0 stable release** (deterministic core + Web OTA + GUI).
Branch map: `main` = 1.0.0 stable (this tree) · historical 1.1/1.2 = frozen
baseline history · Dashboard/AI = `feature/ai-experiments` + future (separate
direction). Mirrors the actual source tree — files not listed here do not exist
in `src/`.

## Source Files (src/)

| File | Purpose | Owner | Status |
|------|---------|-------|--------|
| `Qymera.h` | Master library header for Arduino IDE sketches; public `Qymera::` facade (lifecycle, sensors, actuators, serial control) forwarding to core/sensors | Core team | ✅ Up to date |
| `main.ino` | PlatformIO sketch entry point: `setup()`/`loop()` delegate to `Qymera::begin()`/`Qymera::loop()`; provides hooks `Qymera::init()`/`Qymera::report()`/`Qymera::onCommand()`. `.ino` extension so the Arduino IDE library build ignores it (no duplicate symbols with user sketch) | Platform team | ✅ Working |
| `config.h` | Platform auto-detection (ESP8266/ESP32/S2/S3/C3), system limits, EEPROM layout offsets, PWM abstraction, network defaults | Platform team | ✅ Up to date |
| `core.cpp` | MCU init, WiFi, OTA lifecycle, per-`report()` loop scaffolding, memory reporting | Core team | ✅ Working |
| `core.h` | Core class definition, OTA control | Core team | ✅ Up to date |
| `storage.cpp` | Persistence backend: EEPROM (ESP8266) / Preferences (ESP32), zero-fill on missing keys, calibration UID slots, rules, OTA flag + device token, factory reset | Storage team | ✅ Working |
| `storage.h` | Persistence API (begin/read/write/get/put/commit; credentials; settings; OTA flag; calibration; rules; integrity) | Storage team | ✅ Up to date |
| `web.cpp` | HTTP server, endpoint handlers, rate limiting, strict input validation, OTA toggle/status | Web team | ✅ Working |
| `web.h` | Web server class, endpoint declarations | Web team | ✅ Up to date |
| `html.cpp` | Embedded HTML/CSS/JS (Devices/Settings/Rules/Automations/Logs; renderers for types 1..12) | Web team | ✅ Working |
| `html.h` | Embedded HTML/CSS/JS constants | Web team | ✅ Up to date |
| `sensors.cpp` | Sensor reading, calibration, actuators, remote sensor lifecycle (stale/reclaim), fades, pulses | Sensors team | ✅ Working |
| `sensors.h` | `SensorType` enum (NONE..CONTACT, 1..12), registration callbacks, `Calibration` struct, remote discovery callbacks | Sensors team | ✅ Up to date |
| `mesh.cpp` | UDP + ESP-NOW mesh transport; packet protocol v4/v5 (kind byte, batched datagrams); bounded RX; drain fix | Transport team | ✅ Working |
| `mesh.h` | Transport layer, wire protocol, ESP-NOW peer management | Transport team | ✅ Up to date |
| `espnow_p2p.cpp` | ESP-NOW transport implementation (bounded 8x250B RX FIFO, portMUX) | Transport team | ✅ Working |
| `espnow_p2p.h` | ESP-NOW transport header | Transport team | ✅ Up to date |
| `automations.cpp` | Rule engine, rule storage, `tick()` (every 50 ms: EDGE/THRESHOLD/TIME/INTERVAL, AND/OR, cooldown/delay) | Automations team | ✅ Working |
| `automations.h` | `Rule` struct, evaluation logic, `isIndexReferenced()` | Automations team | ✅ Up to date |
| `log.h` | 3-layer logging interface (CORE/EVENTS/SENSORS) | Logging team | ✅ Working |
| `log.cpp` | Logging implementation, circular buffers, JSON output, remote log ingest | Logging team | ✅ Working |

> Note: there is **no `ota.cpp`/`ota.h`** in `main`. OTA is handled by three
> modules: `storage.cpp` (persisted flag + device identity token), `core.cpp`
> (`ArduinoOTA` lifecycle: single `begin()`, per-loop `handle()`, guards), and
> `web.cpp` (`/ota/status`, `/ota/toggle`).

## Configuration Files

| File | Purpose | Owner |
|------|---------|-------|
| `platformio.ini` | Build config (envs: `esp8266_generic`, `esp32_devkit`, `esp32c3_devkit`) | Platform team |
| `library.properties` | Arduino library metadata | Platform team |
| `.gitignore` | Excludes `.pio`, `.vscode`, `.theia`, `.continue`, `/AGENTS.md`, `/skill` (local-only files) | Lead engineer |
| `README.md` | User documentation (English) | Documentation team |
| `AGENTS.md` | Agent/copilot guidelines (**not versioned** — local workspace file) | Lead engineer |
| `docs/architecture-baseline.md` | System architecture | Lead engineer |
| `progress.md` | Task tracking / final validation table | Lead engineer |
| `structure.md` | File organization (this file) | Lead engineer |
| `todo.md` | Task list | Lead engineer |
| `production-readiness.md` | Production readiness state | Lead engineer |

## Example Sketches

| File | Purpose | Status |
|------|---------|--------|
| `examples/Base/Base.ino` | Base example sketch | ✅ Created |
| `examples/HardwareDemo/HardwareDemo.ino` | Hardware demo | ✅ Present |

## Tests / Tooling

| File | Purpose | Status |
|------|---------|--------|
| `tests/host_sanity.py` | Host suite: timezone conversion, strict float parsing + ranges, ESP-NOW RX FIFO (45 checks / 45 pass) | ✅ Working |
| `scripts/esp32_clean_reset.py` | ESP32 env post-build script (clean reset hook) | ✅ Working |

## Module Responsibilities

- **Core (`core.cpp`)** — deterministic runtime init + loop scaffolding; WiFi STA/AP
  management; OTA lifecycle; first-iteration `report()` → calibration load order.
- **Storage (`storage.cpp`)** — a single persistence API for EEPROM (ESP8266) and
  Preferences (ESP32); zero-fill + validation; UID-based calibration slots;
  OTA flag/token; factory reset. Never calls `ArduinoOTA`.
- **Sensors (`sensors.cpp`)** — registration, `SensorType` values, calibration,
  relay/dimmer, fades/pulses, TIME (UTC clock + timezone offset), remote
  lifecycle (local announces / remote visualizes + configures, never rebinds).
- **Web (`web.cpp`)** — HTTP endpoints with strict validation + rate limiting;
  remote-config routing via owner IP; auth gate (dormant).
- **Mesh/Transport (`mesh.cpp` + `espnow_p2p.cpp`)** — UDP batching, packet
  v4/v5 kind dispatch, ESP-NOW bounded RX FIFO, peer mgmt, STA=UDP / AP=ESP-NOW.
- **Automations (`automations.cpp`)** — deterministic rule evaluation in `loop()`.
- **Logging (`log.cpp`)** — 3-layer circular buffers, JSON for `/logs`, remote
  ingest without re-broadcast.

## Platform Dependencies

### ESP8266 (generic ESP-12E / NodeMCU)
- Core: Arduino core espressif8266, framework package pinned `3.30102.0`.
- Storage: EEPROM (4 kB). `WiFiUdp` for UDP; `ESP8266HTTPUpdateServer` for OTA.
- `raw_address()` framework quirk patched in the pinned core.

### ESP32 (devkit, C3/S2/S3 via `config.h` defines)
- Core: espressif32 platform pinned `@6.5.0`.
- Storage: `Preferences` namespace `eeprom` (address-keyed).
- Web/OTA: `WiFi`-native UDP; `HTTPClient` with `SECURITY_*` constants.

### Framework pinning
- ESP8266 framework package: `framework-arduinoespressif8266@3.30102.0`.
- ESP32: `platform = espressif32@6.5.0`.
- Purpose: avoid framework bugs and unplanned upgrade breakage.

## Known Couplings & Dependencies

### Tight couplings (acceptable)
1. EEPROM ↔ Preferences via the `storage` API (platform swap).
2. WiFi mode ↔ transport mode (STA→UDP, AP→ESP-NOW).
3. OTA flag ↔ runtime flag in `core` (normalized, cached, no per-loop reads).
4. Rule references ↔ calibration slot indices (resolved at eval time).

### To watch
1. Web handlers call into `sensors`/`automations` directly (fine today; an API
   layer is a future nicety).
2. Mesh callbacks in `core` reference network state.
3. Calibration layout is offset-based in `config.h` (component-consistent).

## Build Verification

```bash
pio run -e esp8266_generic   # ESP8266 (full link, main.ino provides setup/loop)
pio run -e esp32_devkit      # ESP32
pio run -e esp32c3_devkit    # ESP32-C3 (build-verified)
pio run                      # all platforms
python tests/host_sanity.py  # host suite (45/45)
```

Expected: full-link SUCCESS on all three envs (this repo builds
`src/main.ino`, which provides `setup()`/`loop()`). Typical footprints
(2026-08-27): ESP8266 RAM 69.6% / Flash 42.2%; ESP32 RAM 22.6% / Flash 73.7%;
ESP32-C3 RAM 20.9% / Flash 72.8%.

## Module Integration Points

```
setup()
  → Qymera::begin()   Phase 1: serial, storage, creds/settings, OTA identity
                       Phase 1b: sensors::init() → Qymera::init() (register
                                 entities) → automations::init()
                       Phase 2: startWiFi() (+esp_netif_init on ESP32)
                       Deferred: web/mesh/OTA once WiFi/AP operational
loop()
  → Qymera::loop()     first iteration: Qymera::report() → ensureTimeRegistered()
                       → loadCalibration() → applyPersistedStates()
     → Rules::tick()   automations (50 ms)
     → WebServer::handleClient()
     → Transport::tick()      mesh UDP/ESP-NOW
     → OTA::handle()          only if ota_enabled && initialized
     → Qymera::report()       sensor reads via Qymera::xxx()