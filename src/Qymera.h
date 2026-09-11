#pragma once
/*
  Qymera.h - Master library header for Arduino IDE sketches.

  The user sketch only needs to implement three hooks, all inside the
  `Qymera` namespace:
    - Qymera::init()                 : initialize hardware libraries (Wire, etc.)
    - Qymera::report()               : read hardware and report values via Qymera::xxx()
    - Qymera::onCommand(uid, type, value, state) : custom logic for received commands

  then call the lifecycle functions from the sketch:
    - Qymera::begin()  in setup()
    - Qymera::loop()   in loop()

  The library handles: WiFi, web server, UDP networking, automations, EEPROM.
  Everything the sketch needs is exposed under `Qymera::` (including
  Qymera::setSerialEnabled()).
*/
#include "config.h"
#include "core.h"
#include "sensors.h"
#include "net.h"
#include "web.h"
#include "automations.h"
#include "log.h"

// ================= PUBLIC FACADE =================
// Simplifies main.ino usage: sensors, actuators, lifecycle and serial control
// all live under `Qymera::`. The three user hooks (init/report/onCommand) are
// declared in core.h and implemented by the sketch; everything else forwards
// to the internal core::/sensors:: implementation.

namespace Qymera {
// ---- lifecycle (library-provided) ----
inline void begin()  { core::begin(); }
inline void loop()   { core::loop(); }

// ---- sensors / actuators (auto-register when first reported) ----
inline void temperature(const String &key, float raw)  { sensors::temperature(key, raw); }
inline void humidity(const String &key, int raw)       { sensors::humidity(key, raw); }
inline void luminosity(const String &key, int raw)     { sensors::luminosity(key, raw); }
inline void level(const String &key, int raw)          { sensors::level(key, raw); }
inline void pressure(const String &key, float raw)     { sensors::pressure(key, raw); }
inline void airQ(const String &key, const int &v)      { sensors::airQ(key, v); }
inline void rain(const String &key, bool v)            { sensors::rain(key, v); }
inline void custom(const String &key, float raw)       { sensors::custom(key, raw); }
inline void contact(const String &key, bool v)         { sensors::contact(key, v); }
inline void relay(const String &key, uint8_t pin, bool inverted = false)
                                                       { sensors::relay(key, pin, inverted); }
inline void dimmer(const String &key, uint8_t pin, bool inverted = false)
                                                       { sensors::dimmer(key, pin, inverted); }

// ---- control ----
inline void setRelay(const String &key, bool target)  { sensors::setRelay(key, target); }
inline void handleToggle(uint32_t uid)                 { sensors::handleToggle(uid); }
inline void handleDimmer(uint32_t uid, int value)      { sensors::handleDimmer(uid, value); }
inline void startFade(const String &key, uint8_t pin, int from, int to, unsigned long dur)
                                                       { sensors::startFade(key, pin, from, to, dur); }

// ---- calibration ----
inline float calibrate(const String &key, float raw)                      { return sensors::calibrate(key, raw); }
inline sensors::Calibration *getCalib(const String &key)                  { return sensors::getCalib(key); }

// ---- time ----
inline void rtc(const sensors::RTCTime &time) { sensors::rtc(time); }
inline void ntp(const sensors::RTCTime &time) { sensors::ntp(time); }
}  // namespace Qymera