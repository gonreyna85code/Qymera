/*
  Base - Minimal Qymera Example (PlatformIO sketch: src/main.ino)
  ==============================================================

  This file is the PLATFORMIO sketch entry point only. It intentionally
  uses the .ino extension so the Arduino IDE library build (which compiles
  only .c/.cpp sources under the library's src/) ignores it, avoiding
  duplicate symbols with the user sketch.

  The user sketch (e.g. examples/Base/Base.ino) only needs to implement
  three hooks under the Qymera namespace:
    1. Qymera::init()    - initialize hardware libraries (Wire, etc.)
    2. Qymera::report()  - read hardware and report values via Qymera::xxx()
    3. Qymera::onCommand() - custom logic for received commands

  then delegate setup()/loop() to the library:
    void setup() { Qymera::begin(); }
    void loop()  { Qymera::loop(); }

  The library handles: WiFi, web server, UDP mesh, automations, EEPROM.
*/

#include "Qymera.h"

// ================================
// Qymera::init - inicialización de hardware
// ================================
void Qymera::init() {
  // init hardware
}

// ================================
// Qymera::report - leer hardware y reportar valores
// ================================
void Qymera::report() {

  // --- Valores de ejemplo/demostración ---
  constexpr float tempF    = 35.2f;
  constexpr float humi    = 35;
  constexpr uint16_t lumi   = 15535;
  constexpr uint8_t airQ   = 2;
  constexpr uint8_t press  = 101;
  constexpr uint8_t level  = 58;
  constexpr bool rain       = true;
  constexpr bool contact    = false;
  constexpr float generic   = 105.35f;

  Qymera::temperature("TEMP", tempF);
  Qymera::humidity("HUMI", humi);
  Qymera::luminosity("LUMI0", lumi);
  Qymera::airQ("AIRQ0", airQ);
  Qymera::pressure("PRES0", press);
  Qymera::level("LEVE0", level);
  Qymera::rain("RAIN0", rain);
  Qymera::contact("CONTACT", contact);
  Qymera::custom("GENERIC", generic);
  Qymera::relay("RELAY0", 5, true);
  Qymera::dimmer("DIMM0", 2, false);
}

// ================================
// Qymera::onCommand - lógica personalizada de comandos recibidos
// ================================
void Qymera::onCommand(uint32_t /*uid*/, uint8_t /*type*/, int /*value*/, bool /*state*/) {
  // custom relay/dimmer logic for received commands
}

// ================================
// Entry points - delega todo a la librería
// ================================
void setup()   { Qymera::begin(); }
void loop()    { Qymera::loop(); }