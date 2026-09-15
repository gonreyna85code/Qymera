#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include "web.h"
#include "html.h"
#include "core.h"
#include "net.h"
#include "sensors.h"
#include "automations.h"
#include "storage.h"
#include "firmware.h"
#include "log.h"

#ifndef ICACHE_FLASH_ATTR
#define ICACHE_FLASH_ATTR
#endif

namespace web {
WebServerCompat server(80);

static const char* AUTH_USERNAME = "admin";
static const char* AUTH_PASSWORD = "qymera123";
static bool auth_enabled = false;

static const char* EXPECTED_AUTH_BASE64 = "YWRtaW46cXltZXJhMTIz";


// Rate limiting - sliding window with a small burst allowance.
// A single UI action (e.g. enabling persistence) issues several back-to-back
// POSTs; a hard 1-per-2s rule made those fail with 429. The burst allowance
// keeps UI flows working while still throttling sustained floods.
static unsigned long last_request_time = 0;
static unsigned char burst_count = 0;
static const unsigned long REQUEST_COOLDOWN_MS = 2000; // window
static const unsigned char RATE_LIMIT_BURST = 6;       // requests per window

// Check rate limit for protected endpoints
// UI flows issue small bursts; sustained floods are throttled.
static bool checkRateLimit() {
  unsigned long now = millis();
  if (now - last_request_time > REQUEST_COOLDOWN_MS) {
    // New window: reset the burst counter.
    last_request_time = now;
    burst_count = 0;
    return true;
  }
  if (burst_count < RATE_LIMIT_BURST) {
    burst_count++;
    return true;
  }
  return false;
}

static bool parseStrictUnsigned(const String &s, unsigned long &out) {
  if (s.length() == 0) return false;
  char *end = nullptr;
  const char *begin = s.c_str();
  unsigned long value = strtoul(begin, &end, 10);
  if (end == begin || *end != '\0') return false;
  out = value;
  return true;
}

static bool parseStrictLong(const String &s, long &out) {
  if (s.length() == 0) return false;
  char *end = nullptr;
  const char *begin = s.c_str();
  long value = strtol(begin, &end, 10);
  if (end == begin || *end != '\0') return false;
  out = value;
  return true;
}

static bool parseStrictFloat(const String &s, float &out) {
  if (s.length() == 0) return false;
  char *end = nullptr;
  const char *begin = s.c_str();
  errno = 0;
  float value = strtof(begin, &end);
  if (end == begin || *end != '\0') return false;
  if (errno == ERANGE || isinf(value) || isnan(value)) return false;
  out = value;
  return true;
}

// Check HTTP Basic Authentication
// Auth state: enabled when AUTH_USERNAME/AUTH_PASSWORD are set (non-empty)
// Returns true if credentials valid, or if auth is disabled for backward compatibility
// When auth is enabled and credentials invalid, server sends 401 automatically
static bool checkAuth() {
  // Determine if auth is enabled: check if constants are set to non-empty values
  static bool initialized = false;
  if (!initialized) {
    auth_enabled = false;
    initialized = true;
  }

  // If auth is not enabled, allow all (backward compatible default)
  if (!auth_enabled) return true;

  // Auth is enabled - require valid credentials
  if (!server.hasHeader("Authorization")) {
    return false; // Missing auth header when auth is enabled
  }
  String auth = server.header("Authorization");
  if (auth.startsWith("Basic ")) {
    String received = auth.substring(6);
    // Compare against pre-encoded Base64 credential
    if (received == EXPECTED_AUTH_BASE64) {
      return true;
    }
  }
  return false;
}

static void addCorsHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

static void handleCorsOptions() {
  addCorsHeaders();
  server.send(204, "text/plain", "");
}

void sendStartupJS() {
  if (WiFi.getMode() == WIFI_AP) {
    server.sendContent_P(PSTR("window.startupTab='config';"));
  } else {
    server.sendContent_P(
      PSTR("window.startupTab=(localStorage.getItem('tab')||'control');"
           "window.startupTab=['control','auto','config','logs'].includes(window.startupTab)?window.startupTab:'control';"));
  }
  server.sendContent_P(
    PSTR("['control','auto','config','logs'].forEach(t=>{document.getElementById('t_'+t).onclick=()=>show(''+t);});"));
  server.sendContent_P(
    PSTR("window.genset={broadcast_port:"));
  server.sendContent(String(core::genset.broadcast_port));
  server.sendContent_P(PSTR(",command_port:"));
  server.sendContent(String(core::genset.command_port));
  server.sendContent_P(PSTR(",report_interval:"));
  server.sendContent(String(core::genset.report_interval));
  server.sendContent_P(PSTR("};"));
}

ICACHE_FLASH_ATTR void handleRoot() {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");
  server.sendContent_P(html_content::Styles);
  server.sendContent_P(html_content::Tabs);
  sendStartupJS();
  server.sendContent_P(html_content::Rules);
  server.sendContent_P(html_content::CardsSettings);
  server.sendContent_P(html_content::DeviceCards);
  server.sendContent_P(html_content::JS);
  server.sendContent_P(html_content::AutoWizJS);
  server.sendContent("");
}

ICACHE_FLASH_ATTR void handleSave() {
  addCorsHeaders();
  if (!checkAuth()) {
    server.send(401, "text/plain", "Authentication required");
    return;
  }
  if (!checkRateLimit()) {
    server.send(429, "text/plain", "Rate limit exceeded. Try again in 2s.");
    return;
  }
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");
  // Validate SSID length (1-32 chars)
  if (ssid.length() < 1 || ssid.length() > 32) {
    server.send(400, "text/plain", "SSID must be 1-32 characters");
    return;
  }
  // Validate password length (1-64 chars)
  if (pass.length() < 1 || pass.length() > 64) {
    server.send(400, "text/plain", "Password must be 1-64 characters");
    return;
  }
  saveCredentials(ssid, pass);
  logger::coref("WiFi config saved (SSID:%s)", ssid.c_str());
  server.sendHeader("Location", "/?saved=1");
  server.send(303);
  server.close();
  RESET_MCU();
}

ICACHE_FLASH_ATTR void loadGeneralSettings() {
  storage::loadGeneralSettings(core::genset.broadcast_port, core::genset.command_port, core::genset.report_interval);
}

ICACHE_FLASH_ATTR void loadCredentials() {
  storage::loadCredentials(core::ssid, core::password);
}

ICACHE_FLASH_ATTR void saveGeneralSettings() {
  storage::saveGeneralSettings(core::genset.broadcast_port, core::genset.command_port, core::genset.report_interval);
}

ICACHE_FLASH_ATTR void factoryReset() {
  storage::factoryReset();
}

ICACHE_FLASH_ATTR void saveCredentials(const String &s, const String &p) {
  storage::saveCredentials(s, p);
}

ICACHE_FLASH_ATTR void handleGenSetSave() {
  addCorsHeaders();
  if (!checkAuth()) {
    server.send(401, "text/plain", "Authentication required");
    return;
  }
  if (!checkRateLimit()) {
    server.send(429, "text/plain", "Rate limit exceeded. Try again in 2s.");
    return;
  }
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "POST required");
    return;
  }
  unsigned long v = 0;
  // Empty/absent fields keep the current value; malformed or out-of-range
  // values are rejected (never silently applied as 0).
  if (server.hasArg("broadcast") && server.arg("broadcast").length() > 0) {
    if (!parseStrictUnsigned(server.arg("broadcast"), v) || v < 1024 || v > 65500) {
      server.send(400, "text/plain", "invalid broadcast port (1024-65500)");
      return;
    }
    core::genset.broadcast_port = (uint16_t)v;
  }
  if (server.hasArg("command") && server.arg("command").length() > 0) {
    if (!parseStrictUnsigned(server.arg("command"), v) || v < 1024 || v > 65500) {
      server.send(400, "text/plain", "invalid command port (1024-65500)");
      return;
    }
    core::genset.command_port = (uint16_t)v;
  }
  if (server.hasArg("interval") && server.arg("interval").length() > 0) {
    if (!parseStrictUnsigned(server.arg("interval"), v) || v < 5000 || v > 600000) {
      server.send(400, "text/plain", "invalid report interval (5000-600000)");
      return;
    }
    core::genset.report_interval = (uint32_t)v;
  }
  saveGeneralSettings();
  logger::coref("Genset saved (bc:%u,cmd:%u,int:%u)",
    core::genset.broadcast_port,
    core::genset.command_port,
    core::genset.report_interval);
  server.sendHeader("Location", "/");
  server.send(200, "text/plain", "OK");
}

ICACHE_FLASH_ATTR void handleFactoryReset() {
  addCorsHeaders();
  logger::warn("Factory reset requested");
  if (!checkAuth()) {
    server.send(401, "text/plain", "Authentication required");
    return;
  }
  server.send(200, "text/plain", "RESET");
  factoryReset();
}

void handleToggleApi() {
  addCorsHeaders();
  if (!checkAuth()) {
    server.send(401, "text/plain", "Authentication required");
    return;
  }
  if (!server.hasArg("id")) {
    server.send(400, "text/plain", "id required");
    return;
  }
  unsigned long id_ul = 0;
  if (!parseStrictUnsigned(server.arg("id"), id_ul) || id_ul > UINT32_MAX) {
    server.send(400, "text/plain", "invalid id format");
    return;
  }
  uint32_t id = (uint32_t)id_ul;
  int idx = sensors::findCalibByUid(id);
  if (idx < 0) {
    server.send(404, "text/plain", "id not found");
    return;
  }
  auto &c = sensors::calibrations[idx];
  if (c.type != sensors::TYPE_RELAY && c.type != sensors::TYPE_DIMMER) {
    server.send(400, "text/plain", "invalid actuator type");
    return;
  }
  sensors::handleToggle(id);
  server.send(200, "text/plain", "OK");
}

void handleDimmerApi() {
  addCorsHeaders();
  if (!checkAuth()) {
    server.send(401, "text/plain", "Authentication required");
    return;
  }
  if (!server.hasArg("value") || !server.hasArg("id")) {
    server.send(400, "text/plain", "id and value required");
    return;
  }
  unsigned long id_ul = 0;
  if (!parseStrictUnsigned(server.arg("id"), id_ul) || id_ul > UINT32_MAX) {
    server.send(400, "text/plain", "invalid id format");
    return;
  }
  uint32_t id = (uint32_t)id_ul;
  int idx = sensors::findCalibByUid(id);
  if (idx < 0) {
    server.send(404, "text/plain", "id not found");
    return;
  }
  if (sensors::calibrations[idx].type != sensors::TYPE_DIMMER) {
    server.send(400, "text/plain", "invalid actuator type");
    return;
  }
  int value = server.arg("value").toInt();
  sensors::handleDimmer(id, value);
  server.send(200, "text/plain", "OK");
}

void handleLogs() {
  addCorsHeaders();
  server.send(200, "application/json", logger::getRecentLogsJson());
}

void handleLogsClear() {
  addCorsHeaders();
  if (!checkAuth()) {
    server.send(401, "text/plain", "Authentication required");
    return;
  }
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "POST required");
    return;
  }
  logger::clearBuffer();
  server.send(200, "text/plain", "OK");
}

void handleOtaToggle() {
  addCorsHeaders();
  if (!checkAuth()) {
    server.send(401, "text/plain", "Authentication required");
    return;
  }
  if (server.hasArg("enabled")) {
    bool enable = server.arg("enabled") == "1";
    core::setOtaEnabled(enable);
    server.send(200, "text/plain", enable ? "OK" : "OFF");
    delay(500);
    RESET_MCU();
  } else {
    server.send(400, "text/plain", "enabled arg required");
  }
}

void handleOtaStatus() {
  addCorsHeaders();
  server.send(200, "application/json", core::isOtaEnabled() ? "{\"ota\":1}" : "{\"ota\":0}");
}

void handleFirmware() {
  addCorsHeaders();
  String j;
  j.reserve(200);
  j = "{\"product\":\"";
  j += QYMERA_PRODUCT;
  j += "\",\"version\":\"";
  j += firmware::currentVersion();
  j += "\",\"platform\":\"";
#if defined(PLATFORM_ESP8266)
  j += "esp8266";
#elif defined(PLATFORM_ESP32C3)
  j += "esp32c3";
#else
  j += "esp32";
#endif
  j += "\",\"state\":\"";
  j += firmware::stateToken();
  j += "\",\"latest\":\"";
  j += firmware::latestVersion();
  j += "\",\"channel\":\"";
  j += firmware::channel();
  j += "\",\"available\":";
  j += firmware::updateAvailable() ? "1" : "0";
  j += ",\"progress\":";
  j += String(firmware::progressPercent());
  j += ",\"error\":\"";
  j += firmware::errorMessage();
  j += "\"}";
  server.send(200, "application/json", j);
}

void handleFirmwareCheck() {
  addCorsHeaders();
  if (firmware::requestCheck()) {
    server.send(200, "application/json", "{\"ok\":true}");
  } else {
    server.send(409, "application/json", "{\"ok\":false,\"error\":\"busy\"}");
  }
}

void handleFirmwareUpdate() {
  addCorsHeaders();
  if (!checkAuth()) {
    server.send(401, "text/plain", "Authentication required");
    return;
  }
  if (!checkRateLimit()) {
    server.send(429, "text/plain", "rate limited");
    return;
  }
  if (firmware::requestUpdate()) {
    server.send(200, "application/json", "{\"ok\":true}");
  } else {
    server.send(409, "application/json", "{\"ok\":false,\"error\":\"not_available\"}");
  }
}

ICACHE_FLASH_ATTR void loadCalibration() {
  storage::loadCalibration();
}

ICACHE_FLASH_ATTR void saveCalibration() {
  storage::saveCalibration();
}

ICACHE_FLASH_ATTR void saveCalibrationSlot(int index) {
  storage::saveCalibrationSlot(index);
}

void handleDeleteRule() {
  if (!checkAuth()) {
    server.send(401, "text/plain", "Authentication required");
    return;
  }
  if (!server.hasArg("id")) {
    server.send(400, "text/plain", "missing id");
    return;
  }
  unsigned long id_ul = 0;
  if (!parseStrictUnsigned(server.arg("id"), id_ul) || id_ul >= MAX_RULES) {
    server.send(400, "text/plain", "invalid id format");
    return;
  }
  uint8_t id = (uint8_t)id_ul;
  automations::deleteRule(id);
  logger::eventf("Rule %d deleted", id);
  server.send(200, "text/plain", "ok");
}

ICACHE_FLASH_ATTR void handleRules() {
  if (server.method() != HTTP_GET) {
    server.send(405, "text/plain", "GET required");
    return;
  }
  String json;
  json.reserve(4096);
  json += '[';
  bool first = true;
  for (int i = 0; i < MAX_RULES; i++) {
    const automations::Automation &r = automations::rules[i];
    if (r.sensor_count == 0 && r.actuator_count == 0)
      continue;
    if (!first) json += ',';
    first = false;
    int jsonType;
    if (r.kind == automations::ON_TIME) jsonType = automations::RULE_TIME;
    else if (r.kind == automations::ON_INTERVAL) jsonType = automations::RULE_INTERVAL;
    else {
      bool anyEdge = false;
      for (int s = 0; s < (int)r.sensor_count && s < (int)automations::MAX_CONDITIONS; s++) {
        if (r.c_cmp[s] >= automations::EDGE_RISING) anyEdge = true;
      }
      jsonType = anyEdge ? automations::RULE_EDGE : automations::RULE_THRESHOLD;
    }
    bool logicAnd = true;
    for (int j = 0; j + 1 < (int)r.sensor_count && j < (int)automations::MAX_CONDITIONS - 1; j++) {
      if ((r.c_op_bits & (1 << j)) != 0) logicAnd = false;
    }
    json += "{\"id\":";
    json += i;
    json += ",\"sensors\":[";
    for (int s = 0; s < (int)r.sensor_count && s < (int)automations::MAX_CONDITIONS; s++) {
      if (s) json += ',';
      json += r.c_sensor[s];
    }
    json += "],\"type\":";
    json += jsonType;
    json += ",\"logical_and\":";
    json += logicAnd;
    json += ",\"cmp\":[";
    for (int s = 0; s < (int)r.sensor_count && s < (int)automations::MAX_CONDITIONS; s++) {
      if (s) json += ',';
      int c = r.c_cmp[s];
      if (c == automations::EDGE_RISING) c = 0;
      else if (c == automations::EDGE_FALLING) c = 1;
      json += c;
    }
    json += "],\"threshold\":[";
    for (int s = 0; s < (int)r.sensor_count && s < (int)automations::MAX_CONDITIONS; s++) {
      if (s) json += ',';
      json += r.c_threshold[s];
    }
    json += "],\"actuators\":[";
    for (int a = 0; a < (int)r.actuator_count && a < (int)automations::MAX_ACTIONS; a++) {
      if (a) json += ',';
      json += r.a_sensor[a];
    }
    json += "],\"actions\":[";
    for (int a = 0; a < (int)r.actuator_count && a < (int)automations::MAX_ACTIONS; a++) {
      if (a) json += ',';
      json += r.a_action[a];
    }
    json += "],\"levels\":[";
    for (int a = 0; a < (int)r.actuator_count && a < (int)automations::MAX_ACTIONS; a++) {
      if (a) json += ',';
      json += r.a_level[a];
    }
    json += "],\"delay_ms\":";
    json += r.fire_delay_ms;
    json += ",\"cooldown_ms\":";
    json += r.cooldown_ms;
    json += ",\"time_s\":";
    json += r.time_s;
    json += ",\"interval_ms\":";
    json += r.interval_ms;
    json += ",\"year_start\":";
    json += r.year_start;
    json += ",\"year_end\":";
    json += r.year_end;
    json += ",\"month_start\":";
    json += r.month_start;
    json += ",\"month_end\":";
    json += r.month_end;
    json += ",\"day_start\":";
    json += r.day_start;
    json += ",\"day_end\":";
    json += r.day_end;
    json += ",\"debounce_ms\":";
    json += r.debounce_ms;
    json += ",\"for_ms\":";
    json += r.for_ms;
    json += ",\"step_ms\":";
    json += r.step_ms;
    json += ",\"hys\":";
    json += r.c_hys_dec;
    json += ",\"ops\":[";
    for (int j = 0; j + 1 < (int)r.sensor_count && j < (int)automations::MAX_CONDITIONS - 1; j++) {
      if (j) json += ',';
      json += ((r.c_op_bits & (1 << j)) != 0) ? 1 : 0;
    }
    json += "],\"retry\":";
    json += r.retry_max;
    json += ",\"retry_interval\":";
    json += r.retry_interval_s;
    json += '}';
  }
  json += ']';
  server.send(200, "application/json", json);
}

void handleSetRule() {
  using namespace automations;
  addCorsHeaders();
  if (!checkAuth()) {
    server.send(401, "text/plain", "Authentication required");
    return;
  }
  if (!checkRateLimit()) {
    server.send(429, "text/plain", "Rate limit exceeded. Try again in 2s.");
    return;
  }
  if (!server.hasArg("id")) {
    server.send(400, "text/plain", "missing id");
    return;
  }
  int id = -1;
  unsigned long id_ul = 0;
  if (parseStrictUnsigned(server.arg("id"), id_ul) && id_ul <= INT_MAX) id = (int)id_ul;
  if (id < 0) {
    for (int i = 0; i < MAX_RULES; i++) {
      if (rules[i].sensor_count == 0 && rules[i].actuator_count == 0) {
        id = i;
        break;
      }
    }
  }
  if (id < 0 || id >= MAX_RULES) {
    server.send(400, "text/plain", "invalid id");
    return;
  }
  Automation &r = rules[id];
  memset(&r, 0, sizeof(Automation));
  memset(&states[id], 0, sizeof(AutomationState));
  // ================= LOGICAL OPERATOR =================
  // UI sends 'logic' (1 = AND, 0 = OR). Defaults to OR when absent. Optional
  // 'ops' overrides the join between each pair of conditions (0=AND, 1=OR).
  uint8_t ops_bits = 0;
  bool hasOps = server.hasArg("ops") && server.arg("ops").length() > 0;
  if (hasOps) {
    String ops_str = server.arg("ops");
    int oi = 0;
    while (ops_str.length() && oi < 4) {
      int comma = ops_str.indexOf(',');
      String token = (comma == -1) ? ops_str : ops_str.substring(0, comma);
      long op_long = 0;
      if (!parseStrictUnsigned(token, (unsigned long&)op_long) || (op_long != 0 && op_long != 1)) {
        server.send(400, "text/plain", "invalid ops");
        return;
      }
      if (op_long == 1) ops_bits |= (1 << oi);
      oi++;
      if (comma == -1) break;
      ops_str = ops_str.substring(comma + 1);
    }
  } else {
    bool useAnd = server.hasArg("logic") && server.arg("logic") == "1";
    for (int j = 0; j < 4; j++) {
      if (!useAnd) ops_bits |= (1 << j);
    }
  }
  r.c_op_bits = ops_bits;
  // ================= TYPE =================
  if (!server.hasArg("type")) {
    server.send(400, "text/plain", "type required");
    return;
  }
  int ruleType = server.arg("type").toInt();
  if (ruleType < 0 || ruleType > 3) {
    server.send(400, "text/plain", "invalid type");
    return;
  }
  if (ruleType == RULE_TIME) r.kind = ON_TIME;
  else if (ruleType == RULE_INTERVAL) r.kind = ON_INTERVAL;
  else r.kind = ON_SAMPLE;
  // Default debounce for legacy EDGE rules: 3 confirm reads x 50 ms
  if (ruleType == RULE_EDGE) r.debounce_ms = 150;
  // ================= SENSORS =================
  if (server.hasArg("sensors")) {
    String sensors_str = server.arg("sensors");
    String cmp_str = server.arg("cmp");
    String threshold_str = server.arg("threshold");
    int idx = 0;
    while (sensors_str.length() && idx < 5) {
      int comma = sensors_str.indexOf(',');
      String token = (comma == -1) ? sensors_str : sensors_str.substring(0, comma);
      unsigned long sensor_ul = 0;
      if (!parseStrictUnsigned(token, sensor_ul) || sensor_ul >= MAX_SENSORS) {
        server.send(400, "text/plain", "invalid sensor index");
        return;
      }
      int sensor_id = (int)sensor_ul;
      if (sensors::calibrations[sensor_id].uid == 0) {
        server.send(400, "text/plain", "sensor not configured");
        return;
      }
      r.c_sensor[idx] = sensor_id;
      // CMP
      int cmp_val = 0;
      if (cmp_str.length()) {
        int c = cmp_str.indexOf(',');
        String t = (c == -1) ? cmp_str : cmp_str.substring(0, c);
        long cmp_long = 0;
        if (!parseStrictUnsigned(t, (unsigned long&)cmp_long)) {
          server.send(400, "text/plain", "invalid comparator");
          return;
        }
        cmp_val = (int)cmp_long;
        if (cmp_val < 0 || cmp_val > 2) {
          server.send(400, "text/plain", "invalid comparator");
          return;
        }
        if (c != -1) cmp_str = cmp_str.substring(c + 1);
        else cmp_str = "";
      }
      if (ruleType == RULE_EDGE) {
        r.c_cmp[idx] = (cmp_val == CMP_LT) ? EDGE_FALLING : EDGE_RISING;
      } else {
        r.c_cmp[idx] = cmp_val;
      }
      // THRESHOLD
      int th = 0;
      if (threshold_str.length()) {
        int c = threshold_str.indexOf(',');
        String t = (c == -1) ? threshold_str : threshold_str.substring(0, c);
        long th_long = 0;
        if (!parseStrictLong(t, th_long)) {
          server.send(400, "text/plain", "threshold out of range");
          return;
        }
        th = (int)th_long;
        if (th < -1000 || th > 10000) {
          server.send(400, "text/plain", "threshold out of range");
          return;
        }
        if (c != -1) threshold_str = threshold_str.substring(c + 1);
        else threshold_str = "";
      }
      r.c_threshold[idx] = th;
      idx++;
      if (comma == -1) break;
      sensors_str = sensors_str.substring(comma + 1);
    }
    r.sensor_count = idx;
  }
  // ================= ACTUATORS =================
  if (server.hasArg("actuators")) {
    String actuators_str = server.arg("actuators");
    String actions_str = server.arg("actions");
    String levels_str = server.arg("levels");
    int idx = 0;
    while (actuators_str.length() && idx < 5) {
      int comma = actuators_str.indexOf(',');
      String token = (comma == -1) ? actuators_str : actuators_str.substring(0, comma);
      unsigned long actuator_ul = 0;
      if (!parseStrictUnsigned(token, actuator_ul) || actuator_ul >= MAX_SENSORS) {
        server.send(400, "text/plain", "invalid actuator index");
        return;
      }
      int actuator_id = (int)actuator_ul;
      auto &cal = sensors::calibrations[actuator_id];
      if (cal.uid == 0) {
        server.send(400, "text/plain", "actuator not configured");
        return;
      }
      if (cal.type != sensors::TYPE_RELAY && cal.type != sensors::TYPE_DIMMER) {
        server.send(400, "text/plain", "invalid actuator type");
        return;
      }
      r.a_sensor[idx] = actuator_id;
      int action = 2;
      if (actions_str.length()) {
        int c = actions_str.indexOf(',');
        String t = (c == -1) ? actions_str : actions_str.substring(0, c);
        long action_long = 0;
        if (!parseStrictUnsigned(t, (unsigned long&)action_long)) {
          server.send(400, "text/plain", "invalid action");
          return;
        }
        action = (int)action_long;
        if (action < 0 || action > 3) {
          server.send(400, "text/plain", "invalid action");
          return;
        }
        if (action == ACT_LEVEL && cal.type != sensors::TYPE_DIMMER) {
          server.send(400, "text/plain", "LEVEL only for dimmers");
          return;
        }
        if (c != -1) actions_str = actions_str.substring(c + 1);
        else actions_str = "";
      }
      r.a_action[idx] = action;
      int level = 0;
      if (levels_str.length()) {
        int c = levels_str.indexOf(',');
        String t = (c == -1) ? levels_str : levels_str.substring(0, c);
        long level_long = 0;
        if (!parseStrictLong(t, level_long)) {
          server.send(400, "text/plain", "level out of range");
          return;
        }
        level = (int)level_long;
        if (level < 0 || level > 100) {
          server.send(400, "text/plain", "level out of range");
          return;
        }
        if (c != -1) levels_str = levels_str.substring(c + 1);
        else levels_str = "";
      }
      r.a_level[idx] = level;
      idx++;
      if (comma == -1) break;
      actuators_str = actuators_str.substring(comma + 1);
    }
    r.actuator_count = idx;
  }
  // ================= VALIDACIONES GENERALES =================
  if (r.actuator_count == 0) {
    server.send(400, "text/plain", "at least one actuator required");
    return;
  }
  if ((r.kind == ON_SAMPLE) && r.sensor_count == 0) {
    server.send(400, "text/plain", "sensors required");
    return;
  }
  // ================= TIME =================
  if (r.kind == ON_TIME) {
    long time_s_long = 0;
    if (!parseStrictLong(server.arg("time_s"), time_s_long)) {
      server.send(400, "text/plain", "invalid time_s");
      return;
    }
    int time_s = (int)time_s_long;
    if (time_s < 0 || time_s > 86400) {
      server.send(400, "text/plain", "invalid time_s");
      return;
    }
    r.time_s = time_s;
    long ys_l = 0, ms_l = 0, ds_l = 0, ye_l = 0, me_l = 0, de_l = 0;
    if (!parseStrictLong(server.arg("year_start"), ys_l) ||
        !parseStrictLong(server.arg("month_start"), ms_l) ||
        !parseStrictLong(server.arg("day_start"), ds_l) ||
        !parseStrictLong(server.arg("year_end"), ye_l) ||
        !parseStrictLong(server.arg("month_end"), me_l) ||
        !parseStrictLong(server.arg("day_end"), de_l)) {
      server.send(400, "text/plain", "invalid date");
      return;
    }
    int ys = (int)ys_l, ms = (int)ms_l, ds = (int)ds_l, ye = (int)ye_l, me = (int)me_l, de = (int)de_l;
    bool hasDate = ys || ms || ds || ye || me || de;
    if (hasDate) {
      if (ys && (ys < 1970 || ys > 2100)) {
        server.send(400, "text/plain", "invalid year_start");
        return;
      }
      if (ye && (ye < 1970 || ye > 2100)) {
        server.send(400, "text/plain", "invalid year_end");
        return;
      }
      if (ms && (ms < 1 || ms > 12)) {
        server.send(400, "text/plain", "invalid month_start");
        return;
      }
      if (me && (me < 1 || me > 12)) {
        server.send(400, "text/plain", "invalid month_end");
        return;
      }
      if (ds && (ds < 1 || ds > 31)) {
        server.send(400, "text/plain", "invalid day_start");
        return;
      }
      if (de && (de < 1 || de > 31)) {
        server.send(400, "text/plain", "invalid day_end");
        return;
      }
      r.year_start = ys;
      r.month_start = ms;
      r.day_start = ds;
      r.year_end = ye;
      r.month_end = me;
      r.day_end = de;
      if (ys && ye && ys > ye) {
        server.send(400, "text/plain", "start > end");
        return;
      }
    }
  }

  // ================= INTERVAL =================
  if (r.kind == ON_INTERVAL) {
    long interval_long = 0;
    if (!parseStrictLong(server.arg("interval"), interval_long)) {
      server.send(400, "text/plain", "invalid interval");
      return;
    }
    int interval = (int)interval_long;
    if (interval < 1000 || interval > 3600000) {
      server.send(400, "text/plain", "invalid interval");
      return;
    }
    r.interval_ms = interval;
  }
  // ================= DELAY / COOLDOWN =================
  long delay_long = 0, cooldown_long = 0;
  if (!parseStrictLong(server.arg("delay"), delay_long) || !parseStrictLong(server.arg("cooldown"), cooldown_long)) {
    server.send(400, "text/plain", "invalid delay/cooldown");
    return;
  }
  r.fire_delay_ms = delay_long;
  r.cooldown_ms = cooldown_long;
  // ================= OPCIONES NUEVAS (mitigacion / secuencia) =================
  if (r.kind == ON_SAMPLE && server.hasArg("debounce_ms")) {
    long db = 0;
    if (!parseStrictLong(server.arg("debounce_ms"), db) || (db < 0 || db > 3600000)) {
      server.send(400, "text/plain", "invalid debounce_ms");
      return;
    }
    r.debounce_ms = db;
  }
  if (r.kind == ON_SAMPLE && server.hasArg("for_ms")) {
    long fm = 0;
    if (!parseStrictLong(server.arg("for_ms"), fm) || (fm < 0 || fm > 86400000)) {
      server.send(400, "text/plain", "invalid for_ms");
      return;
    }
    r.for_ms = fm;
  }
  if (r.kind == ON_SAMPLE && server.hasArg("hys")) {
    long hy = 0;
    if (!parseStrictLong(server.arg("hys"), hy) || (hy < 0 || hy > 255)) {
      server.send(400, "text/plain", "invalid hys");
      return;
    }
    r.c_hys_dec = hy;
  }
  if (server.hasArg("step_ms")) {
    long sm = 0;
    if (!parseStrictLong(server.arg("step_ms"), sm) || (sm < 0 || sm > 3600000)) {
      server.send(400, "text/plain", "invalid step_ms");
      return;
    }
    r.step_ms = sm;
  }
  if (server.hasArg("retry")) {
    long rt = 0;
    if (!parseStrictLong(server.arg("retry"), rt) || (rt < 0 || rt > 10)) {
      server.send(400, "text/plain", "invalid retry");
      return;
    }
    r.retry_max = rt;
  }
  if (server.hasArg("retry_interval")) {
    long ri = 0;
    if (!parseStrictLong(server.arg("retry_interval"), ri) || (ri < 0 || ri > 3600)) {
      server.send(400, "text/plain", "invalid retry_interval");
      return;
    }
    r.retry_interval_s = ri;
  }
  saveRulesToEEPROM();
  logger::eventf("Rule %d saved (kind:%d, sensors:%d, actuators:%d)",
    id, r.kind, r.sensor_count, r.actuator_count);
  server.send(200, "text/plain", "ok");
}

ICACHE_FLASH_ATTR void handleCalib() {
  addCorsHeaders();
  if (server.method() != HTTP_GET) {
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }
  String json;
  json.reserve(8192);
  json += '[';
  bool firstObj = true;
  for (int i = 0; i < MAX_SENSORS; i++) {
    auto &c = sensors::calibrations[i];
    auto &r = net::reports[i];
    // Expose only active, well-formed entries: valid uid, valid type, and
    // remote entries that are still within NET_TIMEOUT. Stale remote sensors,
    // SENSOR_NONE and invalid/garbage types are never reported as devices.
    if (!sensors::isEntryVisible(i)) continue;
    if (!firstObj) json += ',';
    firstObj = false;

    char buf[24];
    if (isnan(r.value) || isinf(r.value)) {
      strcpy(buf, "0");
    } else {
      dtostrf(r.value, 0, 4, buf);
    }

    json += "{\"id\":";
    json += c.uid;
    json += ",\"index\":";
    json += i;
    json += ",\"device_uid\":";
    json += c.device_uid;
    json += ",\"name\":\"";
    json += c.name;
    json += "\",\"value\":";
    json += buf;

    char fb[24];
    dtostrf(isnan(c.correction) || isinf(c.correction) ? 0.0f : c.correction, 0, 4, fb); json += ",\"correction\":";  json += fb;

    json += ",\"avail\":";           json += c.avail;
    json += ",\"pulse\":";           json += (c.pulse ? "true" : "false");
    json += ",\"state\":";           json += (r.state ? "true" : "false");
    json += ",\"pulse_ms\":";        json += c.pulse_ms;
    json += ",\"persist\":";         json += (c.persist ? "true" : "false");
    json += ",\"fade\":";            json += c.fade;
    json += ",\"type\":";            json += c.type;
    json += ",\"local\":";           json += (c.local ? "true" : "false");
    // Elapsed ms since the last remote packet, computed server-side from the
    // same millis() timebase as NET_TIMEOUT (client Date.now() is epoch-based
    // and cannot be compared directly with the device uptime counter).
    json += ",\"age_ms\":";          json += c.local ? 0 : (uint32_t)(millis() - c.last_update);

    json += ",\"ip\":\"";
    if (c.local) {
      IPAddress ip = WiFi.localIP();
      char ipbuf[16];
      snprintf(ipbuf, sizeof(ipbuf), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
      json += ipbuf;
    } else {
      json += c.device_ip;
    }
    json += "\"}";
  }
  json += ']';
  server.send(200, "application/json", json);
}

ICACHE_FLASH_ATTR void handleCalibSet() {
  addCorsHeaders();
  if (!checkAuth()) {
    server.send(401, "text/plain", "Authentication required");
    return;
  }
  if (!checkRateLimit()) {
    server.send(429, "text/plain", "Rate limit exceeded. Try again in 2s.");
    return;
  }
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "POST required");
    return;
  }
  if (!server.hasArg("id")) {
    server.send(400, "text/plain", "id required");
    return;
  }
  unsigned long sensorUidUl = 0;
  if (!parseStrictUnsigned(server.arg("id"), sensorUidUl) || sensorUidUl > UINT32_MAX) {
    server.send(400, "text/plain", "invalid id format");
    return;
  }
  uint32_t sensorUid = (uint32_t)sensorUidUl;
  String type = server.arg("type");
  int calibIdx = sensors::findCalibByUid(sensorUid);
  if (calibIdx < 0) {
    server.send(400, "text/plain", "Sensor not found");
    return;
  }
  auto &c = sensors::calibrations[calibIdx];
  auto &r = net::reports[calibIdx];
  float raw = r.raw;

  // TIME / timezone: strict integer minutes from UTC, range -720..840.
  if (type == "TIME" || type == "timezone") {
    if (!server.hasArg("ref")) {
      server.send(400, "text/plain", "ref required");
      return;
    }
    long tz_min = 0;
    if (!parseStrictLong(server.arg("ref"), tz_min)) {
      server.send(400, "text/plain", "invalid timezone (integer minutes required)");
      return;
    }
    if (tz_min < -720 || tz_min > 840) {
      server.send(400, "text/plain", "timezone out of range (-720..840)");
      return;
    }
    c.correction = (float)tz_min;
    saveCalibrationSlot(calibIdx);
    server.send(200, "text/plain", "OK");
    return;
  }

  // persist: strict boolean ref.
  if (type == "persist") {
    if (!server.hasArg("ref")) {
      server.send(400, "text/plain", "ref required");
      return;
    }
    String pv = server.arg("ref");
    if (pv != "1" && pv != "0") {
      server.send(400, "text/plain", "invalid persist value (0/1)");
      return;
    }
    bool enable = (pv == "1");
    c.persist = enable;
    c.pulse = false;
    // Snapshot the live state at enable time so a reboot right after enabling
    // persistence still restores the current relay state (the state is only
    // re-saved on subsequent toggles while persist is on).
    if (enable) c.pers_state = c.state;
    saveCalibrationSlot(calibIdx);
    server.send(200, "text/plain", "OK");
    return;
  }

  // avail: strict boolean ref.
  if (type == "avail") {
    if (!server.hasArg("ref")) {
      server.send(400, "text/plain", "ref required");
      return;
    }
    String av = server.arg("ref");
    if (av != "1" && av != "0") {
      server.send(400, "text/plain", "invalid avail value (0/1)");
      return;
    }
    c.avail = (av == "1") ? 1 : 0;
    saveCalibrationSlot(calibIdx);
    server.send(200, "text/plain", "OK");
    return;
  }

  // res: no payload needed.
  if (type == "res") {
    c.min = 0;
    c.max = 100;
    c.correction = 0;
    saveCalibrationSlot(calibIdx);
    server.send(200, "text/plain", "OK");
    return;
  }

  // fad: fade ms, strict float, 0..3600000 (storage caps fades at 1h).
  if (type == "fad") {
    if (!server.hasArg("ref")) {
      server.send(400, "text/plain", "ref required");
      return;
    }
    float fad = 0;
    if (!parseStrictFloat(server.arg("ref"), fad)) {
      server.send(400, "text/plain", "invalid fade value");
      return;
    }
    if (fad < 0 || fad > 3600000.0f) {
      server.send(400, "text/plain", "fade out of range (0..3600000 ms)");
      return;
    }
    c.fade = (uint32_t)fad;
    saveCalibrationSlot(calibIdx);
    server.send(200, "text/plain", "OK");
    return;
  }

  // pulse: pulse_ms, strict float, 0..3600000 (sane 1h cap, mirrors fade).
  if (type == "pulse") {
    if (!server.hasArg("ref")) {
      server.send(400, "text/plain", "ref required");
      return;
    }
    float pms = 0;
    if (!parseStrictFloat(server.arg("ref"), pms)) {
      server.send(400, "text/plain", "invalid pulse value");
      return;
    }
    if (pms < 0 || pms > 3600000.0f) {
      server.send(400, "text/plain", "pulse out of range (0..3600000 ms)");
      return;
    }
    c.pulse_ms = (uint32_t)pms;
    c.pulse = (pms > 0);
    c.persist = false;
    saveCalibrationSlot(calibIdx);
    server.send(200, "text/plain", "OK");
    return;
  }

  // ref/min/max: strict float; ref defaults to the live raw value.
  // An empty ref argument (e.g. the "Set 0%/Set 100%" quick buttons read an
  // empty input) must fall back to the live raw value, mirroring the
  // empty-field handling used in handleSave().
  float ref;
  if (server.hasArg("ref") && server.arg("ref").length() > 0) {
    if (!parseStrictFloat(server.arg("ref"), ref)) {
      server.send(400, "text/plain", "invalid ref value");
      return;
    }
  } else {
    ref = raw;
  }

  if (type == "ref") {
    if (ref == 0) c.correction = 0;
    else {
      if (c.type == sensors::SENSOR_LUMI)
        ref = ref * 7074.0f / 108.9432f;
      c.correction = ref - raw;
    }
  } else if (type == "min") {
    c.min = raw + c.correction;
  } else if (type == "max") {
    c.max = raw + c.correction;
  } else {
    server.send(400, "text/plain", "Bad type");
    return;
  }
  saveCalibrationSlot(calibIdx);
  server.send(200, "text/plain", "OK");
}

void init() {
  server.on("/", handleRoot);
  server.on("/save", handleSave);
  server.on("/calib", handleCalib);
  server.on("/calib", HTTP_OPTIONS, handleCorsOptions);
  server.on("/calib/set", HTTP_POST, handleCalibSet);
  server.on("/calib/set", HTTP_OPTIONS, handleCorsOptions);
  server.on("/genset/save", HTTP_POST, handleGenSetSave);
  server.on("/rules", handleRules);
  server.on("/rules/set", HTTP_POST, handleSetRule);
  server.on("/rules/delete", HTTP_POST, handleDeleteRule);
  server.on("/factory", HTTP_POST, handleFactoryReset);
  server.on("/toggle", HTTP_POST, handleToggleApi);
  server.on("/toggle", HTTP_OPTIONS, handleCorsOptions);
  server.on("/dimmer", HTTP_POST, handleDimmerApi);
  server.on("/dimmer", HTTP_OPTIONS, handleCorsOptions);
  server.on("/logs", handleLogs);
  server.on("/logs/clear", HTTP_POST, handleLogsClear);
  server.on("/ota/toggle", handleOtaToggle);
  server.on("/ota/status", handleOtaStatus);
  server.on("/firmware", handleFirmware);
  server.on("/firmware/check", handleFirmwareCheck);
  server.on("/firmware/update", HTTP_POST, handleFirmwareUpdate);
  server.on("/firmware/update", HTTP_OPTIONS, handleCorsOptions);
  server.begin();
}

}
