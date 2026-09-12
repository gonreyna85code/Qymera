#pragma once
#include <stdint.h>
#include "config.h"
#include "automation_core.h"


namespace automations {

// Legacy wire types (GUI JSON contract). ON_SAMPLE is either EDGE (0) or
// THRESHOLD (1) depending on its comparators; matches the old Rule enum values.
constexpr uint8_t RULE_EDGE = 0;
constexpr uint8_t RULE_THRESHOLD = 1;
constexpr uint8_t RULE_TIME = 2;
constexpr uint8_t RULE_INTERVAL = 3;

extern Automation rules[MAX_RULES];
extern AutomationState states[MAX_RULES];

void init();
void tick(uint32_t now_ms);
void saveRulesToEEPROM();
void deleteRule(uint8_t idx);
// True when any rule references the given calibration index (as sensor or
// actuator). Used by the remote-sensor lifecycle to avoid reclaiming a slot
// that an automation still depends on.
bool isIndexReferenced(uint8_t idx);

}