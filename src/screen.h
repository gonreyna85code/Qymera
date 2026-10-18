#pragma once
#include <Arduino.h>
#include "config.h"
#include "entities.h"

// ================================================================
// SCREEN - I2C display facade (v1)
//
// Minimal, non-blocking display driver for small I2C OLED/TFT modules.
// The user only declares the panel geometry (width x height) and the
// SDA/SCL pins. The library owns a single shared TwoWire instance and
// a deferred (30 s) init so boot never stalls WiFi association.
//
// Usage from the sketch (Qymera facade):
//   Qymera::screen(128, 64, /*sda=*/4, /*scl=*/5);
//
// The splash screen renders the QYMERA wordmark plus the live entity
// list (name + state) and scrolls automatically. Non-blocking: all
// refresh work is time-budgeted to a few ms per loop tick.
// ================================================================

namespace screen {

// Initialize the (deferred) display. Called once from core::begin() via
// the user's `Qymera::screen(...)` declaration stored in config.
void init(uint16_t width, uint16_t height, uint8_t sda, uint8_t scl);

// Tick: handles deferred init and periodic redraw. Safe to call every loop.
void tick(uint32_t now_ms);

// Force an immediate redraw (used by splash transitions).
void redraw();

// True once the panel has been successfully initialised.
bool ready();

// Query the configured geometry.
uint16_t width();
uint16_t height();

}  // namespace screen
