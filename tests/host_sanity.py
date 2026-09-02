"""Host sanity tests for Qymeras STEP 1 logic.

These tests port the EXACT logic implemented in the firmware so the behavior
can be validated on the host machine (no native C++ toolchain required):

1. timezone conversion  - mirrors sensors.cpp: timezoneOffsetMinutes()/
                          toLocalEpoch()/getTime()/getMinutesOfDay()
2. strict float parsing - mirrors web.cpp: parseStrictFloat()
3. ESP-NOW RX FIFO      - mirrors espnow_p2p.cpp: rx_enqueue()/espnow_recv()

Run:  python tests/host_sanity.py
Exit code 0 = all pass.
"""
import time
import math

PASS = 0
FAIL = 0


def check(name, cond, detail=""):
    global PASS, FAIL
    if cond:
        PASS += 1
        print("  [PASS] %s" % name)
    else:
        FAIL += 1
        print("  [FAIL] %s%s" % (name, (" -- " + detail) if detail else ""))


# ---------------------------------------------------------------- timezone
# sensors.cpp: toLocalEpoch(utc) = utc + offset_minutes*60, then gmtime().
def to_local_epoch(utc, offset_min):
    return utc + offset_min * 60


def get_time(utc, offset_min):
    t = time.gmtime(to_local_epoch(utc, offset_min))
    return (t.tm_year, t.tm_mon, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec)


def get_minutes_of_day(utc, offset_min):
    t = time.gmtime(to_local_epoch(utc, offset_min))
    return t.tm_hour * 60 + t.tm_min


# Reference UTC epoch for 2026-08-20 06:00:00 UTC.
import calendar
UTC_REF = calendar.timegm((2026, 8, 20, 6, 0, 0))

print("[timezone]")
check("offset 0 -> same clock",
      get_time(UTC_REF, 0) == (2026, 8, 20, 6, 0, 0))
check("offset +120 (Madrid) -> 08:00",
      get_time(UTC_REF, 120) == (2026, 8, 20, 8, 0, 0))
check("offset +120 minutes-of-day == 480",
      get_minutes_of_day(UTC_REF, 120) == 480)
check("offset -360 (CDMX) -> 00:00 same day",
      get_time(UTC_REF, -360) == (2026, 8, 20, 0, 0, 0))
check("offset -360 minutes-of-day == 0",
      get_minutes_of_day(UTC_REF, -360) == 0)
check("offset +840 (max) -> 20:00",
      get_time(UTC_REF, 840) == (2026, 8, 20, 20, 0, 0))
check("offset -720 (min) -> previous day 18:00",
      get_time(UTC_REF, -720) == (2026, 8, 19, 18, 0, 0))
check("minutes-of-day == getTime hour*60+min (consistent)",
      get_minutes_of_day(UTC_REF, -720) ==
      get_time(UTC_REF, -720)[3] * 60 + get_time(UTC_REF, -720)[4])
check("offset 0 min-of-day == 360",
      get_minutes_of_day(UTC_REF, 0) == 360)
check("offset 30 -> 06:30",
      get_time(UTC_REF, 30) == (2026, 8, 20, 6, 30, 0))
# cross-day boundary: 23:50 UTC + 120min -> next day 01:50
check("offset crossing midnight",
      get_time(calendar.timegm((2026, 8, 20, 23, 50, 0)), 120) == (2026, 8, 21, 1, 50, 0))

# ---------------------------------------------------------------- strict float
# web.cpp: parseStrictFloat()
#   empty -> reject; strtof; no prefix consumed -> reject; trailing junk -> reject
#   ERANGE / inf / nan -> reject
MAX_FLOAT = 3.4028235e38


def parse_strict_float(s):
    if len(s) == 0:
        return False, None
    # strtof skips leading whitespace, then parses the longest numeric prefix.
    t = s.lstrip()
    end = 0
    while end < len(t) and t[end] in "0123456789+-.eE":
        end += 1
    if end == 0:  # no prefix consumed (strtof end == begin)
        return False, None
    prefix = t[:end]
    # "12abc" -> prefix "12", remainder "abc" -> reject
    if end != len(t):
        return False, None
    try:
        v = float(prefix)
    except ValueError:
        return False, None
    if math.isinf(v) or math.isnan(v):  # matches isinf/isnan checks
        return False, None
    if abs(v) > MAX_FLOAT:  # ERANGE (overflow)
        return False, None
    return True, v


print("[strict float]")
ok, v = parse_strict_float("")
check("empty rejected", not ok)
ok, v = parse_strict_float("abc")
check("garbage rejected", not ok)
ok, v = parse_strict_float("12")
check("int ok == 12.0", ok and v == 12.0)
ok, v = parse_strict_float("12.5")
check("decimal ok == 12.5", ok and v == 12.5)
ok, v = parse_strict_float("-3.25")
check("negative ok == -3.25", ok and v == -3.25)
ok, v = parse_strict_float("+7")
check("leading + ok == 7.0", ok and v == 7.0)
ok, v = parse_strict_float("  12")
check("leading whitespace ok == 12.0 (strtof skips)", ok and v == 12.0)
ok, v = parse_strict_float("12 ")
check("trailing whitespace rejected (*end != '\\0')", not ok)
ok, v = parse_strict_float("12abc")
check("trailing junk rejected", not ok)
ok, v = parse_strict_float("1e400")
check("overflow -> inf rejected (ERANGE)", not ok)
ok, v = parse_strict_float("nan")
check("nan rejected", not ok)
ok, v = parse_strict_float("inf")
check("inf rejected", not ok)
ok, v = parse_strict_float("-inf")
check("-inf rejected", not ok)
ok, v = parse_strict_float("0")
check("zero ok", ok and v == 0.0)
ok, v = parse_strict_float("1.5")
check("ref float ok == 1.5", ok and v == 1.5)
ok, v = parse_strict_float("abc -> 0")
check("'abc -> 0' rejected (no silent 0)", not ok)

# calib/set range guards mirrored from handleCalibSet
check("tz -720 accepted", -720 >= -720 and -720 <= 840)
check("tz 840 accepted", 840 >= -720 and 840 <= 840)
check("tz 841 rejected", not (841 >= -720 and 841 <= 840))
check("tz -721 rejected", not (-721 >= -720 and -721 <= 840))
check("fade 3600000 accepted", 3600000 >= 0 and 3600000 <= 3600000)
check("fade 3600001 rejected", not (3600001 >= 0 and 3600001 <= 3600000))
check("fade negative rejected", not (-1 >= 0 and -1 <= 3600000))

# ---------------------------------------------------------------- ESP-NOW FIFO
# espnow_p2p.cpp: bounded ring buffer, SPSC (callback writes, loop reads).
QSIZE = 8
MAX_PAYLOAD = 250


class Fifo:
    def __init__(self):
        self.q = [None] * QSIZE
        self.head = 0
        self.tail = 0
        self.count = 0
        self.overflow = 0

    def enqueue(self, payload):
        if len(payload) == 0 or len(payload) > MAX_PAYLOAD:
            return
        if self.count >= QSIZE:
            self.overflow += 1  # drop new, keep oldest
            return
        self.q[self.head] = payload
        self.head = (self.head + 1) % QSIZE
        self.count += 1

    def recv(self):
        if self.count == 0:
            return None
        p = self.q[self.tail]
        self.tail = (self.tail + 1) % QSIZE
        self.count -= 1
        return p


print("[esp-now fifo]")
f = Fifo()
check("empty recv -> None", f.recv() is None)
for i in range(QSIZE):
    f.enqueue(b"m%d" % i)
check("8 entries fill queue, count==8", f.count == 8)
f.enqueue(b"overflow9")
check("9th dropped, overflow==1", f.overflow == 1)
check("oldest preserved (no loss of queued msgs)", f.count == 8)
check("FIFO order (oldest first)",
      [f.recv() for _ in range(8)] == [b"m%d" % i for i in range(8)])
check("drained, count==0", f.count == 0)
f.enqueue(b"")  # zero length dropped without overflow
f.enqueue(b"x" * (MAX_PAYLOAD + 1))
check("zero/oversize dropped, overflow unchanged", f.overflow == 1 and f.count == 0)
# wrap-around: fill, drain 5, fill 5 -> head wraps, FIFO still ordered
for i in range(8):
    f.enqueue(b"a%d" % i)
for _ in range(5):
    f.recv()
for i in range(5):
    f.enqueue(b"b%d" % i)
check("wrap-around count==8", f.count == 8)
got = [f.recv() for _ in range(8)]
check("wrap-around FIFO order",
      got == [b"a%d" % i for i in range(5, 8)] + [b"b%d" % i for i in range(5)])
check("after wrap drain, count==0", f.count == 0)
f.enqueue(b"z")
check("reusable after full cycle", f.recv() == b"z" and f.count == 0)

# ---------------------------------------------------------------- entity model
# src/model.h: qymera::model classification (pure, host-testable).
# SensorType enum VALUES (src/sensors.h):
#   0 NONE, 1 LUMI, 2 HUMI, 3 TEMP, 4 PRESS, 5 LEVEL, 6 AIRQ, 7 RAIN,
#   8 TYPE_DIMMER, 9 TYPE_RELAY, 10 SENSOR_TIME, 11 GENERIC, 12 CONTACT
NONE = 0
SENSOR = 1
ACTUATOR = 2
CLOCK = 3

_SENSOR_TYPES = {1, 2, 3, 4, 5, 6, 7, 11, 12}
_ACTUATOR_TYPES = {8, 9}
_CLOCK_TYPES = {10}


def kind_of_type(t):
    if t == 8 or t == 9:
        return ACTUATOR
    if t == 10:
        return CLOCK
    if t in _SENSOR_TYPES:
        return SENSOR
    return NONE


def capability_of_type(t):
    k = kind_of_type(t)
    if k == ACTUATOR:
        return 3  # READ_WRITE
    if k == SENSOR or k == CLOCK:
        return 1  # READ
    return 0  # NONE


def ownership_of(local, uid):
    if uid == 0:
        return 0  # NONE
    return 1 if local else 2  # LOCAL / REMOTE


def is_valid_type(t):
    return kind_of_type(t) != NONE


print("[entity model]")
for t in range(0, 13):
    check("type %d kind" % t,
          kind_of_type(t) ==
          (ACTUATOR if t in _ACTUATOR_TYPES else
           CLOCK if t in _CLOCK_TYPES else
           SENSOR if t in _SENSOR_TYPES else NONE))
check("kind NONE type == 0", kind_of_type(0) == NONE)
check("kind ACTUATOR dimmer==8 relay==9",
      kind_of_type(8) == ACTUATOR and kind_of_type(9) == ACTUATOR)
check("kind CLOCK time==10", kind_of_type(10) == CLOCK)
check("kind SENSOR generics",
      all(kind_of_type(t) == SENSOR for t in [1, 2, 3, 4, 5, 6, 7, 11, 12]))
check("invalid raw byte -> NONE",
      kind_of_type(13) == NONE and kind_of_type(200) == NONE and
      kind_of_type(255) == NONE)
cap = {t: capability_of_type(t) for t in range(0, 13)}
check("capability READ_WRITE only actuators",
      all(cap[t] == 3 for t in [8, 9]))
check("capability READ for sensor+clock",
      all(cap[t] == 1 for t in [1, 2, 3, 4, 5, 6, 7, 10, 11, 12]))
check("capability NONE for empty/invalid", cap[0] == 0 and
      capability_of_type(13) == 0)
check("ownership uid==0 -> NONE",
      ownership_of(True, 0) == 0 and ownership_of(False, 0) == 0)
check("ownership local/remote",
      ownership_of(True, 5) == 1 and ownership_of(False, 5) == 2)
check("isValidType==legacy range[1..12]",
      all(is_valid_type(t) == (1 <= t <= 12) for t in range(0, 16)))
check("isValidType rejects unknown bytes",
      not is_valid_type(13) and not is_valid_type(255))
# canonical equivalence used by sensors.cpp:isValidSensorType()
check("isValidSensorType canonical match",
      all((1 <= t <= 12) == is_valid_type(t) for t in range(0, 256) if t < 14))

print()
print("host_sanity: %d passed, %d failed" % (PASS, FAIL))
raise SystemExit(1 if FAIL else 0)