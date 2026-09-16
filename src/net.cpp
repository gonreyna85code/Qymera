#include "net.h"
#include "config.h"
#include "core.h"
#include "sensors.h"
#include "entities.h"
#include "log.h"
#include "model.h"
#include "cmd_delivery.h"
#include "transport.h"

namespace net {

using namespace qymera::model;

// Wire-format guards: any change to the packed structs that alters their size
// must be reviewed against parseBuffer/senders (protocol compatibility).
static_assert(sizeof(PacketHeader) == 8, "PacketHeader must be 8 bytes");
static_assert(sizeof(PacketHeaderV4) == 9, "PacketHeaderV4 must be 9 bytes");
static_assert(sizeof(PacketV1) == 10, "PacketV1 must be 10 bytes");
static_assert(sizeof(PacketV2) == 34, "PacketV2 must be 34 bytes");
static_assert(sizeof(PacketV4) == 47, "PacketV4 must be 47 bytes");
static_assert(sizeof(Packet) == 58, "Packet must be 58 bytes");
static_assert(sizeof(LogPacket) == 66, "LogPacket must be 66 bytes");

float MIN_VAL = -50.0f;
float MAX_VAL = 150.0f;
static RemoteDevice remote_devices[MAX_SENSORS];
static int remote_device_count = 0;
static unsigned long last_cleanup = 0;
static SensorDiscoveryCallback sensor_callback = nullptr;
static CommandCallback command_cb = nullptr;

// V2 Protocol Callbacks
static qymera::protocol::v2::V2EntityAnnounceCallback v2_entity_announce_cb = nullptr;
static qymera::protocol::v2::V2StateUpdateCallback v2_state_update_cb = nullptr;
static qymera::protocol::v2::V2CommandCallback v2_command_cb = nullptr;
static qymera::protocol::v2::V2CommandAckCallback v2_command_ack_cb = nullptr;
static qymera::protocol::v2::V2CommandErrorCallback v2_command_error_cb = nullptr;

// V2 message ID counter (monotonic per device)
static uint32_t v2_msg_id_counter = 1;

// Phase 4: reliable command delivery state (outbound retry queue, inbound
// duplicate suppression). Owned by net; driven from net::tick().
static qymera::delivery::ReliableQueue cmd_queue;
static qymera::delivery::DupRing cmd_dup_ring;

// Forward-declared (defined after the send helpers; used from tick()).
static void deliveryTick(uint32_t now_ms);

// ================= TRANSPORT =================
// The selected medium lives in qymera::transport. net only mirrors it for the
// legacy net::setTransport/getTransport API (core.cpp selects WiFi vs AP mode).

void setTransport(Transport t) {
  if (getTransport() == t) return;
  qymera::transport::setActive(
    t == TRANSPORT_ESPNOW ? qymera::transport::Kind::ESP_NOW
                          : qymera::transport::Kind::UDP);
  logger::coref("Net transport: %s",
                t == TRANSPORT_ESPNOW ? "ESP-NOW" : "UDP");
}

Transport getTransport() {
  return qymera::transport::active() == qymera::transport::Kind::ESP_NOW
           ? TRANSPORT_ESPNOW
           : TRANSPORT_UDP;
}

void init() {
  qymera::transport::begin(core::genset.broadcast_port,
                           core::genset.command_port);
}

void setSensorDiscoveryCallback(SensorDiscoveryCallback cb) {
  sensor_callback = cb;
}

void setCommandCallback(CommandCallback cb) {
  command_cb = cb;
}

// V2 Callback Registration
void setV2EntityAnnounceCallback(qymera::protocol::v2::V2EntityAnnounceCallback cb) {
  v2_entity_announce_cb = cb;
}

void setV2StateUpdateCallback(qymera::protocol::v2::V2StateUpdateCallback cb) {
  v2_state_update_cb = cb;
}

void setV2CommandCallback(qymera::protocol::v2::V2CommandCallback cb) {
  v2_command_cb = cb;
}

void setV2CommandAckCallback(qymera::protocol::v2::V2CommandAckCallback cb) {
  v2_command_ack_cb = cb;
}

void setV2CommandErrorCallback(qymera::protocol::v2::V2CommandErrorCallback cb) {
  v2_command_error_cb = cb;
}

// ================= BUFFER PARSER (shared) =================

static const char *ackErrorText(uint8_t status) {
  switch (status) {
    case qymera::delivery::ST_NOT_FOUND: return "Entity not found";
    case qymera::delivery::ST_INVALID:   return "Invalid type";
    case qymera::delivery::ST_NOT_LOCAL: return "Not local";
    case qymera::delivery::ST_BUSY:      return "Busy";
    default:                             return "Command error";
  }
}

static void parseV2Frame(const uint8_t *buf, uint16_t len, const char *remote_ip, uint32_t now_ms) {
  using namespace qymera::protocol::v2;
  Envelope env;
  const uint8_t *payload_ptr = nullptr;
  uint16_t payload_len = 0;
  if (!parseFrame(buf, len, env, payload_ptr, payload_len)) return;

  uint32_t local_uid = GET_CHIP_ID();
  bool is_remote = (env.src_uid != local_uid);

  if (!is_remote) {
    // Locally originated V2 frame (e.g., our own broadcast reflected) - ignore
    return;
  }

  // Track remote device
  int idx = -1;
  for (int i = 0; i < remote_device_count; i++) {
    if (remote_devices[i].uid == env.src_uid) { idx = i; break; }
  }
  if (idx == -1 && remote_device_count < MAX_SENSORS) {
    idx = remote_device_count++;
  }
  if (idx >= 0) {
    remote_devices[idx].uid = env.src_uid;
    strncpy(remote_devices[idx].ip, remote_ip, sizeof(remote_devices[idx].ip) - 1);
    remote_devices[idx].ip[sizeof(remote_devices[idx].ip) - 1] = '\0';
    remote_devices[idx].last_seen = now_ms;
    remote_devices[idx].online = true;
  }

  // Dispatch by message type
  switch ((MsgType)env.msg_type) {
    case MsgType::HELLO: {
      const HelloPayload *hp = asHello(payload_ptr, payload_len);
      if (hp) {
        // Serial-only (never GUI/log ring): peers announce V2 HELLO every
        // BROADCAST_INTERVAL and every pair would flood the end-user log
        // buffer. Serial is the developer layer; keep it fully informative.
        logger::serialf(logger::CORE, logger::INFO, "V2 HELLO from %08X caps=%04X",
                        env.src_uid, hp->capabilities);
      }
      break;
    }
    case MsgType::ENTITY_ANNOUNCE: {
      const EntityAnnouncePayload *eap = asEntityAnnounce(payload_ptr, payload_len);
      if (eap && v2_entity_announce_cb) {
        v2_entity_announce_cb(env.src_uid, remote_ip, *eap);
      }
      break;
    }
    case MsgType::STATE_UPDATE: {
      const StateUpdatePayload *sup = asStateUpdate(payload_ptr, payload_len);
      if (sup && v2_state_update_cb) {
        v2_state_update_cb(env.src_uid, *sup);
      }
      break;
    }
    case MsgType::COMMAND: {
      const CommandPayload *cp = asCommand(payload_ptr, payload_len);
      if (cp && v2_command_cb) {
        // Duplicate suppression: a retransmitted COMMAND (sender never got its
        // ACK) must NOT execute the actuator twice. Re-send the stored result.
        uint8_t status;
        if (!cmd_dup_ring.isDuplicate(env.src_uid, env.msg_id, now_ms)) {
          status = v2_command_cb(env.src_uid, remote_ip, env.msg_id, *cp);
          cmd_dup_ring.record(env.src_uid, env.msg_id, status, now_ms);
        } else {
          status = cmd_dup_ring.statusFor(env.src_uid, env.msg_id, now_ms);
        }
        // The callback only executes; the transport owns the response. One
        // ACK/ERROR per originating msg_id (no provisional double-ACK).
        if (env.flags & (uint16_t)Flags::ACK_REQ) {
          if (status == qymera::delivery::ST_OK) {
            sendV2CommandAck(env.src_uid, remote_ip, env.msg_id, cp->entity_id,
                             qymera::delivery::ST_OK);
          } else {
            sendV2CommandError(env.src_uid, remote_ip, env.msg_id,
                               cp->entity_id, status, ackErrorText(status));
          }
        }
      }
      break;
    }
    case MsgType::COMMAND_ACK: {
      const CommandAckPayload *cap = asCommandAck(payload_ptr, payload_len);
      if (cap) {
        // ACK correlation: match to the pending command and stop retrying.
        if (cmd_queue.onAck(cap->msg_id) != 0xFF) {
          logger::coref("V2 reliable: ACK msg=%08X entity=%08X status=%d",
                        cap->msg_id, cap->entity_id, cap->status);
        }
        if (v2_command_ack_cb) {
          v2_command_ack_cb(env.src_uid, *cap);
        }
      }
      break;
    }
    case MsgType::COMMAND_ERROR: {
      const CommandErrorPayload *cep = asCommandError(payload_ptr, payload_len);
      if (cep) {
        // An explicit error completes (cancels) the pending retry loop.
        if (cmd_queue.onError(cep->msg_id) != 0xFF) {
          logger::warnf("V2 reliable: ERROR msg=%08X code=%d (%s)",
                        cep->msg_id, cep->error_code, cep->message);
        }
        if (v2_command_error_cb) {
          v2_command_error_cb(env.src_uid, *cep);
        }
      }
      break;
    }
    case MsgType::LOG: {
      const LogPayload *lp = asLog(payload_ptr, payload_len);
      if (lp && lp->layer <= logger::EVENTS && lp->level <= logger::ERROR) {
        logger::logRemote((logger::Layer)lp->layer, (logger::Level)lp->level, lp->message);
      }
      break;
    }
    default:
      break;
  }
}

static void parseBuffer(const uint8_t *buf, uint16_t len, const char *remote_ip, uint32_t now_ms) {
  // Try V2 first (magic 0xA6)
  if (len >= sizeof(qymera::protocol::v2::Envelope)) {
    if (buf[0] == qymera::protocol::v2::V2_MAGIC) {
      parseV2Frame(buf, len, remote_ip, now_ms);
      return;  // V2 frame handled, don't fall through to legacy
    }
  }

  // Legacy v1-v5 parsing (magic 0xA5)
  if (len < sizeof(PacketHeader)) return;
  PacketHeader hdr;
  memcpy(&hdr, buf, sizeof(hdr));
  if (hdr.magic != 0xA5) return;
  if (hdr.version < 1 || hdr.version > PACKET_VERSION) return;
  if (hdr.size != len) return;
  uint32_t local_uid = GET_CHIP_ID();
  bool is_remote = (hdr.uid != local_uid);

  // Protocol v4/v5 carry an explicit packet kind byte. Legacy v1/v2/v3 have no
  // kind byte and are always sensor packets.
  int header_size = sizeof(PacketHeader);
  PacketKind kind = PACKET_SENSOR;
  if (hdr.version == 4 || hdr.version == PACKET_VERSION) {
    if (len < header_size + 1) return;
    kind = (PacketKind)buf[header_size];
    header_size += 1;
    if (kind != PACKET_SENSOR && kind != PACKET_LOG) {
      logger::warnf("Net: unknown packet kind %u rejected", (uint8_t)kind);
      return;
    }
  }

  int remaining = hdr.size - header_size;
  if (remaining <= 0) return;

  // ---- Log payload: must NEVER reach sensor_callback() ----
  if (kind == PACKET_LOG) {
    if (remaining != (int)sizeof(LogPacket)) return;  // exact-size validation
    LogPacket lp;
    memcpy(&lp, buf + header_size, sizeof(lp));
    lp.message[sizeof(lp.message) - 1] = '\0';
    if (is_remote && lp.layer <= logger::EVENTS && lp.level <= logger::ERROR) {
      // Ingest into the local log GUI/serial buffer WITHOUT re-broadcasting
      // (prevents a broadcast ping-pong loop between devices).
      logger::logRemote((logger::Layer)lp.layer, (logger::Level)lp.level, lp.message);
    }
    return;
  }

  // ---- Sensor payload (v4/v5 PACKET_SENSOR, or legacy v1/v2/v3) ----
  int packet_len = (hdr.version == 1) ? sizeof(PacketV1) :
                   (hdr.version == 2) ? sizeof(PacketV2) :
                   (hdr.version <= 4) ? sizeof(PacketV4) :
                                        sizeof(Packet);
  // Exact-multiple validation: a payload whose size is not a whole number of
  // sensor packets is rejected. Legacy v3 log packets (66-byte payloads) fail
  // this check and are dropped instead of being fragmented into fake sensors.
  if (remaining % packet_len != 0) return;
  const uint8_t *ptr = buf + header_size;
  int parsed_packets = 0;

  while (remaining >= packet_len) {
    Packet pkt;
    memset(&pkt, 0, sizeof(pkt));
    if (hdr.version == 1) {
      PacketV1 pkt_v1;
      memcpy(&pkt_v1, ptr, sizeof(pkt_v1));
      pkt.id    = pkt_v1.id;
      pkt.type  = pkt_v1.type;
      pkt.value = pkt_v1.value;
      pkt.state = pkt_v1.state;
      pkt.min = 0; pkt.max = 100; pkt.correction = 0; pkt.avail = 0;
      pkt.name[0] = '\0';
    } else if (hdr.version == 2) {
      PacketV2 pkt_v2;
      memcpy(&pkt_v2, ptr, sizeof(pkt_v2));
      pkt.id    = pkt_v2.id;
      pkt.type  = pkt_v2.type;
      pkt.value = pkt_v2.value;
      pkt.state = pkt_v2.state;
      memcpy(pkt.name, pkt_v2.name, sizeof(pkt.name));
      pkt.name[sizeof(pkt.name) - 1] = '\0';
      pkt.min = 0; pkt.max = 100; pkt.correction = 0; pkt.avail = 0;
    } else if (hdr.version == 3 || hdr.version == 4) {
      // Legacy 47-byte v3/v4 packet: copy into the 58-byte v5 layout. The extra
      // config fields (fade/persist/pers_state/pulse/pulse_ms) stay 0 because
      // the legacy peer did not transmit them.
      PacketV4 pkt_v4;
      memcpy(&pkt_v4, ptr, sizeof(pkt_v4));
      pkt.id        = pkt_v4.id;
      pkt.type      = pkt_v4.type;
      pkt.value     = pkt_v4.value;
      pkt.state     = pkt_v4.state;
      memcpy(pkt.name, pkt_v4.name, sizeof(pkt.name));
      pkt.name[sizeof(pkt.name) - 1] = '\0';
      pkt.min       = pkt_v4.min;
      pkt.max       = pkt_v4.max;
      pkt.correction = pkt_v4.correction;
      pkt.avail     = pkt_v4.avail;
    } else {
      memcpy(&pkt, ptr, sizeof(pkt));
      pkt.name[sizeof(pkt.name) - 1] = '\0';
    }
    ptr += packet_len;
    remaining -= packet_len;
    parsed_packets++;

    if (is_remote) {
      if (sensor_callback) {
        sensor_callback(
          hdr.uid, remote_ip,
          pkt.id, String(pkt.name),
          pkt.type, pkt.state,
          pkt.value, pkt.min, pkt.max,
          pkt.correction, pkt.avail,
          pkt.fade, pkt.persist, pkt.pers_state, pkt.pulse, pkt.pulse_ms);
      }
      int idx = -1;
      for (int i = 0; i < remote_device_count; i++) {
        if (remote_devices[i].uid == hdr.uid) { idx = i; break; }
      }
      if (idx == -1 && remote_device_count < MAX_SENSORS) {
        idx = remote_device_count++;
      }
      if (idx >= 0) {
        remote_devices[idx].uid       = hdr.uid;
        strncpy(remote_devices[idx].ip, remote_ip, sizeof(remote_devices[idx].ip) - 1);
        remote_devices[idx].ip[sizeof(remote_devices[idx].ip) - 1] = '\0';
        remote_devices[idx].last_seen = now_ms;
        remote_devices[idx].online    = true;
      }
    } else {
      if (command_cb) {
        command_cb(pkt.type, pkt.id, pkt.value, pkt.state != 0);
      }
    }
  }
  if (now_ms - last_cleanup > 30000) {
    last_cleanup = now_ms;
    for (int i = 0; i < remote_device_count; i++) {
      if (now_ms - remote_devices[i].last_seen > NET_TIMEOUT) {
        remote_devices[i].online = false;
      }
    }
  }
}

// ================= TICK =================

void tick(uint32_t now_ms) {
  // Drain the active backend through the transport abstraction. The RX budgets
  // (per-socket storm throttle) and medium guards live in transport::poll().
  qymera::transport::beginPoll();
  qymera::transport::Frame frame;
  while (qymera::transport::poll(frame)) {
    parseBuffer(frame.data, frame.len, frame.peer.address, now_ms);
  }

  // Phase 4: retransmit unacked commands with backoff, expire TTLs.
  deliveryTick(now_ms);
}

// ================= DEVICES =================

RemoteDevice *getRemoteDevice(uint32_t uid) {
  for (int i = 0; i < remote_device_count; i++) {
    if (remote_devices[i].uid == uid) {
      return &remote_devices[i];
    }
  }
  return nullptr;
}

int getRemoteDeviceCount() {
  return remote_device_count;
}

bool isDeviceOnline(uint32_t uid) {
  RemoteDevice *dev = getRemoteDevice(uid);
  return dev != nullptr && dev->online;
}

uint32_t encodeFloat(float v) {
  if (v < MIN_VAL) v = MIN_VAL;
  if (v > MAX_VAL) v = MAX_VAL;
  return (uint32_t)((v - MIN_VAL) / (MAX_VAL - MIN_VAL) * 0xFFFFFFFF);
}

// ================= SEND =================

void sendCommand(uint32_t remote_uid, const char *remote_ip, uint32_t sensor_id, uint8_t type, uint32_t value, bool state) {
  if (!getRemoteDevice(remote_uid)) return;
  PacketHeaderV4 hdr;
  hdr.magic   = 0xA5;
  hdr.version = PACKET_VERSION;
  hdr.uid     = GET_CHIP_ID();
  hdr.kind    = PACKET_SENSOR;
  hdr.size    = sizeof(PacketHeaderV4) + sizeof(Packet);
  Packet pkt;
  memset(&pkt, 0, sizeof(pkt));
  pkt.id    = sensor_id;
  pkt.type  = type;
  pkt.value = value;
  pkt.state = state ? 1 : 0;

  uint8_t buf[sizeof(PacketHeaderV4) + sizeof(Packet)];
  memcpy(buf, &hdr, sizeof(hdr));
  memcpy(buf + sizeof(hdr), &pkt, sizeof(pkt));

  qymera::transport::unicast(remote_ip, buf, sizeof(buf));
}

static void fillPacket(const Entity &c, Packet &pkt) {
  memset(&pkt, 0, sizeof(pkt));
  pkt.id = c.identity.entity_id;
  pkt.type = c.config.type;
  pkt.state = c.state.state ? 1 : 0;
  pkt.min = c.config.min;
  pkt.max = c.config.max;
  pkt.correction = c.config.correction;
  pkt.avail = c.state.avail;
  pkt.fade = c.config.fade;
  pkt.persist = c.config.persist ? 1 : 0;
  pkt.pers_state = c.config.pers_state ? 1 : 0;
  pkt.pulse = c.config.pulse ? 1 : 0;
  pkt.pulse_ms = c.config.pulse_ms;
  strncpy(pkt.name, c.config.name, sizeof(pkt.name) - 1);
  if (c.config.type == SENSOR_LUMI || c.config.type == SENSOR_TIME) {
    pkt.value = (uint32_t)c.state.value;
  } else {
    pkt.value = encodeFloat(c.state.value);
  }
}

// Send one UDP discovery batch: a v4 header followed by sensor_count Packets.
// The receiver already parses N packets per datagram (remaining % packet_len).
static void sendUdpBatch(uint8_t *buf, int sensor_count) {
  if (sensor_count <= 0) return;
  PacketHeaderV4 hdr;
  hdr.magic = 0xA5;
  hdr.version = PACKET_VERSION;
  hdr.uid = GET_CHIP_ID();
  hdr.kind = PACKET_SENSOR;
  hdr.size = sizeof(PacketHeaderV4) + sensor_count * sizeof(Packet);
  memcpy(buf, &hdr, sizeof(hdr));
  qymera::transport::broadcast(buf, hdr.size);
}

void sendBinaryReport() {
  if (qymera::transport::active() == qymera::transport::Kind::UDP) {
    // Batching: group local entities into datagrams of up to
    // DISCOVERY_MAX_UDP_PACKET bytes. Only local entities are announced.
    static uint8_t batch[DISCOVERY_MAX_UDP_PACKET];
    int sensor_count = 0;
    const int header_size = sizeof(PacketHeaderV4);
    for (int i = 0; i < MAX_SENSORS; i++) {
      const Entity &c = entities::at((uint8_t)i);
      if (!entities::isLocal((uint8_t)i)) continue;
      if (c.config.type == SENSOR_NONE || c.identity.entity_id == 0) continue;
      if (sensor_count > 0 &&
          header_size + (sensor_count + 1) * sizeof(Packet) > DISCOVERY_MAX_UDP_PACKET) {
        sendUdpBatch(batch, sensor_count);
        sensor_count = 0;
      }
      Packet pkt;
      fillPacket(c, pkt);
      memcpy(batch + header_size + sensor_count * sizeof(Packet), &pkt, sizeof(pkt));
      sensor_count++;
    }
    sendUdpBatch(batch, sensor_count);
  } else {
    // ESP-NOW: one entity per broadcast (RX side uses a 250-byte buffer).
    for (int i = 0; i < MAX_SENSORS; i++) {
      const Entity &c = entities::at((uint8_t)i);
      if (!entities::isLocal((uint8_t)i)) continue;
      if (c.config.type == SENSOR_NONE || c.identity.entity_id == 0) continue;
      PacketHeaderV4 hdr;
      hdr.magic = 0xA5;
      hdr.version = PACKET_VERSION;
      hdr.uid = GET_CHIP_ID();
      hdr.kind = PACKET_SENSOR;
      hdr.size = sizeof(PacketHeaderV4) + sizeof(Packet);
      Packet pkt;
      fillPacket(c, pkt);
      uint8_t buf[sizeof(PacketHeaderV4) + sizeof(Packet)];
      memcpy(buf, &hdr, sizeof(hdr));
      memcpy(buf + sizeof(hdr), &pkt, sizeof(pkt));
      qymera::transport::broadcast(buf, sizeof(buf));
    }
  }
}

void sendLog(uint8_t layer, uint8_t level, const char *message) {
  PacketHeaderV4 hdr;
  hdr.magic = 0xA5;
  hdr.version = PACKET_VERSION;
  hdr.uid = GET_CHIP_ID();
  hdr.kind = PACKET_LOG;
  hdr.size = sizeof(PacketHeaderV4) + sizeof(LogPacket);

  LogPacket pkt;
  memset(&pkt, 0, sizeof(pkt));
  pkt.layer = layer;
  pkt.level = level;
  strncpy(pkt.message, message, sizeof(pkt.message) - 1);

  uint8_t buf[sizeof(PacketHeaderV4) + sizeof(LogPacket)];
  memcpy(buf, &hdr, sizeof(hdr));
  memcpy(buf + sizeof(hdr), &pkt, sizeof(pkt));

  qymera::transport::broadcast(buf, sizeof(buf));
}

// ================= PROTOCOL V2 SENDS =================

void sendV2Hello() {
  using namespace qymera::protocol::v2;
  HelloPayload hp = {};
  hp.device_id = GET_CHIP_ID();
  hp.capabilities = (uint16_t)Capability::ENTITY_ANNOUNCE |
                    (uint16_t)Capability::STATE_UPDATE |
                    (uint16_t)Capability::COMMAND_ACK |
                    (uint16_t)Capability::COMMAND_ERROR;
  hp.protocol_versions = 0x1F;  // v1-v5 supported

  uint8_t buf[sizeof(Envelope) + sizeof(HelloPayload)];
  uint16_t frame_len = buildFrame(buf, sizeof(buf),
                                  MsgType::HELLO, GET_CHIP_ID(), 0,  // broadcast
                                  v2_msg_id_counter++, (uint16_t)Flags::NONE,
                                  &hp, sizeof(hp));
  if (frame_len == 0) return;

  qymera::transport::broadcast(buf, frame_len);
}

void sendV2EntityAnnounce(uint8_t index) {
  if (index >= MAX_SENSORS) return;
  const Entity &c = entities::at(index);
  if (!entities::isLocal(index)) return;
  if (c.config.type == SENSOR_NONE || c.identity.entity_id == 0) return;

  using namespace qymera::protocol::v2;
  EntityAnnouncePayload eap = {};
  eap.entity_id = c.identity.entity_id;
  eap.device_id = GET_CHIP_ID();
  eap.type = c.config.type;
  // Map capabilities
  switch (c.config.type) {
    case TYPE_RELAY:
    case TYPE_DIMMER:
      eap.capabilities = (uint8_t)EntityCapability::READ_WRITE;
      break;
    default:
      eap.capabilities = (uint8_t)EntityCapability::READ;
  }
  eap.ownership = (uint8_t)EntityOwnership::OWNER_LOCAL;
  strncpy(eap.name, c.config.name, sizeof(eap.name) - 1);
  eap.min = c.config.min;
  eap.max = c.config.max;
  eap.correction = c.config.correction;
  eap.avail = c.state.avail;
  eap.persist = c.config.persist ? 1 : 0;
  eap.pers_state = c.config.pers_state ? 1 : 0;
  eap.pulse = c.config.pulse ? 1 : 0;
  eap.pulse_ms = c.config.pulse_ms;
  eap.fade = c.config.fade;

  uint8_t buf[sizeof(Envelope) + sizeof(EntityAnnouncePayload)];
  uint16_t frame_len = buildFrame(buf, sizeof(buf),
                                  MsgType::ENTITY_ANNOUNCE, GET_CHIP_ID(), 0,
                                  v2_msg_id_counter++, (uint16_t)Flags::NONE,
                                  &eap, sizeof(eap));
  if (frame_len == 0) return;

  qymera::transport::broadcast(buf, frame_len);
}

void sendV2StateUpdate(uint8_t index) {
  if (index >= MAX_SENSORS) return;
  const Entity &c = entities::at(index);
  if (!entities::isLocal(index)) return;
  if (c.config.type == SENSOR_NONE || c.identity.entity_id == 0) return;

  using namespace qymera::protocol::v2;
  StateUpdatePayload sup = {};
  sup.entity_id = c.identity.entity_id;
  sup.value = (c.config.type == SENSOR_LUMI || c.config.type == SENSOR_TIME)
                ? (uint32_t)c.state.value
                : encodeFloat(c.state.value);
  sup.state = c.state.state ? 1 : 0;
  sup.avail = c.state.avail;

  uint8_t buf[sizeof(Envelope) + sizeof(StateUpdatePayload)];
  uint16_t frame_len = buildFrame(buf, sizeof(buf),
                                  MsgType::STATE_UPDATE, GET_CHIP_ID(), 0,
                                  v2_msg_id_counter++, (uint16_t)Flags::NONE,
                                  &sup, sizeof(sup));
  if (frame_len == 0) return;

  qymera::transport::broadcast(buf, frame_len);
}

// Shared V2 COMMAND sender used by the one-shot path, the reliable path and
// the retry scheduler. Envelope msg_id is passed explicitly so retries reuse
// the original msg_id (ACK correlation depends on it).
static void sendCommandFrame(uint32_t msg_id, uint32_t remote_uid,
                             const char *remote_ip, uint16_t flags,
                             const qymera::protocol::v2::CommandPayload &cp) {
  using namespace qymera::protocol::v2;
  uint8_t buf[sizeof(Envelope) + sizeof(CommandPayload)];
  uint16_t frame_len = buildFrame(buf, sizeof(buf),
                                  MsgType::COMMAND, GET_CHIP_ID(), remote_uid,
                                  msg_id, flags, &cp, sizeof(cp));
  if (frame_len == 0) return;

  qymera::transport::unicast(remote_ip, buf, frame_len);
}

void sendV2Command(uint32_t remote_uid, const char *remote_ip,
                   uint32_t entity_id, uint8_t type, uint32_t value, bool state,
                   bool ack_requested) {
  if (!getRemoteDevice(remote_uid) || entity_id == 0) return;

  if (ack_requested) {
    // Reliable delivery delegates to the Phase 4 queue (retry + ACK).
    sendReliableV2Command(remote_uid, remote_ip, entity_id, type, value, state);
    return;
  }

  using namespace qymera::protocol::v2;
  CommandPayload cp = {};
  cp.entity_id = entity_id;
  cp.type = type;
  cp.value = value;
  cp.state = state ? 1 : 0;
  sendCommandFrame(v2_msg_id_counter++, remote_uid, remote_ip,
                   (uint16_t)Flags::NONE, cp);
}

bool sendReliableV2Command(uint32_t remote_uid, const char *remote_ip,
                           uint32_t entity_id, uint8_t type, uint32_t value,
                           bool state) {
  if (!getRemoteDevice(remote_uid) || entity_id == 0) return false;

  using namespace qymera::protocol::v2;
  CommandPayload cp = {};
  cp.entity_id = entity_id;
  cp.type = type;
  cp.value = value;
  cp.state = state ? 1 : 0;
  cp.flags = (uint8_t)Flags::ACK_REQ;

  uint32_t msg_id = v2_msg_id_counter++;
  sendCommandFrame(msg_id, remote_uid, remote_ip, (uint16_t)Flags::ACK_REQ, cp);

  uint8_t slot = cmd_queue.enqueue(msg_id, remote_uid, entity_id, type, value,
                                   state, millis());
  if (slot == 0xFF) {
    logger::warnf("V2 reliable queue full - command msg=%08X dropped (no retry)",
                  msg_id);
    return false;
  }
  return true;
}

// Resends a queued command, reusing its original msg_id.
static void resendPendingCommand(const qymera::delivery::PendingCommand &pc) {
  using namespace qymera::protocol::v2;
  CommandPayload cp = {};
  cp.entity_id = pc.entity_id;
  cp.type = pc.type;
  cp.value = pc.value;
  cp.state = pc.target_state ? 1 : 0;
  cp.flags = (uint8_t)Flags::ACK_REQ;

  RemoteDevice *dev = getRemoteDevice(pc.remote_uid);
  if (!dev) return;
  sendCommandFrame(pc.msg_id, pc.remote_uid, dev->ip,
                   (uint16_t)Flags::ACK_REQ, cp);
}

// Phase 4: drives the reliable delivery state machine from the loop.
static void deliveryTick(uint32_t now_ms) {
  uint8_t expired = cmd_queue.expire(now_ms);
  if (expired > 0) {
    logger::warnf("V2 reliable: %u command(s) timed out (no ACK)", expired);
  }

  uint8_t i;
  while ((i = cmd_queue.nextDue(now_ms)) != 0xFF) {
    const qymera::delivery::PendingCommand &pc = cmd_queue.slots[i];
    resendPendingCommand(pc);
    cmd_queue.rescheduled(i, now_ms);
    logger::coref("V2 reliable retry: msg=%08X entity=%08X attempt=%u",
                  pc.msg_id, pc.entity_id, pc.attempts + 1);
  }
}

void sendV2CommandAck(uint32_t remote_uid, const char *remote_ip,
                      uint32_t msg_id, uint32_t entity_id, uint8_t status) {
  using namespace qymera::protocol::v2;
  CommandAckPayload cap = {};
  cap.msg_id = msg_id;
  cap.entity_id = entity_id;
  cap.status = status;

  uint8_t buf[sizeof(Envelope) + sizeof(CommandAckPayload)];
  uint16_t frame_len = buildFrame(buf, sizeof(buf),
                                  MsgType::COMMAND_ACK, GET_CHIP_ID(), remote_uid,
                                  v2_msg_id_counter++, (uint16_t)Flags::NONE,
                                  &cap, sizeof(cap));
  if (frame_len == 0) return;

  qymera::transport::unicast(remote_ip, buf, frame_len);
}

void sendV2CommandError(uint32_t remote_uid, const char *remote_ip,
                        uint32_t msg_id, uint32_t entity_id, uint8_t status,
                        const char *message) {
  using namespace qymera::protocol::v2;
  CommandErrorPayload cep = {};
  cep.msg_id = msg_id;
  cep.entity_id = entity_id;
  cep.error_code = status;
  if (message) strncpy(cep.message, message, sizeof(cep.message) - 1);

  uint8_t buf[sizeof(Envelope) + sizeof(CommandErrorPayload)];
  uint16_t frame_len = buildFrame(buf, sizeof(buf),
                                  MsgType::COMMAND_ERROR, GET_CHIP_ID(), remote_uid,
                                  v2_msg_id_counter++, (uint16_t)Flags::NONE,
                                  &cep, sizeof(cep));
  if (frame_len == 0) return;

  qymera::transport::unicast(remote_ip, buf, frame_len);
}

}  // namespace net
