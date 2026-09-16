#include "entities.h"
#include "automations.h"
#include "log.h"

namespace entities {

static Entity g_entities[MAX_SENSORS];

// ----------------------------------------------------------------
// ID factory: xorshift32 PRNG seeded from chip id ^ millis. The upper
// 16 bits carry the chip id so generated ids are device-unique; the
// slot index is deliberately never part of the identity.
// ----------------------------------------------------------------
static uint32_t entity_id_seed = 0;

uint32_t nextEntityId() {
  if (entity_id_seed == 0) {
    entity_id_seed = GET_CHIP_ID() ^ (uint32_t)millis();
    entity_id_seed |= 1;  // ensure non-zero
  }
  uint32_t x = entity_id_seed;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  entity_id_seed = x;
  return (GET_CHIP_ID() & 0xFFFF0000) | (x & 0xFFFF);
}

void init() {
  for (int i = 0; i < MAX_SENSORS; i++) g_entities[i] = Entity();
}

uint8_t capacity() { return (uint8_t)MAX_SENSORS; }

Entity &at(uint8_t index) { return g_entities[index]; }
const Entity &peek(uint8_t index) { return g_entities[index]; }

bool isUsed(uint8_t index) {
  if (index >= MAX_SENSORS) return false;
  return g_entities[index].identity.entity_id != ENTITY_ID_NONE;
}

bool isLocal(uint8_t index) {
  return isUsed(index) && g_entities[index].runtime.local;
}

bool isStaleRemote(uint8_t index) {
  if (index >= MAX_SENSORS) return false;
  const Entity &e = g_entities[index];
  if (e.runtime.local || e.identity.entity_id == ENTITY_ID_NONE) return false;
  // Wrap-safe elapsed check (millis() overflow after ~49 days).
  return (uint32_t)(millis() - e.state.last_update) > NET_TIMEOUT;
}

bool isVisible(uint8_t index) {
  if (index >= MAX_SENSORS) return false;
  const Entity &e = g_entities[index];
  if (e.identity.entity_id == ENTITY_ID_NONE) return false;
  if (!qymera::model::isValidType(e.config.type)) return false;
  if (e.runtime.local) return true;
  return !isStaleRemote(index);
}

void markSeen(uint8_t index) {
  if (index >= MAX_SENSORS) return;
  Entity &e = g_entities[index];
  e.state.last_update = millis();
  e.runtime.last_seen = e.state.last_update;
  e.runtime.online = true;
}

int8_t findFree() {
  for (int i = 0; i < MAX_SENSORS; i++) {
    if (g_entities[i].identity.entity_id == ENTITY_ID_NONE) return i;
  }
  return -1;
}

int8_t findById(uint32_t entity_id) {
  if (entity_id == ENTITY_ID_NONE) return -1;
  for (int i = 0; i < MAX_SENSORS; i++) {
    if (g_entities[i].identity.entity_id == entity_id) return i;
  }
  return -1;
}

int8_t findLocalByName(const char *name) {
  if (!name || !name[0]) return -1;
  for (int i = 0; i < MAX_SENSORS; i++) {
    const Entity &e = g_entities[i];
    if (e.runtime.local && e.identity.entity_id != ENTITY_ID_NONE &&
        strcmp(e.config.name, name) == 0) {
      return i;
    }
  }
  return -1;
}

int8_t findRemoteByPeer(uint32_t device_id, uint32_t entity_id) {
  if (entity_id == ENTITY_ID_NONE) return -1;
  for (int i = 0; i < MAX_SENSORS; i++) {
    const Entity &e = g_entities[i];
    if (!e.runtime.local && e.identity.entity_id == entity_id &&
        e.identity.device_id == device_id) {
      return i;
    }
  }
  return -1;
}

int8_t registerLocal(const char *name, uint8_t type) {
  int idx = findLocalByName(name);
  if (idx >= 0) {
    // Re-reporting an existing entity: keep identity/config, refresh only
    // the runtime fields (owner ip + last sighting).
    Entity &e = g_entities[idx];
    e.config.type = type;  // re-bind keeps the canonical type authoritative
    IPAddress ip = WiFi.localIP();
    snprintf(e.runtime.device_ip, ENTITY_IP_LEN, "%d.%d.%d.%d",
             ip[0], ip[1], ip[2], ip[3]);
    e.runtime.last_seen = millis();
    e.runtime.online = true;
    return idx;
  }

  int free = findFree();
  if (free < 0) return -1;  // registry full

  Entity &e = g_entities[free];
  e = Entity();
  e.runtime.local = true;
  e.runtime.online = true;
  e.identity.device_id = GET_CHIP_ID();
  e.config.type = type;
  strncpy(e.config.name, name, ENTITY_NAME_LEN - 1);
  IPAddress ip = WiFi.localIP();
  snprintf(e.runtime.device_ip, ENTITY_IP_LEN, "%d.%d.%d.%d",
           ip[0], ip[1], ip[2], ip[3]);
  e.identity.entity_id = nextEntityId();
  logger::serialf(logger::SENSORS, logger::INFO,
                  "Entity registered: idx=%d entity_id=%08X name=%s",
                  free, e.identity.entity_id, name);
  return free;
}

void release(uint8_t index) {
  if (index >= MAX_SENSORS) return;
  g_entities[index] = Entity();
}

void reclaimStale() {
  static unsigned long last_pass = 0;
  // Run at most once per timeout window.
  if ((uint32_t)(millis() - last_pass) < NET_TIMEOUT) return;
  last_pass = millis();
  for (int i = 0; i < MAX_SENSORS; i++) {
    const Entity &e = g_entities[i];
    if (e.runtime.local || e.identity.entity_id == ENTITY_ID_NONE) continue;
    if (!isStaleRemote((uint8_t)i)) continue;
    // Never reclaim a slot an automation still references: rules address
    // entities by slot, so reusing the slot would change what the rule
    // acts on. Referenced stale entries stay hidden/occupied until the
    // owning rule is deleted.
    if (automations::isIndexReferenced((uint8_t)i)) continue;
    release((uint8_t)i);
    logger::sensorsf("Reclaimed stale remote slot %d", i);
  }
}

uint8_t localCount() {
  uint8_t n = 0;
  for (int i = 0; i < MAX_SENSORS; i++) {
    if (g_entities[i].runtime.local &&
        g_entities[i].identity.entity_id != ENTITY_ID_NONE) n++;
  }
  return n;
}

uint8_t remoteCount() {
  uint8_t n = 0;
  for (int i = 0; i < MAX_SENSORS; i++) {
    if (!g_entities[i].runtime.local &&
        g_entities[i].identity.entity_id != ENTITY_ID_NONE) n++;
  }
  return n;
}

}  // namespace entities