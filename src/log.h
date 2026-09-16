#pragma once
#include <Arduino.h>
#include "config.h"

namespace logger {

// ================= LAYERS =================
enum Layer : uint8_t {
  CORE    = 0,
  SENSORS = 1,
  EVENTS  = 2
};

// ================= LEVELS =================
enum Level : uint8_t {
  INFO  = 0,
  WARN  = 1,
  ERROR = 2
};

// ================= CONFIG =================
static const uint8_t MAX_LOG_MSG = 64;
static const uint8_t LOG_BUFFER_SIZE = 30;

// ================= INIT =================
void init();

// ================= SERIAL CONTROL =================


// ================= LAYER FILTER =================
void setLayerEnabled(Layer layer, bool enabled);
bool isLayerEnabled(Layer layer);

// ================= LEVEL FILTER =================
void setMinLevel(Level level);
Level getMinLevel();

// ================= LOG FUNCTIONS =================
void log(Layer layer, Level level, const char *msg);
void log(Layer layer, Level level, const String &msg);
void logf(Layer layer, Level level, const char *fmt, ...);

// Serial-only diagnostics: printed on serial (honoring filters) but never
// stored in the GUI buffer or broadcast over the net. Use for messages that
// matter to developers but not to end users (e.g. boot registration details).
void serial(Layer layer, Level level, const char *msg);
void serialf(Layer layer, Level level, const char *fmt, ...);

// ================= CONVENIENCE =================
void core(const char *msg);
void core(const String &msg);
void sensors(const char *msg);
void sensors(const String &msg);
void event(const char *msg);
void event(const String &msg);

void coref(const char *fmt, ...);
void sensorsf(const char *fmt, ...);
void eventf(const char *fmt, ...);

void warn(const char *msg);
void warn(const String &msg);
void error(const char *msg);
void error(const String &msg);

void warnf(const char *fmt, ...);
void errorf(const char *fmt, ...);

// ================= REMOTE NET LOGS =================
// Ingest a log received over the net into the serial/GUI buffers WITHOUT
// re-broadcasting it (prevents a broadcast ping-pong loop between devices).
void logRemote(Layer layer, Level level, const char *msg);

// ================= GUI ACCESS =================
// Stream recent logs as a JSON array without building a giant String (the
// ESP8266 web server cannot build large Strings under heap pressure).
typedef void (*LogJsonSink)(const char *chunk);
void streamRecentLogsJson(LogJsonSink sink);
void clearBuffer();

}  // namespace logger

// ================= SERIAL CONTROL =================
// Exposed to the sketch as Qymera::setSerialEnabled() / Qymera::isSerialEnabled().
namespace Qymera {
void setSerialEnabled(bool enabled);
bool isSerialEnabled();
}
