#pragma once
#include <Arduino.h>

// ============================================================================
// QYMERA TRANSPORT ABSTRACTION (Phase 5)
//
// Pure medium-agnostic channel between the application messaging layer (net)
// and the concrete backends (UDP datagrams / ESP-NOW broadcast).
//
// net and anything above it must never touch WiFiUdp/espnow_* or branch on
// raw sockets. They use only:
//
//   qymera::transport::broadcast()   -> every peer
//   qymera::transport::unicast()     -> one peer (ESP-NOW: broadcast fallback)
//   qymera::transport::poll()        -> next inbound frame
//   qymera::transport::setActive()   -> backend selection
//
// Backends are owned here; changing medium does not rewrite entity/command/
// discovery logic.
// ============================================================================

namespace qymera {
namespace transport {

// Backend selection: the single active medium for all net traffic.
enum class Kind : uint8_t {
  UDP = 0,
  ESP_NOW = 1,
};

// Peer identity as seen by callers: "IPv4" for UDP ("255.255.255.255" when the
// frame came from a broadcast), "AA:BB:CC:DD:EE:FF" for ESP-NOW.
struct Peer {
  char address[24];
};

// Largest inbound datagram accepted (bound by the discovery batch MTU limit).
static const uint16_t FRAME_MAX = 1400;
// ESP-NOW frames are capped by the 802.11 vendor buffer (net RX contract).
static const uint16_t ESP_NOW_FRAME_MAX = 250;
// Per-UDP-socket RX budget per poll cycle: a broadcast storm must never
// starve loop(); leftovers are handled on the next cycle.
static const uint8_t RECV_BUDGET = 8;

// One inbound unit handed to the application layer by poll().
// `data` is only valid until the next poll() call overwrites the RX buffer.
struct Frame {
  const uint8_t *data;
  uint16_t len;
  Peer peer;
};

// ---- lifecycle --------------------------------------------------------------
// Binds both UDP sockets and initializes ESP-NOW. Ports are captured for the
// whole runtime; reconfiguration requires a re-begin.
void begin(uint16_t broadcast_port, uint16_t command_port);

// Select the active backend and enable/disable the ESP-NOW assistant
// accordingly.
void setActive(Kind kind);
Kind active();

// ---- send -------------------------------------------------------------------
// To every peer. UDP: 255.255.255.255 broadcast; ESP-NOW: broadcast.
// Returns false for invalid frames or when the UDP backend is not TX-ready.
bool broadcast(const uint8_t *data, uint16_t len);

// To one peer. UDP: unicast datagram to peer_address. ESP-NOW: no unicast peer
// API exists in this version, so it degrades to a broadcast (documented
// limitation, keeps the Phase 3/4 behavior).
bool unicast(const char *peer_address, const uint8_t *data, uint16_t len);

// ---- receive ----------------------------------------------------------------
// Resets the per-socket RX budgets. Call once at the start of each drain cycle
// (e.g. top of the net tick).
void beginPoll();

// True while an inbound frame is available; fills `frame`. Drains the broadcast
// socket first, then the command socket (RECV_BUDGET each), then the ESP-NOW
// FIFO. Returns false when the whole cycle is drained.
bool poll(Frame &frame);

}  // namespace transport
}  // namespace qymera