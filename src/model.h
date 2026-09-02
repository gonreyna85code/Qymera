#pragma once
#include <stdint.h>

// ================================================================
// QYMERA ENTITY MODEL - Phase 1 vocabulary
//
// Explicit conceptual decomposition of a runtime entity. Today the
// storage/transport representation is the monolithic Calibration struct
// (see src/sensors.h). This header declares the vocabulary the runtime
// uses to *classify* and *reason about* entities without inferring
// semantics from scattered flags:
//
//   EntityKind        -> "qu� es"            (sensor / actuator / clock)
//   EntityCapability  -> "qu� puede hacer"   (read / write)
//   EntityOwnership   -> "qui�n lo posee"    (local / remote)
//   EntityIdentity    -> "identidad estable" (entity id / device id)
//   EntityConfig      -> "c�mo est� configurado"
//   EntityState       -> "qu� estado tiene"
//
// Deliberately pure C++ (no Arduino.h, no String, no EEPROM) so it can be
// unit-tested on the host and reused by any module. No state is stored
// here: it is a classification/description layer, not a new storage array.
// During the migration window Calibration remains the persisted/transport
// representation; later phases (2+: Identity, persistence) will move the
// fields onto these types.
// ================================================================

namespace qymera {
namespace model {

enum class EntityKind : uint8_t {
  NONE = 0,     // empty slot / unknown / invalid type
  SENSOR,       // measurement entity (temp, humi, lumi, ...)
  ACTUATOR,     // effect entity (relay, dimmer)
  CLOCK         // SENSOR_TIME (system clock exposed as entity)
};

enum class EntityCapability : uint8_t {
  NONE = 0,
  READ = 1,         // state readable (discovery / API)
  WRITE = 2,        // state settable
  READ_WRITE = 3    // both (actuators also report their state)
};

enum class EntityOwnership : uint8_t {
  NONE = 0,   // empty slot
  OWNER_LOCAL,      // owned by this device
  REMOTE      // owned by another device (mesh)
};

// Stable identity, independent of the runtime slot index. The internal
// index is ONLY a runtime reference; it must never be used as the logical
// identity (Phase 2 enforces this on the wire/persistence).
struct EntityIdentity {
  uint32_t entity_id = 0;  // stable entity uid (protocol / persistence)
  uint32_t device_id = 0;  // owning device chip id (GET_CHIP_ID())
  char device_ip[16] = {}; // last known IPv4 of the owning device
  uint8_t index = 0;       // runtime slot ONLY - not identity
};

// Configuration that changes how the entity behaves and how raw samples are
// interpreted. Conceptually separated from runtime state.
struct EntityConfig {
  float min = 0.0f;        // calibration min (scaling)
  float max = 100.0f;      // calibration max (scaling)
  float correction = 0.0f; // calibration offset (also TIME timezone minutes)
  bool persist = false;    // relay state survives reboot
  bool pers_state = false; // last persisted relay state
  bool pulse = false;      // relay pulse mode
  uint32_t pulse_ms = 0;   // pulse duration
  uint32_t fade = 0;       // dimmer fade duration (ms)
  bool inverted = false;   // active-low IO
  uint8_t pin = 0;         // GPIO pin
};

// Runtime state, conceptually distinct from configuration.
struct EntityState {
  bool state = false;          // boolean state (relay on/off, contact)
  float value = 0.0f;          // normalized/calibrated value
  float raw = 0.0f;            // last raw (uncalibrated) sample when available
  uint8_t avail = 0;           // availability counter
  unsigned long last_update = 0;  // millis() of last update (wrap-safe)
};

// ----------------------------------------------------------------
// Classification helpers (pure, host-testable). They mirror the SensorType
// enum VALUES without depending on sensors.h, keeping this header usable
// anywhere. Keep in sync with enum SensorType in src/sensors.h:
//
//   0 SENSOR_NONE, 1 LUMI, 2 HUMI, 3 TEMP, 4 PRESS, 5 LEVEL, 6 AIRQ,
//   7 RAIN, 8 TYPE_DIMMER, 9 TYPE_RELAY, 10 SENSOR_TIME, 11 GENERIC,
//   12 CONTACT
// ----------------------------------------------------------------

inline EntityKind kindOfType(uint8_t type) {
  switch (type) {
    case 8:
    case 9:
      return EntityKind::ACTUATOR;  // TYPE_DIMMER, TYPE_RELAY
    case 10:
      return EntityKind::CLOCK;     // SENSOR_TIME
    case 1: case 2: case 3:
    case 4: case 5: case 6:
    case 7: case 11: case 12:
      return EntityKind::SENSOR;    // measurement entities
    default:
      return EntityKind::NONE;      // empty / unknown / invalid
  }
}

inline EntityCapability capabilityOfType(uint8_t type) {
  switch (kindOfType(type)) {
    case EntityKind::ACTUATOR:
      return EntityCapability::READ_WRITE;  // actuators report AND accept
    case EntityKind::SENSOR:
    case EntityKind::CLOCK:
      return EntityCapability::READ;        // measurement / clock are read-only
    default:
      return EntityCapability::NONE;
  }
}

inline EntityOwnership ownershipOf(bool local, uint32_t uid) {
  if (uid == 0) return EntityOwnership::NONE;  // empty slot
  return local ? EntityOwnership::OWNER_LOCAL : EntityOwnership::REMOTE;
}

// A type is "valid" only if it maps to a real entity kind. This is the
// canonical definition of isValidSensorType() in src/sensors.cpp.
inline bool isValidType(uint8_t type) {
  return kindOfType(type) != EntityKind::NONE;
}

}  // namespace model
}  // namespace qymera