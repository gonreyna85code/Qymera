#include "espnow_p2p.h"

#include "log.h"

#if defined(ESP8266)
#include <espnow.h>
#elif defined(ESP32)
#include <esp_now.h>
#endif

namespace qymera {
namespace espnow {

// Bounded RX FIFO (single producer = ESP-NOW callback, single consumer =
// loop()). No dynamic memory. Each entry stores payload + length + src MAC.
static const uint8_t ESPNOW_MAX_PAYLOAD = 250;
static const uint8_t ESPNOW_RX_QUEUE_SIZE = 8;

struct RxEntry {
  uint8_t payload[ESPNOW_MAX_PAYLOAD];
  uint16_t len;
  uint8_t src[6];
};

static RxEntry rx_queue[ESPNOW_RX_QUEUE_SIZE];
static volatile uint8_t rx_head = 0;   // next write slot (callback side)
static volatile uint8_t rx_tail = 0;   // next read slot (loop side)
static volatile uint8_t rx_count = 0;  // entries in the queue
static volatile uint32_t rx_overflow = 0;

#if defined(ESP32)
static portMUX_TYPE rx_mux = portMUX_INITIALIZER_UNLOCKED;
#endif

static void rx_enqueue(const uint8_t *mac, const uint8_t *data, uint16_t len) {
  if (len == 0 || len > ESPNOW_MAX_PAYLOAD) return;
#if defined(ESP32)
  portENTER_CRITICAL(&rx_mux);
#endif
  if (rx_count >= ESPNOW_RX_QUEUE_SIZE) {
    // Queue full: drop the new message, keep the oldest. Callback must never
    // block, so we only bump a counter here; logging happens from loop().
    rx_overflow++;
#if defined(ESP32)
    portEXIT_CRITICAL(&rx_mux);
#endif
    return;
  }
  RxEntry &e = rx_queue[rx_head];
  memcpy(e.src, mac, 6);
  memcpy(e.payload, data, len);
  e.len = len;
  rx_head = (rx_head + 1) % ESPNOW_RX_QUEUE_SIZE;
  rx_count++;
#if defined(ESP32)
  portEXIT_CRITICAL(&rx_mux);
#endif
}

static uint8_t peers[25][6];
static int peer_count = 0;
static bool espnow_ready = false;
static bool espnow_enabled = false;

// ================= CALLBACKS =================

#if defined(ESP8266)
static void espnow_send_cb(uint8_t *mac, uint8_t status) {}
static void espnow_recv_cb(uint8_t *mac, uint8_t *data, uint8_t len) {
  rx_enqueue(mac, data, len);
}
#elif defined(ESP32)
#include <esp_idf_version.h>
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
static void espnow_send_cb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status) {}
static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
  rx_enqueue(recv_info->src_addr, data, (uint16_t)len);
}
#else
static void espnow_send_cb(const uint8_t *mac, esp_now_send_status_t status) {}
static void espnow_recv_cb(const uint8_t *mac, const uint8_t *data, int len) {
  rx_enqueue(mac, data, (uint16_t)len);
}
#endif
#endif

// ================= API =================

bool init() {
  if (esp_now_init() == 0) {
#if defined(ESP8266)
    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
#endif
    esp_now_register_send_cb(espnow_send_cb);
    esp_now_register_recv_cb(espnow_recv_cb);
    espnow_ready = true;
    logger::core("ESP-NOW initialized");
    return true;
  }
  logger::error("ESP-NOW init failed");
  return false;
}

void send_broadcast(const uint8_t *data, uint16_t len) {
  if (!espnow_ready || !espnow_enabled) return;
  for (int i = 0; i < peer_count; i++) {
    esp_now_send(peers[i], (uint8_t *)data, len);
  }
  uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  esp_now_send(broadcast, (uint8_t *)data, len);
}

bool recv(uint8_t *buf, uint16_t *len, uint8_t *src_mac) {
#if defined(ESP32)
  portENTER_CRITICAL(&rx_mux);
#endif
  if (rx_count == 0) {
#if defined(ESP32)
    portEXIT_CRITICAL(&rx_mux);
#endif
    return false;
  }
  const RxEntry &e = rx_queue[rx_tail];
  memcpy(buf, e.payload, e.len);
  *len = e.len;
  memcpy(src_mac, e.src, 6);
  rx_tail = (rx_tail + 1) % ESPNOW_RX_QUEUE_SIZE;
  rx_count--;
#if defined(ESP32)
  portEXIT_CRITICAL(&rx_mux);
#endif
  return true;
}

uint32_t get_rx_overflow() {
  return rx_overflow;
}

uint8_t get_rx_queue_depth() {
  return rx_count;
}

void add_peer(const uint8_t *mac) {
  if (!espnow_ready) return;
  if (peer_count >= 25) return;
  for (int i = 0; i < peer_count; i++) {
    if (memcmp(peers[i], mac, 6) == 0) return;
  }
  memcpy(peers[peer_count], mac, 6);
#if defined(ESP8266)
  esp_now_add_peer(peers[peer_count], ESP_NOW_ROLE_COMBO, 1, NULL, 0);
#elif defined(ESP32)
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, peers[peer_count], 6);
  peer.channel = 0;
  peer.encrypt = false;
  esp_now_add_peer(&peer);
#endif
  peer_count++;
  logger::coref("ESP-NOW peer added (%d total)", peer_count);
}

void clear_peers() {
  if (!espnow_ready) return;
  for (int i = 0; i < peer_count; i++) {
    esp_now_del_peer(peers[i]);
  }
  peer_count = 0;
}

uint8_t get_peer_count() {
  return peer_count;
}

void set_enabled(bool enabled) {
  espnow_enabled = enabled;
}

bool is_enabled() {
  return espnow_enabled;
}

}  // namespace espnow
}  // namespace qymera
