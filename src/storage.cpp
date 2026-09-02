#include "storage.h"
#include <EEPROM.h>
#ifdef ESP32
#include <Preferences.h>
#endif
#include "automations.h"
#include "core.h"
#include "log.h"
#include "mesh.h"
#include "sensors.h"

namespace storage {

#if defined(ESP32)
static Preferences prefs;
static bool prefs_ready = false;

void begin() {
  if (!prefs_ready) {
    prefs_ready = prefs.begin("eeprom", false);
    if (!prefs_ready) logger::error("Preferences begin FAILED - defaults will be used");
  }
}
uint8_t read(int addr) { return prefs.getUChar(String(addr).c_str(), 0); }
void write(int addr, uint8_t val) { prefs.putUChar(String(addr).c_str(), val); }
template<typename T> bool get(int addr, T &obj) {
  // IMPORTANT: prefs.getBytes() leaves the buffer untouched when the key is
  // missing or corrupted. Zero-fill first so unprovisioned slots never expose
  // stack garbage (this was the "random fade on every sensor" bug).
  memset(&obj, 0, sizeof(T));
  size_t n = prefs.getBytes(String(addr).c_str(), &obj, sizeof(T));
  return n == sizeof(T);
}
template<typename T> bool put(int addr, const T &obj) {
  return prefs.putBytes(String(addr).c_str(), &obj, sizeof(T)) == sizeof(T);
}
void commit() {}
void clearAll() { prefs.clear(); }

#else
void begin() { EEPROM.begin(EEPROM_SIZE); }
uint8_t read(int addr) { return EEPROM.read(addr); }
void write(int addr, uint8_t val) { EEPROM.write(addr, val); }
template<typename T> bool get(int addr, T &obj) { EEPROM.get(addr, obj); return true; }
template<typename T> bool put(int addr, const T &obj) { EEPROM.put(addr, obj); return true; }
void commit() { EEPROM.commit(); }
void clearAll() {
  memset(EEPROM.getDataPtr() + EEPROM_RELAY_STATE_START, 0, EEPROM_RELAY_STATE_SIZE);
  memset(EEPROM.getDataPtr() + EEPROM_CRED_START, 0, EEPROM_CRED_SIZE);
  memset(EEPROM.getDataPtr() + EEPROM_GENSET_START + 12, 0, EEPROM_SIZE - EEPROM_GENSET_START - 12);
}

#endif

/* Persisted calibration slot.
   v1: magic/version/uid/... (legacy)
   v2: entity_id stored in separate map (see EEPROM_ENTITY_ID_START) */
static const uint32_t CALIB_MAGIC = 0x514D434C;  // "QMCL"
static const uint16_t CALIB_VERSION = 2;

struct __attribute__((packed)) CalibrationPersist {
  uint32_t magic;
  uint16_t version;
  uint32_t uid;              // legacy wire identity
  bool pers_state;
  float min;
  float max;
  float correction;
  uint8_t avail;
  bool persist;
  bool pulse;
  uint32_t pulse_ms;
  uint32_t fade;
};
static_assert(sizeof(CalibrationPersist) == EEPROM_CALIB_SLOT_SIZE,
              "CalibrationPersist size must match EEPROM_CALIB_SLOT_SIZE in config.h");

struct RulesHeader {
  uint32_t magic;
  uint16_t version;
  uint16_t count;
};

static const uint32_t RULES_MAGIC = 0x4155544F;  // "AUTO"
static const uint16_t RULES_VERSION = 1;

// Entity ID map helpers (slot_index -> entity_id)
static inline int entityIdMapAddr(int slot) {
  return EEPROM_ENTITY_ID_START + slot * 4;
}
static uint32_t loadEntityId(int slot) {
  uint32_t eid = 0;
  int addr = entityIdMapAddr(slot);
#if defined(ESP32)
  begin();
  prefs.getBytes(String(addr).c_str(), &eid, 4);
#else
  EEPROM.get(addr, eid);
#endif
  return eid;
}
static void saveEntityId(int slot, uint32_t entity_id) {
  int addr = entityIdMapAddr(slot);
#if defined(ESP32)
  begin();
  prefs.putBytes(String(addr).c_str(), &entity_id, 4);
#else
  EEPROM.put(addr, entity_id);
  EEPROM.commit();
#endif
}

static const uint32_t OTA_HASH_BYTES = 256;
static bool ota_integrity_verified = false;

static uint32_t calculateFirmwareHash() {
  // Not a full-image hash: a prior `(uint8_t*)0x0000` deref here caused
  // Exception 28 (LOAD Prohibited) on every OTA enable -- the reported crash.
  // Use the chip-unique MAC token (GET_CHIP_ID in config.h) instead: no flash
  // deref (cannot fault) and stable across firmware updates, so with no
  // ArduinoOTA.onEnd() re-provisioning the gate cannot self-disable OTA after
  // an update. Best-effort / detection-only (no SHA-256), per design.
  return GET_CHIP_ID();
}

bool verifyOtaIntegrity() {
  begin();
  uint32_t expected = 0;
  get(EEPROM_OTA_HASH_ADDR, expected);
  commit();
  bool provisioned = (expected != 0xFFFFFFFFu && expected != 0u);
  uint32_t actual = calculateFirmwareHash();
  if (!provisioned) {
    begin();
    put(EEPROM_OTA_HASH_ADDR, actual);
    commit();
    ota_integrity_verified = true;
    logger::core("OTA baseline hash stored (provisioning step)");
    return true;
  }
  ota_integrity_verified = (actual == expected);
  if (!ota_integrity_verified) logger::warnf("OTA integrity FAILED (stored:%08X actual:%08X)", expected, actual);
  else logger::core("OTA integrity verified");
  return ota_integrity_verified;
}

bool isOtaIntegrityVerified() { return ota_integrity_verified; }

uint8_t loadOtaFlag() {
  begin();
  // Normalize: only 1 means enabled. 0xFF (unprovisioned) or anything else = disabled.
  return (read(EEPROM_OTA_FLAG_ADDR) == 1) ? 1 : 0;
}

void saveOtaFlag(uint8_t flag) {
  begin();
  write(EEPROM_OTA_FLAG_ADDR, (flag == 1) ? 1 : 0);
  commit();
}

bool isOtaEnabled() { return loadOtaFlag() == 1; }

void loadCredentials(String &ssid, String &password) {
  begin();
  int slen = read(EEPROM_CRED_START);
  if (slen < 0 || slen > 32) slen = 0;
  char ssid_buf[33] = {0};
  for (int i = 0; i < slen; i++) {
    char c = read(EEPROM_CRED_START + 1 + i);
    if (c == 0xFF || c == 0) break;
    ssid_buf[i] = c;
  }
  ssid = ssid_buf;
  int plen_addr = EEPROM_CRED_START + 1 + slen;
  int plen = read(plen_addr);
  if (plen < 0 || plen > 64) plen = 0;
  char pass_buf[65] = {0};
  for (int i = 0; i < plen; i++) {
    char c = read(plen_addr + 1 + i);
    if (c == 0xFF || c == 0) break;
    pass_buf[i] = c;
  }
  password = pass_buf;
}

void saveCredentials(const String &ssid, const String &password) {
  begin();
  write(EEPROM_CRED_START, ssid.length());
  for (int i = 0; i < ssid.length(); i++) write(EEPROM_CRED_START + 1 + i, ssid[i]);
  int offset = EEPROM_CRED_START + 1 + ssid.length();
  write(offset, password.length());
  for (int i = 0; i < password.length(); i++) write(offset + 1 + i, password[i]);
  commit();
}

void loadGeneralSettings(uint16_t &broadcast_port, uint16_t &command_port, uint32_t &report_interval) {
  begin();
  int addr = EEPROM_GENSET_START;
  get(addr, broadcast_port); addr += sizeof(uint16_t);
  get(addr, command_port); addr += sizeof(uint16_t);
  get(addr, report_interval); addr += sizeof(uint32_t);
  if (broadcast_port < 1024 || broadcast_port > 65500) broadcast_port = BROADCAST_PORT;
  if (command_port < 1024 || command_port > 65500) command_port = COMMAND_PORT;
  if (report_interval < 5000 || report_interval > 600000) report_interval = BROADCAST_INTERVAL;
}

void saveGeneralSettings(uint16_t broadcast_port, uint16_t command_port, uint32_t report_interval) {
  begin();
  if (broadcast_port < 1024 || broadcast_port > 65500) broadcast_port = BROADCAST_PORT;
  if (command_port < 1024 || command_port > 65500) command_port = COMMAND_PORT;
  if (report_interval < 5000 || report_interval > 600000) report_interval = BROADCAST_INTERVAL;
  int addr = EEPROM_GENSET_START;
  put(addr, broadcast_port); addr += sizeof(uint16_t);
  put(addr, command_port); addr += sizeof(uint16_t);
  put(addr, report_interval); addr += sizeof(uint32_t);
  commit();
}

void factoryReset() {
  begin();
#if defined(ESP32)
  prefs.clear();
#else
  clearAll();
  uint16_t def_broadcast = BROADCAST_PORT;
  uint16_t def_command = COMMAND_PORT;
  uint32_t def_interval = BROADCAST_INTERVAL;
  int addr = EEPROM_GENSET_START;
  put(addr, def_broadcast); addr += sizeof(uint16_t);
  put(addr, def_command); addr += sizeof(uint16_t);
  put(addr, def_interval);
#endif
  commit();
  delay(100);
  RESET_MCU();
}

void loadCalibration() {
  begin();
  for (int i = 0; i < MAX_PERSISTED_SENSORS; i++) {
    int addr = EEPROM_CALIB_START + i * sizeof(CalibrationPersist);
    CalibrationPersist p = {};
    if (!get(addr, p)) continue;                    // missing/corrupt key on ESP32
    if (p.magic != CALIB_MAGIC || p.version != CALIB_VERSION) continue;  // unprovisioned
    if (p.uid == 0) continue;
    if (!isfinite(p.min) || !isfinite(p.max) || !isfinite(p.correction)) continue;
    if (p.fade > 3600000UL) continue;               // sane fade cap (1h)
    int idx = sensors::findCalibByUid(p.uid);       // attach to the exact device
    if (idx < 0) continue;                          // no registered device with this uid
    auto &c = sensors::calibrations[idx];
    if (!c.local) continue;
    c.pers_state = p.pers_state;
    c.min = p.min;
    c.max = p.max;
    c.correction = p.correction;
    c.avail = p.avail;
    c.persist = p.persist;
    c.pulse = p.pulse;
    c.pulse_ms = p.pulse_ms;
    c.fade = p.fade;
    // Restore stable entity_id from map (v2+)
    uint32_t eid = loadEntityId(i);
    if (eid != 0) {
      c.entity_id = eid;
    }
  }
}

void saveCalibrationSlot(int index) {
  if (index < 0 || index >= MAX_PERSISTED_SENSORS) return;
  begin();
  int addr = EEPROM_CALIB_START + index * sizeof(CalibrationPersist);
  auto &c = sensors::calibrations[index];
  CalibrationPersist current = {};
  if (c.local && c.uid != 0) {
    current.magic = CALIB_MAGIC;
    current.version = CALIB_VERSION;
    current.uid = c.uid;
    current.pers_state = c.pers_state;
    current.min = c.min;
    current.max = c.max;
    current.correction = c.correction;
    current.avail = c.avail;
    current.persist = c.persist;
    current.pulse = c.pulse;
    current.pulse_ms = c.pulse_ms;
    current.fade = c.fade;
    // Persist stable entity_id in separate map
    if (c.entity_id != 0) {
      saveEntityId(index, c.entity_id);
    }
  }
  CalibrationPersist stored = {};
  get(addr, stored);
  if (memcmp(&current, &stored, sizeof(CalibrationPersist)) != 0) {
    put(addr, current);
    commit();
  }
}

void saveCalibration() {
  begin();
  for (int i = 0; i < MAX_PERSISTED_SENSORS; i++) saveCalibrationSlot(i);
}

void loadRules() {
  begin();
  RulesHeader h;
  int addr = EEPROM_RULES_START;
  get(addr, h);
  addr += sizeof(RulesHeader);
  if (h.magic != RULES_MAGIC || h.version != RULES_VERSION) {
    memset(automations::rules, 0, sizeof(automations::rules));
    return;
  }
  for (int i = 0; i < MAX_RULES; i++) {
    get(addr, automations::rules[i]);
    addr += sizeof(automations::Rule);
  }
}

void saveRules() {
  begin();
  RulesHeader h;
  h.magic = RULES_MAGIC;
  h.version = RULES_VERSION;
  h.count = MAX_RULES;
  int addr = EEPROM_RULES_START;
  bool dirty = false;
  RulesHeader stored_h;
  get(addr, stored_h);
  if (memcmp(&h, &stored_h, sizeof(RulesHeader)) != 0) {
    dirty = true;
    put(addr, h);
  }
  addr += sizeof(RulesHeader);
  for (int i = 0; i < MAX_RULES; i++) {
    automations::Rule stored;
    get(addr, stored);
    if (memcmp(&automations::rules[i], &stored, sizeof(automations::Rule)) != 0) {
      dirty = true;
      put(addr, automations::rules[i]);
    }
    addr += sizeof(automations::Rule);
  }
  if (dirty) commit();
}

void deleteRule(uint8_t idx) {
  automations::deleteRule(idx);
}

}  // namespace storage
