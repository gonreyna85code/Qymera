# Qymera Testing Strategy

**Version:** 1.1.0 (baseline)  
**Date:** 2026-08-31  
**Branch:** main

---

## 1. Testing Pyramid

```
                    ┌─────────────────┐
                    │  Hardware Tests │  ← Few, high confidence
                    ├─────────────────┤
                    │ Integration     │  ← Medium
                    ├─────────────────┤
                    │ Protocol/State  │  ← More
                    ├─────────────────┤
                    │ Unit Tests      │  ← Many, fast, isolated
                    └─────────────────┘
```

---

## 2. Current Test Suite

### Host Sanity Tests (`tests/host_sanity.py`)

**Run**: `python tests/host_sanity.py`  
**Exit code**: 0 = pass, 1 = fail  
**Categories**: 45 tests total

| Category | Tests | Description |
|----------|-------|-------------|
| Timezone | 10 | UTC→local epoch, minutes-of-day, cross-day boundaries |
| Strict Float | 18 | `parseStrictFloat`: empty, garbage, valid, whitespace, junk, overflow, nan, inf |
| ESP-NOW FIFO | 12 | Bounded ring buffer: enqueue, overflow, FIFO order, wrap-around, drain, reuse |

**CI Integration**: Run on every build/PR. Must pass 45/45.

---

## 3. Required Test Layers (Phase 16)

### 3.1 Unit Tests (New)

**Target**: Pure logic functions, no hardware dependencies

| Module | Functions to Test |
|--------|-------------------|
| `sensors` | `calibrate()`, `findLocalCalib()`, `findCalibByUid()`, `isStaleRemote()`, `isEntryVisible()`, `makeSensorUid()` |
| `web` | `parseStrictUnsigned()`, `parseStrictLong()`, `parseStrictFloat()`, `checkRateLimit()`, `checkAuth()` |
| `automations` | `isDateInRange()`, `executeActions()` logic, rule condition evaluation |
| `net` | `encodeFloat()`, `fillPacket()`, `parseBuffer()` validation logic |
| `storage` | `CalibrationPersist` serialization, `RulesHeader` validation, migration logic |
| `logger` | `getRecentLogsJson()`, buffer ring logic, level/layer filtering |
| `config` | `pwmWritePin()`, `pwmReadPin()` cross-platform consistency |

**Framework**: Unity or custom minimal (host-compiled C++)

### 3.2 Protocol Tests (New)

**Target**: Wire-format validation

| Test | Description |
|------|-------------|
| Valid v1-v5 packets | Parse → correct field extraction |
| Invalid magic | Rejected |
| Invalid version | Rejected |
| Size mismatch | Rejected |
| Wrong payload multiple | Rejected |
| Log packet exact size | Rejected if wrong |
| Unknown kind byte | Rejected |
| v1-v4 backward compat | Parsed into v5 layout |
| Log packet never triggers sensor callback | Verified |

### 3.3 State Machine Tests (New)

**Target**: Rule engine, remote lifecycle, persistence

| Test | Description |
|------|-------------|
| Rule EDGE: rising/falling | Anti-bounce counter, stable state |
| Rule THRESHOLD: GT/LT/EQ | Float comparison, hysteresis |
| Rule TIME: cross-day, date range | Time zone, DST not applicable |
| Rule INTERVAL: exact interval | Cooldown, delay, pending state |
| Rule AND/OR logic | Short-circuit evaluation |
| Remote stale → hidden → reclaimed | `isStaleRemote`, `isEntryVisible`, `reclaimStaleSlots` |
| Rule protects stale slot | `isIndexReferenced` prevents reclaim |
| Relay persistence | Boot: `persist+pers_state` → ON, else OFF |
| Pulse mode | ON starts pulse, OFF cancels, state sync |
| Dimmer fade | Start → progress → complete, inverted |

### 3.4 Persistence Tests (New)

| Test | Description |
|------|-------------|
| Calibration persist round-trip | Save → reboot → load → fields match |
| Rules persist round-trip | Save → reboot → rules intact |
| Factory reset | All regions cleared, defaults restored |
| OTA flag | Enable/disable persists |
| Schema migration | Old format → new format (when added) |
| Corruption detection | Bad magic/version → defaults |
| Write minimization | `saveCalibrationSlot` only writes on change |
| Concurrent write safety | No corruption under power loss (best effort) |

### 3.5 Integration Tests (New)

| Test | Description |
|------|-------------|
| Boot sequence | `begin()` → `loop()` first report → persistence load |
| WiFi connect → services init | STA connect → NTP, net, web, OTA |
| WiFi timeout → AP fallback | 15s timeout → AP mode, web server up |
| Transport switch | WiFi down → ESP-NOW, WiFi up → UDP |
| Remote discovery | Broadcast → callback → local entity created |
| Remote stale → hidden | Timeout → `isEntryVisible` false |
| Command delivery | UDP unicast → `onRemoteCommand` → actuator toggle |
| Log broadcast → remote ingest | `sendLog` → `logRemote` → buffer |
| OTA enable/disable | Toggle → `ArduinoOTA.begin()` / disabled |

### 3.6 Hardware Tests (Existing + Expanded)

| Test | Platform | Description |
|------|----------|-------------|
| Boot | ESP8266, ESP32, ESP32-C3 | Clean boot, serial output |
| WiFi connect | All | STA connect, IP acquired |
| AP mode | All | SSID visible, web UI accessible |
| Web UI | All | Tabs, cards, wizard, settings, logs |
| Relay toggle | All | `/toggle` → GPIO state change |
| Dimmer | All | `/dimmer` → PWM value |
| Pulse | All | Pulse mode ON/OFF, state sync |
| Fade | All | Fade duration, inverted |
| Rule EDGE | All | Rising/falling, anti-bounce |
| Rule THRESHOLD | All | GT/LT/EQ, float compare |
| Rule TIME | All | Time-of-day, date range |
| Rule INTERVAL | All | Interval, cooldown |
| Persistence | All | Reboot → state restored |
| Factory reset | All | `/factory` → AP mode, clean config |
| OTA | All | `/ota/toggle` → upload → verify |
| Peer discovery | 2+ nodes | Cross-entity visibility |
| Remote actuator | 2+ nodes | Toggle remote relay/dimmer |
| Remote stale | 2+ nodes | Power off remote → hidden → reclaim |

### 3.7 Stress / Chaos Tests (Phase 17)

| Test | Description |
|------|-------------|
| 10 nodes | Full peer group, all endpoints |
| 20 nodes | Scale test |
| 64 entities | Max entities per node |
| 20 rules | Max rules per node |
| Rapid packets | 100 packets/sec per node |
| 10% packet loss | Simulated via `tc` or Faraday cage |
| Duplicate packets | Replay captured packets |
| Reordered packets | Out-of-order delivery |
| WiFi disconnect/reconnect | Pull plug, restore |
| ESP reboot | Watchdog, manual reset |
| Remote disappearance | Power off node → stale → reclaim |
| Remote return | Power on node → rediscovery |
| HTTP flood | 100 concurrent requests |
| OTA interruption | Power loss during upload |
| Storage corruption | Bit-flip in EEPROM → recovery |

---

## 4. Test Infrastructure

### 4.1 CI Pipeline (Target)

```yaml
# .github/workflows/ci.yml (conceptual)
jobs:
  build:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        env: [esp8266_generic, esp32_devkit, esp32c3_devkit]
    steps:
      - pio run -e ${{ matrix.env }}
      
  host-tests:
    runs-on: ubuntu-latest
    steps:
      - python tests/host_sanity.py
      - ./run_unit_tests    # Future
      - ./run_protocol_tests # Future

  hardware:
    runs-on: self-hosted    # Requires physical devices
    steps:
      - flash_and_test.sh   # Future
```

### 4.2 Test Utilities Needed

| Utility | Purpose |
|---------|---------|
| `test_fixture.cpp` | Mock `sensors::calibrations`, `net::reports`, etc. |
| `mock_time.cpp` | Controllable `millis()`, `time()` for deterministic tests |
| `mock_serial.cpp` | Capture `Serial.printf` for assertions |
| `packet_builder.py` | Generate valid/invalid test packets |
| `net_simulator.py` | Simulate multi-node peer-group tests |

---

## 5. Metrics & Gates

### 5.1 Mandatory Gates (Every PR)

| Gate | Threshold |
|------|-----------|
| Build | 3/3 environments SUCCESS |
| Host tests | 45/45 PASS |
| Unit tests | 100% pass (when implemented) |
| Protocol tests | 100% pass (when implemented) |
| Static analysis | No new warnings (current: 4 accepted) |

### 5.2 Release Gates (Before Tag)

| Gate | Threshold |
|------|-----------|
| Hardware validation | ESP8266 + ESP32 + ESP32-C3 |
| Hardware stress | 24h soak, 2-node peer group |
| Factory reset | Verified on all 3 platforms |
| OTA | Successful upload + boot on all 3 |
| Memory | No leaks in 24h soak (heap stable) |
| Security | Auth enabled, rate limit, validation |
| Docs | Architecture, API, protocol match code |

---

## 6. Test Organization

```
tests/
├── host_sanity.py           # Current: 45 tests
├── unit/
│   ├── test_sensors.cpp
│   ├── test_web.cpp
│   ├── test_automations.cpp
│   ├── test_net.cpp
│   ├── test_storage.cpp
│   └── test_logger.cpp
├── protocol/
│   ├── test_packet_parsing.cpp
│   └── test_validation.cpp
├── statemachine/
│   ├── test_rules.cpp
│   ├── test_remote_lifecycle.cpp
│   └── test_persistence.cpp
├── integration/
│   ├── test_boot.cpp
│   ├── test_net.cpp
│   └── test_api.cpp
└── hardware/
    ├── test_esp8266.py
    ├── test_esp32.py
    └── test_esp32c3.py
```

---

## 6. Execution

### Local Development

```bash
# Host tests (fast, no hardware)
python tests/host_sanity.py

# Build all targets
pio run -e esp8266_generic -e esp32_devkit -e esp32c3_devkit

# Unit tests (future)
pio test -e native  # Requires Unity + native platform
```

### Pre-Commit Hook (Recommended)

```bash
#!/bin/bash
# .git/hooks/pre-commit
python tests/host_sanity.py || exit 1
pio run -e esp8266_generic || exit 1
```

---

## 7. Metrics Tracking

| Metric | Current | Target |
|--------|---------|--------|
| Host test count | 45 | ≥ 100 |
| Unit test coverage | 0% | ≥ 80% |
| Protocol test coverage | 0% | 100% of packet types |
| State machine coverage | 0% | All rule types + lifecycle |
| Hardware test matrix | 3 platforms | 3 platforms + 2-node peer group |
| Stress test duration | 0h | 24h soak |
| CI time | ~2 min | < 10 min |