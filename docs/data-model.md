# Qymera Data Model

**Version:** 1.1.0 (baseline)  
**Date:** 2026-08-31  
**Branch:** main

---

## 1. Current State: The Monolithic `Calibration` Struct

The single most important data structure in the codebase is `sensors::Calibration` (defined in `sensors.h:23-45`). It conflates **identity, configuration, state, persistence, runtime metadata, and network identity** into one 88-byte struct (approx):

```cpp
struct Calibration {
    // Identity
    uint8_t id = 0;              // Runtime array index (0..63)
    uint32_t uid = 0;            // ChipID + index + 1 (derivable)
    String name;                 // User-facing name
    
    // Type & Capability
    SensorType type;             // TEMP, HUMI, RELAY, DIMMER, etc.
    uint8_t pin;                 // GPIO pin (actuators)
    bool inverted;               // Active-low logic
    
    // Configuration
    float min = 0;               // Calibration min
    float max = 100;             // Calibration max
    float correction;            // Offset / timezone minutes
    uint8_t avail;               // Availability counter
    
    // Persistence
    bool persist;                // Enable state persistence
    bool pers_state;             // Last persisted state (relay)
    bool pulse;                  // Pulse mode enabled
    uint32_t pulse_ms;           // Pulse duration
    uint32_t fade;               # Fade duration (ms)
    
    // Runtime State
    bool state;                  // Current ON/OFF
    float value;                 // Current sensor value
    bool local = true;           // Local vs remote entity
    
    // Network Identity (Remote)
    char device_ip[16];          // Owner IP (remote)
    uint32_t device_uid = 0;     // Owner chip ID (remote)
    unsigned long last_update = 0; // Staleness tracking (remote)
    
    // Availability
    uint8_t avail;               // Discovery availability counter
};
```

---

## 2. Problems with Current Model

| Concern | Manifestation |
|---------|---------------|
| **Identity** | `uid = ChipID + index + 1` — derived from runtime array index; not stable across reorderings |
| **Config vs State** | `min/max/correction` (config) mixed with `value/state` (runtime) |
| **Persistence** | `persist/pers_state/pulse/pulse_ms/fade` — what to persist scattered |
| **Remote Identity** | `device_uid/device_ip/last_update` — only for remote entities |
| **Availability** | `avail` used for discovery; `local` flag for ownership |
| **Index coupling** | Rules reference entities by **array index** (`sensor_idxs`, `actuator_idxs`) — fragile |

---

## 3. Target Model: Explicit Separation

### 3.1 Entity Identity

```cpp
struct EntityIdentity {
    uint32_t entity_id;          // Stable UUID (not index-derived)
    uint32_t device_id;          // Owner device chip ID
    String name;                 // User-facing name
    bool local;                  // true = owned by this device
};
```

### 3.2 Entity Configuration (Persisted)

```cpp
struct EntityConfig {
    uint32_t entity_id;
    SensorType type;
    uint8_t pin;                 // Actuators only
    bool inverted;
    float min, max;              // Calibration range
    float correction;            // Offset / timezone
    uint8_t avail;               // Discovery availability
    // Persistence settings
    bool persist;                // Enable state persistence
    bool pulse;
    uint32_t pulse_ms;
    uint32_t fade;
};
```

### 3.3 Entity Runtime State (Volatile)

```cpp
struct EntityState {
    uint32_t entity_id;
    bool state;                  // ON/OFF
    float value;                 // Current reading
    float raw_value;             // Uncalibrated raw
    unsigned long last_update;   // For staleness (remote)
};
```

### 3.4 Capabilities (Type-Specific)

```cpp
struct RelayCapability {
    bool persist;
    bool pers_state;
    bool pulse;
    uint32_t pulse_ms;
};

struct DimmerCapability {
    uint32_t fade;
    // pulse not applicable
};

struct SensorCapability {
    float min, max;
    float correction;
    uint8_t avail;
};
```

### 3.5 Network Identity (Remote Entities Only)

```cpp
struct NetworkIdentity {
    uint32_t device_uid;
    char device_ip[16];
    unsigned long last_update;   // For staleness
    // Mirrored capabilities from owner
    uint32_t fade;
    bool persist, pers_state, pulse;
    uint32_t pulse_ms;
};
```

### 3.6 Ownership

```cpp
struct Ownership {
    uint32_t device_id;          // Owner chip ID
    bool local;                  // true = this device owns it
    // For remote: device_uid + device_ip + last_update
};
```

---

## 4. Migration Strategy

### Phase 1: Introduce New Types (Non-Breaking)

1. Add new structs alongside `Calibration` in `sensors.h`
2. Keep `Calibration` as the runtime aggregate
3. Add helper functions to decompose `Calibration` → new types
4. Update internal logic to use new types where possible

### Phase 2: Refactor Internal Logic

1. Replace direct `Calibration` field access with typed accessors
2. Update `storage.cpp` to persist new separated structures
3. Add migration logic: `CalibrationPersist` → new persisted format

### Phase 3: Rule Reference Decoupling

1. Rules currently store `sensor_idxs` / `actuator_idxs` (array indices)
2. Introduce `entity_id` references in rules
3. Add indirection: `entity_id` → runtime index lookup

---

## 5. Persistence Schema Evolution

### Current: `CalibrationPersist` (34 bytes, packed)

```cpp
struct CalibrationPersist {
    uint32_t magic;       // 0x514D434C ("QMCL")
    uint16_t version;     // 1
    uint32_t uid;         // Device UID (for identity matching)
    bool pers_state;
    float min, max, correction;
    uint8_t avail;
    bool persist, pulse;
    uint32_t pulse_ms, fade;
};
```

### Target: Split Persistence

```cpp
// EntityConfigPersist (configuration + persistence settings)
struct EntityConfigPersist {
    uint32_t magic;       // 0x45435043 ("ECPC")
    uint16_t version;
    uint32_t entity_id;   // Stable identity
    SensorType type;
    uint8_t pin;
    bool inverted;
    float min, max, correction;
    uint8_t avail;
    bool persist, pulse;
    uint32_t pulse_ms, fade;
};

// EntityStatePersist (only for entities with persist=true)
struct EntityStatePersist {
    uint32_t magic;       // 0x45535053 ("ESPS")
    uint16_t version;
    uint32_t entity_id;
    bool pers_state;      // Last relay state
};
```

### Migration

```cpp
// On load: if old format detected, migrate
if (old_magic == CALIB_MAGIC) {
    // Extract entity_id from uid (or generate new if uid==0)
    // Split into EntityConfigPersist + EntityStatePersist
    // Write new format, mark old slot cleared
}
```

---

## 6. Rule Reference Model

### Current: Array Indices (Fragile)

```cpp
struct Rule {
    uint8_t sensor_idxs[5];    // Array indices into calibrations[]
    uint8_t actuator_idxs[5];  // Array indices into calibrations[]
    // ...
};
```

**Problem**: Reclaiming a stale remote slot shifts indices → rules point to wrong entity.

### Target: Stable Entity IDs

```cpp
struct Rule {
    uint32_t sensor_ids[5];     // Stable entity_id
    uint32_t actuator_ids[5];   // Stable entity_id
    // Runtime resolution: entity_id → index via lookup table
    // Reclamation safe: lookup fails → rule condition = false
};
```

### Resolution Layer

```cpp
namespace automations {
    // Runtime mapping: entity_id → calibration index
    static int resolveEntity(uint32_t entity_id) {
        for (int i = 0; i < MAX_SENSORS; i++) {
            if (sensors::calibrations[i].entity_id == entity_id) return i;
        }
        return -1; // Not found (stale remote, not registered)
    }
}
```

---

## 7. API Impact

### Public API (`Qymera::` facade)

Minimal changes — facade already abstracts sensors:

```cpp
// Current (works unchanged)
Qymera::temperature("TEMP", 23.5);
Qymera::relay("RELAY0", 5, false);
Qymera::setRelay("RELAY0", true);

// Future: entity_id-based variants (optional)
Qymera::setRelayById(12345, true);  // Optional convenience
```

### Internal API (`sensors::`)

Gradual migration — new typed functions alongside existing:

```cpp
// New
void setRelayById(uint32_t entity_id, bool target);
EntityConfig getConfig(uint32_t entity_id);
EntityState getState(uint32_t entity_id);

// Legacy (kept for compatibility during migration)
void setRelay(const String &key, bool target);
```

---

## 7. Validation Checklist for Phase 1 Completion

- [ ] New structs defined in `sensors.h`
- [ ] `Calibration` decomposition helpers implemented
- [ ] `storage.cpp` persists new split format (with migration)
- [ ] Rule engine uses `entity_id` for references (or documented as next step)
- [ ] Remote lifecycle uses explicit `NetworkIdentity`
- [ ] Ownership explicit in all code paths
- [ ] Build passes all 3 environments
- [ ] Host tests pass
- [ ] No runtime regressions on hardware

---

## 8. Open Questions

1. **String vs Fixed-Char for names**: `String` (heap) vs `char[32]` (stack/EEPROM)
2. **Entity ID generation**: Random UUID vs `ChipID + counter` vs hash of `(device_id, name)`
3. **Rule migration**: How to handle existing EEPROM rules referencing old indices?
4. **Remote entity ID stability**: Owner's `entity_id` must be known to remotes → include in discovery packets

---

## 9. Phase 1 Delivered (2026-08-31)

**Scope**: Per professionalization roadmap Phase 1 — introduce explicit Entity Model vocabulary *without* a destructive rewrite. The monolithic `Calibration` struct remains the storage/transport representation; a new pure-C++ classification layer (`src/model.h`) provides the conceptual vocabulary.

### Delivered

| Artifact | Purpose |
|----------|---------|
| `src/model.h` | Pure C++ header (no Arduino deps) declaring: `EntityKind` (SENSOR/ACTUATOR/CLOCK/NONE), `EntityCapability` (READ/WRITE/READ_WRITE/NONE), `EntityOwnership` (OWNER_LOCAL/REMOTE/NONE), `EntityIdentity`, `EntityConfig`, `EntityState` structs (documented PODs for future phases), and constexpr classification functions: `kindOfType()`, `capabilityOfType()`, `ownershipOf()`, `isValidType()`. |
| `sensors.cpp` | `isValidSensorType()` now delegates to `qymera::model::isValidType()` (behavior identical: types 1..12 valid). Remote command handler guards with `capabilityOfType(command_type) == READ_WRITE` — documents that only actuators are commandable; behavior unchanged. |
| `tests/host_sanity.py` | +26 tests mirroring the model.h classification table: kindOfType for all 0..12 + invalid, capabilityOfType, ownershipOf, isValidType equivalence to legacy range. Host tests: **71/71 PASS** (45 baseline + 26 new). |
| Build matrix | All 3 environments PASS. Memory footprint **identical to baseline**: ESP8266 69.6% RAM / 42.2% Flash; ESP32 22.6% / 73.7%; ESP32-C3 20.9% / 72.8%. Zero RAM overhead (header-only constexpr). |

### Rationale

- **No new arrays/state** → zero memory cost, no migration risk.
- **Pure C++** → host-testable in Python mirror; usable by any module without pulling Arduino.h.
- **Canonical `isValidSensorType`** now single-sourced from model.h — eliminates drift.
- **Capability guard** on commands makes intent explicit (actuators only) while preserving exact behavior.

### Next (Phase 2: Identity System)

- Stable entity ID generation (`entity_id` independent of runtime slot index).
- Migration from `uid = ChipID + index + 1` to stable `entity_id`.
- Update discovery packets to carry stable `entity_id`.
- Persistence schema version bump to carry `entity_id`.

---

## 10. Phase 2 Delivered (2026-08-31)

**Scope**: Per professionalization roadmap Phase 2 — introduce stable `entity_id` decoupled from runtime array index, with persistence and migration. The legacy `uid` (wire-protocol identity) is preserved for backward compatibility on the wire.

### Delivered

| Artifact | Purpose |
|----------|---------|
| `sensors.h` | Added `uint32_t entity_id = 0;` to `Calibration` struct (stable identity, NOT index-derived). |
| `sensors.cpp` | `nextEntityId()` xorshift32 PRNG (seeded from chip_id + millis), `bindLocalSensor()` assigns `entity_id` once on first registration, `findCalibByEntityId()` lookup helper. |
| `config.h` | New EEPROM region `EEPROM_ENTITY_ID_START` / `EEPROM_ENTITY_ID_SIZE` (40 slots × 4 bytes = 160 bytes) for slot_index → entity_id map. **No existing EEPROM layout changed**. |
| `storage.cpp` | `CALIB_VERSION` bumped to 2. `loadEntityId()` / `saveEntityId()` helpers. `loadCalibration()` restores `entity_id` from map. `saveCalibrationSlot()` persists `entity_id` to map. Migration: v1 slots (no entity_id map entry) get new `entity_id` on next registration; v2 slots retain stable ID. |
| `tests/host_sanity.py` | +15 tests: xorshift32 deterministic sequence, entity_id properties (non-zero, chip-specific, unique), `findCalibByEntityId` logic, `bindLocalSensor` idempotency. Host tests: **86/86 PASS** (71 baseline + 15 new). |
| Build matrix | All 3 environments PASS. Memory: +~256 bytes RAM (64 slots × 4 bytes entity_id). ESP8266 70.0% / 42.3%; ESP32 22.7% / 73.8%; ESP32-C3 21.0% / 72.8%. |

### Rationale

- **Zero layout disruption**: Entity ID map uses previously unused EEPROM space after OTA flag. Existing devices boot without factory reset — v1 slots migrate lazily on next registration.
- **Stable identity**: `entity_id` generated once on first registration, persisted, survives slot reclamation/reordering. `uid` remains for wire protocol (Phase 3 Protocol V2 will carry `entity_id` on the wire).
- **Device-scoped uniqueness**: Upper 16 bits = chip_id upper 16 bits; lower 16 bits = xorshift32 sequence. Collisions across devices extremely unlikely.
- **Idempotent assignment**: `bindLocalSensor()` only sets `entity_id` if zero — survives reboots and re-registration of same slot.

### Migration Strategy (Legacy UID → Entity ID)

| Scenario | Behavior |
|----------|----------|
| Existing device (v1 persist, no entity_id map) | On boot: `loadCalibration()` finds no map entry → `entity_id` stays 0. On next `bindLocalSensor()` call (or new registration): `entity_id` generated, persisted to map. |
| New registration | `entity_id` generated immediately, persisted to map. |
| Slot reclaimed & re-registered | New `entity_id` generated (correct: it's a different logical entity). |
| Remote entities | `entity_id` not applicable (owned by another device); `device_uid` + `uid` used for wire identity. Phase 3 will propagate owner's `entity_id` in discovery. |

### Open Questions (Updated)

1. **String vs Fixed-Char for names**: `String` (heap) vs `char[32]` (stack/EEPROM)
2. **Rule migration**: How to handle existing EEPROM rules referencing old indices? (Phase 3+)
3. **Remote entity ID stability**: Owner's `entity_id` must be known to remotes → include in discovery packets (Phase 3 Protocol V2)
4. **Entity ID collision handling**: Theoretical; xorshift32 + chip_id bits make it negligible. Document only.

---

## 11. Validation Checklist for Phase 2 Completion

- [x] `entity_id` field added to `Calibration`
- [x] Stable generation (`nextEntityId` xorshift32)
- [x] Assignment on first registration (`bindLocalSensor`)
- [x] Lookup helper (`findCalibByEntityId`)
- [x] Persistence (separate EEPROM map, no layout shift)
- [x] Migration v1→v2 (lazy on next registration)
- [x] Build passes 3/3 environments
- [x] Host tests pass (86/86)
- [x] No wire protocol changes (uid preserved)
- [x] No factory reset required for existing devices