#include "automations.h"
#include "entities.h"
#include "sensors.h"
#include "log.h"
#include "storage.h"

namespace automations {

using namespace qymera::model;

// ----------------- REGLAS -----------------

static const uint32_t SAMPLE_MS = 50;
static uint32_t last_run = 0;

Automation rules[MAX_RULES];
AutomationState states[MAX_RULES];

// ----------------- ACCIONES -----------------
static bool executeAction(const Automation &a, uint8_t index) {
  if (index >= a.actuator_count) return true;
  uint8_t idx = a.a_sensor[index];
  if (idx >= MAX_SENSORS) return false;
  const Entity &c = entities::peek(idx);
  if (!entities::isUsed(idx) || c.identity.entity_id == 0) return false;
  const uint8_t type = c.config.type;
  const bool state = c.state.state;
  const uint32_t entity_id = c.identity.entity_id;
  switch (a.a_action[index]) {
    case ACT_ON:
      if (!qymera::model::isValidType(type) ||
          qymera::model::capabilityOfType(type) != EntityCapability::READ_WRITE) return false;
      if (!state) sensors::handleToggle(entity_id);
      return true;
    case ACT_OFF:
      if (!qymera::model::isValidType(type) ||
          qymera::model::capabilityOfType(type) != EntityCapability::READ_WRITE) return false;
      if (state) sensors::handleToggle(entity_id);
      return true;
    case ACT_TOGGLE:
      if (!qymera::model::isValidType(type) ||
          qymera::model::capabilityOfType(type) != EntityCapability::READ_WRITE) return false;
      sensors::handleToggle(entity_id);
      return true;
    case ACT_LEVEL:
      if (type != TYPE_DIMMER) return false;
      sensors::handleDimmer(entity_id, a.a_level[index]);
      return true;
    default:
      return false;
  }
}

static void advanceSequence(Automation &a, AutomationState &s, uint32_t now_ms) {
  if (!s.seq_active) return;
  if (s.seq_at != 0 && now_ms < s.seq_at) return;
  while (true) {
    if (s.seq_index >= a.actuator_count) {
      s.seq_active = false;
      s.seq_index = 0;
      s.seq_at = 0;
      s.retry_at = 0;
      return;
    }
    if (!executeAction(a, s.seq_index)) {
      if (s.seq_retries_left > 0) {
        s.seq_retries_left--;
        s.retry_at = now_ms + (uint32_t)a.retry_interval_s * 1000UL;
        s.seq_at = s.retry_at;
        logger::warnf("Auto %d action %d failed, retries left %d",
                     (int)(&a - rules), s.seq_index, s.seq_retries_left);
        return;
      }
      logger::warnf("Auto %d action %d failed, giving up", (int)(&a - rules), s.seq_index);
      s.seq_active = false;
      s.seq_index = 0;
      s.seq_at = 0;
      s.retry_at = 0;
      return;
    }
    s.seq_index++;
    if (s.seq_index < a.actuator_count && a.step_ms > 0) {
      s.seq_at = now_ms + a.step_ms;
      s.retry_at = 0;
      return;
    }
    s.retry_at = 0;
    s.seq_at = 0;
  }
}

// ----------------- TICK -----------------
void tick(uint32_t now_ms) {
  if (now_ms - last_run < SAMPLE_MS) return;
  last_run = now_ms;

  for (int i = 0; i < MAX_RULES; i++) {
    Automation &a = rules[i];
    AutomationState &s = states[i];

    if (a.sensor_count == 0 && a.actuator_count == 0) continue;

    bool fire = false;
    bool isSample = isSampleKind(a);

    if (isSample) {
      CondSample smp[MAX_CONDITIONS];
      for (int j = 0; j < (int)a.sensor_count && j < MAX_CONDITIONS; j++) {
        if (a.c_sensor[j] < MAX_SENSORS && entities::isUsed(a.c_sensor[j])) {
          const Entity &c = entities::peek(a.c_sensor[j]);
          smp[j].value = c.state.value;
          smp[j].state = c.state.state;
        } else {
          smp[j].value = 0;
          smp[j].state = false;
        }
      }
      uint8_t mask = sampleConditions(a, s, smp, SAMPLE_MS);
      bool tree = evalTree(a, mask);
      if (a.for_ms > 0) fire = forWindowTick(a, s, tree, now_ms);
      else fire = entryTick(a, s, tree);
    } else {
      int y = 0, m = 0, d = 0;
      uint32_t minOfDay = 0, dateCode = 0;
      bool timeOk = sensors::timeValid();
      if (timeOk) {
        sensors::RTCTime t = sensors::getTime();
        y = t.year; m = t.month; d = t.day;
        minOfDay = sensors::getMinutesOfDay();
        dateCode = (uint32_t)t.year * 10000UL + (uint32_t)t.month * 100UL + (uint32_t)t.day;
      }
      if (!dateInWindow(a, y, m, d)) continue;
      if (a.kind == ON_TIME) {
        fire = timeGateTick(a, s, minOfDay, dateCode, timeOk);
      } else {
        fire = intervalGateTick(a, s, now_ms);
      }
    }

    bool launch = false;
    if (s.delayed) {
      if (now_ms - s.delay_start >= a.fire_delay_ms) {
        s.delayed = false;
        s.delay_start = 0;
        launch = true;
      }
    } else if (fire) {
      if (now_ms - s.last_action >= a.cooldown_ms) {
        if (a.fire_delay_ms > 0) {
          s.delayed = true;
          s.delay_start = now_ms;
        } else {
          launch = true;
        }
      }
    }

    if (launch && !s.seq_active) {
      s.seq_active = true;
      s.seq_index = 0;
      s.seq_retries_left = a.retry_max;
      s.seq_at = 0;
      s.retry_at = 0;
      s.last_action = now_ms;
      logger::eventf("Rule %d fired (kind:%d)", i, a.kind);
    }

    advanceSequence(a, s, now_ms);
  }
}

void loadRulesFromEEPROM() {
  storage::loadRules();
}

void saveRulesToEEPROM() {
  storage::saveRules();
}

void deleteRule(uint8_t idx) {
  if (idx >= MAX_RULES) return;
  memset(&rules[idx], 0, sizeof(Automation));
  memset(&states[idx], 0, sizeof(AutomationState));
  saveRulesToEEPROM();
}

bool isIndexReferenced(uint8_t idx) {
  if (idx >= MAX_SENSORS) return false;
  for (int i = 0; i < MAX_RULES; i++) {
    const Automation &a = rules[i];
    if (a.sensor_count == 0 && a.actuator_count == 0) continue;
    for (int j = 0; j < (int)a.sensor_count && j < MAX_CONDITIONS; j++) {
      if (a.c_sensor[j] == idx) return true;
    }
    for (int j = 0; j < (int)a.actuator_count && j < MAX_ACTIONS; j++) {
      if (a.a_sensor[j] == idx) return true;
    }
  }
  return false;
}

// ----------------- INIT -----------------
void init() {
  loadRulesFromEEPROM();
  for (int i = 0; i < MAX_RULES; i++) {
    memset(&states[i], 0, sizeof(AutomationState));
  }
}

}  // namespace automations