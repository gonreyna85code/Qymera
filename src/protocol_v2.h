#pragma once
#include <stdint.h>

// ================================================================
// QYMERA PROTOCOL V2 - Explicit Envelope/Payload Separation
//
// Coexists with legacy v1-v5 (magic 0xA5). V2 uses magic 0xA6.
// Dual-stack: parse both, generate V2 for new messages.
// ================================================================

namespace qymera {
namespace protocol {
namespace v2 {

// Magic byte for V2 frames (distinct from legacy 0xA5)
static const uint8_t V2_MAGIC = 0xA6;
static const uint8_t V2_VERSION = 2;

// V2 Message Types
enum class MsgType : uint8_t {
  HELLO           = 0x01,  // Device introduction, capabilities
  ENTITY_ANNOUNCE = 0x02,  // Entity config + initial state
  STATE_UPDATE    = 0x03,  // State delta (entity_id + state/value)
  COMMAND         = 0x04,  // Actuator command (target entity_id)
  COMMAND_ACK     = 0x05,  // ACK for COMMAND (msg_id + status)
  COMMAND_ERROR   = 0x06,  // Error response
  LOG             = 0x07,  // Structured log entry
};

// V2 Envelope Flags
enum class Flags : uint16_t {
  NONE         = 0x0000,
  ACK_REQ      = 0x0001,  // Request COMMAND_ACK
  ENCRYPTED    = 0x0002,  // Payload encrypted (future)
  FRAGMENTED   = 0x0004,  // Part of fragmented message
  LAST_FRAG    = 0x0008,  // Last fragment
};

// V2 Envelope (fixed 24 bytes)
#pragma pack(push, 1)
struct Envelope {
  uint8_t  magic;         // 0xA6
  uint8_t  version;       // 2
  uint16_t flags;         // Flags bitmask
  uint32_t msg_id;        // Monotonic per sender
  uint16_t sequence;      // Fragment sequence (0 = unfragmented)
  uint32_t src_uid;       // Source device chip ID
  uint32_t dst_uid;       // Destination device (0 = broadcast)
  uint8_t  msg_type;      // MsgType enum
  uint16_t payload_len;   // Payload length in bytes
  uint16_t crc16;         // CRC16 of payload (0 = none)
  uint8_t  reserved;      // Padding to 24 bytes
};
#pragma pack(pop)
static_assert(sizeof(Envelope) == 24, "V2 Envelope must be 24 bytes");

// ================================================================
// V2 Payload Definitions
// ================================================================

// HELLO: Device introduction, broadcast periodically
#pragma pack(push, 1)
struct HelloPayload {
  uint32_t device_id;          // Chip ID (GET_CHIP_ID())
  uint16_t capabilities;       // Capability bitmask
  uint8_t  protocol_versions;  // Supported legacy versions bitmask (v1-v5)
  uint8_t  reserved;
};
#pragma pack(pop)
static_assert(sizeof(HelloPayload) == 8, "HelloPayload must be 8 bytes");

// Capability bitmask for HelloPayload
enum class Capability : uint16_t {
  NONE              = 0x0000,
  ENTITY_ANNOUNCE   = 0x0001,  // Supports V2 entity announce
  STATE_UPDATE      = 0x0002,  // Supports V2 state updates
  COMMAND_ACK       = 0x0004,  // Supports COMMAND_ACK
  COMMAND_ERROR     = 0x0008,  // Supports COMMAND_ERROR
  RELIABLE_DELIVERY = 0x0010,  // Implements retry/ACK
  ENCRYPTION        = 0x0020,  // Supports payload encryption
  TIME_SYNC         = 0x0040,  // Can provide time sync
};

// ENTITY_ANNOUNCE: Full entity config + initial state (replaces legacy announce)
#pragma pack(push, 1)
struct EntityAnnouncePayload {
  uint32_t entity_id;      // Stable entity ID (NEW!)
  uint32_t device_id;      // Owner device chip ID
  uint8_t  type;           // SensorType enum value
  uint8_t  capabilities;   // READ/WRITE/READ_WRITE (model::EntityCapability)
  uint8_t  ownership;      // LOCAL/REMOTE (model::EntityOwnership)
  uint8_t  reserved;
  char     name[24];       // Null-terminated
  float    min;            // Calibration min
  float    max;            // Calibration max
  float    correction;     // Calibration offset
  uint8_t  avail;          // Availability counter
  uint8_t  persist;        // State persistence enabled
  uint8_t  pers_state;     // Last persisted state
  uint8_t  pulse;          // Pulse mode
  uint32_t pulse_ms;       // Pulse duration
  uint32_t fade;           // Fade duration (ms)
  uint8_t  reserved2[4];   // Padding to 64 bytes
};
#pragma pack(pop)
static_assert(sizeof(EntityAnnouncePayload) == 64, "EntityAnnouncePayload must be 64 bytes");

// STATE_UPDATE: Minimal state delta (entity_id + state/value/avail)
#pragma pack(push, 1)
struct StateUpdatePayload {
  uint32_t entity_id;      // Target entity
  uint32_t value;          // Encoded value
  uint8_t  state;          // Boolean state
  uint8_t  avail;          // Availability
  uint16_t reserved;
};
#pragma pack(pop)
static_assert(sizeof(StateUpdatePayload) == 12, "StateUpdatePayload must be 12 bytes");

// COMMAND: Actuator command targeting entity_id
#pragma pack(push, 1)
struct CommandPayload {
  uint32_t entity_id;      // Target entity (stable ID!)
  uint8_t  type;           // SensorType (RELAY/DIMMER)
  uint8_t  flags;          // ACK_REQ, etc.
  uint16_t reserved;
  uint32_t value;          // Encoded value (dimmer level, relay flag)
  uint8_t  state;          // Target state
  uint8_t  reserved2[3];
};
#pragma pack(pop)
static_assert(sizeof(CommandPayload) == 16, "CommandPayload must be 16 bytes");

// COMMAND_ACK: Acknowledgment for COMMAND
#pragma pack(push, 1)
struct CommandAckPayload {
  uint32_t msg_id;         // Original envelope.msg_id
  uint32_t entity_id;      // Target entity
  uint8_t  status;         // 0=OK, 1=NOT_FOUND, 2=INVALID_TYPE, 3=NOT_AUTHORIZED, 4=BUSY
  uint8_t  reserved[3];
};
#pragma pack(pop)
static_assert(sizeof(CommandAckPayload) == 12, "CommandAckPayload must be 12 bytes");

// COMMAND_ERROR: Error response
#pragma pack(push, 1)
struct CommandErrorPayload {
  uint32_t msg_id;         // Original envelope.msg_id
  uint32_t entity_id;      // Target entity (0 if unknown)
  uint8_t  error_code;     // Same as status in ACK
  uint8_t  reserved[3];
  char     message[32];    // Human-readable error
};
#pragma pack(pop)
static_assert(sizeof(CommandErrorPayload) == 44, "CommandErrorPayload must be 44 bytes");

// LOG: Structured log entry (replaces legacy LogPacket)
#pragma pack(push, 1)
struct LogPayload {
  uint8_t layer;           // CORE=0, SENSORS=1, EVENTS=2, NETWORK=3, SECURITY=4
  uint8_t level;           // INFO=0, WARN=1, ERROR=2, DEBUG=3
  uint16_t reserved;
  uint32_t timestamp;      // Unix epoch (UTC)
  char     message[56];    // Null-terminated
};
#pragma pack(pop)
static_assert(sizeof(LogPayload) == 64, "LogPayload must be 64 bytes");

// ================================================================
// CRC16 (CCITT) for payload integrity
// ================================================================
inline uint16_t crc16_ccitt(const uint8_t *data, uint16_t len, uint16_t crc = 0xFFFF) {
  while (len--) {
    crc ^= (uint16_t)*data++ << 8;
    for (int i = 0; i < 8; i++) {
      crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
    }
  }
  return crc;
}

// ================================================================
// Helpers: Build V2 frames (for senders)
// ================================================================

// Build a V2 frame into a pre-allocated buffer.
// Returns total frame size (envelope + payload), or 0 on error.
inline uint16_t buildFrame(uint8_t *buf, uint16_t buf_len,
                           MsgType type, uint32_t src_uid, uint32_t dst_uid,
                           uint32_t msg_id, uint16_t flags,
                           const void *payload, uint16_t payload_len) {
  if (buf_len < sizeof(Envelope) + payload_len) return 0;
  Envelope env = {};
  env.magic = V2_MAGIC;
  env.version = V2_VERSION;
  env.flags = flags;
  env.msg_id = msg_id;
  env.sequence = 0;
  env.src_uid = src_uid;
  env.dst_uid = dst_uid;
  env.msg_type = (uint8_t)type;
  env.payload_len = payload_len;
  env.crc16 = (payload_len > 0) ? crc16_ccitt((const uint8_t*)payload, payload_len) : 0;

  memcpy(buf, &env, sizeof(env));
  if (payload_len > 0) memcpy(buf + sizeof(env), payload, payload_len);
  return sizeof(Envelope) + payload_len;
}

// ================================================================
// Helpers: Parse V2 frame (for receivers)
// Returns true if valid V2 frame parsed, false otherwise.
// On success: envelope filled, payload_ptr points to payload (if any),
// payload_len set.
// ================================================================
inline bool parseFrame(const uint8_t *buf, uint16_t len,
                       Envelope &envelope,
                       const uint8_t *&payload_ptr,
                       uint16_t &payload_len) {
  if (len < sizeof(Envelope)) return false;
  memcpy(&envelope, buf, sizeof(envelope));
  if (envelope.magic != V2_MAGIC) return false;
  if (envelope.version != V2_VERSION) return false;
  if (envelope.msg_type < 1 || envelope.msg_type > 7) return false;
  if (len != sizeof(Envelope) + envelope.payload_len) return false;
  if (envelope.payload_len > 0) {
    if (envelope.crc16 != crc16_ccitt(buf + sizeof(Envelope), envelope.payload_len)) {
      return false;  // CRC mismatch
    }
  }
  payload_ptr = buf + sizeof(Envelope);
  payload_len = envelope.payload_len;
  return true;
}

// V2 Callback Types (for registration)
typedef void (*V2EntityAnnounceCallback)(
  uint32_t remote_uid,
  const char *remote_ip,
  const EntityAnnouncePayload &payload);

typedef void (*V2StateUpdateCallback)(
  uint32_t remote_uid,
  const StateUpdatePayload &payload);

// Returns an ACK status code (0 = OK; see qymera::delivery::AckStatus).
// The transport layer sends COMMAND_ACK/COMMAND_ERROR on behalf of the handler.
typedef uint8_t (*V2CommandCallback)(
  uint32_t remote_uid,
  const char *remote_ip,
  uint32_t msg_id,
  const CommandPayload &payload);

typedef void (*V2CommandAckCallback)(
  uint32_t remote_uid,
  const CommandAckPayload &payload);

typedef void (*V2CommandErrorCallback)(
  uint32_t remote_uid,
  const CommandErrorPayload &payload);

// ================================================================
// Type-safe payload access (after successful parseFrame)
// ================================================================
inline const HelloPayload* asHello(const uint8_t *p, uint16_t l) {
  return (l == sizeof(HelloPayload)) ? (const HelloPayload*)p : nullptr;
}
inline const EntityAnnouncePayload* asEntityAnnounce(const uint8_t *p, uint16_t l) {
  return (l == sizeof(EntityAnnouncePayload)) ? (const EntityAnnouncePayload*)p : nullptr;
}
inline const StateUpdatePayload* asStateUpdate(const uint8_t *p, uint16_t l) {
  return (l == sizeof(StateUpdatePayload)) ? (const StateUpdatePayload*)p : nullptr;
}
inline const CommandPayload* asCommand(const uint8_t *p, uint16_t l) {
  return (l == sizeof(CommandPayload)) ? (const CommandPayload*)p : nullptr;
}
inline const CommandAckPayload* asCommandAck(const uint8_t *p, uint16_t l) {
  return (l == sizeof(CommandAckPayload)) ? (const CommandAckPayload*)p : nullptr;
}
inline const CommandErrorPayload* asCommandError(const uint8_t *p, uint16_t l) {
  return (l == sizeof(CommandErrorPayload)) ? (const CommandErrorPayload*)p : nullptr;
}
inline const LogPayload* asLog(const uint8_t *p, uint16_t l) {
  return (l == sizeof(LogPayload)) ? (const LogPayload*)p : nullptr;
}

}  // namespace v2
}  // namespace protocol
}  // namespace qymera