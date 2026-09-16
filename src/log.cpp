#include "log.h"
#include "net.h"
#include <stdio.h>
#include <stdarg.h>

static bool serial_enabled = true;

namespace logger {

// ================= STATE =================


static bool layer_enabled[3] = {true, true, true};
static Level min_level = INFO;

struct LogEntry {
  Level level;
  uint32_t timestamp;
  char message[MAX_LOG_MSG];
};

static LogEntry buffers[3][LOG_BUFFER_SIZE];
static uint8_t buffer_head[3] = {0, 0, 0};
static uint8_t buffer_count[3] = {0, 0, 0};

// ================= LAYER / LEVEL NAMES =================

static const char *layer_name(Layer l) {
  switch (l) {
    case CORE:    return "CORE";
    case SENSORS: return "SENS";
    case EVENTS:  return "EVNT";
    default:      return "???";
  }
}

static const char *level_name(Level lv) {
  switch (lv) {
    case INFO:  return "INF";
    case WARN:  return "WRN";
    case ERROR: return "ERR";
    default:    return "???";
  }
}

// ================= INIT =================

void init() {
  serial_enabled = true;
  layer_enabled[CORE] = true;
  layer_enabled[SENSORS] = true;
  layer_enabled[EVENTS] = true;
  min_level = INFO;
  memset(buffer_head, 0, sizeof(buffer_head));
  memset(buffer_count, 0, sizeof(buffer_count));
}

// ================= LAYER FILTER =================

void setLayerEnabled(Layer layer, bool enabled) {
  if (layer <= EVENTS) {
    layer_enabled[layer] = enabled;
  }
}

bool isLayerEnabled(Layer layer) {
  return layer <= EVENTS ? layer_enabled[layer] : false;
}

// ================= LEVEL FILTER =================

void setMinLevel(Level level) {
  min_level = level;
}

Level getMinLevel() {
  return min_level;
}

// ================= OUTPUT =================

static void store_to_buffer(Layer layer, Level level, const char *msg) {
  if (layer <= EVENTS) {
    LogEntry &entry = buffers[layer][buffer_head[layer]];
    entry.level = level;
    entry.timestamp = millis();
    strncpy(entry.message, msg, MAX_LOG_MSG - 1);
    entry.message[MAX_LOG_MSG - 1] = '\0';
    buffer_head[layer] = (buffer_head[layer] + 1) % LOG_BUFFER_SIZE;
    if (buffer_count[layer] < LOG_BUFFER_SIZE) buffer_count[layer]++;
  }
}

static void output(Layer layer, Level level, const char *msg, bool to_gui) {
  // Filter
  if (level < min_level) return;
  if (!layer_enabled[layer]) return;

  // --- Serial ---
  if (serial_enabled) {
    Serial.printf("[%lu][%s][%s] %s\n", millis(), layer_name(layer), level_name(level), msg);
  }

  // --- GUI buffer + UDP broadcast (skipped for serial-only diagnostics) ---
  if (to_gui) {
    store_to_buffer(layer, level, msg);
    net::sendLog(layer, level, msg);
  }
}

void logRemote(Layer layer, Level level, const char *msg) {
  // Remote net logs: same filters/output as local logs, but WITHOUT the UDP
  // broadcast (net::sendLog). Re-broadcasting received logs would create an
  // unbounded broadcast ping-pong between devices.
  if (level < min_level) return;
  if (!layer_enabled[layer]) return;

  if (serial_enabled) {
    Serial.printf("[%lu][%s][%s] %s\n", millis(), layer_name(layer), level_name(level), msg);
  }

  store_to_buffer(layer, level, msg);
}

// ================= LOG FUNCTIONS =================

void log(Layer layer, Level level, const char *msg) {
  output(layer, level, msg, true);
}

void log(Layer layer, Level level, const String &msg) {
  output(layer, level, msg.c_str(), true);
}

void logf(Layer layer, Level level, const char *fmt, ...) {
  char buf[MAX_LOG_MSG];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  output(layer, level, buf, true);
}

void serial(Layer layer, Level level, const char *msg) {
  output(layer, level, msg, false);
}

void serialf(Layer layer, Level level, const char *fmt, ...) {
  char buf[MAX_LOG_MSG];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  output(layer, level, buf, false);
}

// ================= CONVENIENCE =================

void core(const char *msg)    { output(CORE, INFO, msg, true); }
void core(const String &msg)  { output(CORE, INFO, msg.c_str(), true); }
void sensors(const char *msg) { output(SENSORS, INFO, msg, true); }
void sensors(const String &msg){ output(SENSORS, INFO, msg.c_str(), true); }
void event(const char *msg)   { output(EVENTS, INFO, msg, true); }
void event(const String &msg) { output(EVENTS, INFO, msg.c_str(), true); }

void coref(const char *fmt, ...) {
  char buf[MAX_LOG_MSG];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  output(CORE, INFO, buf, true);
}

void sensorsf(const char *fmt, ...) {
  char buf[MAX_LOG_MSG];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  output(SENSORS, INFO, buf, true);
}

void eventf(const char *fmt, ...) {
  char buf[MAX_LOG_MSG];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  output(EVENTS, INFO, buf, true);
}

void warn(const char *msg)    { output(CORE, WARN, msg, true); }
void warn(const String &msg)  { output(CORE, WARN, msg.c_str(), true); }
void error(const char *msg)   { output(CORE, ERROR, msg, true); }
void error(const String &msg) { output(CORE, ERROR, msg.c_str(), true); }

void warnf(const char *fmt, ...) {
  char buf[MAX_LOG_MSG];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  output(CORE, WARN, buf, true);
}

void errorf(const char *fmt, ...) {
  char buf[MAX_LOG_MSG];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  output(CORE, ERROR, buf, true);
}

// ================= GUI ACCESS =================

String getRecentLogsJson() {
  String json = "[";
  bool first = true;

  for (uint8_t layer = 0; layer < 3; layer++) {
    uint8_t start = buffer_count[layer] < LOG_BUFFER_SIZE
      ? 0
      : buffer_head[layer];

    for (uint8_t i = 0; i < buffer_count[layer]; i++) {
      uint8_t idx = (start + i) % LOG_BUFFER_SIZE;
      const LogEntry &e = buffers[layer][idx];
      if (!first) json += ",";
      first = false;
      json += "{\"t\":";
      json += e.timestamp;
      json += ",\"l\":\"";
      json += layer_name((Layer)layer);
      json += "\",\"v\":\"";
      json += level_name(e.level);
      json += "\",\"m\":\"";
      for (const char *p = e.message; *p; p++) {
        if (*p == '"') json += "\\\"";
        else if (*p == '\\') json += "\\\\";
        else if (*p == '\n') json += "\\n";
        else json += *p;
      }
      json += "\"}";
    }
  }
  json += "]";
  return json;
}

void clearBuffer() {
  memset(buffer_head, 0, sizeof(buffer_head));
  memset(buffer_count, 0, sizeof(buffer_count));
}

}  // namespace logger


// ================= SERIAL CONTROL =================

namespace Qymera {
void setSerialEnabled(bool enabled) {
  serial_enabled = enabled;
}

bool isSerialEnabled() {
  return serial_enabled;
}
}  // namespace Qymera

