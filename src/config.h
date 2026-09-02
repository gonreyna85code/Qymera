#pragma once
#include <Arduino.h>

/* =========================
   PLATAFORMA & COMPATIBILIDAD
   ========================= */

// Auto-detección
#if defined(ESP8266)
#define PLATFORM_ESP8266 1
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
typedef ESP8266WebServer WebServerCompat;
#define PROGMEM_ATTR ICACHE_FLASH_ATTR
#define GET_CHIP_ID() ESP.getChipId()
#define SET_WIFI_SLEEP() WiFi.setSleepMode(WIFI_NONE_SLEEP)
#define SET_AUTO_CONNECT() WiFi.setAutoConnect(true), WiFi.setAutoReconnect(true)
#define RESET_MCU() ESP.reset()

#elif defined(ESP32)
#define PLATFORM_ESP32 1
#include <WiFi.h>
#include <WebServer.h>
typedef WebServer WebServerCompat;
#define ICACHE_FLASH IRAM_ATTR
#define GET_CHIP_ID() ((uint32_t)ESP.getEfuseMac())
#define SET_WIFI_SLEEP() WiFi.setSleep(false)
#define SET_AUTO_CONNECT() WiFi.setAutoReconnect(true)
#define RESET_MCU() ESP.restart()

#elif defined(ESP32S2)
#define PLATFORM_ESP32S2 1
#include <WiFi.h>
#include <WebServer.h>
typedef WebServer WebServerCompat;
#define ICACHE_FLASH IRAM_ATTR
#define GET_CHIP_ID() ((uint32_t)ESP.getEfuseMac())
#define SET_WIFI_SLEEP() WiFi.setSleep(false)
#define SET_AUTO_CONNECT() WiFi.setAutoReconnect(true)
#define RESET_MCU() ESP.restart()

#elif defined(ESP32S3)
#define PLATFORM_ESP32S3 1
#include <WiFi.h>
#include <WebServer.h>
typedef WebServer WebServerCompat;
#define ICACHE_FLASH IRAM_ATTR
#define GET_CHIP_ID() ((uint32_t)ESP.getEfuseMac())
#define SET_WIFI_SLEEP() WiFi.setSleep(false)
#define SET_AUTO_CONNECT() WiFi.setAutoReconnect(true)
#define RESET_MCU() ESP.restart()

#elif defined(ESP32C3)
#define PLATFORM_ESP32C3 1
#include <WiFi.h>
#include <WebServer.h>
typedef WebServer WebServerCompat;
#define ICACHE_FLASH IRAM_ATTR
#define GET_CHIP_ID() ((uint32_t)ESP.getEfuseMac())
#define SET_WIFI_SLEEP() WiFi.setSleep(false)
#define SET_AUTO_CONNECT() WiFi.setAutoReconnect(true)
#define RESET_MCU() ESP.restart()

#else
#error "Plataforma no soportada. Usa: ESP8266, ESP32, ESP32S2, ESP32S3, ESP32C3"
#endif

/* =========================
   LIMITES DEL SISTEMA
   ========================= */

#define MAX_SENSORS 64
#define MAX_PERSISTED_SENSORS 40
#define MAX_RULES 20

/* =========================
   EEPROM
   ========================= */

#define EEPROM_SIZE 4096

/* Relay state (LEGACY/RESERVED region)
   Relay persistence now lives inside CalibrationPersist slots; this region is
   kept only as the layout anchor and is zeroed by factory reset. Do not use. */
#define EEPROM_RELAY_STATE_START 0
#define EEPROM_RELAY_STATE_SIZE 10

/* OTA flag & device identity token relocated below (after the rules region) so
   they do not alias the relay-state region (0..9) or the credentials block.
   The token is a chip-ID provisioning check, NOT a firmware hash. */

/* WiFi credentials */
#define EEPROM_CRED_START (EEPROM_RELAY_STATE_START + EEPROM_RELAY_STATE_SIZE)
#define EEPROM_CRED_SIZE 100

/* Genset config */
#define EEPROM_GENSET_START (EEPROM_CRED_START + EEPROM_CRED_SIZE)
#define EEPROM_GENSET_SIZE 12

/* Calibration (solo sensores físicos aprox) */
/* Slot layout must match CalibrationPersist in storage.cpp:
   magic(4)+version(2)+uid(4)+pers_state(1)+min(4)+max(4)+correction(4)+
   avail(1)+persist(1)+pulse(1)+pulse_ms(4)+fade(4) = 34 bytes */
#define EEPROM_CALIB_SLOT_SIZE 34
#define EEPROM_CALIB_START (EEPROM_GENSET_START + EEPROM_GENSET_SIZE)
#define EEPROM_CALIB_SIZE (MAX_PERSISTED_SENSORS * EEPROM_CALIB_SLOT_SIZE)

/* Automation rules */
#define EEPROM_RULES_START (EEPROM_CALIB_START + EEPROM_CALIB_SIZE)
#define EEPROM_RULES_SIZE 1600

/* =========================
   PROTECCION EEPROM
   ========================= */

#if (EEPROM_RULES_START + EEPROM_RULES_SIZE) > EEPROM_SIZE
#error EEPROM layout overflow
#endif

/* OTA flag + 4-byte device identity token, placed in the reserved area after
   the rules block. Non-aliased: does not overlap relay state (0..9) or
   credentials. An all-0xFF or all-0x00 slot means "unprovisioned". */
#define EEPROM_OTA_HASH_ADDR  (EEPROM_RULES_START + EEPROM_RULES_SIZE)
#define EEPROM_OTA_HASH_SIZE  4
#define EEPROM_OTA_FLAG_ADDR  (EEPROM_OTA_HASH_ADDR + EEPROM_OTA_HASH_SIZE)

/* Entity ID map (slot_index -> entity_id), 4 bytes per slot.
   Allows stable identity without shifting existing EEPROM layout. */
#define EEPROM_ENTITY_ID_START  (EEPROM_OTA_FLAG_ADDR + 1)
#define EEPROM_ENTITY_ID_SIZE   (MAX_PERSISTED_SENSORS * 4)

#if (EEPROM_ENTITY_ID_START + EEPROM_ENTITY_ID_SIZE) > EEPROM_SIZE
#error Entity ID map region overflow
#endif

/* =========================
   RED
   ========================= */

#define AP_SSID "QymeraSetup"

#define BROADCAST_PORT 13345
#define COMMAND_PORT 13346

#define BROADCAST_INTERVAL 5000
#define WIFI_RETRY_INTERVAL 180000

/* =========================
   PWM ABSTRACTION (0-255, 8-bit)
   ESP8266: default analogWrite range is 1023 (10-bit)
   ESP32:   default analogWrite range is 255 (8-bit)
   Common abstraction: map to 0-255 on both platforms
   ========================= */

#ifdef PLATFORM_ESP8266
  #define PWM_MAX_RAW 1023
  #define PWM_MAX_OUT 255
  inline void pwmSetup(uint8_t pin) {
    static bool pwm_range_set = false;
    if (!pwm_range_set) {
      analogWriteRange(PWM_MAX_OUT);
      analogWriteFreq(1000);
      pwm_range_set = true;
    }
    pinMode(pin, OUTPUT);
  }
  inline void pwmWritePin(uint8_t pin, uint8_t value) {
    analogWrite(pin, value);
  }
  inline uint8_t pwmReadPin(uint8_t pin) {
    return (uint8_t)(analogRead(pin) * PWM_MAX_OUT / PWM_MAX_RAW);
  }

#elif defined(PLATFORM_ESP32) || defined(PLATFORM_ESP32S2) \
   || defined(PLATFORM_ESP32S3) || defined(PLATFORM_ESP32C3)
  #define PWM_MAX_RAW 4095
  #define PWM_MAX_OUT 255
  inline void pwmSetup(uint8_t pin) {
    pinMode(pin, OUTPUT);
  }
  inline void pwmWritePin(uint8_t pin, uint8_t value) {
    analogWrite(pin, value);
  }
  inline uint8_t pwmReadPin(uint8_t pin) {
    return (uint8_t)(analogRead(pin) * PWM_MAX_OUT / PWM_MAX_RAW);
  }
#endif
