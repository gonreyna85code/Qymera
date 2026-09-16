#pragma once
#include <Arduino.h>
#include "config.h"
#include "model.h"
#include "entities.h"
#include "protocol_v2.h"

// ================================================================
// SENSOR / ACTUATOR FACADE (v1)
//
// Thin hardware-facing layer on top of the entity registry. Entities
// (state + config) live in `entities`; this module only owns the
// hardware I/O (GPIO, PWM, fades, pulses, clock) and the sensor-type
// classification helpers. No entity state is stored here.
// ================================================================

namespace sensors {

using namespace qymera::model;

struct Fade {
  uint8_t pin;
  int startVal;
  int endVal;
  unsigned long startTime;
  unsigned long duration;
  bool active;
};

struct PulseState {
  uint8_t pin;
  bool inverted;
  unsigned long start_ms;
  uint32_t pulse_ms;
  bool active;
};

struct RTCTime {
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
};

enum TimeSource : uint8_t {
  TIME_NONE,
  TIME_NTP,
  TIME_RTC
};

// Per-slot runtime helper state (index-aligned with the registry).
extern Fade activeFades[MAX_SENSORS];
extern PulseState activePulses[MAX_SENSORS];

void init();
void applyPersistedStates();
void applyFades();
void checkPulses();
// Registers the TIME entity deterministically (before loadCalibration()) so its
// persisted timezone can be restored. Safe to call before NTP sync.
void ensureTimeRegistered();

// Local actuator control. `entity_id` is the canonical identity.
void setRelay(const String &key, bool target);
void handleDimmer(uint32_t entity_id, int value);
void handleToggle(uint32_t entity_id);
void handleToggle(const String &key);

// Time
RTCTime getTime();
uint16_t getMinutesOfDay();
uint32_t getUnixTime();
bool timeValid();
TimeSource getTimeSource();
void initNTP();
void updateNTPTime();
void rtc(const RTCTime &time);
void ntp(const RTCTime &time);

// Sensores (auto-register when first reported)
void temperature(const String &key, float raw);
void humidity(const String &key, int raw);
void luminosity(const String &key, int raw);
void level(const String &key, int raw);
void pressure(const String &key, float raw);
void airQ(const String &key, const int &v);
void rain(const String &key, bool v);
void custom(const String &key, float raw);
void contact(const String &key, bool v);
void relay(const String &key, uint8_t pin, bool inverted = false);
void dimmer(const String &key, uint8_t pin, bool inverted = false);

// Fades
void startFade(const String &key, uint8_t pin, int from, int to, unsigned long dur);
// Calibración
float calibrate(const String &key, float raw);
Entity *getCalib(const String &key);

// Net callbacks - procesadas por sensors.cpp (operan sobre el registry)
void onRemoteSensorDiscovered(
  uint32_t remote_uid,
  const char *remote_ip,
  uint32_t sensor_id,
  const String &sensor_name,
  uint8_t sensor_type,
  bool sensor_state,
  uint32_t sensor_value,
  float sensor_min,
  float sensor_max,
  float sensor_correction,
  uint8_t sensor_avail,
  uint32_t sensor_fade,
  bool sensor_persist,
  bool sensor_pers_state,
  bool sensor_pulse,
  uint32_t sensor_pulse_ms);

void onRemoteCommand(
  uint8_t command_type,
  uint32_t sensor_id,
  uint32_t value,
  bool state);

// V2 Protocol Callbacks
void onV2EntityAnnounce(
  uint32_t remote_uid,
  const char *remote_ip,
  const qymera::protocol::v2::EntityAnnouncePayload &payload);

void onV2StateUpdate(
  uint32_t remote_uid,
  const qymera::protocol::v2::StateUpdatePayload &payload);

uint8_t onV2Command(
  uint32_t remote_uid,
  const char *remote_ip,
  uint32_t msg_id,
  const qymera::protocol::v2::CommandPayload &payload);

void onV2CommandAck(
  uint32_t remote_uid,
  const qymera::protocol::v2::CommandAckPayload &payload);

void onV2CommandError(
  uint32_t remote_uid,
  const qymera::protocol::v2::CommandErrorPayload &payload);

}  // namespace sensors