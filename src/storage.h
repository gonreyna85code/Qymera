#pragma once
#include <Arduino.h>
#include "config.h"

namespace storage {

void begin();
uint8_t read(int addr);
void write(int addr, uint8_t val);
template<typename T> bool get(int addr, T &obj);
template<typename T> bool put(int addr, const T &obj);
void commit();

void loadCredentials(String &ssid, String &password);
void saveCredentials(const String &ssid, const String &password);

void loadGeneralSettings(uint16_t &broadcast_port, uint16_t &command_port, uint32_t &report_interval);
void saveGeneralSettings(uint16_t broadcast_port, uint16_t command_port, uint32_t report_interval);

uint8_t loadOtaFlag();
void saveOtaFlag(uint8_t flag);

void loadCalibration();
void saveCalibration();
void saveCalibrationSlot(int index);

void loadRules();
void saveRules();
void deleteRule(uint8_t idx);

bool verifyOtaIntegrity();
bool isOtaEnabled();
bool isOtaIntegrityVerified();

uint32_t commitCount();

void factoryReset();

}  // namespace storage
