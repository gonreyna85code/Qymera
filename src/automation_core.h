#pragma once
#include <Arduino.h>
#include <stdint.h>

namespace automations {

constexpr uint8_t MAX_CONDITIONS = 5;
constexpr uint8_t MAX_ACTIONS = 5;

enum AutoKind : uint8_t {
  ON_SAMPLE = 0,
  ON_TIME = 1,
  ON_INTERVAL = 2
};

enum CondCmp : uint8_t {
  CMP_GT = 0,
  CMP_LT = 1,
  CMP_EQ = 2,
  EDGE_RISING = 3,
  EDGE_FALLING = 4
};

enum ActCmd : uint8_t {
  ACT_ON = 0,
  ACT_OFF = 1,
  ACT_TOGGLE = 2,
  ACT_LEVEL = 3
};

enum JoinOp : uint8_t {
  JOIN_AND = 0,
  JOIN_OR = 1
};

struct CondSample {
  float value;
  bool state;
  // Fail-safe availability: false means the sensor is absent, stale (remote
  // not sighted within NET_TIMEOUT) or its value is invalid (NaN/Inf). While
  // unavailable a threshold condition releases its engagement (never
  // satisfied) and an edge condition suppresses transitions. Rules therefore
  // never fire from data we cannot trust. See Phase 8 audit.
  bool available = true;
};

struct Automation {
  uint8_t kind;
  uint8_t sensor_count;
  uint8_t actuator_count;
  uint8_t reserved;
  uint8_t c_sensor[MAX_CONDITIONS];
  uint8_t c_cmp[MAX_CONDITIONS];
  int16_t c_threshold[MAX_CONDITIONS];
  uint8_t c_hys_dec;
  uint8_t c_op_bits;
  uint8_t a_sensor[MAX_ACTIONS];
  uint8_t a_action[MAX_ACTIONS];
  uint8_t a_level[MAX_ACTIONS];
  uint8_t retry_max;
  uint8_t retry_interval_s;
  uint32_t debounce_ms;
  uint32_t for_ms;
  uint32_t fire_delay_ms;
  uint32_t step_ms;
  uint32_t cooldown_ms;
  uint32_t time_s;
  uint32_t interval_ms;
  uint16_t year_start;
  uint16_t year_end;
  uint8_t month_start;
  uint8_t month_end;
  uint8_t day_start;
  uint8_t day_end;
};

static_assert(sizeof(Automation) == 80, "Automation must be exactly 80 bytes");

struct AutomationState {
  uint8_t last[MAX_CONDITIONS];
  uint8_t stable[MAX_CONDITIONS];
  uint16_t counter[MAX_CONDITIONS];
  uint32_t cond_active;
  bool window_active;
  bool window_fired;
  bool entry_fired;
  bool delayed;
  uint32_t window_start;
  uint32_t delay_start;
  uint32_t last_action;
  uint32_t last_time_exec;
  uint32_t last_interval_exec;
  bool seq_active;
  uint8_t seq_index;
  uint32_t seq_at;
  uint8_t seq_retries_left;
  uint32_t retry_at;
};

inline bool isSampleKind(const Automation &a) { return a.kind != ON_TIME && a.kind != ON_INTERVAL; }

inline bool dateInWindow(const Automation &a, int y, int m, int d) {
  if (a.year_start == 0 && a.year_end == 0) return true;
  if (y < (int)a.year_start || y > (int)a.year_end) return false;
  if (y == (int)a.year_start &&
      (m < (int)a.month_start || (m == (int)a.month_start && d < (int)a.day_start)))
    return false;
  if (y == (int)a.year_end &&
      (m > (int)a.month_end || (m == (int)a.month_end && d > (int)a.day_end)))
    return false;
  return true;
}

inline uint8_t sampleConditions(const Automation &a, AutomationState &s,
                                const CondSample *smp, uint32_t sample_ms) {
  uint8_t mask = 0;
  uint32_t debounce_samples = a.debounce_ms > 0
      ? ((a.debounce_ms + sample_ms - 1) / sample_ms)
      : 1;

  for (uint8_t j = 0; j < a.sensor_count && j < MAX_CONDITIONS; j++) {
    uint8_t cmp = a.c_cmp[j];
    bool state = false;

    if (!smp[j].available) {
      // Fail-safe policy (Phase 8): an unavailable condition is never
      // satisfied. Threshold comparators release their hysteresis engagement;
      // edge comparators keep their armed marker so no artificial edge is
      // replayed by the transition out of the unavailable state.
      if (cmp == EDGE_RISING || cmp == EDGE_FALLING) continue;
      s.cond_active &= ~(1UL << j);
      s.last[j] = 0;
      s.counter[j] = 0;
      continue;
    }

    if (cmp == EDGE_RISING || cmp == EDGE_FALLING) {
      bool raw = smp[j].state;
      if (raw == s.last[j]) {
        if (s.counter[j] < debounce_samples) s.counter[j]++;
      } else {
        s.last[j] = raw ? 1 : 0;
        s.counter[j] = 1;
      }
      if (s.counter[j] >= debounce_samples) {
        bool declared = s.last[j] != 0;
        if (cmp == EDGE_RISING) state = declared && !s.stable[j];
        else state = !declared && s.stable[j];
        s.stable[j] = declared ? 1 : 0;
      }
    } else {
      float val = smp[j].value;
      float th = (float)a.c_threshold[j];
      float hys = ((float)a.c_hys_dec) * 0.1f;
      bool engaged = (s.cond_active & (1UL << j)) != 0;
      if (cmp == CMP_GT) {
        if (val > th) engaged = true;
        else if (val < th - hys) engaged = false;
      } else if (cmp == CMP_LT) {
        if (val < th) engaged = true;
        else if (val > th + hys) engaged = false;
      } else {
        float band = hys > 0.0f ? hys : 0.5f;
        engaged = (val >= th - band) && (val <= th + band);
      }
      s.cond_active = (s.cond_active & ~(1UL << j)) | (engaged ? (1UL << j) : 0UL);

      bool conv = s.last[j] != 0;
      if (engaged == conv) {
        if (s.counter[j] < debounce_samples) s.counter[j]++;
      } else {
        s.last[j] = engaged ? 1 : 0;
        s.counter[j] = 1;
      }
      state = (s.counter[j] >= debounce_samples) && engaged;
    }

    if (state) mask |= (uint8_t)(1 << j);
  }
  return mask;
}

inline bool evalTree(const Automation &a, uint8_t cond_mask) {
  if (a.sensor_count == 0) return false;
  uint8_t n = a.sensor_count > MAX_CONDITIONS ? MAX_CONDITIONS : a.sensor_count;
  bool t = (cond_mask & 1) != 0;
  for (uint8_t j = 1; j < n; j++) {
    bool cj = (cond_mask & (1 << j)) != 0;
    bool orJoin = (a.c_op_bits & (1 << (j - 1))) != 0;
    t = orJoin ? (t || cj) : (t && cj);
  }
  return t;
}

inline bool entryTick(const Automation &a, AutomationState &s, bool tree) {
  (void)a;
  if (!tree) {
    s.entry_fired = false;
    return false;
  }
  if (!s.entry_fired) {
    s.entry_fired = true;
    return true;
  }
  return false;
}

inline bool forWindowTick(const Automation &a, AutomationState &s, bool tree, uint32_t now) {
  if (!tree) {
    s.window_active = false;
    s.window_fired = false;
    s.window_start = now;
    return false;
  }
  if (!s.window_active) {
    s.window_active = true;
    s.window_start = now;
    s.window_fired = false;
  }
  if (!s.window_fired && (now - s.window_start) >= a.for_ms) {
    s.window_fired = true;
    return true;
  }
  return false;
}

inline bool timeGateTick(const Automation &a, AutomationState &s, uint32_t minute_of_day,
                         uint32_t date_code, bool time_valid) {
  if (!time_valid) return false;
  uint32_t target = a.time_s / 60;
  if (minute_of_day == target) {
    if (s.last_time_exec != date_code) {
      s.last_time_exec = date_code;
      return true;
    }
  }
  return false;
}

inline bool intervalGateTick(const Automation &a, AutomationState &s, uint32_t now) {
  if ((now - s.last_interval_exec) < a.interval_ms) return false;
  s.last_interval_exec = now;
  return true;
}

}  // namespace automations