#pragma once
#include <Arduino.h>
#include "config.h"
#include "model.h"

// ================================================================
// ENTITY REGISTRY (v1)
//
// Single source of truth for every entity this device knows about,
// local and remote. Logical identity is ALWAYS identity.entity_id;
// a runtime slot (index) is an implementation detail and is never
// used as identity. All modules (sensors facade, net, storage, web,
// automations) address entities through this registry.
//
// Registry invariants:
//   * one entity per slot; slots are recycled via release()/reclaim()
//   * identity.entity_id == ENTITY_ID_NONE  <->  empty slot
//   * local entities own identity.entity_id; remote entities mirror the
//     (identity.device_id, identity.entity_id) pair announced by peers
// ================================================================

namespace entities {

using namespace qymera::model;

void init();
uint8_t capacity();                    // MAX_SENSORS

// Index-based access for modules that sweep all slots (persistence,
// announcers, web serialization). The index is NEVER identity.
Entity &at(uint8_t index);
const Entity &peek(uint8_t index);

// Slot predicates (index-based convenience; identity already in Entity)
bool isUsed(uint8_t index);            // identity.entity_id != 0
bool isLocal(uint8_t index);           // used && runtime.local
bool isStaleRemote(uint8_t index);     // remote && no sighting within NET_TIMEOUT
bool isVisible(uint8_t index);         // used && valid type && (local || !stale)
void markSeen(uint8_t index);          // refresh runtime last_seen/online

// Lookup
int8_t findFree();
int8_t findById(uint32_t entity_id);            // any ownership
int8_t findLocalByName(const char *name);       // local entities only
int8_t findRemoteByPeer(uint32_t device_id, uint32_t entity_id);

// Local registration: find-or-create by name. Assigns a stable entity_id
// on first registration (persisted across reboots via the storage map).
// Returns the slot index, or -1 when the registry is full.
int8_t registerLocal(const char *name, uint8_t type);

// Lifecycle
void release(uint8_t index);           // wipe slot back to empty
void reclaimStale();                   // drop stale, unreferenced remotes

uint32_t nextEntityId();               // stable id factory (not index-derived)

uint8_t localCount();
uint8_t remoteCount();

}  // namespace entities