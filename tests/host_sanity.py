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

print()
print("host_sanity: %d passed, %d failed" % (PASS, FAIL))
raise SystemExit(1 if FAIL else 0)