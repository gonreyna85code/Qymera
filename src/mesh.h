#pragma once
#include <Arduino.h>
#include "config.h"
#include "protocol_v2.h"

namespace mesh {

// ================= TRANSPORT MODE =================
enum Transport : uint8_t {
  TRANSPORT_UDP = 0,
  TRANSPORT_ESPNOW = 1
};

// Inicialización
void init();
void setTransport(Transport t);
Transport getTransport();

// ============================================================================
// Devices remotos
// ============================================================================

struct RemoteDevice {
  uint32_t uid;
  char ip[16];
  unsigned long last_seen;
  bool online;
};

// ============================================================================
// Callbacks
// ============================================================================

typedef void (*SensorDiscoveryCallback)(
  uint32_t device_uid,
  const char *device_ip,
  uint32_t sensor_id,
  const String &sensor_name,
  uint8_t sensor_type,
  bool sensor_state,
  uint32_t sensor_value,
  float sensor_min,
  float sensor_max,
  float sensor_correction,
  uint8_t sensor_avail,
  uint32_t sensor_fade,
  bool sensor_persist,
  bool sensor_pers_state,
  bool sensor_pulse,
  uint32_t sensor_pulse_ms);

typedef void (*CommandCallback)(
  uint8_t command_type,
  uint32_t sensor_id,
  uint32_t value,
  bool state);

// V2 Protocol Callbacks (Phase 3)
typedef void (*V2EntityAnnounceCallback)(
  uint32_t remote_uid,
  const char *remote_ip,
  const qymera::protocol::v2::EntityAnnouncePayload &payload);

typedef void (*V2StateUpdateCallback)(
  uint32_t remote_uid,
  const qymera::protocol::v2::StateUpdatePayload &payload);

// Returns an ACK status code (0 = OK; see qymera::delivery::AckStatus).
// The transport layer sends COMMAND_ACK/COMMAND_ERROR on behalf of the handler.
typedef uint8_t (*V2CommandCallback)(
  uint32_t remote_uid,
  const char *remote_ip,
  uint32_t msg_id,
  const qymera::protocol::v2::CommandPayload &payload);

typedef void (*V2CommandAckCallback)(
  uint32_t remote_uid,
  const qymera::protocol::v2::CommandAckPayload &payload);

typedef void (*V2CommandErrorCallback)(
  uint32_t remote_uid,
  const qymera::protocol::v2::CommandErrorPayload &payload);

// ============================================================================
// Reportes
// ============================================================================

struct ReportEntry {
  uint32_t uid;
  float value;
  float raw;
  bool state;
};

extern ReportEntry reports[MAX_SENSORS];

// ============================================================================
// Protocolo
// ============================================================================

#pragma pack(push, 1)

static const uint8_t PACKET_VERSION = 5;
static const uint8_t SENSOR_NAME_LEN = 24;

/* Explicit payload kind. Required so LogPacket payloads are never parsed as
   sensor Packet payloads (they share the broadcast transport). */
enum PacketKind : uint8_t {
  PACKET_SENSOR = 1,
  PACKET_LOG = 2
};

/* Legacy base header (protocol v1/v2/v3, no kind byte). */
struct PacketHeader {
  uint8_t magic;
  uint8_t version;
  uint16_t size;
  uint32_t uid;
};

/* Current header (protocol v4): adds the explicit kind byte. */
struct PacketHeaderV4 {
  uint8_t magic;
  uint8_t version;
  uint16_t size;
  uint32_t uid;
  uint8_t kind;
};

struct PacketV1 {
  uint32_t id;
  uint8_t type;
  uint32_t value;
  uint8_t state;
};

struct PacketV2 {
  uint32_t id;
  uint8_t type;
  uint32_t value;
  uint8_t state;
  char name[SENSOR_NAME_LEN];
};

/* Protocol v4 sensor packet (47 bytes): no persistence/config fields. Kept so
   mixed fleets (v4 peers still announcing) are parsed instead of dropped. */
struct PacketV4 {
  uint32_t id;
  uint8_t type;
  uint32_t value;
  uint8_t state;
  char name[SENSOR_NAME_LEN];
  float min;
  float max;
  float correction;
  uint8_t avail;
};

/* Current sensor packet (protocol v5, 58 bytes): adds fade, persist,
   pers_state, pulse and pulse_ms so remote entities mirror the owner's
   persistence/actuator config in the UI. */
struct Packet {
  uint32_t id;
  uint8_t type;
  uint32_t value;
  uint8_t state;
  char name[SENSOR_NAME_LEN];
  float min;
  float max;
  float correction;
  uint8_t avail;
  uint32_t fade;
  uint8_t persist;
  uint8_t pers_state;
  uint8_t pulse;
  uint32_t pulse_ms;
};

#pragma pack(pop)

// ============================================================================
// API pública
// ============================================================================

void setReport(
  uint8_t index,
  uint32_t uid,
  float value,
  float raw,
  bool state);

uint32_t encodeFloat(float v);

void tick(uint32_t now_ms);

void sendBinaryReport();

// ============================
// LOG OVER UDP
// ============================

struct LogPacket {
  uint8_t layer;
  uint8_t level;
  char message[64];
};

void sendLog(uint8_t layer, uint8_t level, const char *message);

// ============================================================================
// Registro de callbacks
// ============================================================================

void setSensorDiscoveryCallback(SensorDiscoveryCallback cb);

void setCommandCallback(CommandCallback cb);

void sendCommand(
  uint32_t remote_uid,
  const char *remote_ip,
  uint32_t sensor_id,
  uint8_t type,
  uint32_t value,
  bool state);

// V2 Protocol Sends (Phase 3)
void sendV2Hello();
void sendV2EntityAnnounce(uint8_t index);
void sendV2StateUpdate(uint8_t index);
void sendV2Command(uint32_t remote_uid, const char *remote_ip,
                   uint32_t entity_id, uint8_t type, uint32_t value, bool state,
                   bool ack_requested = true);

// V2 Command Delivery Semantics (Phase 4): sends a COMMAND with ACK_REQ and
// tracks it until COMMAND_ACK/COMMAND_ERROR or timeout, retrying with
// exponential backoff from mesh::tick(). Returns false if not queued (queue
// full or unknown remote).
bool sendReliableV2Command(uint32_t remote_uid, const char *remote_ip,
                           uint32_t entity_id, uint8_t type, uint32_t value,
                           bool state);
void sendV2CommandAck(uint32_t remote_uid, const char *remote_ip,
                      uint32_t msg_id, uint32_t entity_id, uint8_t status);
void sendV2CommandError(uint32_t remote_uid, const char *remote_ip,
                        uint32_t msg_id, uint32_t entity_id, uint8_t status,
                        const char *message);

// V2 Callback Registration
void setV2EntityAnnounceCallback(V2EntityAnnounceCallback cb);
void setV2StateUpdateCallback(V2StateUpdateCallback cb);
void setV2CommandCallback(V2CommandCallback cb);
void setV2CommandAckCallback(V2CommandAckCallback cb);
void setV2CommandErrorCallback(V2CommandErrorCallback cb);

// ============================================================================
// Utilidades
// ============================================================================

RemoteDevice *getRemoteDevice(uint32_t uid);

int getRemoteDeviceCount();

bool isDeviceOnline(uint32_t uid);

// ============================================================================
// Config
// ============================================================================

#define MESH_TIMEOUT 30000

// Discovery UDP batching: one datagram carries multiple local entities instead
// of one datagram per sensor. The limit keeps a batch below the Ethernet MTU to
// avoid IP fragmentation: sizeof(PacketHeaderV4)=9 and sizeof(Packet)=58, so
// up to floor((1400-9)/58)=23 sensors fit per datagram.
#define DISCOVERY_MAX_UDP_PACKET 1400

}  // namespace mesh