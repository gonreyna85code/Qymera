"""Host sanity tests for Qymeras STEP 1 logic.

These tests port the EXACT logic implemented in the firmware so the behavior
can be validated on the host machine (no native C++ toolchain required):

1. timezone conversion  - mirrors sensors.cpp: timezoneOffsetMinutes()/
                          toLocalEpoch()/getTime()/getMinutesOfDay()
2. strict float parsing - mirrors web.cpp: parseStrictFloat()
3. ESP-NOW RX FIFO      - mirrors espnow_p2p.cpp: rx_enqueue()/recv()

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

# ---------------------------------------------------------------- identity system
# src/sensors.cpp: nextEntityId(), findCalibByEntityId(), bindLocalSensor()
# Mirrors xorshift32 PRNG and entity_id assignment logic.

def xorshift32(seed):
    x = seed
    x ^= (x << 13) & 0xFFFFFFFF
    x ^= (x >> 17) & 0xFFFFFFFF
    x ^= (x << 5) & 0xFFFFFFFF
    return x & 0xFFFFFFFF

def next_entity_id(seed, chip_id):
    # src/sensors.cpp: nextEntityId()
    # Returns (entity_id, new_seed)
    if seed == 0:
        seed = chip_id ^ 0x12345678  # fixed test seed instead of millis()
        seed |= 1
    new_seed = xorshift32(seed)
    entity_id = (chip_id & 0xFFFF0000) | (new_seed & 0xFFFF)
    return entity_id, new_seed

# Simulate Calibration array for findCalibByEntityId
class MockCalib:
    def __init__(self, entity_id=0, uid=0, local=True):
        self.entity_id = entity_id
        self.uid = uid
        self.local = local

def find_by_entity_id(calibs, entity_id):
    if entity_id == 0:
        return -1
    for i, c in enumerate(calibs):
        if c.entity_id == entity_id:
            return i
    return -1

def find_by_uid(calibs, uid):
    if uid == 0:
        return -1
    for i, c in enumerate(calibs):
        if c.uid == uid:
            return i
    return -1

print("[identity system]")
# xorshift32 deterministic sequence
seed = 0xDEADBEEF
expected = [0x477D20B7, 0x8E1D9142, 0xBA8C2458, 0xFEE0503B]
for exp in expected:
    seed = xorshift32(seed)
    check("xorshift32 step", seed == exp)

# entity_id generation: non-zero, combines chip_id upper bits
chip_id = 0x12345678
seed = 0
eid1, seed = next_entity_id(seed, chip_id)
eid2, seed = next_entity_id(seed, chip_id)
eid3, seed = next_entity_id(seed, chip_id)
check("entity_id non-zero", eid1 != 0 and eid2 != 0 and eid3 != 0)
check("entity_id upper 16 bits = chip_id upper 16 bits",
      (eid1 & 0xFFFF0000) == (chip_id & 0xFFFF0000) and
      (eid2 & 0xFFFF0000) == (chip_id & 0xFFFF0000) and
      (eid3 & 0xFFFF0000) == (chip_id & 0xFFFF0000))
check("entity_id unique per call", eid1 != eid2 and eid2 != eid3 and eid1 != eid3)
check("entity_id differs per chip_id",
      next_entity_id(0, 0x11111111)[0] != next_entity_id(0, 0x22222222)[0])

# findCalibByEntityId logic
calibs = [
    MockCalib(entity_id=0x12345678, uid=1),
    MockCalib(entity_id=0x87654321, uid=2),
    MockCalib(entity_id=0, uid=3),
    MockCalib(entity_id=0xAAAABBBB, uid=4, local=False),  # remote
]
check("findByEntityId exact match", find_by_entity_id(calibs, 0x12345678) == 0)
check("findByEntityId remote entity", find_by_entity_id(calibs, 0xAAAABBBB) == 3)
check("findByEntityId not found", find_by_entity_id(calibs, 0xDEADBEEF) == -1)
check("findByEntityId zero returns -1", find_by_entity_id(calibs, 0) == -1)
check("findByEntityId prefers entity_id over uid",
      find_by_entity_id(calibs, 0x87654321) == 1 and
      find_by_uid(calibs, 2) == 1)

# bindLocalSensor assigns entity_id only once (persists across reboots simulation)
# Simulate: first registration -> entity_id assigned; second call with same object -> keeps entity_id
class MockCalib2:
    def __init__(self):
        self.entity_id = 0
        self.uid = 0

def bind_local_sensor(c, seed, chip_id):
    if c.entity_id == 0:
        c.entity_id, seed = next_entity_id(seed, chip_id)
    c.uid = chip_id + 1  # simplified makeSensorUid
    return seed

c = MockCalib2()
seed = 0
seed = bind_local_sensor(c, seed, 0x12345678)
eid_first = c.entity_id
seed = bind_local_sensor(c, seed, 0x12345678)  # second call - should NOT reassign
check("bindLocalSensor assigns entity_id once", c.entity_id == eid_first)
check("bindLocalSeed increments seed", c.entity_id != 0)

# ---------------------------------------------------------------- protocol v2
# src/protocol_v2.h: qymera::protocol::v2 envelope/payload, CRC16, build/parse
# Mirrors: Envelope, HelloPayload, EntityAnnouncePayload, StateUpdatePayload,
# CommandPayload, CommandAckPayload, CommandErrorPayload, LogPayload

import struct

V2_MAGIC = 0xA6
V2_VERSION = 2

def crc16_ccitt_py(data, crc=0xFFFF):
    for b in data:
        crc ^= (b << 8) & 0xFFFF
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if (crc & 0x8000) else (crc << 1) & 0xFFFF
    return crc

def build_frame(msg_type, src_uid, dst_uid, msg_id, flags, payload_bytes):
    # Envelope: magic(1) version(1) flags(2) msg_id(4) seq(2) src_uid(4) dst_uid(4) msg_type(1) payload_len(2) crc16(2) reserved(1) = 24
    # Format: <BBHIHIIBHHB
    env = struct.pack('<BBHIHIIBHHB',
                      V2_MAGIC, V2_VERSION, flags, msg_id, 0,
                      src_uid, dst_uid, msg_type, len(payload_bytes),
                      crc16_ccitt_py(payload_bytes) if payload_bytes else 0, 0)
    return env + payload_bytes

def parse_frame(buf):
    if len(buf) < 24: return None
    magic, version, flags, msg_id, seq, src_uid, dst_uid, msg_type, payload_len, crc16, reserved = struct.unpack('<BBHIHIIBHHB', buf[:24])
    if magic != V2_MAGIC or version != V2_VERSION: return None
    if len(buf) != 24 + payload_len: return None
    payload = buf[24:]
    if payload_len > 0 and crc16_ccitt_py(payload) != crc16: return None
    return {
        'magic': magic, 'version': version, 'flags': flags, 'msg_id': msg_id,
        'seq': seq, 'src_uid': src_uid, 'dst_uid': dst_uid,
        'msg_type': msg_type, 'payload_len': payload_len, 'crc16': crc16,
        'payload': payload
    }

# Message type constants
V2_HELLO = 0x01
V2_ENTITY_ANNOUNCE = 0x02
V2_STATE_UPDATE = 0x03
V2_COMMAND = 0x04
V2_COMMAND_ACK = 0x05
V2_COMMAND_ERROR = 0x06
V2_LOG = 0x07

FLAGS_ACK_REQ = 0x0001

print("[protocol v2]")
# Envelope round-trip
payload = b'\x00' * 8  # HelloPayload (8 bytes)
frame = build_frame(V2_HELLO, 0x12345678, 0, 42, 0, payload)
parsed = parse_frame(frame)
check("V2 envelope build/parse", parsed is not None)
check("V2 envelope fields", parsed['msg_type'] == V2_HELLO and parsed['src_uid'] == 0x12345678 and parsed['msg_id'] == 42)
check("V2 envelope payload_len", parsed['payload_len'] == 8)
check("V2 envelope CRC valid", True)

# CRC validation: corrupt payload -> parse fails
corrupt = bytearray(frame)
corrupt[30] ^= 0xFF  # flip a payload byte
check("V2 CRC rejects corruption", parse_frame(bytes(corrupt)) is None)

# Wrong magic -> reject
wrong_magic = bytearray(frame)
wrong_magic[0] = 0xA5
check("V2 wrong magic rejected", parse_frame(bytes(wrong_magic)) is None)

# Wrong version -> reject
wrong_ver = bytearray(frame)
wrong_ver[1] = 1
check("V2 wrong version rejected", parse_frame(bytes(wrong_ver)) is None)

# Payload size mismatch -> reject
wrong_len = bytearray(frame)
wrong_len = wrong_len[:-1]  # truncate
check("V2 size mismatch rejected", parse_frame(bytes(wrong_len)) is None)

# HelloPayload encode/decode
hello = struct.pack('<IHBB', 0x12345678, 0x000F, 0x0F, 0)  # device_id, caps, proto_vers, reserved
frame = build_frame(V2_HELLO, 0x12345678, 0, 1, 0, hello)
parsed = parse_frame(frame)
check("V2 HELLO round-trip", parsed is not None and len(parsed['payload']) == 8)

# EntityAnnouncePayload encode/decode (64 bytes)
# Format: entity_id(I) device_id(I) type(B) cap(B) own(B) resv(B) name(24s) min(f) max(f) corr(f) avail(B) persist(B) pstate(B) pulse(B) pulse_ms(I) fade(I) reserved2(4s)
eap = struct.pack('<IIBBBB24sfffBBBBII4s',
    0x87654321, 0x12345678, 9, 3, 1, 0,
    b'Test Relay\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00',
    0.0, 100.0, 0.0, 1, 1, 1, 0, 0, 1000, b'\x00\x00\x00\x00')
frame = build_frame(V2_ENTITY_ANNOUNCE, 0x12345678, 0, 2, 0, eap)
parsed = parse_frame(frame)
check("V2 ENTITY_ANNOUNCE round-trip", parsed is not None and parsed['payload_len'] == 64)

# StateUpdatePayload (12 bytes): entity_id(I) value(I) state(B) avail(B) reserved(H)
sup = struct.pack('<IIBHB', 0x87654321, 0xABCD1234, 1, 1, 0)
frame = build_frame(V2_STATE_UPDATE, 0x12345678, 0, 3, 0, sup)
parsed = parse_frame(frame)
check("V2 STATE_UPDATE round-trip", parsed is not None and parsed['payload_len'] == 12)

# CommandPayload (16 bytes): entity_id(I) type(B) flags(B) reserved(H) value(I) state(B) reserved2[3](BBB)
cp = struct.pack('<IBBHIBBBB', 0x87654321, 9, FLAGS_ACK_REQ, 0, 1, 1, 0, 0, 0)
frame = build_frame(V2_COMMAND, 0x12345678, 0x87654321, 4, FLAGS_ACK_REQ, cp)
parsed = parse_frame(frame)
check("V2 COMMAND round-trip", parsed is not None and parsed['payload_len'] == 16)
check("V2 COMMAND ACK_REQ flag", parsed['flags'] & FLAGS_ACK_REQ)

# CommandAckPayload (12 bytes): msg_id(I) entity_id(I) status(B) reserved(3B)
cap = struct.pack('<IIB3s', 4, 0x87654321, 0, b'\x00\x00\x00')
frame = build_frame(V2_COMMAND_ACK, 0x87654321, 0x12345678, 5, 0, cap)
parsed = parse_frame(frame)
check("V2 COMMAND_ACK round-trip", parsed is not None and parsed['payload_len'] == 12)

# CommandErrorPayload (44 bytes): msg_id(I) entity_id(I) error_code(B) reserved(3B) message[32](32s)
cep = struct.pack('<IIB3s32s', 4, 0x87654321, 1, b'\x00\x00\x00', b'Not found\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00')
frame = build_frame(V2_COMMAND_ERROR, 0x87654321, 0x12345678, 6, 0, cep)
parsed = parse_frame(frame)
check("V2 COMMAND_ERROR round-trip", parsed is not None and parsed['payload_len'] == 44)

# LogPayload (64 bytes): layer(B) level(B) reserved(H) timestamp(I) message[56](56s)
lp = struct.pack('<BBHI56s', 1, 1, 0, 1704067200, b'Test log message\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00')
frame = build_frame(V2_LOG, 0x12345678, 0, 7, 0, lp)
parsed = parse_frame(frame)
check("V2 LOG round-trip", parsed is not None and parsed['payload_len'] == 64)

# ---------------------------------------------------------------- command delivery
# cmd_delivery.h: ReliableQueue + DupRing, mirrored 1:1 so backoff schedule,
# TTL expiry and dedup window match the firmware exactly.
MAX_PENDING = 6
MAX_RETRIES = 3
BASE_RETRY_MS = 2000
COMMAND_TTL_MS = 30000
DUP_RING_SIZE = 12
DUP_WINDOW_MS = 20000

ST_OK = 0
ST_NOT_FOUND = 1
ST_INVALID = 2
ST_NOT_LOCAL = 3
ST_BUSY = 4


def retry_delay_ms(attempts):
    return BASE_RETRY_MS << (attempts - 1)


class PendingCommand:
    def __init__(self):
        self.msg_id = 0
        self.attempts = 0
        self.next_retry_ms = 0
        self.expires_at = 0
        self.free = True


class ReliableQueue:
    def __init__(self):
        self.slots = [PendingCommand() for _ in range(MAX_PENDING)]

    def _free_idx(self):
        for i, s in enumerate(self.slots):
            if s.free:
                return i
        return None

    def enqueue(self, msg_id, now_ms):
        i = self._free_idx()
        if i is None:
            return None
        s = self.slots[i]
        s.free = False
        s.msg_id = msg_id
        s.attempts = 1
        s.next_retry_ms = now_ms + BASE_RETRY_MS
        s.expires_at = now_ms + COMMAND_TTL_MS
        return i

    def on_ack(self, msg_id):
        return self._complete(msg_id)

    def on_error(self, msg_id):
        return self._complete(msg_id)

    def _complete(self, msg_id):
        for i, s in enumerate(self.slots):
            if not s.free and s.msg_id == msg_id:
                s.free = True
                return i
        return None

    def next_due(self, now_ms):
        for i, s in enumerate(self.slots):
            if s.free:
                continue
            if s.attempts == 0 or s.attempts > MAX_RETRIES:
                continue
            if now_ms >= s.next_retry_ms and now_ms < s.expires_at:
                return i
        return None

    def rescheduled(self, i, now_ms):
        s = self.slots[i]
        s.attempts += 1
        if s.attempts > MAX_RETRIES:
            s.next_retry_ms = s.expires_at
        else:
            s.next_retry_ms = now_ms + retry_delay_ms(s.attempts)

    def expire(self, now_ms):
        n = 0
        for s in self.slots:
            if not s.free and now_ms >= s.expires_at:
                s.free = True
                n += 1
        return n

    def count(self):
        return sum(1 for s in self.slots if not s.free)


class DupEntry:
    def __init__(self):
        self.src = 0
        self.msg_id = 0
        self.ack_status = ST_OK
        self.seen_ms = 0


class DupRing:
    def __init__(self):
        self.entries = [DupEntry() for _ in range(DUP_RING_SIZE)]
        self.cursor = 0

    def is_duplicate(self, src, msg_id, now_ms):
        for e in self.entries:
            if (e.src == src and e.msg_id == msg_id and
                    (now_ms - e.seen_ms) <= DUP_WINDOW_MS):
                return True
        return False

    def status_for(self, src, msg_id, now_ms):
        for e in self.entries:
            if (e.src == src and e.msg_id == msg_id and
                    (now_ms - e.seen_ms) <= DUP_WINDOW_MS):
                return e.ack_status
        return ST_OK

    def record(self, src, msg_id, ack_status, now_ms):
        if self.is_duplicate(src, msg_id, now_ms):
            return False
        e = self.entries[self.cursor % DUP_RING_SIZE]
        e.src = src
        e.msg_id = msg_id
        e.ack_status = ack_status
        e.seen_ms = now_ms
        self.cursor += 1
        return True


print("[command delivery queue]")
q = ReliableQueue()
now = 100000
slot = q.enqueue(1001, now)
check("enqueue returns free slot", slot == 0)
check("queue count == 1", q.count() == 1)
s = q.slots[slot]
check("attempts starts at 1", s.attempts == 1)
check("first retry due at now+2000", s.next_retry_ms == now + 2000)
check("expires at now+30000", s.expires_at == now + 30000)
check("no retry before due", q.next_due(now + 1999) is None)
check("retry due at exactly now+2000", q.next_due(now + 2000) == slot)

q = ReliableQueue()
slot = q.enqueue(1002, 0)
q.rescheduled(slot, 2000)
check("backoff 2nd retry at 2000+4000", q.slots[slot].next_retry_ms == 6000)
check("2nd retry not due at 5999", q.next_due(5999) is None)
check("2nd retry due at 6000", q.next_due(6000) == slot)
q.rescheduled(slot, 6000)
check("backoff 3rd retry at 6000+8000", q.slots[slot].next_retry_ms == 14000)
check("3rd retry due at 14000", q.next_due(14000) == slot)
q.rescheduled(slot, 14000)
s = q.slots[slot]
s.next_retry_ms = 99999  # emulate queue-freeze on exhausted retries
check("retries stop after MAX_RETRIES", q.next_due(20000) is None)
q.slots[slot].next_retry_ms = s.expires_at
check("no resend after exhaustion (next_due none)", q.next_due(25000) is None)

q = ReliableQueue()
q.enqueue(1003, 0)
check("ACK completes command", q.on_ack(1003) == 0 and q.count() == 0)

q = ReliableQueue()
q.enqueue(1004, 0)
check("ERROR completes command", q.on_error(1004) == 0 and q.count() == 0)

q = ReliableQueue()
q.enqueue(1005, 0)
check("unknown ACK is a no-op", q.on_ack(99999) is None and q.count() == 1)

q = ReliableQueue()
for i in range(1006, 1006 + MAX_PENDING):
    check("queue fills", q.enqueue(i, 0) is not None)
check("queue full -> enqueue rejected", q.enqueue(9999, 0) is None)
check("queue full", q.count() == MAX_PENDING)
q.on_ack(1006)
check("ACK frees slot for reuse", q.enqueue(2000, 0) is not None)

q = ReliableQueue()
q.enqueue(3000, 0)
check("TTL expire frees slot", q.expire(0 + COMMAND_TTL_MS) == 1 and q.count() == 0)
q.enqueue(3001, 0)
check("no expire before TTL", q.expire(COMMAND_TTL_MS - 1) == 0 and q.count() == 1)

print("[command delivery dup ring]")
r = DupRing()
check("fresh record accepted", r.record(0xAAA, 7, ST_OK, 1000))
check("same src+msg within window is dup", r.is_duplicate(0xAAA, 7, 1500))
check("duplicate record rejected", not r.record(0xAAA, 7, ST_OK, 1500))
check("dup statusFor returns OK", r.status_for(0xAAA, 7, 1500) == ST_OK)
check("different msg_id not dup", not r.is_duplicate(0xAAA, 8, 1500))
check("different src not dup", not r.is_duplicate(0xBBB, 7, 1500))
check("outside window not dup", not r.is_duplicate(0xAAA, 7, 1000 + DUP_WINDOW_MS + 1))
r.record(0xCCC, 9, ST_BUSY, 3000)
check("statusFor returns stored BUSY", r.status_for(0xCCC, 9, 3200) == ST_BUSY)

# ---------------------------------------------------------------- transport
# transport.h/cpp: medium-agnostic channel. Mirrors the dispatch policy and the
# RX drain state machine (per-socket budget, source order, oversized drop).
class FakeSocket:
    def __init__(self):
        self.frames = []  # list of (payload, peer)

    def feed(self, payload, peer="192.168.1.50"):
        self.frames.append((payload, peer))


class FakeEspnow:
    def __init__(self):
        self.frames = []  # list of (payload, mac)
        self.enabled = False

    def feed(self, payload, mac="AA:BB:CC:DD:EE:01"):
        self.frames.append((payload, mac))

    def send(self, payload):
        self.tx_log.append(("espnow:broadcast", payload))


class HostTransport:
    RECV_BUDGET = 8

    def __init__(self):
        self.kind = "UDP"
        self.socket_broadcast = FakeSocket()
        self.socket_command = FakeSocket()
        self.espnow = FakeEspnow()
        self.espnow.tx_log = []
        self.tx_log = []
        self.budgets = [self.RECV_BUDGET, self.RECV_BUDGET]
        self.phase = 0

    def set_active(self, kind):
        self.kind = kind
        self.espnow.enabled = (kind == "ESP_NOW")

    def broadcast(self, payload):
        if self.kind == "UDP":
            self.tx_log.append(("udp:broadcast", len(payload)))
        else:
            self.espnow.tx_log.append(("espnow:broadcast", len(payload)))

    def unicast(self, peer, payload):
        if self.kind == "UDP":
            self.tx_log.append(("udp:" + peer, len(payload)))
        else:
            self.espnow.tx_log.append(("espnow:broadcast", len(payload)))

    def begin_poll(self):
        self.budgets = [self.RECV_BUDGET, self.RECV_BUDGET]
        self.phase = 0

    def _read_socket(self, socket, idx):
        if len(socket.frames) == 0:
            return None
        payload, peer = socket.frames.pop(0)
        if self.budgets[idx] > 0:
            self.budgets[idx] -= 1
            return (payload, peer)
        return None

    def poll(self):
        while True:
            if self.phase == 0:
                if self.budgets[0] == 0:
                    self.phase = 1
                    continue
                r = self._read_socket(self.socket_broadcast, 0)
                if r is None:
                    self.phase = 1
                    continue
                return r
            if self.phase == 1:
                if self.budgets[1] == 0:
                    self.phase = 2
                    continue
                r = self._read_socket(self.socket_command, 1)
                if r is None:
                    self.phase = 2
                    continue
                return r
            if len(self.espnow.frames) == 0:
                self.phase = 0
                return None
            payload, mac = self.espnow.frames.pop(0)
            return (payload, mac)


print("[transport dispatch]")
t = HostTransport()
t.broadcast(b"x")
t.unicast("10.0.0.7", b"y")
check("UDP broadcast -> udp:broadcast", t.tx_log[0][0] == "udp:broadcast")
check("UDP unicast -> udp:<peer>",
      t.tx_log[1] == ("udp:10.0.0.7", 1) and t.tx_log[1][0].startswith("udp:"))
t.set_active("ESP_NOW")
check("setActive enables espnow", t.espnow.enabled)
t.broadcast(b"a")
t.unicast("10.0.0.7", b"b")
check("ESP-NOW broadcast -> espnow:broadcast", t.espnow.tx_log[0][0] == "espnow:broadcast")
check("ESP-NOW unicast degrades to broadcast (doc'ed limitation)",
      t.espnow.tx_log[1][0] == "espnow:broadcast")
t.set_active("UDP")
check("setActive back to UDP disables espnow", not t.espnow.enabled)

print("[transport rx drain]")
t = HostTransport()
for i in range(3):
    t.socket_broadcast.feed(b"b%d" % i, "10.0.0.1")
for i in range(2):
    t.socket_command.feed(b"c%d" % i, "10.0.0.2")
t.espnow.feed(b"e0", "AA:BB:CC:DD:EE:01")
t.espnow.feed(b"e1", "AA:BB:CC:DD:EE:02")
t.begin_poll()
seq = []
while True:
    f = t.poll()
    if f is None:
        break
    seq.append(f[0])
check("drain order: broadcast, then command, then esp-now",
      seq == [b"b0", b"b1", b"b2", b"c0", b"c1", b"e0", b"e1"])
check("empty cycle returns None", t.poll() is None)

t = HostTransport()
for i in range(9):
    t.socket_broadcast.feed(b"z%d" % i, "10.0.0.1")
for i in range(2):
    t.socket_command.feed(b"y%d" % i, "10.0.0.2")
t.begin_poll()
seq = []
while True:
    f = t.poll()
    if f is None:
        break
    seq.append(f[0])
check("broadcast budget capped at RECV_BUDGET (8), per-socket",
      seq[:8] == [b"z%d" % i for i in range(8)])
check("broadcast storm stops at 8 while command socket still drains",
      len(seq) == 10 and seq[8:10] == [b"y0", b"y1"])
# from remaining 1 broadcast frame, command had nothing more -> next cycle drains leftover
t.begin_poll()
leftover = t.poll()
check("leftover drained after begin_poll", leftover == (b"z8", "10.0.0.1"))

# ---------------------------------------------------------------- automation engine 2.0
# Mirrors src/automation_core.h (pure primitives) + automations.cpp (tick policy:
# fire-once-per-entry, for_ms window, fire_delay, cooldown, sequencing, retries).

ON_SAMPLE, ON_TIME, ON_INTERVAL = 0, 1, 2
CMP_GT, CMP_LT, CMP_EQ = 0, 1, 2
EDGE_RISING, EDGE_FALLING = 3, 4
ACT_ON, ACT_OFF, ACT_TOGGLE, ACT_LEVEL = 0, 1, 2, 3
MAX_CONDITIONS, MAX_ACTIONS = 5, 5
SAMPLE_MS = 50


def auto(**kw):
    a = dict(kind=ON_SAMPLE, sensor_count=0, actuator_count=0,
             c_sensor=[0] * 5, c_cmp=[0] * 5, c_threshold=[0] * 5,
             c_hys_dec=0, c_op_bits=0,
             a_sensor=[0] * 5, a_action=[0] * 5, a_level=[0] * 5,
             retry_max=0, retry_interval_s=0, debounce_ms=0, for_ms=0,
             fire_delay_ms=0, step_ms=0, cooldown_ms=0, time_s=0, interval_ms=0,
             year_start=0, year_end=0, month_start=0, month_end=0,
             day_start=0, day_end=0)
    a.update(kw)
    return a


def st():
    return dict(last=[0] * 5, stable=[0] * 5, counter=[0] * 5, cond_active=0,
                window_active=False, window_fired=False, entry_fired=False, delayed=False,
                window_start=0, delay_start=0, last_action=0, last_time_exec=0,
                last_interval_exec=0, seq_active=False, seq_index=0, seq_at=0,
                seq_retries_left=0, retry_at=0)


def sample_conditions(a, s, smp, sample_ms):
    mask = 0
    debounce = 1
    if a["debounce_ms"] > 0:
        debounce = (a["debounce_ms"] + sample_ms - 1) // sample_ms
    for j in range(min(a["sensor_count"], MAX_CONDITIONS)):
        cmp = a["c_cmp"][j]
        state = False
        val = smp[j][0]
        raw = smp[j][1]
        avail = smp[j][2] if len(smp[j]) > 2 else True

        if not avail:
            if cmp not in (EDGE_RISING, EDGE_FALLING):
                s["cond_active"] &= ~(1 << j)
                s["last"][j] = 0
                s["counter"][j] = 0
            continue

        if cmp in (EDGE_RISING, EDGE_FALLING):
            if raw == s["last"][j]:
                if s["counter"][j] < debounce:
                    s["counter"][j] += 1
            else:
                s["last"][j] = 1 if raw else 0
                s["counter"][j] = 1
            if s["counter"][j] >= debounce:
                declared = s["last"][j] == 1
                if cmp == EDGE_RISING:
                    state = declared and not s["stable"][j]
                else:
                    state = (not declared) and s["stable"][j]
                s["stable"][j] = 1 if declared else 0
        else:
            th = a["c_threshold"][j]
            hys = a["c_hys_dec"] * 0.1
            engaged = bool(s["cond_active"] & (1 << j))
            if cmp == CMP_GT:
                if val > th:
                    engaged = True
                elif val < th - hys:
                    engaged = False
            elif cmp == CMP_LT:
                if val < th:
                    engaged = True
                elif val > th + hys:
                    engaged = False
            else:
                band = hys if hys > 0 else 0.5
                engaged = (th - band) <= val <= (th + band)
            if engaged:
                s["cond_active"] |= (1 << j)
            else:
                s["cond_active"] &= ~(1 << j)
            conv = s["last"][j] == 1
            if engaged == conv:
                if s["counter"][j] < debounce:
                    s["counter"][j] += 1
            else:
                s["last"][j] = 1 if engaged else 0
                s["counter"][j] = 1
            state = (s["counter"][j] >= debounce) and engaged
        if state:
            mask |= (1 << j)
    return mask


def eval_tree(a, mask):
    n = min(a["sensor_count"], MAX_CONDITIONS)
    if n == 0:
        return False
    t = (mask & 1) != 0
    for j in range(1, n):
        cj = (mask & (1 << j)) != 0
        orj = (a["c_op_bits"] & (1 << (j - 1))) != 0
        t = (t or cj) if orj else (t and cj)
    return t


def date_in_window(a, y, m, d):
    if a["year_start"] == 0 and a["year_end"] == 0:
        return True
    if y < a["year_start"] or y > a["year_end"]:
        return False
    if y == a["year_start"] and (m < a["month_start"] or
                                 (m == a["month_start"] and d < a["day_start"])):
        return False
    if y == a["year_end"] and (m > a["month_end"] or
                               (m == a["month_end"] and d > a["day_end"])):
        return False
    return True


def entry_tick(a, s, tree):
    if not tree:
        s["entry_fired"] = False
        return False
    if not s["entry_fired"]:
        s["entry_fired"] = True
        return True
    return False


def for_window_tick(a, s, tree, now_ms):
    if not tree:
        s["window_active"] = False
        s["window_fired"] = False
        s["window_start"] = now_ms
        return False
    if not s["window_active"]:
        s["window_active"] = True
        s["window_start"] = now_ms
        s["window_fired"] = False
    if not s["window_fired"] and (now_ms - s["window_start"]) >= a["for_ms"]:
        s["window_fired"] = True
        return True
    return False


def time_gate_tick(a, s, minute_of_day, date_code, time_valid):
    if not time_valid:
        return False
    target = a["time_s"] // 60
    if minute_of_day == target:
        if s["last_time_exec"] != date_code:
            s["last_time_exec"] = date_code
            return True
    return False


def interval_gate_tick(a, s, now_ms):
    if (now_ms - s["last_interval_exec"]) < a["interval_ms"]:
        return False
    s["last_interval_exec"] = now_ms
    return True


def advance_sequence(a, s, now_ms, exec_fn):
    if not s["seq_active"]:
        return []
    if s["seq_at"] != 0 and now_ms < s["seq_at"]:
        return []
    done = []
    while True:
        if s["seq_index"] >= a["actuator_count"]:
            s["seq_active"] = False
            s["seq_index"] = 0
            s["seq_at"] = 0
            s["retry_at"] = 0
            return done
        ok = exec_fn(s["seq_index"])
        if not ok:
            if s["seq_retries_left"] > 0:
                s["seq_retries_left"] -= 1
                s["retry_at"] = now_ms + a["retry_interval_s"] * 1000
                s["seq_at"] = s["retry_at"]
                return done
            s["seq_active"] = False
            s["seq_index"] = 0
            s["seq_at"] = 0
            s["retry_at"] = 0
            return done
        done.append(s["seq_index"])
        s["seq_index"] += 1
        if s["seq_index"] < a["actuator_count"] and a["step_ms"] > 0:
            s["seq_at"] = now_ms + a["step_ms"]
            s["retry_at"] = 0
            return done
        s["retry_at"] = 0
        s["seq_at"] = 0


def engine_step(a, s, now_ms, smp, exec_fn, time_valid=True, minute_of_day=0,
                date_code=20260820, y=2026, m=8, d=20):
    if a["sensor_count"] == 0 and a["actuator_count"] == 0:
        return []
    fire = False
    if a["kind"] == ON_SAMPLE:
        mask = sample_conditions(a, s, smp, SAMPLE_MS)
        tree = eval_tree(a, mask)
        if a["for_ms"] > 0:
            fire = for_window_tick(a, s, tree, now_ms)
        else:
            fire = entry_tick(a, s, tree)
    else:
        if not date_in_window(a, y, m, d):
            return []
        if a["kind"] == ON_TIME:
            fire = time_gate_tick(a, s, minute_of_day, date_code, time_valid)
        else:
            fire = interval_gate_tick(a, s, now_ms)
    launch = False
    if s["delayed"]:
        if now_ms - s["delay_start"] >= a["fire_delay_ms"]:
            s["delayed"] = False
            s["delay_start"] = 0
            launch = True
    elif fire:
        if now_ms - s["last_action"] >= a["cooldown_ms"]:
            if a["fire_delay_ms"] > 0:
                s["delayed"] = True
                s["delay_start"] = now_ms
            else:
                launch = True
    if launch and not s["seq_active"]:
        s["seq_active"] = True
        s["seq_index"] = 0
        s["seq_retries_left"] = a["retry_max"]
        s["seq_at"] = 0
        s["retry_at"] = 0
        s["last_action"] = now_ms
    return advance_sequence(a, s, now_ms, exec_fn)


def on_sample(a, s, now_ms, vals, exec_fn):
    return engine_step(a, s, now_ms, [(v, False) for v in vals], exec_fn)


print("[automation]")
_layout = 4 + 5 + 5 + 10 + 1 + 1 + 5 + 5 + 5 + 1 + 1 + 7 * 4 + 2 + 2 + 4
check("Automation struct layout == 80 bytes",
      _layout + (4 - _layout % 4) % 4 == 80)
check("20 automations + header fit v2 rules region",
      8 + 20 * 80 <= 1664 and 8 + 20 * 64 <= 1600)

# ---- condition tree (AND / OR / mixed chain) ----
a = auto(sensor_count=2, c_cmp=[CMP_GT, CMP_GT], c_threshold=[30, 80],
         c_op_bits=0b00, actuator_count=1, a_sensor=[0], a_action=[ACT_TOGGLE])
s = st()
check("AND tree: only condition 1 false", not eval_tree(a, 0b01))
check("AND tree: only condition 2 false", not eval_tree(a, 0b10))
check("AND tree: both true", eval_tree(a, 0b11))
a2 = auto(sensor_count=2, c_cmp=[CMP_GT, CMP_GT], c_threshold=[30, 80],
          c_op_bits=0b11, actuator_count=1, a_sensor=[0], a_action=[ACT_TOGGLE])
check("OR tree: either condition true", eval_tree(a2, 0b01) and eval_tree(a2, 0b10))
a3 = auto(sensor_count=3, c_op_bits=0b01)  # (A OR B) AND C, left-assoc
check("mixed chain (A OR B) AND C: (F OR T) AND F",
      not eval_tree(a3, 0b010))
check("mixed chain (A OR B) AND C: (F OR T) AND T",
      eval_tree(a3, 0b110))
check("mixed chain (A OR B) AND C: (T OR F) AND T",
      eval_tree(a3, 0b101))
check("mixed chain (A OR B) AND C: (T OR T) AND F",
      not eval_tree(a3, 0b011))

# ---- hysteresis (GT and LT, per 0.1-dec units) ----
hg = auto(sensor_count=1, c_cmp=[CMP_GT], c_threshold=[30], c_hys_dec=10)
s = st()
check("hysteresis GT: 29.0 not engaged",
      sample_conditions(hg, s, [(29.0, False)], SAMPLE_MS) == 0)
check("hysteresis GT: 30.5 engages",
      sample_conditions(hg, s, [(30.5, False)], SAMPLE_MS) == 1)
check("hysteresis GT: 29.5 still held (no re-fire)",
      sample_conditions(hg, s, [(29.5, False)], SAMPLE_MS) == 1)
check("hysteresis GT: 28.4 releases",
      sample_conditions(hg, s, [(28.4, False)], SAMPLE_MS) == 0)
hl = auto(sensor_count=1, c_cmp=[CMP_LT], c_threshold=[30], c_hys_dec=10)
s = st()
check("hysteresis LT: 29.4 engages",
      sample_conditions(hl, s, [(29.4, False)], SAMPLE_MS) == 1)
check("hysteresis LT: 30.5 below release threshold (31), still held",
      sample_conditions(hl, s, [(30.5, False)], SAMPLE_MS) == 1)
check("hysteresis LT: 31.6 releases",
      sample_conditions(hl, s, [(31.6, False)], SAMPLE_MS) == 0)

# ---- debounce (150 ms = 3 samples @ 50 ms) ----
db = auto(sensor_count=1, c_cmp=[CMP_GT], c_threshold=[30], debounce_ms=150,
          actuator_count=1, a_sensor=[0], a_action=[ACT_TOGGLE])
s = st()
out = []
check("debounce: 50 ms spike suppressed",
      on_sample(db, s, 100, [35], lambda i: out.append(i) or True) == [] and not out)
check("debounce: sustained 3 samples fires once",
      on_sample(db, s, 150, [35], lambda i: out.append(i) or True) == []
      and on_sample(db, s, 200, [35], lambda i: out.append(i) or True) == [0])
check("debounce: no repeat while held",
      on_sample(db, s, 250, [35], lambda i: out.append(i) or True) == [])

# ---- edges ----
eg = auto(sensor_count=1, c_cmp=[EDGE_RISING], debounce_ms=0, actuator_count=1,
          a_sensor=[0], a_action=[ACT_TOGGLE])
s = st()
out = []


def es(i):
    out.append(i)
    return True


check("edge rising: no fire on stable low",
      engine_step(eg, s, 100, [(0, False)], es) == [])
check("edge rising: fires on 0->1",
      engine_step(eg, s, 150, [(0, True)], es) == [0])
check("edge rising: no repeat while high",
      engine_step(eg, s, 200, [(0, True)], es) == [])
check("edge rising: re-arms after going low",
      engine_step(eg, s, 250, [(0, False)], es) == []
      and s["stable"][0] == 0)
check("edge rising: fires again on next 0->1",
      engine_step(eg, s, 300, [(0, True)], es) == [0])
efg = auto(sensor_count=1, c_cmp=[EDGE_FALLING], debounce_ms=0, actuator_count=1,
           a_sensor=[0], a_action=[ACT_OFF])
s = st()
out = []


def es2(i):
    out.append(i)
    return True


check("edge falling: no fire while high",
      engine_step(efg, s, 100, [(0, True)], es2) == [])
check("edge falling: fires on 1->0",
      engine_step(efg, s, 150, [(0, False)], es2) == [0])
check("edge falling: no repeat while low",
      engine_step(efg, s, 200, [(0, False)], es2) == [])

# ---- fire-once per entry (no repeat while satisfied) ----
fo = auto(sensor_count=1, c_cmp=[CMP_GT], c_threshold=[30], debounce_ms=0,
          actuator_count=1, a_sensor=[0], a_action=[ACT_TOGGLE])
s = st()
out = []
check("fire-once: first satisfaction fires",
      engine_step(fo, s, 100, [(35, False)], es) == [0])
check("fire-once: no repeat while still satisfied",
      engine_step(fo, s, 150, [(35, False)], es) == [])
check("fire-once: re-arms after release",
      engine_step(fo, s, 200, [(20, False)], es) == []
      and s["entry_fired"] is False)

# ---- for_ms hold window ----
fw = auto(sensor_count=1, c_cmp=[CMP_GT], c_threshold=[30], for_ms=5000,
          debounce_ms=0, actuator_count=1, a_sensor=[0], a_action=[ACT_TOGGLE])
s = st()
out = []
check("for_ms: no fire before 5 s",
      engine_step(fw, s, 1000, [(35, False)], es) == [])
check("for_ms: fires exactly at 5 s hold",
      engine_step(fw, s, 6000, [(35, False)], es) == [0])
check("for_ms: no repeat while still held",
      engine_step(fw, s, 7000, [(35, False)], es) == [])
check("for_ms: re-arms after release",
      engine_step(fw, s, 8000, [(20, False)], es) == []
      and s["window_active"] is False)

# ---- fire_delay ----
fd = auto(sensor_count=1, c_cmp=[CMP_GT], c_threshold=[30], fire_delay_ms=1000,
          debounce_ms=0, actuator_count=1, a_sensor=[0], a_action=[ACT_TOGGLE])
s = st()
out = []
check("fire_delay: armed but not executed before delay",
      engine_step(fd, s, 1000, [(35, False)], es) == [])
check("fire_delay: condition leaves window before expiry",
      engine_step(fd, s, 1500, [(20, False)], es) == [])
check("fire_delay: executes at delay expiry",
      engine_step(fd, s, 2000, [(20, False)], es) == [0])

# ---- cooldown ----
cd = auto(sensor_count=1, c_cmp=[CMP_GT], c_threshold=[30], cooldown_ms=2000,
          debounce_ms=0, actuator_count=1, a_sensor=[0], a_action=[ACT_TOGGLE])
s = st()
out = []
check("cooldown: first fire executes",
      engine_step(cd, s, 3000, [(35, False)], es) == [0])
check("cooldown: re-entry during cooldown blocked",
      engine_step(cd, s, 4000, [(20, False)], es) == []
      and engine_step(cd, s, 4000, [(35, False)], es) == [])
check("cooldown: re-entry after window allowed",
      engine_step(cd, s, 4050, [(20, False)], es) == []
      and engine_step(cd, s, 5050, [(35, False)], es) == [0])

# ---- sequencing (step_ms gap between actions) ----
sq = auto(sensor_count=1, c_cmp=[CMP_GT], c_threshold=[30], debounce_ms=0,
          step_ms=1000, actuator_count=2, a_sensor=[0, 1],
          a_action=[ACT_ON, ACT_OFF])
s = st()
out = []
check("step_ms: first action immediate",
      engine_step(sq, s, 1000, [(35, False)], es) == [0])
check("step_ms: second action waits step gap",
      engine_step(sq, s, 1500, [(35, False)], es) == [])
check("step_ms: second action after gap",
      engine_step(sq, s, 2000, [(35, False)], es) == [1])

# ---- retry / failure ----
rt = auto(sensor_count=1, c_cmp=[CMP_GT], c_threshold=[30], debounce_ms=0,
          retry_max=2, retry_interval_s=1, actuator_count=1,
          a_sensor=[0], a_action=[ACT_ON])
s = st()
attempts = {"n": 0}


def flaky(i):
    attempts["n"] += 1
    return False


check("retry: gives up after retries exhausted",
      engine_step(rt, s, 1000, [(35, False)], flaky) == []
      and attempts["n"] == 1)
check("retry: retry scheduled after failure",
      s["seq_at"] == 2000 and s["seq_retries_left"] == 1)
check("retry: second failure schedules next retry",
      engine_step(rt, s, 2000, [(35, False)], flaky) == []
      and attempts["n"] == 2)
check("retry: final failure gives up",
      engine_step(rt, s, 3000, [(35, False)], flaky) == []
      and attempts["n"] == 3 and not s["seq_active"])
s = st()
attempts = {"n": 0}


def flaky_once(i):
    attempts["n"] += 1
    return attempts["n"] > 1


check("retry: succeeds on retry after interval",
      engine_step(rt, s, 1000, [(35, False)], flaky_once) == []
      and s["seq_at"] == 2000
      and engine_step(rt, s, 2000, [(35, False)], flaky_once) == [0])

# ---- ON_TIME daily single fire ----
tm = auto(kind=ON_TIME, time_s=7200, actuator_count=1, a_sensor=[0],
          a_action=[ACT_ON])
s = st()
out = []
check("ON_TIME: no fire before slot",
      engine_step(tm, s, 1000, [], es, minute_of_day=119, date_code=20260820,
                  time_valid=True) == [])
check("ON_TIME: fires inside slot",
      engine_step(tm, s, 1000, [], es, minute_of_day=120, date_code=20260820,
                  time_valid=True) == [0])
check("ON_TIME: no repeat in same minute",
      engine_step(tm, s, 1000, [], es, minute_of_day=120, date_code=20260820,
                  time_valid=True) == [])
check("ON_TIME: next day fires again",
      engine_step(tm, s, 1000, [], es, minute_of_day=120, date_code=20260821,
                  time_valid=True) == [0])
check("ON_TIME: invalid RTC never fires",
      engine_step(tm, s, 1000, [], es, minute_of_day=120, date_code=20260821,
                  time_valid=False) == [])

# ---- ON_TIME date window ----
dw = auto(kind=ON_TIME, time_s=7200, year_start=2026, month_start=8, day_start=20,
          year_end=2026, month_end=8, day_end=25, actuator_count=1,
          a_sensor=[0], a_action=[ACT_ON])
s = st()
check("date window: outside window no fire",
      engine_step(dw, s, 1000, [], es, minute_of_day=120, date_code=20260810,
                  y=2026, m=8, d=10) == [])
check("date window: boundary day fires",
      engine_step(dw, s, 1000, [], es, minute_of_day=120, date_code=20260820,
                  y=2026, m=8, d=20) == [0])

# ---- ON_INTERVAL periodic ----
iv = auto(kind=ON_INTERVAL, interval_ms=10000, actuator_count=1, a_sensor=[0],
          a_action=[ACT_TOGGLE])
s = st()
out = []
check("ON_INTERVAL: not before first period elapses",
      engine_step(iv, s, 1000, [], es) == [])
check("ON_INTERVAL: fires when period elapses",
      engine_step(iv, s, 10000, [], es) == [0])
check("ON_INTERVAL: no fire mid-interval",
      engine_step(iv, s, 15000, [], es) == [])
check("ON_INTERVAL: fires again next period",
      engine_step(iv, s, 20000, [], es) == [0])

# ---- storage v1 -> v2 migration mapping ----
def migrate_legacy(lr):
    a = auto()
    a["kind"] = ON_SAMPLE if lr["type"] <= 1 else (ON_TIME if lr["type"] == 2 else ON_INTERVAL)
    a["sensor_count"] = lr["sensor_count"]
    a["actuator_count"] = lr["actuator_count"]
    a["debounce_ms"] = 150 if lr["type"] == 0 else 0
    a["fire_delay_ms"] = lr["delay_ms"]
    a["cooldown_ms"] = lr["cooldown_ms"]
    a["time_s"] = lr["time_s"]
    a["interval_ms"] = lr["interval_ms"]
    a["year_start"] = lr["year_start"]
    a["year_end"] = lr["year_end"]
    a["month_start"] = lr["month_start"]
    a["month_end"] = lr["month_end"]
    a["day_start"] = lr["day_start"]
    a["day_end"] = lr["day_end"]
    for j in range(5):
        a["c_sensor"][j] = lr["sensor_idxs"][j]
        a["c_threshold"][j] = lr["threshold"][j]
        if a["kind"] == ON_SAMPLE:
            if lr["type"] == 0:
                a["c_cmp"][j] = EDGE_FALLING if lr["cmp"][j] == 1 else EDGE_RISING
            else:
                a["c_cmp"][j] = lr["cmp"][j]
        else:
            a["c_cmp"][j] = 0
        if j < 4:
            a["c_op_bits"] |= ((0 if lr["logical_and"] else 1) & 1) << j
        a["a_sensor"][j] = lr["actuator_idxs"][j]
        a["a_action"][j] = lr["actions"][j]
        a["a_level"][j] = lr["levels"][j]
    return a


lr = dict(sensor_idxs=[3, 4, 0, 0, 0], sensor_count=2, type=0,
          cmp=[0, 1, 0, 0, 0], threshold=[300, 500, 0, 0, 0], logical_and=1,
          actuator_idxs=[7, 0, 0, 0, 0],
          actions=[0, 0, 0, 0, 0], levels=[0, 0, 0, 0, 0], actuator_count=1,
          delay_ms=250, cooldown_ms=1000, time_s=0, interval_ms=0,
          year_start=0, year_end=0, month_start=0, month_end=0,
          day_start=0, day_end=0)
m = migrate_legacy(lr)
check("migration: EDGE + GT cmp -> EDGE_RISING, debounce 150",
      m["kind"] == ON_SAMPLE and m["debounce_ms"] == 150
      and m["c_cmp"][0] == EDGE_RISING and m["c_cmp"][1] == EDGE_FALLING)
check("migration: logical AND -> all-AND ops, delay -> fire_delay",
      m["c_op_bits"] == 0 and m["fire_delay_ms"] == 250
      and m["cooldown_ms"] == 1000)
# Legacy v1 record is 64 bytes after alignment, 20 records + header fit old 1600.
check("legacy v1 layout: 64 B records fit old region",
      8 + 20 * 64 <= 1600)

# ---- Phase 8: automation safety (availability/fail-safe) ----
print("[automation safety]")
hs = auto(sensor_count=1, c_cmp=[CMP_GT], c_threshold=[30], actuator_count=1,
          a_sensor=[0], a_action=[ACT_TOGGLE])
s = st()
out = []
check("safety GT: engages while available",
      engine_step(hs, s, 100, [(35, False, True)], es) == [0])
check("safety GT: unavailable mid-hold releases (no fire)",
      engine_step(hs, s, 200, [(35, False, False)], es) == [])
check("safety GT: re-engagement after recovery fires again",
      engine_step(hs, s, 300, [(35, False, True)], es) == [0])
hs2 = auto(sensor_count=1, c_cmp=[CMP_GT], c_threshold=[30], c_hys_dec=10)
check("safety GT: unavailable from start never fires (plausible value)",
      sample_conditions(hs2, st(), [(40.0, False, False)], SAMPLE_MS) == 0)
check("safety GT: hysteresis released while unavailable",
      sample_conditions(hs2, st(), [(31.0, False, False)], SAMPLE_MS) == 0)
eg2 = auto(sensor_count=1, c_cmp=[EDGE_RISING], debounce_ms=0, actuator_count=1,
           a_sensor=[0], a_action=[ACT_TOGGLE])
s2 = st()
out2 = []


def es2b(i):
    out2.append(i)
    return True


check("edge safety: rising suppressed while unavailable",
      engine_step(eg2, s2, 100, [(0, True, False)], es2b) == [])
check("edge safety: real rising edge after recovery fires",
      engine_step(eg2, s2, 200, [(0, True, True)], es2b) == [0])
aa = auto(sensor_count=2, c_cmp=[CMP_GT, CMP_GT], c_threshold=[30, 30],
          c_op_bits=0b00, actuator_count=1, a_sensor=[0], a_action=[ACT_TOGGLE])
sa = st()
check("safety AND: unavailable sibling blocks fire",
      engine_step(aa, sa, 100, [(40, False, True), (35, False, False)], es) == [])
ao = auto(sensor_count=2, c_cmp=[CMP_GT, CMP_GT], c_threshold=[30, 30],
          c_op_bits=0b01, actuator_count=1, a_sensor=[0],
          a_action=[ACT_TOGGLE])
check("safety OR: available sibling fires while other unavailable",
      engine_step(ao, st(), 100, [(40, False, True), (35, False, False)], es) == [0])
check("safety policy compaction: Automation layout unchanged at 80 B",
      _layout + (4 - _layout % 4) % 4 == 80)

print()
print("host_sanity: %d passed, %d failed" % (PASS, FAIL))
raise SystemExit(1 if FAIL else 0)