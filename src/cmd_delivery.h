#pragma once
#include <stdint.h>

// ================================================================
// QYMERA COMMAND DELIVERY SEMANTICS - Phase 4
//
// UDP and ESP-NOW do not guarantee sent == received == executed.
// This header implements the reliability layer on top of the V2
// COMMAND / COMMAND_ACK / COMMAND_ERROR protocol:
//
//   Outbound (ReliableQueue): a bounded queue of in-flight commands.
//     - A command is retried with exponential backoff (2s, 4s, 8s)
//       until the matching COMMAND_ACK arrives or the TTL (30s)
//       expires. A command is only considered complete on ACK.
//     - COMMAND_ERROR completes (cancels) the pending command.
//   Inbound (DupRing): deduplicates retransmitted COMMANDs so an
//   actuator executes exactly once even when the first ACK is lost.
//     - The last ACK status is remembered, so a duplicate re-sends
//       the original response instead of re-executing.
//
// Deliberately pure C++ (no Arduino), fixed-size, no dynamic
// allocation so it can be unit-tested on the host.
// ================================================================

namespace qymera {
namespace delivery {

// --- Tunables ---------------------------------------------------
// The retry schedule covers 2s + 4s + 8s = up to 14s of retransmits;
// DUP_WINDOW_MS must stay above that so retransmissions never fall out
// of the dedup window while retries are still in flight.
static const uint8_t  MAX_PENDING    = 6;     // outbound queue depth
static const uint8_t  MAX_RETRIES    = 3;     // retransmissions after the first send
static const uint32_t BASE_RETRY_MS  = 2000;  // first retry delay
static const uint32_t RETRY_BACKOFF  = 2;     // delay multiplier per attempt
static const uint32_t COMMAND_TTL_MS = 30000; // drop unacknowledged after this
static const uint8_t  DUP_RING_SIZE  = 12;    // receiver dedup ring depth
static const uint32_t DUP_WINDOW_MS  = 20000; // dedup window (covers the schedule)

// ACK / ERROR status codes - matches CommandAckPayload.status
enum AckStatus : uint8_t {
  ST_OK        = 0,
  ST_NOT_FOUND = 1,
  ST_INVALID   = 2,
  ST_NOT_LOCAL = 3,
  ST_BUSY      = 4,
};

enum class State : uint8_t {
  FREE = 0,
  PENDING = 1,
};

struct PendingCommand {
  uint32_t msg_id = 0;
  uint32_t remote_uid = 0;
  uint32_t entity_id = 0;
  uint8_t  type = 0;
  uint32_t value = 0;
  bool     target_state = false;
  uint8_t  attempts = 0;      // completed sends (1 initial + retries)
  uint32_t next_retry_ms = 0; // monotonic clock
  uint32_t expires_at = 0;    // monotonic clock
  State    state = State::FREE;
};

// Exponential backoff delay after `attempts` completed sends:
// 1 -> BASE, 2 -> BASE*2, 3 -> BASE*4, ...
static inline uint32_t retryDelayMs(uint8_t attempts) {
  return BASE_RETRY_MS << (attempts - 1);
}

// Bounded outbound pending-command queue.
class ReliableQueue {
public:
  PendingCommand slots[MAX_PENDING];

  ReliableQueue() { clear(); }

  void clear() {
    for (uint8_t i = 0; i < MAX_PENDING; i++) {
      slots[i].state = State::FREE;
    }
  }

  // Queue a freshly-sent command for ACK/retry tracking.
  // Returns the slot index, or 0xFF when the queue is full.
  uint8_t enqueue(uint32_t msg_id, uint32_t remote_uid, uint32_t entity_id,
                  uint8_t type, uint32_t value, bool target_state,
                  uint32_t now_ms) {
    uint8_t i = freeIdx();
    if (i == 0xFF) return 0xFF;
    PendingCommand &s = slots[i];
    s.msg_id = msg_id;
    s.remote_uid = remote_uid;
    s.entity_id = entity_id;
    s.type = type;
    s.value = value;
    s.target_state = target_state;
    s.attempts = 1;
    s.next_retry_ms = now_ms + BASE_RETRY_MS;
    s.expires_at = now_ms + COMMAND_TTL_MS;
    s.state = State::PENDING;
    return i;
  }

  // Complete a command: ACK (delivered) and ERROR (rejected by the peer)
  // both stop retrying. Returns the matched slot index, or 0xFF.
  uint8_t onAck(uint32_t msg_id)   { return complete(msg_id); }
  uint8_t onError(uint32_t msg_id) { return complete(msg_id); }

  // First PENDING entry whose retry is due (within TTL, retries left),
  // or 0xFF when nothing should be resent now.
  uint8_t nextDue(uint32_t now_ms) {
    for (uint8_t i = 0; i < MAX_PENDING; i++) {
      PendingCommand &s = slots[i];
      if (s.state != State::PENDING) continue;
      if (s.attempts == 0 || s.attempts > MAX_RETRIES) continue;
      if (now_ms >= s.next_retry_ms && now_ms < s.expires_at) return i;
    }
    return 0xFF;
  }

  // Call after a successful retransmission of slots[i]: schedules the next
  // retry with backoff, or stops retrying once MAX_RETRIES is exhausted.
  void rescheduled(uint8_t i, uint32_t now_ms) {
    if (i >= MAX_PENDING) return;
    PendingCommand &s = slots[i];
    s.attempts++;
    if (s.attempts > MAX_RETRIES) {
      s.next_retry_ms = s.expires_at;  // no more retries; wait for TTL
    } else {
      s.next_retry_ms = now_ms + retryDelayMs(s.attempts);
    }
  }

  // Free entries past their TTL. Returns how many timed out.
  uint8_t expire(uint32_t now_ms) {
    uint8_t n = 0;
    for (uint8_t i = 0; i < MAX_PENDING; i++) {
      PendingCommand &s = slots[i];
      if (s.state == State::PENDING && now_ms >= s.expires_at) {
        s.state = State::FREE;
        n++;
      }
    }
    return n;
  }

  uint8_t count() const {
    uint8_t n = 0;
    for (uint8_t i = 0; i < MAX_PENDING; i++) {
      if (slots[i].state == State::PENDING) n++;
    }
    return n;
  }

  bool isFull() const { return freeIdx() == 0xFF; }

private:
  uint8_t freeIdx() const {
    for (uint8_t i = 0; i < MAX_PENDING; i++) {
      if (slots[i].state == State::FREE) return i;
    }
    return 0xFF;
  }

  uint8_t complete(uint32_t msg_id) {
    for (uint8_t i = 0; i < MAX_PENDING; i++) {
      if (slots[i].state == State::PENDING && slots[i].msg_id == msg_id) {
        slots[i].state = State::FREE;
        return i;
      }
    }
    return 0xFF;
  }
};

// --- Receiver duplicate suppression -----------------------------
// Remembers (src_uid, msg_id) + last ACK status so a retransmitted COMMAND
// is acknowledged with the original outcome instead of re-executing.
struct DupEntry {
  uint32_t src = 0;
  uint32_t msg_id = 0;
  uint8_t  ack_status = ST_OK;
  uint32_t seen_ms = 0;
};

class DupRing {
public:
  DupEntry entries[DUP_RING_SIZE];
  uint8_t cursor = 0;

  DupRing() { clear(); }

  void clear() {
    cursor = 0;
    for (uint8_t i = 0; i < DUP_RING_SIZE; i++) {
      entries[i].src = 0;
      entries[i].msg_id = 0;
      entries[i].ack_status = ST_OK;
      entries[i].seen_ms = 0;
    }
  }

  bool isDuplicate(uint32_t src, uint32_t msg_id, uint32_t now_ms) const {
    for (uint8_t i = 0; i < DUP_RING_SIZE; i++) {
      const DupEntry &e = entries[i];
      if (e.src == src && e.msg_id == msg_id &&
          (now_ms - e.seen_ms) <= DUP_WINDOW_MS) {
        return true;
      }
    }
    return false;
  }

  // Stored ACK status for a duplicate pair (defaults to ST_OK).
  uint8_t statusFor(uint32_t src, uint32_t msg_id, uint32_t now_ms) const {
    for (uint8_t i = 0; i < DUP_RING_SIZE; i++) {
      const DupEntry &e = entries[i];
      if (e.src == src && e.msg_id == msg_id &&
          (now_ms - e.seen_ms) <= DUP_WINDOW_MS) {
        return e.ack_status;
      }
    }
    return ST_OK;
  }

  // Insert a fresh pair. Returns false when already present (duplicate).
  bool record(uint32_t src, uint32_t msg_id, uint8_t ack_status, uint32_t now_ms) {
    if (isDuplicate(src, msg_id, now_ms)) return false;
    DupEntry &e = entries[cursor % DUP_RING_SIZE];
    e.src = src;
    e.msg_id = msg_id;
    e.ack_status = ack_status;
    e.seen_ms = now_ms;
    cursor++;
    return true;
  }
};

}  // namespace delivery
}  // namespace qymera