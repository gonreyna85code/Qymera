#pragma once
#include <stdint.h>

// ================================================================
// QYMERA DOMAIN MODEL (v1)
//
// The one canonical entity model. Every layer — registry, protocol,
// storage, web API, UI — reasons about entities through these types.
// This header is deliberately pure C++ (no Arduino.h, no String, no
// EEPROM) so it is host-testable and free of platform coupling.
//
//   EntityIdentity -> who it is          (stable entity_id + owner device_id)
//   EntityConfig   -> how it is configured (type/calibration/limits/IO/...)
//   EntityState    -> what it reads/writes (value, state, availability)
//   EntityRuntime  -> non-persistent runtime (ownership, lifecycle, address)
//
// Invariant: `entity_id` is the ONLY logical entity identity. A runtime
// slot/index is an implementation detail and is never used as identity.
// ================================================================

namespace qymera {
namespace model {

// ----------------------------------------------------------------
// Classification
// ----------------------------------------------------------------

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
  READ_WRITE = 3    // both: actuators also report their state
};

enum class EntityOwnership : uint8_t {
  NONE = 0,            // empty slot
  OWNER_LOCAL = 1,     // owned by this device
  OWNER_REMOTE = 2     // owned by another device (network)
};

// ----------------------------------------------------------------
// Entity types
// ----------------------------------------------------------------

// The single numeric type space. Kept as a global-scope enum so sketch
// code and legacy constants (SENSOR_LUMI, TYPE_RELAY, ...) keep working
// unchanged. This is the one source of truth; the classification helpers
// below map these values onto kinds/capabilities.
enum SensorType : uint8_t {
  SENSOR_NONE = 0,
  SENSOR_LUMI,    // 1
  SENSOR_HUMI,    // 2
  SENSOR_TEMP,    // 3
  SENSOR_PRESS,   // 4
  SENSOR_LEVEL,   // 5
  SENSOR_AIRQ,    // 6
  SENSOR_RAIN,    // 7
  TYPE_DIMMER,    // 8
  TYPE_RELAY,     // 9
  SENSOR_TIME,    // 10
  SENSOR_GENERIC, // 11
  SENSOR_CONTACT  // 12
};

constexpr uint8_t ENTITY_NAME_LEN = 24;
constexpr uint8_t ENTITY_IP_LEN = 16;
constexpr uint32_t ENTITY_ID_NONE = 0;

// Logical identity. Independent from any runtime slot.
struct EntityIdentity {
  uint32_t entity_id = ENTITY_ID_NONE;  // the stable logical entity identity
  uint32_t device_id = 0;               // owning device chip id
};

// Persistent configuration.
struct EntityConfig {
  uint8_t type = 0;                      // SensorType numeric
  char name[ENTITY_NAME_LEN] = {0};      // display / lookup name
  float min = 0.0f;                      // calibration min (scaling)
  float max = 100.0f;                    // calibration max (scaling)
  float correction = 0.0f;               // calibration offset (also TIME timezone minutes)
  uint8_t pin = 0;                       // GPIO pin
  bool inverted = false;                 // active-low IO
  bool persist = false;                  // relay state survives reboot
  bool pers_state = false;               // last persisted relay state
  bool pulse = false;                    // relay pulse mode
  uint32_t pulse_ms = 0;                 // pulse duration
  uint32_t fade = 0;                     // dimmer fade duration (ms)
};

// Runtime state.
struct EntityState {
  bool state = false;
  float value = 0.0f;
  float raw = 0.0f;                      // last raw (uncalibrated) sample
  uint8_t avail = 0;                     // availability counter
  uint32_t last_update = 0;              // millis() of last update (wrap-safe)
};

// Non-persistent runtime metadata.
struct EntityRuntime {
  bool local = true;                     // owned by this device
  bool online = false;                   // network lifecycle flag
  char device_ip[ENTITY_IP_LEN] = {0};   // last known IPv4 of the owner
  uint32_t last_seen = 0;                // ms of last network sighting
};

// Canonical entity aggregate.
struct Entity {
  EntityIdentity identity;
  EntityConfig config;
  EntityState state;
  EntityRuntime runtime;
};

// ----------------------------------------------------------------
// Classification helpers (pure, host-testable)
// ----------------------------------------------------------------

inline EntityKind kindOfType(uint8_t type) {
  switch (type) {
    case TYPE_DIMMER:
    case TYPE_RELAY:
      return EntityKind::ACTUATOR;
    case SENSOR_TIME:
      return EntityKind::CLOCK;
    case SENSOR_LUMI: case SENSOR_HUMI: case SENSOR_TEMP:
    case SENSOR_PRESS: case SENSOR_LEVEL: case SENSOR_AIRQ:
    case SENSOR_RAIN: case SENSOR_GENERIC: case SENSOR_CONTACT:
      return EntityKind::SENSOR;
    default:
      return EntityKind::NONE;
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

inline EntityOwnership ownershipOf(bool local) {
  return local ? EntityOwnership::OWNER_LOCAL : EntityOwnership::OWNER_REMOTE;
}

// A type is "valid" only if it maps to a real entity kind.
inline bool isValidType(uint8_t type) {
  return kindOfType(type) != EntityKind::NONE;
}

}  // namespace model
}  // namespace qymera