#pragma once
#include <Arduino.h>
#include "config.h"

namespace qymera {
namespace espnow {

bool init();
void send_broadcast(const uint8_t *data, uint16_t len);
bool recv(uint8_t *buf, uint16_t *len, uint8_t *src_mac);
// RX queue metrics: total dropped messages (queue full) and current depth.
uint32_t get_rx_overflow();
uint8_t get_rx_queue_depth();
void add_peer(const uint8_t *mac);
void clear_peers();
uint8_t get_peer_count();
void set_enabled(bool enabled);
bool is_enabled();

}  // namespace espnow
}  // namespace qymera
