#include "sensors.h"
#include <WiFiClient.h>
#include <time.h>
#include "config.h"
#include "model.h"
#include "core.h"
#include "mesh.h"
#include "web.h"
#include "automations.h"
#include "log.h"


namespace sensors {

Calibration calibrations[MAX_SENSORS];
Fade activeFades[MAX_SENSORS];
PulseState activePulses[MAX_SENSORS];

static TimeSource time_source = TIME_NONE;

// Simple PRNG for entity ID generation (xorshift32).
static uint32_t entity_id_seed = 0;

static uint32_t nextEntityId() {
  if (entity_id_seed == 0) {
    // Seed from chip ID + millis for variability across boots before persistence loads.
    entity_id_seed = GET_CHIP_ID() ^ (uint32_t)millis();
    entity_id_seed |= 1;  // ensure non-zero
  }
  // xorshift32
  uint32_t x = entity_id_seed;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  entity_id_seed = x;
  // Combine with chip ID upper bits for device-level uniqueness.
  return (GET_CHIP_ID() & 0xFFFF0000) | (x & 0xFFFF);
}

static uint32_t makeSensorUid(uint8_t index) {
  return GET_CHIP_ID() + (uint32_t)index + 1;
}

static int findFreeCalib() {
  for (int i = 0; i < MAX_SENSORS; i++) {
    if (calibrations[i].uid == 0) return i;
  }
  return -1;
}

static void bindLocalSensor(uint8_t idx, const String &name, SensorType type) {
  auto &c = calibrations[idx];
  c.id = idx;
  c.name = name;
  c.uid = makeSensorUid(idx);
  c.type = type;
  c.local = true;
  c.device_uid = GET_CHIP_ID();
  IPAddress ip = WiFi.localIP();
  snprintf(c.device_ip, sizeof(c.device_ip), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
  // Assign stable entity_id on first registration (persisted across reboots).
  if (c.entity_id == 0) {
    c.entity_id = nextEntityId();
    logger::sensorsf("Entity registered: idx=%d uid=%u entity_id=%08X name=%s",
                     idx, c.uid, c.entity_id, name.c_str());
  }
}

// Local-only name lookup for the sensor read/registration functions. A name
// match against a REMOTE entity must never rebind that entity as local:
// bindLocalSensor() would then re-announce it under a NEW local uid, which
// feeds the discovery redistribution loop (remote -> stolen -> re-broadcast
// -> duplicate). Remote entities are read-only for binding purposes.
static int findLocalCalib(const String &key) {
  for (int i = 0; i < MAX_SENSORS; i++) {
    if (calibrations[i].local && calibrations[i].uid != 0 &&
        calibrations[i].name == key) return i;
  }
  return -1;
}

void init() {
  for (int i = 0; i < MAX_SENSORS; i++) {
    calibrations[i] = Calibration();
    activeFades[i] = Fade();
  }
  mesh::setSensorDiscoveryCallback(onRemoteSensorDiscovered);
  mesh::setCommandCallback(onRemoteCommand);
}

void applyPersistedStates() {
  for (int i = 0; i < MAX_SENSORS; i++) {
    auto &c = calibrations[i];
    if (c.type != TYPE_RELAY) continue;
    if (!c.local) continue;
    // Deterministic boot state, applied exactly once before any report:
    // persistent relays restore their last state; non-persistent relays are OFF.
    bool on = c.persist ? c.pers_state : false;
    pinMode(c.pin, OUTPUT);
    digitalWrite(c.pin, c.inverted ? !on : on);
    c.state = on;
    mesh::setReport(i, c.uid, c.value, c.value, c.state);
  }
}

// ================= REMOTE SENSOR LIFECYCLE =================

bool isValidSensorType(uint8_t type) {
  // Canonical entity-model definition: a type is valid iff it maps to a real
  // entity kind (sensor / actuator / clock). Mirrors qymera::model::isValidType.
  return qymera::model::isValidType(type);
}

bool isStaleRemote(int index) {
  if (index < 0 || index >= MAX_SENSORS) return false;
  auto &c = calibrations[index];
  if (c.local || c.uid == 0) return false;
  // Wrap-safe elapsed check (millis() overflow after ~49 days).
  return (uint32_t)(millis() - c.last_update) > MESH_TIMEOUT;
}

bool isEntryVisible(int index) {
  if (index < 0 || index >= MAX_SENSORS) return false;
  auto &c = calibrations[index];
  if (c.uid == 0) return false;
  if (!isValidSensorType((uint8_t)c.type)) return false;
  if (c.local) return true;
  return !isStaleRemote(index);
}

void reclaimStaleSlots() {
  static unsigned long last_pass = 0;
  // Run at most once per timeout window.
  if ((uint32_t)(millis() - last_pass) < MESH_TIMEOUT) return;
  last_pass = millis();
  for (int i = 0; i < MAX_SENSORS; i++) {
    auto &c = calibrations[i];
    if (c.local || c.uid == 0) continue;
    if (!isStaleRemote(i)) continue;
    // Never reclaim a slot an automation still references: rules address
    // sensors/actuators by calibration index, so reusing the slot would change
    // what the rule acts on. Referenced stale entries are kept (hidden) and
    // stay occupied until the rule is deleted.
    if (automations::isIndexReferenced((uint8_t)i)) continue;
    calibrations[i] = Calibration();
    logger::sensorsf("Reclaimed stale remote slot %d", i);
  }
}

void applyFades() {
  for (int i = 0; i < MAX_SENSORS; i++) {
    if (!activeFades[i].active) continue;
    auto &f = activeFades[i];
    auto &c = calibrations[i];
    if (!c.local) continue;
    unsigned long elapsed = millis() - f.startTime;
    if (elapsed >= f.duration) {
      int pwm = f.endVal;
      if (c.inverted)
        pwm = PWM_MAX_OUT - pwm;
      pwmWritePin(f.pin, (uint8_t)pwm);
      f.active = false;
    } else {
      float progress = (float)elapsed / f.duration;
      int current =
        f.startVal + (f.endVal - f.startVal) * progress;
      int pwm = current;
      if (c.inverted)
        pwm = PWM_MAX_OUT - pwm;
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
      // Sincronizar estado lógico y reportar
      auto &c = calibrations[i];
      c.state = false;
      if (c.persist && c.pers_state != c.state) {
        c.pers_state = c.state;
        web::saveCalibrationSlot(i);
      }
      logger::sensorsf("Relay %s -> OFF", c.name.c_str());
      mesh::setReport(i, c.uid, c.value, c.value, c.state);
    }
  }
}

int findCalib(const String &key) {
  for (int i = 0; i < MAX_SENSORS; i++) {
    if (calibrations[i].name == key) return i;
  }
  return -1;
}

int findCalibByUid(uint32_t uid) {
  for (int i = 0; i < MAX_SENSORS; i++) {
    if (calibrations[i].uid == uid) return i;
  }
  return -1;
}

int findCalibByIndex(uint8_t index) {
  if (index >= MAX_SENSORS) return -1;
  return calibrations[index].uid == 0 ? -1 : index;
}

int findCalibByEntityId(uint32_t entity_id) {
  if (entity_id == 0) return -1;
  for (int i = 0; i < MAX_SENSORS; i++) {
    if (calibrations[i].entity_id == entity_id) return i;
  }
  return -1;
}

void setRelay(const String &key, bool target) {
  int idx = findCalib(key);
  if (idx < 0) return;
  auto &c = calibrations[idx];
  if (c.type != TYPE_RELAY) return;

  if (!c.local) {
    // ---- Actuador REMOTO: enviar comando UDP al dispositivo propietario ----
    mesh::sendCommand(
      c.device_uid,
      c.device_ip,
      c.uid,
      (uint8_t)TYPE_RELAY,
      target ? 1u : 0u,
      target);
    return;
  }

  // ---- Actuador LOCAL: operar GPIO ----
  if (c.pulse) {
    if (target) {
      // ---- Activar modo pulse ----
      digitalWrite(c.pin, (true  ^ c.inverted) ? HIGH : LOW);
      activePulses[idx].pin = c.pin;
      activePulses[idx].inverted = c.inverted;
      activePulses[idx].start_ms = millis();
      activePulses[idx].pulse_ms = c.pulse_ms;
      activePulses[idx].active = true;
      c.state = true;  // Relay IS ON while pulse is active
    } else {
      // ---- Desactivar modo pulse (apagar relay y detener timer) ----
      activePulses[idx].active = false;  // Detener timer de pulse
      digitalWrite(c.pin, (false ^ c.inverted) ? HIGH : LOW);
      c.state = false;
    }
  } else {
    // Normal ON/OFF mode (sin pulse)
    if (target) {
      digitalWrite(c.pin, (true ^ c.inverted) ? HIGH : LOW);
    } else {
      digitalWrite(c.pin, (false ^ c.inverted) ? HIGH : LOW);
    }
    c.state = target;
  }

  // ---- Persistencia en EEPROM (solo si cambió el estado) ----
  if (c.persist && c.pers_state != c.state) {
    c.pers_state = c.state;
    web::saveCalibrationSlot(idx);
  }

  logger::sensorsf("Relay %s -> %s", c.name.c_str(), target ? "ON" : "OFF");
  mesh::setReport(idx, c.uid, c.value, c.value, c.state);
}

void handleDimmer(const String &key, int value) {
  int idx = findCalib(key);
  if (idx < 0) return;
  auto &c = calibrations[idx];
  if (c.type != TYPE_DIMMER) return;
  value = constrain(value, 0, 100);

  if (!c.local) {
    // ---- Actuador REMOTO: enviar comando UDP ----
    mesh::sendCommand(
      c.device_uid,
      c.device_ip,
      c.uid,
      (uint8_t)TYPE_DIMMER,
      (uint32_t)value,
      value > 0);
    return;
  }

  // ---- Actuador LOCAL: operar PWM ----
  int pwm_val = map(value, 0, 100, 0, PWM_MAX_OUT);
  if (c.inverted)
    pwm_val = PWM_MAX_OUT - pwm_val;
  if (c.fade > 0) {
    int current = pwmReadPin(c.pin);
    startFade(key, c.pin, current, pwm_val, c.fade);
  } else {
    pwmWritePin(c.pin, (uint8_t)pwm_val);
  }
  c.value = value;
  c.state = (value > 0);
  logger::sensorsf("Dimmer %s -> %d%%", c.name.c_str(), value);
  mesh::setReport(idx, c.uid, c.value, c.state ? c.value : 0, c.state);
}

void handleToggle(uint32_t uid) {
  int idx = findCalibByUid(uid);
  if (idx < 0) return;
  auto &c = calibrations[idx];
  if (c.type == TYPE_RELAY) {
    // Cuando se hace toggle, enviar el estado contrario
    // Si pulse está activo, esto lo desactivará
    // Si pulse no está activo, toggla ON/OFF normal
    setRelay(c.name, !c.state);
    // Forzar re-carga de dispositivos después
    // (esto provocará que loadDevices() lea el nuevo c.state)
  } else if (c.type == TYPE_DIMMER) {
    c.state = !c.state;
    int pwm_val = map(c.value, 0, 100, 0, PWM_MAX_OUT);
    if (c.inverted)
      pwm_val = PWM_MAX_OUT - pwm_val;
    if (c.fade > 0) {
      int current = pwmReadPin(c.pin);
      startFade(
        c.name,
        c.pin,
        current,
        c.state ? pwm_val : 0,
        c.fade);
    } else {
      pwmWritePin(
        c.pin,
        (uint8_t)(c.state ? pwm_val : 0));
    }
    logger::sensorsf("Dimmer %s -> %s", c.name.c_str(), c.state ? "ON" : "OFF");
    mesh::setReport(
      idx,
      c.uid,
      c.value,
      c.state ? c.value : 0,
      c.state);
  }
}

void handleDimmer(uint32_t uid, int value) {
  int idx = findCalibByUid(uid);
  if (idx < 0) return;
  handleDimmer(calibrations[idx].name, value);
}

void handleToggle(const String &key) {
  int idx = findCalib(key);
  if (idx < 0) return;
  handleToggle(calibrations[idx].uid);
}

void startFade(const String &key, uint8_t pin, int from, int to, unsigned long dur) {
  int idx = findCalib(key);
  if (idx < 0) return;
  activeFades[idx].pin = pin;
  activeFades[idx].startVal = from;
  activeFades[idx].endVal = to;
  activeFades[idx].startTime = millis();
  activeFades[idx].duration = dur;
  activeFades[idx].active = true;
}

void temperature(const String &key, float raw) {
  int idx = findLocalCalib(key);
  if (idx < 0) idx = findFreeCalib();
  if (idx < 0) return;
  auto &c = calibrations[idx];
  bindLocalSensor(idx, key, SENSOR_TEMP);
  c.value = calibrate(key, raw);
  mesh::setReport(idx, c.uid, c.value, raw, c.state);
}

void humidity(const String &key, int raw) {
  int idx = findLocalCalib(key);
  if (idx < 0) idx = findFreeCalib();
  if (idx < 0) return;
  auto &c = calibrations[idx];
  bindLocalSensor(idx, key, SENSOR_HUMI);
  c.value = calibrate(key, raw);
  mesh::setReport(idx, c.uid, c.value, raw, c.state);
}

void luminosity(const String &key, int raw) {
  int idx = findLocalCalib(key);
  if (idx < 0) idx = findFreeCalib();
  if (idx < 0) return;
  auto &c = calibrations[idx];
  bindLocalSensor(idx, key, SENSOR_LUMI);
  c.value = calibrate(key, raw);
  mesh::setReport(idx, c.uid, c.value, raw, c.state);
}

void level(const String &key, int raw) {
  int idx = findLocalCalib(key);
  if (idx < 0) idx = findFreeCalib();
  if (idx < 0) return;
  auto &c = calibrations[idx];
  bindLocalSensor(idx, key, SENSOR_LEVEL);
  c.value = calibrate(key, raw);
  mesh::setReport(idx, c.uid, c.value, raw, c.state);
}

void pressure(const String &key, float raw) {
  int idx = findLocalCalib(key);
  if (idx < 0) idx = findFreeCalib();
  if (idx < 0) return;
  auto &c = calibrations[idx];
  bindLocalSensor(idx, key, SENSOR_PRESS);
  c.value = calibrate(key, raw);
  mesh::setReport(idx, c.uid, c.value, raw, c.state);
}

void airQ(const String &key, const int &v) {
  int idx = findLocalCalib(key);
  if (idx < 0) idx = findFreeCalib();
  if (idx < 0) return;
  auto &c = calibrations[idx];
  bindLocalSensor(idx, key, SENSOR_AIRQ);
  c.value = v;
  mesh::setReport(idx, c.uid, c.value, v, c.state);
}

void rain(const String &key, bool v) {
  int idx = findLocalCalib(key);
  if (idx < 0) idx = findFreeCalib();
  if (idx < 0) return;
  auto &c = calibrations[idx];
  bindLocalSensor(idx, key, SENSOR_RAIN);
  c.state = v;
  c.value = v ? 1.0f : 0.0f;
  mesh::setReport(idx, c.uid, c.value, c.value, c.state);
}

void custom(const String &key, float raw) {
  int idx = findLocalCalib(key);
  if (idx < 0) idx = findFreeCalib();
  if (idx < 0) return;
  auto &c = calibrations[idx];
  bindLocalSensor(idx, key, SENSOR_GENERIC);
  c.value = calibrate(key, raw);
  mesh::setReport(idx, c.uid, c.value, raw, c.state);
}

void contact(const String &key, bool v) {
  int idx = findLocalCalib(key);
  if (idx < 0) idx = findFreeCalib();
  if (idx < 0) return;
  auto &c = calibrations[idx];
  bindLocalSensor(idx, key, SENSOR_CONTACT);
  c.state = v;
  c.value = v ? 0.0f : 1.0f;
  mesh::setReport(idx, c.uid, c.value, c.value, c.state);
}

void relay(const String &key, uint8_t pin, bool inverted) {
  int idx = findCalib(key);
  bool is_new = (idx < 0);
  if (is_new) {
    idx = findFreeCalib();
    if (idx < 0) return;
  }
  auto &c = calibrations[idx];
  if (is_new) {
    Serial.printf(
      "REGISTER idx=%d name=%s persist=%d pers=%d\n",
      idx,
      c.name.c_str(),
      c.persist,
      c.pers_state);
    bindLocalSensor(idx, key, TYPE_RELAY);
    c.pin = pin;
    c.inverted = inverted;
    // Only configure the pin here. The initial GPIO state is applied once in
    // applyPersistedStates() (before the first report) to avoid an
    // OFF -> ON glitch on persistent relays at boot.
    pinMode(pin, OUTPUT);
  }
  mesh::setReport(idx, c.uid, c.value, c.value, c.state);
}

void dimmer(const String &key, uint8_t pin, bool inverted) {
  int idx = findCalib(key);
  bool is_new = (idx < 0);
  if (is_new) {
    idx = findFreeCalib();
    if (idx < 0) return;
  }
  auto &c = calibrations[idx];
  if (is_new) {
    bindLocalSensor(idx, key, TYPE_DIMMER);
    c.pin = pin;
    c.inverted = inverted;
    pwmSetup(pin);
    int off_pwm = 0;
    if (c.inverted)
      off_pwm = PWM_MAX_OUT;
    pwmWritePin(pin, (uint8_t)off_pwm);
  }
  mesh::setReport(idx, c.uid, c.value, c.value, c.state);
}

void ensureTimeRegistered() {
  int idx = findCalib("TIME");
  if (idx >= 0) return;
  idx = findFreeCalib();
  if (idx < 0) return;
  bindLocalSensor(idx, "TIME", SENSOR_TIME);
  calibrations[idx].state = true;
}

static void updateTimeSensor() {
  time_t now = time(nullptr);
  if (now < 1704067200) return;
  int idx = findCalib("TIME");
  if (idx < 0) {
    idx = findFreeCalib();
    if (idx < 0) return;
    bindLocalSensor(idx, "TIME", SENSOR_TIME);
    calibrations[idx].state = true;
  }
  auto &c = calibrations[idx];
  c.value = (float)now;
  mesh::setReport(idx, c.uid, c.value, c.value, c.state);
}

float calibrate(const String &key, float raw) {
  Calibration *c = getCalib(key);
  if (!c) return raw;
  float v = raw + c->correction;
  if (c->type == SENSOR_LUMI) return v;
  if (c->type == SENSOR_TEMP) return v;
  if (c->type == SENSOR_GENERIC) return v;
  if (c->type == SENSOR_PRESS) return v;
  if (c->max <= c->min) return v;
  if (c->type == SENSOR_HUMI || c->type == SENSOR_LEVEL) {
    float span = c->max - c->min;
    if (span <= 0.001f) return v;
    return constrain((v - c->min) / span * 100.0f, 0.0f, 100.0f);
  }
  return constrain(v, c->min, c->max);
}

Calibration *getCalib(const String &key) {
  int idx = findCalib(key);
  return (idx >= 0) ? &calibrations[idx] : nullptr;
}

// ========================================
// TIMEZONE
// ========================================
// The SENSOR_TIME calibration's `correction` field IS the timezone offset in
// minutes from UTC (persisted). The runtime clock itself always stays UTC:
// NTP syncs UTC and time() is never shifted. UTC -> local is an explicit,
// portable conversion (epoch + offset_minutes*60 decomposed with gmtime()),
// avoiding libc timezone globals (configTime TZ / setenv TZ) whose semantics
// differ between ESP8266 (newlib) and ESP32 (lwip) and would break parity.
static int32_t timezoneOffsetMinutes() {
  int idx = findCalib("TIME");
  if (idx < 0) return 0;
  return (int32_t)calibrations[idx].correction;
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
  if (getTimeSource() == TIME_RTC)
    return;
  time_t now = time(nullptr);
  if (now < 1704067200)
    return;
  time_t local = toLocalEpoch(now);
  struct tm *timeinfo = gmtime(&local);
  if (!timeinfo)
    return;
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

/**
 * @brief Invocada por mesh::tick() cuando llega un paquete dirigido a este
 *        dispositivo (is_remote == false).  Busca el actuador por su uid y
 *        delega en setRelay / handleDimmer para ejecutar la acción LOCAL.
 *
 * @param command_type  Tipo de sensor/actuador del paquete (TYPE_RELAY, etc.).
 * @param sensor_id     UID del actuador destino.
 * @param value         Valor asociado (nivel dimmer o flag relay).
 * @param state         Estado ON/OFF.
 */
void onRemoteCommand(
  uint8_t command_type,
  uint32_t sensor_id,
  uint32_t value,
  bool state) {
  int idx = findCalibByUid(sensor_id);
  if (idx < 0) return;
  auto &c = calibrations[idx];
  if (!c.local) return;  // sólo actuamos sobre sensores propios

  // Only READ_WRITE entities (actuators: relay / dimmer) are commandable.
  if (qymera::model::capabilityOfType(command_type) !=
      qymera::model::EntityCapability::READ_WRITE) {
    return;
  }

  if (command_type == (uint8_t)TYPE_RELAY) {
    setRelay(c.name, state);
  } else if (command_type == (uint8_t)TYPE_DIMMER) {
    handleDimmer(c.name, (int)value);
  }
}

// ========================================
// MESH CALLBACKS - Procesadas por sensors.cpp
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
    int idx = findCalib("TIME");
    if (idx < 0) return;
    auto &c = calibrations[idx];
    if (c.correction == 0 && sensor_correction != 0) {
      c.correction = sensor_correction;
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
  int idx = -1;
  for (int i = 0; i < MAX_SENSORS; i++) {
    if (!calibrations[i].local && calibrations[i].device_uid == remote_uid && calibrations[i].uid == sensor_id) {
      idx = i;
      break;
    }
  }
  bool is_new = false;
  if (idx == -1) {
    for (int i = 0; i < MAX_SENSORS; i++) {
      if (calibrations[i].uid == 0) {
        idx = i;
        is_new = true;
        break;
      }
    }
  }
  if (idx < 0) {
    // No free slot for a new remote entity; it is dropped. Slots are reclaimed
    // by reclaimStaleSlots() once the owning device stops announcing it.
    return;
  }
  auto &c = calibrations[idx];
  c.local = false;
  c.device_uid = remote_uid;
  strncpy(c.device_ip, remote_ip, sizeof(c.device_ip) - 1);
  c.device_ip[sizeof(c.device_ip) - 1] = '\0';
  c.id = idx;
  c.uid = sensor_id;
  c.type = (SensorType)sensor_type;
  if (is_new) c.avail = 0;
  c.state = sensor_state;
  c.last_update = millis();
  // Mirror the owner's persistence/actuator config so the local GUI shows the
  // real remote values (fade, persist, pers_state, pulse, pulse_ms).
  c.fade = sensor_fade;
  c.persist = sensor_persist;
  c.pers_state = sensor_pers_state;
  c.pulse = sensor_pulse;
  c.pulse_ms = sensor_pulse_ms;
  if (c.type == SENSOR_LUMI) {
    c.value = (uint32_t)sensor_value;
  } else {
    float normalized = (float)sensor_value / 0xFFFFFFFF;
    c.value = normalized * (150.0f - (-50.0f)) + (-50.0f);
  }
  if (sensor_name.length()) {
    c.name = sensor_name;
  } else if (c.name == "") {
    char namebuf[32];
    snprintf(namebuf, sizeof(namebuf), "Remote_%X_%u", remote_uid, sensor_id);
    c.name = namebuf;
  }
  if (is_new) {
    logger::sensorsf("New remote sensor '%s' (type:%d, uid:%u)", c.name.c_str(), sensor_type, sensor_id);
  }
  mesh::setReport(idx, c.uid, c.value, c.value, c.state);
}

}  // namespace sensors
