#include "transport.h"
#include <WiFiUdp.h>
#include "espnow_p2p.h"

namespace qymera {
namespace transport {

// ---- backends (owned exclusively here) --------------------------------------
static WiFiUDP broadcast_socket;  // discovery + logs + V2 broadcast traffic
static WiFiUDP command_socket;    // legacy commands + V2 ACK/ERROR unicasts
static uint16_t broadcast_port = 0;
static uint16_t command_port = 0;
static Kind active_kind = Kind::UDP;

// ---- RX drain state ---------------------------------------------------------
static uint8_t udp_budget[2];   // per socket, reset by beginPoll()
static uint8_t rx_phase = 0;    // 0=broadcast, 1=command, 2=ESP-NOW
static uint8_t rx_buf[FRAME_MAX];
static uint8_t espnow_buf[ESP_NOW_FRAME_MAX];

static bool udpTxReady() {
#if defined(ESP32)
  return WiFi.getMode() != WIFI_MODE_NULL;
#else
  return true;
#endif
}

// ---- lifecycle --------------------------------------------------------------

void begin(uint16_t bport, uint16_t cport) {
  broadcast_port = bport;
  command_port = cport;
  broadcast_socket.begin(broadcast_port);
  command_socket.begin(command_port);
  mesh::espnow_init();
}

void setActive(Kind kind) {
  active_kind = kind;
  mesh::espnow_set_enabled(kind == Kind::ESP_NOW);
}

Kind active() {
  return active_kind;
}

// ---- send -------------------------------------------------------------------

bool broadcast(const uint8_t *data, uint16_t len) {
  if (!data || len == 0 || len > FRAME_MAX) return false;

  if (active_kind == Kind::UDP) {
    if (!udpTxReady()) return false;
    broadcast_socket.beginPacket("255.255.255.255", broadcast_port);
    broadcast_socket.write(data, len);
    broadcast_socket.endPacket();
  } else {
    mesh::espnow_send_broadcast(data, len);
  }
  return true;
}

bool unicast(const char *peer_address, const uint8_t *data, uint16_t len) {
  if (!peer_address || !data || len == 0 || len > FRAME_MAX) return false;

  if (active_kind == Kind::UDP) {
    command_socket.beginPacket(peer_address, command_port);
    command_socket.write(data, len);
    command_socket.endPacket();
  } else {
    // No ESP-NOW unicast peer API in this version: degrade to broadcast.
    mesh::espnow_send_broadcast(data, len);
  }
  return true;
}

// ---- receive ----------------------------------------------------------------

void beginPoll() {
  udp_budget[0] = RECV_BUDGET;
  udp_budget[1] = RECV_BUDGET;
  rx_phase = 0;
}

// Reads one datagram from `socket`. Mirrors the legacy guards:
//  - oversized datagram  -> drop-and-drain, stop draining this socket this cycle
//  - read() failure      -> drain and stop as well (prevents a re-yield spin)
// On success decrements the socket budget and fills `frame`.
static bool readUdp(WiFiUDP &socket, uint8_t idx, Frame &frame) {
  int size = socket.parsePacket();
  if (size <= 0) return false;  // nothing pending on this socket

  if (size > (int)FRAME_MAX) {
    while (socket.available()) socket.read();
    udp_budget[idx] = 0;
    return false;
  }

  int len = socket.read(rx_buf, FRAME_MAX);
  if (len <= 0) {
    while (socket.available()) socket.read();
    udp_budget[idx] = 0;
    return false;
  }

  // Guarantee the datagram is fully dequeued: on ESP32 a socket can re-yield
  // the same datagram if the leading read() does not consume it entirely.
  while (socket.available()) socket.read();

  IPAddress rip = socket.remoteIP();
  snprintf(frame.peer.address, sizeof(frame.peer.address), "%d.%d.%d.%d",
           rip[0], rip[1], rip[2], rip[3]);
  frame.data = rx_buf;
  frame.len = (uint16_t)len;
  if (udp_budget[idx] > 0) udp_budget[idx]--;
  return true;
}

bool poll(Frame &frame) {
  while (true) {
    if (rx_phase == 0) {
      if (udp_budget[0] == 0 || !readUdp(broadcast_socket, 0, frame)) {
        rx_phase = 1;
        continue;
      }
      return true;
    }
    if (rx_phase == 1) {
      if (udp_budget[1] == 0 || !readUdp(command_socket, 1, frame)) {
        rx_phase = 2;
        continue;
      }
      return true;
    }
    // rx_phase == 2: ESP-NOW FIFO (bounded by the driver).
    uint16_t len = 0;
    uint8_t src[6];
    if (!mesh::espnow_recv(espnow_buf, &len, src)) {
      rx_phase = 0;
      return false;
    }
    snprintf(frame.peer.address, sizeof(frame.peer.address),
             "%02X:%02X:%02X:%02X:%02X:%02X",
             src[0], src[1], src[2], src[3], src[4], src[5]);
    frame.data = espnow_buf;
    frame.len = len;
    return true;
  }
}

}  // namespace transport
}  // namespace qymera