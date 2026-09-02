# Phase 0 Audit - Baseline & Scope Control

**Date:** 2026-08-31  
**Branch:** main  
**Commit:** `77606f4`  
**Auditor:** Automated baseline inspection

---

## 1. Repository Inspection

### Branch Confirmation
- Current branch: `main`
- HEAD: `77606f4` (docs(gui): freeze audit sync - CODE FREEZE / PRODUCTION BASELINE)
- No experimental AI code detected in `src/`
- No `feature/ai-experiments` or `feature/GUI` code merged

### Module Map (21 source files)

```
src/
├── Qymera.h              # Public facade (namespace Qymera::)
├── config.h              # Platform abstraction, constants, EEPROM layout
├── core.h/cpp            # Runtime orchestration, WiFi, OTA, main loop
├── sensors.h/cpp         # Entity model, sensors/actuators, mesh callbacks
├── web.h/cpp             # HTTP server, API endpoints, auth, rate limit
├── mesh.h/cpp            # UDP/ESP-NOW transport, packet parsing, discovery
├── automations.h/cpp     # Rule engine (EDGE/THRESHOLD/TIME/INTERVAL)
├── storage.h/cpp         # EEPROM/Preferences persistence, migration
├── log.h/cpp             # Layered logging, UDP broadcast, remote log ingest
├── espnow_p2p.cpp        # ESP-NOW RX FIFO (bounded ring buffer)
├── html.cpp              # Embedded GUI (single-page app, ~70KB)
├── main.cpp              # Sketch entry point
```

### Dependency Graph (simplified)

```
main.cpp
    └── Qymera.h (facade)
        ├── core.h/cpp
        │   ├── config.h
        │   ├── sensors.h
        │   ├── mesh.h
        │   ├── web.h
        │   ├── automations.h
        │   ├── storage.h
        │   └── log.h
        ├── sensors.h/cpp
        │   ├── config.h, core.h, mesh.h, web.h, automations.h, log.h
        ├── web.h/cpp
        │   ├── config.h, core.h, mesh.h, sensors.h, automations.h, storage.h, log.h
        ├── mesh.h/cpp
        │   ├── config.h, core.h, sensors.h, log.h
        ├── automations.h/cpp
        │   ├── sensors.h, log.h, storage.h
        ├── storage.h/cpp
        │   ├── automations.h, core.h, log.h, mesh.h, sensors.h
        ├── log.h/cpp
        │   ├── mesh.h
        └── config.h (platform abstraction)
```

**No circular dependencies detected.** Clear layering: `core` orchestrates; `sensors`, `web`, `mesh`, `automations`, `storage`, `log` are leaf modules called by `core`.

---

## 2. AI/Experimental Code Check

**Result: CLEAN**

- No `ai.cpp`, `ai.h` files
- No `aidig`, `aiana`, `QMAI` references in `src/`
- No `feature/ai-experiments` code merged
- No LLM/Ollama integration code
- No browser agent code
- No cloud integration code
- No Matter/MQTT/Zigbee/Z-Wave code
- No mobile app code

Confirmed: `main` is a clean embedded automation runtime baseline.

---

## 3. Build Matrix (Baseline)

| Environment | Status | RAM | Flash | Duration |
|-------------|--------|-----|-------|----------|
| esp8266_generic | SUCCESS | 69.6% (57,024 B / 81,920 B) | 42.2% (441,188 B / 1,044,464 B) | 18.9s |
| esp32_devkit | SUCCESS | 22.6% (74,028 B / 327,680 B) | 73.7% (966,033 B / 1,310,720 B) | 18.2s |
| esp32c3_devkit | SUCCESS | 20.9% (68,564 B / 327,680 B) | 72.8% (953,568 B / 1,310,720 B) | 15.9s |

**All 3 environments: SUCCESS**

---

## 4. Host Test Suite (Baseline)

```
python tests/host_sanity.py
```

**Result: 45/45 PASS**

| Category | Tests | Status |
|----------|-------|--------|
| Timezone | 10 | PASS |
| Strict Float | 18 | PASS |
| ESP-NOW FIFO | 12 | PASS |
| **Total** | **45** | **PASS** |

---

## 5. Key Metrics (Baseline)

| Metric | Value |
|--------|-------|
| Git SHA | `77606f4` |
| Version | 1.1.0 |
| Build environments | 3/3 SUCCESS |
| Host tests | 45/45 PASS |
| ESP8266 RAM | 69.6% (57,024 B / 81,920 B) |
| ESP8266 Flash | 42.2% (441,188 B / 1,044,464 B) |
| ESP32 RAM | 22.6% (74,028 B / 327,680 B) |
| ESP32 Flash | 73.7% (966,033 B / 1,310,720 B) |
| ESP32-C3 RAM | 20.9% (68,564 B / 327,680 B) |
| ESP32-C3 Flash | 72.8% (953,568 B / 1,310,720 B) |
| HTML payload | ~70KB (embedded in `html.cpp`) |
| Max sensors | 64 (40 persisted) |
| Max rules | 20 |
| Mesh timeout | 30s (MESH_TIMEOUT) |
| OTA integrity | Chip-ID provisioning (not full hash) |

---

## 6. Documentation Created (Phase 0 Deliverables)

| File | Purpose |
|------|---------|
| `docs/runtime-architecture.md` | Module map, execution flow, concurrency, data ownership, invariants |
| `docs/data-model.md` | Current `Calibration` analysis, target entity model, migration strategy |
| `docs/protocol.md` | Packet formats, validation, discovery, commands, lifecycle, V2 design |
| `docs/security-model.md` | Threat model, current controls, gaps, hardening roadmap |
| `docs/testing-strategy.md` | Test pyramid, required layers, infrastructure, metrics, gates |
| `docs/professionalization-roadmap.md` | 20-phase roadmap (this document as reference) |

---

## 7. Scope Confirmation

**Main branch contains ONLY:**

- Arduino firmware for ESP8266/ESP32/ESP32-C3
- Web UI (embedded in `html.cpp`)
- Deterministic automation runtime
- UDP/ESP-NOW mesh
- EEPROM/Preferences persistence
- Rule engine (EDGE/THRESHOLD/TIME/INTERVAL)
- HTTP API with Basic Auth + rate limit
- OTA with chip-ID provisioning check
- Layered logging with UDP broadcast

**No AI, Dashboard, Link, ESP-IDF, Matter, MQTT, Zigbee, Mobile App code.**

---

## 8. Readiness for Phase 1

**Status: READY**

- Baseline documented ✓
- Builds pass ✓
- Tests pass ✓
- No AI contamination ✓
- Scope boundaries clear ✓
- Documentation created ✓

**Next step**: Phase 1 - Entity Model (introduce explicit `Entity`, `EntityConfig`, `EntityState`, `Capability`, `Ownership` types alongside existing `Calibration` struct)

---

## 9. Files Modified in Phase 0

**Documentation only (no code changes):**

- `docs/runtime-architecture.md` (created)
- `docs/data-model.md` (created)
- `docs/protocol.md` (created)
- `docs/security-model.md` (created)
- `docs/testing-strategy.md` (created)
- `docs/professionalization-roadmap.md` (created)
- `docs/audits/phase-00.md` (this file)

**No code modifications in Phase 0.** Baseline established for comparison.