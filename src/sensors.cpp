#include "sensors.h"
#include <WiFiClient.h>
#include <time.h>
#include "config.h"
#include "model.h"
#include "entities.h"
#include "core.h"
#include "net.h"
#include "web.h"
#include "cmd_delivery.h"
#include "automations.h"
#include "log.h"

namespace sensors {

Fade activeFades[MAX_SENSORS];
PulseState activePulses[MAX_SENSORS];

static TimeSource time_source = TIME_NONE;

// ================= REGISTRO =================

// Local-only name lookup for the sensor read/registration functions. A name
// match against a REMOTE entity must never rebind that entity as local:
// registerLocal() would then re-announce it as a NEW local entity, feeding
// the discovery redistribution loop (remote -> stolen -> re-broadcast ->
// duplicate). Remote entities are read-only for binding purposes.
static int findLocalIdx(const String &key) {
  if (key.length() == 0) return -1;
  return entities::findLocalByName(key.c_str());
}

void init() {
  entities::init();
  for (int i = 0; i < MAX_SENSORS; i++) {
    activeFades[i] = Fade();
    activePulses[i] = PulseState();
  }
  net::setSensorDiscoveryCallback(onRemoteSensorDiscovered);
  net::setCommandCallback(onRemoteCommand);

  // V2 Protocol callbacks
  net::setV2EntityAnnounceCallback(onV2EntityAnnounce);
  net::setV2StateUpdateCallback(onV2StateUpdate);
  net::setV2CommandCallback(onV2Command);
  net::setV2CommandAckCallback(onV2CommandAck);
  net::setV2CommandErrorCallback(onV2CommandError);
}

void applyPersistedStates() {
  // Deterministic boot state, applied exactly once before any report:
  // persistent relays restore their last state; non-persistent relays are OFF.
  for (int i = 0; i < MAX_SENSORS; i++) {
    Entity &c = entities::at((uint8_t)i);
    if (c.config.type != TYPE_RELAY) continue;
    if (!c.runtime.local) continue;
    bool on = c.config.persist ? c.config.pers_state : false;
    pinMode(c.config.pin, OUTPUT);
    digitalWrite(c.config.pin, c.config.inverted ? !on : on);
    c.state.state = on;
  }
}

// ================= REMOTE LIFECYCLE (delegado al registry) =================

// ================= FADES / PULSES =================

void applyFades() {
  for (int i = 0; i < MAX_SENSORS; i++) {
    if (!activeFades[i].active) continue;
    auto &f = activeFades[i];
    const Entity &c = entities::peek((uint8_t)i);
    if (!c.runtime.local) continue;
    unsigned long elapsed = millis() - f.startTime;
    if (elapsed >= f.duration) {
      int pwm = f.endVal;
      if (c.config.inverted) pwm = PWM_MAX_OUT - pwm;
      pwmWritePin(f.pin, (uint8_t)pwm);
      f.active = false;
    } else {
      float progress = (float)elapsed / f.duration;
      int current = f.startVal + (int)((f.endVal - f.startVal) * progress);
      int pwm = current;
      if (c.config.inverted) pwm = PWM_MAX_OUT - pwm;
      pwmWritePin(f.pin, (uint8_t)pwm);
    }
  }
}

void checkPulses() {
  unsigned long now = millis();
  for (int i = 0; i < MAX_SENSORS; i++) {
    if (!activePulses[i].active) continue;
    if (now - activePulses[i].start_ms >= activePulses[i].pulse_ms) {
      digitalWrite(activePulses[i].pin,
        (false ^ activePulses[i].inverted) ? HIGH : LOW);
      activePulses[i].active = false;
      // Sincronizar estado lógico
      Entity &c = entities::at((uint8_t)i);
      c.state.state = false;
      if (c.config.persist && c.config.pers_state != c.state.state) {
        c.config.pers_state = c.state.state;
        web::saveCalibrationSlot(i);
      }
      logger::sensorsf("Relay %s -> OFF", c.config.name);
    }
  }
}

// ================= ACTUADORES =================

// Core relay driver. Handles local GPIO/pulse/persistence and remote
// delivery (V2 reliable for entity-keyed remotes, legacy otherwise).
static void setRelayByIndex(int idx, bool target) {
  Entity &c = entities::at((uint8_t)idx);

  if (!c.runtime.local) {
    // ---- Actuador REMOTO: ----
    if (c.identity.entity_id != 0) {
      net::sendReliableV2Command(
        c.identity.device_id, c.runtime.device_ip, c.identity.entity_id,
        (uint8_t)TYPE_RELAY, target ? 1u : 0u, target);
    } else {
      net::sendCommand(
        c.identity.device_id, c.runtime.device_ip, c.identity.entity_id,
        (uint8_t)TYPE_RELAY, target ? 1u : 0u, target);
    }
    return;
  }

  // ---- Actuador LOCAL: operar GPIO ----
  if (c.config.pulse) {
    if (target) {
      digitalWrite(c.config.pin, (true ^ c.config.inverted) ? HIGH : LOW);
      activePulses[idx].pin = c.config.pin;
      activePulses[idx].inverted = c.config.inverted;
      activePulses[idx].start_ms = millis();
      activePulses[idx].pulse_ms = c.config.pulse_ms;
      activePulses[idx].active = true;
      c.state.state = true;  // Relay IS ON while pulse is active
    } else {
      activePulses[idx].active = false;
      digitalWrite(c.config.pin, (false ^ c.config.inverted) ? HIGH : LOW);
      c.state.state = false;
    }
  } else {
    digitalWrite(c.config.pin, (target ^ c.config.inverted) ? HIGH : LOW);
    c.state.state = target;
  }

  // ---- Persistencia en EEPROM (solo si cambió el estado) ----
  if (c.config.persist && c.config.pers_state != c.state.state) {
    c.config.pers_state = c.state.state;
    web::saveCalibrationSlot(idx);
  }

  c.state.last_update = millis();
  logger::sensorsf("Relay %s -> %s", c.config.name, target ? "ON" : "OFF");
}

void setRelay(const String &key, bool target) {
  int idx = entities::findLocalByName(key.c_str());
  if (idx < 0) return;
  if (entities::at((uint8_t)idx).config.type != TYPE_RELAY) return;
  setRelayByIndex(idx, target);
}

// Core dimmer driver. Local PWM/fade + remote delivery.
static void dimmerByIndex(int idx, int value) {
  Entity &c = entities::at((uint8_t)idx);
  value = constrain(value, 0, 100);

  if (!c.runtime.local) {
    if (c.identity.entity_id != 0) {
      net::sendReliableV2Command(
        c.identity.device_id, c.runtime.device_ip, c.identity.entity_id,
        (uint8_t)TYPE_DIMMER, (uint32_t)value, value > 0);
    } else {
      net::sendCommand(
        c.identity.device_id, c.runtime.device_ip, c.identity.entity_id,
        (uint8_t)TYPE_DIMMER, (uint32_t)value, value > 0);
    }
    return;
  }

  int pwm_val = map(value, 0, 100, 0, PWM_MAX_OUT);
  if (c.config.inverted) pwm_val = PWM_MAX_OUT - pwm_val;
  if (c.config.fade > 0) {
    int current = pwmReadPin(c.config.pin);
    startFade(c.config.name, c.config.pin, current, pwm_val, c.config.fade);
  } else {
    pwmWritePin(c.config.pin, (uint8_t)pwm_val);
  }
  c.state.value = value;
  c.state.state = (value > 0);
  c.state.last_update = millis();
  logger::sensorsf("Dimmer %s -> %d%%", c.config.name, value);
}

void handleDimmer(const String &key, int value) {
  int idx = entities::findLocalByName(key.c_str());
  if (idx < 0) return;
  if (entities::at((uint8_t)idx).config.type != TYPE_DIMMER) return;
  dimmerByIndex(idx, value);
}

void handleDimmer(uint32_t entity_id, int value) {
  int idx = entities::findById(entity_id);
  if (idx < 0) return;
  if (entities::at((uint8_t)idx).config.type != TYPE_DIMMER) return;
  dimmerByIndex(idx, value);
}

void handleToggle(uint32_t entity_id) {
  int idx = entities::findById(entity_id);
  if (idx < 0) return;
  Entity &c = entities::at((uint8_t)idx);
  if (c.config.type == TYPE_RELAY) {
    setRelayByIndex(idx, !c.state.state);
  } else if (c.config.type == TYPE_DIMMER) {
    bool on = !c.state.state;
    int pwm_val = map((int)c.state.value, 0, 100, 0, PWM_MAX_OUT);
    if (c.config.inverted) pwm_val = PWM_MAX_OUT - pwm_val;
    if (c.config.fade > 0) {
      int current = pwmReadPin(c.config.pin);
      startFade(c.config.name, c.config.pin, current, on ? pwm_val : 0, c.config.fade);
    } else {
      pwmWritePin(c.config.pin, (uint8_t)(on ? pwm_val : 0));
    }
    c.state.state = on;
    c.state.last_update = millis();
    logger::sensorsf("Dimmer %s -> %s", c.config.name, on ? "ON" : "OFF");
  }
}

void handleToggle(const String &key) {
  int idx = entities::findLocalByName(key.c_str());
  if (idx < 0) return;
  handleToggle(entities::at((uint8_t)idx).identity.entity_id);
}

void startFade(const String &key, uint8_t pin, int from, int to, unsigned long dur) {
  int idx = entities::findLocalByName(key.c_str());
  if (idx < 0) return;
  activeFades[idx].pin = pin;
  activeFades[idx].startVal = from;
  activeFades[idx].endVal = to;
  activeFades[idx].startTime = millis();
  activeFades[idx].duration = dur;
  activeFades[idx].active = true;
}

// ================= SENSORES (AUTO-REGISTRO) =================

// Register-if-needed and return a mutable local entity reference.
static Entity &ensureLocalEntity(const String &key, uint8_t type, int *out_idx) {
  int idx = entities::findLocalByName(key.c_str());
  if (idx < 0) idx = entities::registerLocal(key.c_str(), type);
  if (out_idx) *out_idx = idx;
  static Entity dummy;
  if (idx < 0) {
    dummy = Entity();
    dummy.runtime.local = true;
    dummy.runtime.online = true;
    return dummy;  // registry full: drop (mirrors old behavior)
  }
  return entities::at((uint8_t)idx);
}

void temperature(const String &key, float raw) {
  int idx = -1;
  Entity &c = ensureLocalEntity(key, SENSOR_TEMP, &idx);
  if (idx < 0) return;
  c.state.value = calibrate(key, raw);
  c.state.raw = raw;
  c.state.last_update = millis();
}

void humidity(const String &key, int raw) {
  int idx = -1;
  Entity &c = ensureLocalEntity(key, SENSOR_HUMI, &idx);
  if (idx < 0) return;
  c.state.value = calibrate(key, raw);
  c.state.raw = (float)raw;
  c.state.last_update = millis();
}

void luminosity(const String &key, int raw) {
  int idx = -1;
  Entity &c = ensureLocalEntity(key, SENSOR_LUMI, &idx);
  if (idx < 0) return;
  c.state.value = calibrate(key, raw);
  c.state.raw = (float)raw;
  c.state.last_update = millis();
}

void level(const String &key, int raw) {
  int idx = -1;
  Entity &c = ensureLocalEntity(key, SENSOR_LEVEL, &idx);
  if (idx < 0) return;
  c.state.value = calibrate(key, raw);
  c.state.raw = (float)raw;
  c.state.last_update = millis();
}

void pressure(const String &key, float raw) {
  int idx = -1;
  Entity &c = ensureLocalEntity(key, SENSOR_PRESS, &idx);
  if (idx < 0) return;
  c.state.value = calibrate(key, raw);
  c.state.raw = raw;
  c.state.last_update = millis();
}

void airQ(const String &key, const int &v) {
  int idx = -1;
  Entity &c = ensureLocalEntity(key, SENSOR_AIRQ, &idx);
  if (idx < 0) return;
  c.state.value = (float)v;
  c.state.raw = (float)v;
  c.state.last_update = millis();
}

void rain(const String &key, bool v) {
  int idx = -1;
  Entity &c = ensureLocalEntity(key, SENSOR_RAIN, &idx);
  if (idx < 0) return;
  c.state.state = v;
  c.state.value = v ? 1.0f : 0.0f;
  c.state.raw = c.state.value;
  c.state.last_update = millis();
}

void custom(const String &key, float raw) {
  int idx = -1;
  Entity &c = ensureLocalEntity(key, SENSOR_GENERIC, &idx);
  if (idx < 0) return;
  c.state.value = calibrate(key, raw);
  c.state.raw = raw;
  c.state.last_update = millis();
}

void contact(const String &key, bool v) {
  int idx = -1;
  Entity &c = ensureLocalEntity(key, SENSOR_CONTACT, &idx);
  if (idx < 0) return;
  c.state.state = v;
  c.state.value = v ? 0.0f : 1.0f;
  c.state.raw = c.state.value;
  c.state.last_update = millis();
}

// Local actuator registration. `type` is authoritative for the entity kind;
// binding runs once and never rebinds a remote entity as local.
static bool bindLocalActuator(const String &key, uint8_t type) {
  int idx = entities::registerLocal(key.c_str(), type);
  if (idx < 0) return false;
  Entity &c = entities::at((uint8_t)idx);
  c.runtime.local = true;
  c.runtime.online = true;
  // Keep an already-restored entity_id (persistence); only assign when empty.
  if (c.identity.entity_id == ENTITY_ID_NONE) {
    c.identity.entity_id = entities::nextEntityId();
  }
  return true;
}

void relay(const String &key, uint8_t pin, bool inverted) {
  int idx = entities::findLocalByName(key.c_str());
  bool is_new = (idx < 0);
  if (is_new) {
    if (!bindLocalActuator(key, TYPE_RELAY)) return;
    idx = entities::findLocalByName(key.c_str());
    if (idx < 0) return;
  }
  Entity &c = entities::at((uint8_t)idx);
  c.config.pin = pin;
  c.config.inverted = inverted;
  if (is_new) {
    // Only configure the pin here. The initial GPIO state is applied once in
    // applyPersistedStates() (before the first report) to avoid an
    // OFF -> ON glitch on persistent relays at boot.
    pinMode(pin, OUTPUT);
  }
}

void dimmer(const String &key, uint8_t pin, bool inverted) {
  int idx = entities::findLocalByName(key.c_str());
  bool is_new = (idx < 0);
  if (is_new) {
    if (!bindLocalActuator(key, TYPE_DIMMER)) return;
    idx = entities::findLocalByName(key.c_str());
    if (idx < 0) return;
  }
  Entity &c = entities::at((uint8_t)idx);
  c.config.pin = pin;
  c.config.inverted = inverted;
  if (is_new) {
    pwmSetup(pin);
    int off_pwm = 0;
    if (c.config.inverted) off_pwm = PWM_MAX_OUT;
    pwmWritePin(pin, (uint8_t)off_pwm);
  }
}

void ensureTimeRegistered() {
  int idx = entities::findLocalByName("TIME");
  if (idx >= 0) return;
  idx = entities::registerLocal("TIME", SENSOR_TIME);
  if (idx < 0) return;
  entities::at((uint8_t)idx).state.state = true;
}

static void updateTimeSensor() {
  time_t now = time(nullptr);
  if (now < 1704067200) return;
  int idx = entities::findLocalByName("TIME");
  if (idx < 0) {
    idx = entities::registerLocal("TIME", SENSOR_TIME);
    if (idx < 0) return;
    entities::at((uint8_t)idx).state.state = true;
  }
  Entity &c = entities::at((uint8_t)idx);
  c.state.value = (float)now;
  c.state.last_update = millis();
}

// ================= CALIBRACIÓN =================

float calibrate(const String &key, float raw) {
  Entity *e = getCalib(key);
  if (!e) return raw;
  const EntityConfig &cfg = e->config;
  float v = raw + cfg.correction;
  if (cfg.type == SENSOR_LUMI) return v;
  if (cfg.type == SENSOR_TEMP) return v;
  if (cfg.type == SENSOR_GENERIC) return v;
  if (cfg.type == SENSOR_PRESS) return v;
  if (cfg.max <= cfg.min) return v;
  if (cfg.type == SENSOR_HUMI || cfg.type == SENSOR_LEVEL) {
    float span = cfg.max - cfg.min;
    if (span <= 0.001f) return v;
    return constrain((v - cfg.min) / span * 100.0f, 0.0f, 100.0f);
  }
  return constrain(v, cfg.min, cfg.max);
}

Entity *getCalib(const String &key) {
  int idx = entities::findLocalByName(key.c_str());
  return (idx >= 0) ? &entities::at((uint8_t)idx) : nullptr;
}

// ========================================
// TIMEZONE
// ========================================
// The SENSOR_TIME entity's `correction` field IS the timezone offset in
// minutes from UTC (persisted). The runtime clock itself always stays UTC:
// NTP syncs UTC and time() is never shifted. UTC -> local is an explicit,
// portable conversion (epoch + offset_minutes*60 decomposed with gmtime()),
// avoiding libc timezone globals (configTime TZ / setenv TZ) whose semantics
// differ between ESP8266 (newlib) and ESP32 (lwip) and would break parity.
static int32_t timezoneOffsetMinutes() {
  int idx = entities::findLocalByName("TIME");
  if (idx < 0) return 0;
  return (int32_t)entities::at((uint8_t)idx).config.correction;
}

static time_t toLocalEpoch(time_t utc) {
  return utc + (time_t)timezoneOffsetMinutes() * 60;
}

RTCTime getTime() {
  time_t local = toLocalEpoch(time(nullptr));
  struct tm *timeinfo = gmtime(&local);
  RTCTime rt = {
    (uint16_t)(timeinfo->tm_year + 1900),
    (uint8_t)(timeinfo->tm_mon + 1),
    (uint8_t)timeinfo->tm_mday,
    (uint8_t)timeinfo->tm_hour,
    (uint8_t)timeinfo->tm_min,
    (uint8_t)timeinfo->tm_sec
  };
  return rt;
}

uint16_t getMinutesOfDay() {
  time_t local = toLocalEpoch(time(nullptr));
  struct tm *timeinfo = gmtime(&local);
  return timeinfo->tm_hour * 60 + timeinfo->tm_min;
}

uint32_t getUnixTime() {
  return (uint32_t)time(nullptr);
}

bool timeValid() {
  time_t now = time(nullptr);
  return now > 1704067200;
}

TimeSource getTimeSource() {
  return time_source;
}

void rtc(const RTCTime &t) {
  time_source = TIME_RTC;
  updateTimeSensor();
}

void ntp(const RTCTime &t) {
  time_source = TIME_NTP;
  updateTimeSensor();
}

void initNTP() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
}

void updateNTPTime() {
  if (getTimeSource() == TIME_RTC) return;
  time_t now = time(nullptr);
  if (now < 1704067200) return;
  time_t local = toLocalEpoch(now);
  struct tm *timeinfo = gmtime(&local);
  if (!timeinfo) return;
  RTCTime ntpTime = {
    static_cast<uint16_t>(timeinfo->tm_year + 1900),
    static_cast<uint8_t>(timeinfo->tm_mon + 1),
    static_cast<uint8_t>(timeinfo->tm_mday),
    static_cast<uint8_t>(timeinfo->tm_hour),
    static_cast<uint8_t>(timeinfo->tm_min),
    static_cast<uint8_t>(timeinfo->tm_sec)
  };
  ntp(ntpTime);
  updateTimeSensor();
}

// ========================================
// CALLBACK DE COMANDOS REMOTOS
// ========================================

// Only READ_WRITE entities (relay / dimmer) are commandable.
static bool isCommandableType(uint8_t type) {
  return qymera::model::capabilityOfType(type) == EntityCapability::READ_WRITE;
}

void onRemoteCommand(
  uint8_t command_type,
  uint32_t sensor_id,
  uint32_t value,
  bool state) {
  int idx = entities::findById(sensor_id);
  if (idx < 0) return;
  Entity &c = entities::at((uint8_t)idx);
  if (!c.runtime.local) return;  // sólo actuamos sobre entidades propias
  if (!isCommandableType(command_type)) return;

  if (command_type == (uint8_t)TYPE_RELAY) {
    setRelayByIndex(idx, state);
  } else if (command_type == (uint8_t)TYPE_DIMMER) {
    dimmerByIndex(idx, (int)value);
  }
}

// ========================================
// NET CALLBACKS - Procesadas por sensors.cpp
// ========================================

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
  uint32_t sensor_pulse_ms) {
  if (sensor_type == SENSOR_TIME) {
    int idx = entities::findLocalByName("TIME");
    if (idx < 0) return;
    Entity &c = entities::at((uint8_t)idx);
    if (c.config.correction == 0 && sensor_correction != 0) {
      c.config.correction = sensor_correction;
      web::saveCalibrationSlot(idx);
    }
    if (!timeValid() && sensor_value > 1704067200) {
      timeval tv;
      tv.tv_sec = (time_t)sensor_value;
      tv.tv_usec = 0;
      settimeofday(&tv, nullptr);
      rtc(getTime());
    }
    return;
  }

  // Find-or-create the remote entity keyed by (device_id, entity_id).
  int idx = entities::findRemoteByPeer(remote_uid, sensor_id);
  bool is_new = false;
  if (idx < 0) {
    idx = entities::findFree();
    is_new = (idx >= 0);
  }
  if (idx < 0) {
    // No free slot for a new remote entity; it is dropped. Slots are reclaimed
    // by entities::reclaimStale() once the owning device stops announcing it.
    return;
  }
  Entity &c = entities::at((uint8_t)idx);
  if (is_new) {
    c = Entity();
    c.runtime.local = false;
    c.identity.device_id = remote_uid;
    c.identity.entity_id = sensor_id;
    c.state.avail = 0;
  }
  strncpy(c.runtime.device_ip, remote_ip, ENTITY_IP_LEN - 1);
  c.runtime.device_ip[ENTITY_IP_LEN - 1] = '\0';
  c.config.type = sensor_type;
  c.state.state = sensor_state;
  c.state.last_update = millis();
  c.runtime.last_seen = c.state.last_update;
  c.runtime.online = true;
  // Mirror the owner's persistence/actuator config so the local GUI shows the
  // real remote values (fade, persist, pers_state, pulse, pulse_ms).
  c.config.fade = sensor_fade;
  c.config.persist = sensor_persist;
  c.config.pers_state = sensor_pers_state;
  c.config.pulse = sensor_pulse;
  c.config.pulse_ms = sensor_pulse_ms;
  c.config.min = sensor_min;
  c.config.max = sensor_max;
  c.config.correction = sensor_correction;
  if (c.config.type == SENSOR_LUMI) {
    c.state.value = (uint32_t)sensor_value;
  } else {
    float normalized = (float)sensor_value / 0xFFFFFFFF;
    c.state.value = normalized * (net::MAX_VAL - net::MIN_VAL) + net::MIN_VAL;
  }
  if (sensor_name.length()) {
    strncpy(c.config.name, sensor_name.c_str(), ENTITY_NAME_LEN - 1);
  } else if (c.config.name[0] == '\0') {
    char namebuf[32];
    snprintf(namebuf, sizeof(namebuf), "Remote_%X_%u", remote_uid, sensor_id);
    strncpy(c.config.name, namebuf, ENTITY_NAME_LEN - 1);
  }
  if (is_new) {
    logger::sensorsf("New remote sensor '%s' (type:%d, entity_id:%u)",
                     c.config.name, sensor_type, sensor_id);
  }
}

// ========================================
// PROTOCOL V2 CALLBACKS
// ========================================

void onV2EntityAnnounce(
  uint32_t remote_uid,
  const char *remote_ip,
  const qymera::protocol::v2::EntityAnnouncePayload &payload) {
  // Match by stable (device_id, entity_id) pair first.
  int idx = entities::findRemoteByPeer(remote_uid, payload.entity_id);
  bool is_new = false;
  if (idx < 0) {
    int by_id = entities::findById(payload.entity_id);
    if (by_id >= 0 && !entities::at((uint8_t)by_id).runtime.local) {
      idx = by_id;
    }
  }
  if (idx < 0) {
    idx = entities::findFree();
    is_new = (idx >= 0);
  }
  if (idx < 0) return;  // no free slot

  Entity &c = entities::at((uint8_t)idx);
  if (is_new) {
    c = Entity();
    c.runtime.local = false;
    c.identity.device_id = remote_uid;
    c.identity.entity_id = payload.entity_id;
    c.state.avail = 0;
    // Actuators start in a known state (OFF); measurements default to ONLINE.
    c.state.state = (payload.capabilities & (uint8_t)EntityCapability::WRITE)
                      ? false : true;
  }
  strncpy(c.runtime.device_ip, remote_ip, ENTITY_IP_LEN - 1);
  c.runtime.device_ip[ENTITY_IP_LEN - 1] = '\0';
  c.config.type = payload.type;
  c.state.last_update = millis();
  c.runtime.last_seen = c.state.last_update;
  c.runtime.online = true;
  c.config.fade = payload.fade;
  c.config.persist = payload.persist;
  c.config.pers_state = payload.pers_state;
  c.config.pulse = payload.pulse;
  c.config.pulse_ms = payload.pulse_ms;
  c.config.min = payload.min;
  c.config.max = payload.max;
  c.config.correction = payload.correction;
  c.state.avail = payload.avail;
  if (payload.name[0]) {
    strncpy(c.config.name, payload.name, ENTITY_NAME_LEN - 1);
  } else if (c.config.name[0] == '\0') {
    char namebuf[32];
    snprintf(namebuf, sizeof(namebuf), "Remote_%X_%u", remote_uid, payload.entity_id);
    strncpy(c.config.name, namebuf, ENTITY_NAME_LEN - 1);
  }
  if (is_new) {
    logger::sensorsf("V2 New remote entity '%s' (type:%d, entity_id:%08X)",
                     c.config.name, payload.type, payload.entity_id);
  }
}

void onV2StateUpdate(
  uint32_t remote_uid,
  const qymera::protocol::v2::StateUpdatePayload &payload) {
  int idx = entities::findById(payload.entity_id);
  if (idx < 0) return;  // unknown entity
  Entity &c = entities::at((uint8_t)idx);
  if (c.runtime.local) return;  // only update remotes
  if (c.identity.device_id != remote_uid) return;  // not our device

  c.state.state = payload.state;
  c.state.value = (c.config.type == SENSOR_LUMI || c.config.type == SENSOR_TIME)
                    ? (float)payload.value
                    : ((float)payload.value / 0xFFFFFFFF) * (net::MAX_VAL - net::MIN_VAL) + net::MIN_VAL;
  c.state.avail = payload.avail;
  c.state.last_update = millis();
  c.runtime.last_seen = c.state.last_update;
}

uint8_t onV2Command(
  uint32_t remote_uid,
  const char * /*remote_ip*/,
  uint32_t /*msg_id*/,
  const qymera::protocol::v2::CommandPayload &payload) {
  // Find local actuator by entity_id. The transport layer sends the
  // COMMAND_ACK / COMMAND_ERROR using the returned status.
  int idx = entities::findById(payload.entity_id);
  if (idx < 0) return qymera::delivery::ST_NOT_FOUND;
  Entity &c = entities::at((uint8_t)idx);
  if (!c.runtime.local) return qymera::delivery::ST_NOT_LOCAL;
  // Only actuators (READ_WRITE) are commandable.
  if (!isCommandableType(payload.type)) return qymera::delivery::ST_INVALID;

  if (payload.type == (uint8_t)TYPE_RELAY) {
    setRelayByIndex(idx, payload.state);
  } else if (payload.type == (uint8_t)TYPE_DIMMER) {
    dimmerByIndex(idx, (int)payload.value);
  } else {
    return qymera::delivery::ST_INVALID;
  }
  return qymera::delivery::ST_OK;
}

void onV2CommandAck(
  uint32_t remote_uid,
  const qymera::protocol::v2::CommandAckPayload &payload) {
  logger::coref("V2 Command ACK from %08X entity=%08X status=%d",
                remote_uid, payload.entity_id, payload.status);
}

void onV2CommandError(
  uint32_t remote_uid,
  const qymera::protocol::v2::CommandErrorPayload &payload) {
  logger::warnf("V2 Command ERROR from %08X entity=%08X code=%d: %s",
                remote_uid, payload.entity_id, payload.error_code, payload.message);
}

}  // namespace sensors