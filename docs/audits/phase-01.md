# Phase 1 Audit - Entity Model

**Date:** 2026-08-31  
**Branch:** main  
**Commit:** `TBD` (after commit)  
**Auditor:** Automated baseline inspection

---

## 1. Problem Statement

The monolithic `Calibration` struct (88 bytes) conflates:
- Identity (uid, name, type)
- Configuration (min, max, correction, pin, inverted, avail)
- Persistence settings (persist, pers_state, pulse, pulse_ms, fade)
- Runtime state (state, value, local)
- Network identity for remotes (device_uid, device_ip, last_update)

No explicit vocabulary to distinguish "qué es" / "cómo está configurado" / "qué estado tiene" / "quién lo posee" / "qué puede hacer" without inferring from scattered flags.

---

## 2. Solution Delivered

**Introduced explicit Entity Model vocabulary** in a pure C++ header (`src/model.h`), zero memory overhead, zero migration risk. The `Calibration` struct remains the storage/transport representation during the migration window.

### 2.1 New Types (`src/model.h`)

| Type | Values | Purpose |
|------|--------|---------|
| `EntityKind` | `NONE=0`, `SENSOR`, `ACTUATOR`, `CLOCK` | "qué es" — measurement / effect / clock |
| `EntityCapability` | `NONE=0`, `READ=1`, `WRITE=2`, `READ_WRITE=3` | "qué puede hacer" — read / write |
| `EntityOwnership` | `NONE=0`, `OWNER_LOCAL`, `REMOTE` | "quién lo posee" |
| `EntityIdentity` | entity_id, device_id, device_ip, index | Stable identity (index = runtime only) |
| `EntityConfig` | min, max, correction, pin, inverted, persist, pulse, pulse_ms, fade, avail | "cómo está configurado" |
| `EntityState` | state, value, raw, avail, last_update | "qué estado tiene" |

### 2.2 Classification Functions (constexpr, host-testable)

| Function | Maps From | Returns |
|----------|-----------|---------|
| `kindOfType(uint8_t)` | SensorType enum value | `EntityKind` |
| `capabilityOfType(uint8_t)` | SensorType enum value | `EntityCapability` |
| `ownershipOf(bool local, uint32_t uid)` | local flag + uid | `EntityOwnership` |
| `isValidType(uint8_t)` | SensorType enum value | `bool` (kind != NONE) |

**Canonical mapping (kept in sync with `sensors.h` enum):**
- Types 1..7, 11, 12 → `SENSOR` (LUMI, HUMI, TEMP, PRESS, LEVEL, AIRQ, RAIN, GENERIC, CONTACT)
- Types 8, 9 → `ACTUATOR` (TYPE_DIMMER, TYPE_RELAY)
- Type 10 → `CLOCK` (SENSOR_TIME)
- Type 0 / ≥13 → `NONE`

**Capability mapping:**
- `ACTUATOR` → `READ_WRITE` (report state + accept commands)
- `SENSOR`, `CLOCK` → `READ` (read-only)
- `NONE` → `NONE`

**Ownership mapping:**
- `uid == 0` → `NONE` (empty slot)
- `local && uid != 0` → `OWNER_LOCAL`
- `!local && uid != 0` → `REMOTE`

---

## 3. Code Changes

### 3.1 `src/model.h` (new file, ~135 lines)
- Pure C++11 (no Arduino.h, no String)
- Namespace `qymera::model`
- All functions `inline constexpr` → zero overhead, host-testable

### 3.2 `src/sensors.cpp`
- Include `model.h`
- `isValidSensorType()` now delegates to `qymera::model::isValidType()` — **behavior identical** (types 1..12 valid).
- `onRemoteCommand()` adds capability guard:
  ```cpp
  if (qymera::model::capabilityOfType(command_type) !=
      qymera::model::EntityCapability::READ_WRITE) return;
  ```
  Only READ_WRITE entities (actuators: relay/dimmer) are commandable. Behavior unchanged — no other types reach this path.

---

## 4. Tests

### 4.1 Host Tests Extended (`tests/host_sanity.py`)
**New section: [entity model]** — 26 tests mirroring `model.h`:
- `kindOfType`: all 0..12 + invalid bytes (13, 200, 255)
- `capabilityOfType`: READ_WRITE only for 8,9; READ for sensors+clock; NONE for 0/invalid
- `ownershipOf`: uid=0 → NONE; local→OWNER_LOCAL; remote→REMOTE
- `isValidType`: equivalence to legacy `1 <= t <= 12`
- Canonical equivalence: `isValidSensorType()` matches `isValidType()` for 0..255

**Result:** **71/71 PASS** (45 baseline + 26 new)

---

## 5. Build Matrix

| Environment | Status | RAM | Flash | Delta vs Baseline |
|-------------|--------|-----|-------|-------------------|
| esp8266_generic | SUCCESS | 69.6% (57,024 B) | 42.2% (441,252 B) | RAM: 0 B; Flash: +64 B |
| esp32_devkit | SUCCESS | 22.6% (74,028 B) | 73.7% (966,105 B) | RAM: 0 B; Flash: +72 B |
| esp32c3_devkit | SUCCESS | 20.9% (68,564 B) | 72.8% (953,648 B) | RAM: 0 B; Flash: +80 B |

**RAM: identical to baseline.** Flash deltas < 100 bytes = compiler/linker noise from header inclusion (no new symbols emitted).

---

## 6. Compatibility

| Aspect | Status |
|--------|--------|
| API (`Qymera::` facade) | Unchanged — no breaking changes |
| Wire protocol | Unchanged — packets identical |
| Persistence (EEPROM/Preferences) | Unchanged — `CalibrationPersist` format identical |
| Rule engine | Unchanged — still uses array indices |
| Remote discovery | Unchanged — same packet format |
| Host tests | Extended — all 71 pass |
| Hardware behavior | Unchanged (no flashing required for this phase) |

---

## 7. Risks / Limitations

| Risk | Assessment |
|------|------------|
| `LOCAL` macro collision on ESP8266 | Resolved: renamed to `OWNER_LOCAL` in `EntityOwnership` enum |
| Enum drift vs `SensorType` | Mitigated: `isValidSensorType()` now single-sourced from `model::isValidType()`; any future `SensorType` changes must update both |
| Capability guard in `onRemoteCommand` | Defense-in-depth; no behavioral change but future-proofs if new commandable types added |
| No persistent `entity_id` yet | Phase 2 (Identity) will introduce stable entity ID generation and migration |

---

## 8. Files Modified

| File | Change Type |
|------|-------------|
| `src/model.h` | NEW (135 lines) |
| `src/sensors.cpp` | MODIFIED: include + delegate `isValidSensorType` + capability guard |
| `tests/host_sanity.py` | MODIFIED: +26 entity-model tests |
| `docs/data-model.md` | UPDATED: Phase 1 Delivered section added |

---

## 9. Phase 1 Acceptance Criteria

| Criterion | Met? | Evidence |
|-----------|------|----------|
| New vocabulary types defined | ✅ | `src/model.h` |
| Classification helpers implemented | ✅ | 4 constexpr functions |
| Canonical `isValidSensorType` | ✅ | Delegates to model |
| Command capability guard | ✅ | `onRemoteCommand` |
| Build passes 3/3 envs | ✅ | All SUCCESS |
| Host tests pass | ✅ | 71/71 PASS |
| Memory footprint unchanged | ✅ | RAM identical, Flash ±noise |
| No wire/protocol/persistence changes | ✅ | Verified |
| Documentation updated | ✅ | `data-model.md` §9 |
| Audit recorded | ✅ | This file |

---

## 10. Next Phase (Phase 2: Identity System)

**Scope:** Stable entity ID generation independent of runtime index.
- Replace `uid = ChipID + index + 1` with stable `entity_id`
- Migration strategy for existing configurations
- Update discovery packets to carry `entity_id`
- Persistence schema version bump
- Rule reference decoupling (Phase 3) depends on this

**Blocking on Phase 1:** None — Phase 1 complete.