#pragma once
#include <stdint.h>
#include "config.h"
#include "version.h"

// Web OTA module. Pulls the release manifest from the official GitHub releases
// (latest release), verifies product/platform/size, streams the firmware image
// into the Update subsystem while computing its SHA-256, and reboots on
// success. Complements (does not replace) the legacy ArduinoOTA service that
// is toggled through /ota/toggle.
//
// The flow is asynchronous and driven from firmware::tick() (called from
// core::loop()), so the web server keeps responding during the download and
// the GUI can poll /firmware for progress.

namespace firmware {

enum State : uint8_t {
  FW_IDLE = 0,        // nothing happening
  FW_CHECKING,        // fetching/parsing the release manifest
  FW_READY,           // manifest parsed (updateAvailable() tells if newer)
  FW_DOWNLOADING,     // streaming firmware -> Update
  FW_INSTALLING,      // Update.end() succeeded, rebooting
  FW_ERROR            // terminal error; errorMessage() has the reason
};

const char *stateToken();
bool updateAvailable();
const char *currentVersion();
const char *latestVersion();
const char *channel();
const char *errorMessage();
uint8_t progressPercent();

// Arm an asynchronous manifest check. Returns false if a check/update is busy.
bool requestCheck();

// Arm the update. Only allowed when FW_READY and an update is available.
bool requestUpdate();

// Driver. Call from core::loop().
void tick();

}  // namespace firmware