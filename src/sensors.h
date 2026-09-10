#pragma once
#include <Arduino.h>
#include "config.h"
#include "protocol_v2.h"

namespace sensors {

enum SensorType : uint8_t {
  SENSOR_NONE,
  SENSOR_LUMI,
  SENSOR_HUMI,
  SENSOR_TEMP,
  SENSOR_PRESS,
  SENSOR_LEVEL,
  SENSOR_AIRQ,
  SENSOR_RAIN,
  TYPE_DIMMER,
  TYPE_RELAY,
  SENSOR_TIME,
  SENSOR_GENERIC,
  SENSOR_CONTACT
};

struct Calibration {
  float min = 0;
  float max = 100;
  float correction;
  uint8_t avail;
  bool persist;
  bool pers_state;
  bool pulse;
  uint32_t pulse_ms;
  uint32_t fade;
  bool state;
  float value;
  SensorType type;
  uint8_t pin;
  bool inverted;
  String name;
  uint8_t id = 0;
  uint32_t uid = 0;            // wire-protocol identity (index-derived, legacy)
  uint32_t entity_id = 0;      // stable identity (persisted, NOT index-derived)
  bool local = true;
  char device_ip[16];
  uint32_t device_uid = 0;
  unsigned long last_update = 0;
};

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

extern Calibration calibrations[MAX_SENSORS];
extern Fade activeFades[MAX_SENSORS];
extern PulseState activePulses[MAX_SENSORS];
void init();
void applyPersistedStates();
void applyFades();
void checkPulses();
// Registers the TIME entity deterministically (before loadCalibration()) so its
// persisted correction/timezone can be restored. Safe to call before NTP sync.
void ensureTimeRegistered();
extern int findCalib(const String &key);
extern int findCalibByUid(uint32_t uid);
extern int findCalibByIndex(uint8_t index);
extern int findCalibByEntityId(uint32_t entity_id);
// Remote-sensor lifecycle: stale remotes are hidden from the active API/UI and
// their unreferenced slots are reclaimed so MAX_SENSORS cannot be exhausted.
bool isValidSensorType(uint8_t type);
bool isStaleRemote(int index);
bool isEntryVisible(int index);
void reclaimStaleSlots();
void setRelay(const String &key, bool target);
void handleDimmer(uint32_t uid, int value);
void handleToggle(uint32_t uid);
RTCTime getTime();
uint16_t getMinutesOfDay();
uint32_t getUnixTime();
bool timeValid();
TimeSource getTimeSource();
void initNTP();
void updateNTPTime();

// Time
void rtc(const RTCTime &time);
void ntp(const RTCTime &time);

// Sensores
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
Calibration *getCalib(const String &key);

// Mesh callbacks - Procesadas por sensors.cpp
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

// V2 Protocol Callbacks (Phase 3)
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
