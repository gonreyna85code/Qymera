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