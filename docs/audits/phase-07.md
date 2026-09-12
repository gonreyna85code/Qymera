# Phase 7 Audit - Automation Engine 2.0

**Date:** 2026-09-11
**Branch:** main
**Commit:** `a841ef8` (features)

---

## 1. Problem Statement

The legacy `Rule` structure mixed triggers, conditions, actions and policies into
one flat struct with an implicit boolean AND/OR. It had no hysteresis, fixed
anti-bounce sampling (3 reads), no "condition held for N seconds", no explicit
condition tree, and actions fired as an atomic list (no sequencing, no per-action
retry).

The roadmap (Phase 7) required separating **Trigger / Condition / Condition Tree
/ Action / Action List / Execution Policy**, implementing first: `hysteresis`,
`configurable debounce`, `for N seconds`, `explicit AND/OR tree`, `action
sequencing`, and `action failure handling`. "No agregar features no necesarias todavía."

## 2. Decision: fixed 80-byte Automation + pure evaluator

- New `src/automation_core.h`: `Automation` (exactly **80 bytes**, `static_assert`ed)
  + `AutomationState` + pure primitives (debounced sampling with hysteresis/edges,
  left-associative AND/OR tree via `c_op_bits`, date window, `for_ms` window tracker,
  fire-once-per-entry, `ON_TIME` daily gate, `ON_INTERVAL` gate).
- `src/automations.cpp` orchestrates tick policy (delay, cooldown, sequencing,
  retries) and the only impure part: sensor reads + `executeAction`.
- EEPROM rules region grew `1600 → 1664` bytes (`src/config.h`) so 20 × 80 B +
  8 B header (1608) fits; layout guards intact, no overlap (full region ends at
  `3082 + 4 + 1 + 160 = 3247 < EEPROM_SIZE 4096`).
- Storage schema `RULES_VERSION` **1 → 2** with automatic in-RAM migration of
  legacy records (`storage.cpp migrateLegacy` + rewrite on load).
- GUI/API stays compatible: the `/rules` JSON keeps `id/sensors/type/logical_and/
  cmp/threshold/actuators/actions/levels/delay_ms/cooldown_ms/time_s/interval_ms/
  date fields` and adds optional new keys (`debounce_ms`, `for_ms`, `step_ms`,
  `hys`, `ops`, `retry`, `retry_interval`) which the GUI ignores.

### Semantics (final)

- **Trigger**: `ON_SAMPLE` (edge or threshold comparators), `ON_TIME` (daily slot,
  `time_s`/60 minute, fires once per day via date-code), `ON_INTERVAL`.
- **Debounce**: sample-conformity counted against `debounce_ms / 50 ms`
  (default 150 ms for legacy EDGE rules — maps prior `CONFIRM_READS=3`).
- **Hysteresis** (`c_hys_dec`, 0.1 units): GT engages above th, releases below
  th−hys; LT engages below th, releases above th+hys; EQ is a ±band window.
  Occupancy is sticky in `cond_active` bits.
- **Edge**: `EDGE_RISING`/`EDGE_FALLING` fire once per confirmed transition
  (debounce + `stable[]`).
- **Condition tree**: bit per adjacency (1 = OR, 0 = AND), evaluated
  left-associatively; `logical_and` on the wire is derived (true ⇔ all AND).
- **Fire policy**: fire-once-per-window-entry (a satisfied threshold no longer
  re-fires every tick); optional `for_ms` hold window (fires when the satisfied
  state has been continuous for `for_ms`); `fire_delay_ms` (armed at trigger,
  executes at expiry even if the condition leaves first — legacy behavior kept);
  `cooldown_ms` between launches.
- **Sequence**: actions execute in order; `step_ms` inserts a gap between steps;
  a failed action retries up to `retry_max` times after `retry_interval_s` seconds,
  then gives up (logged).

## 3. Code Changes

| File | Change |
|------|--------|
| `src/automation_core.h` | NEW — `Automation`/`AutomationState` + pure evaluator (sampleConditions, evalTree, dateInWindow, forWindowTick, entryTick, timeGateTick, intervalGateTick). `static_assert(sizeof(Automation)==80)` |
| `src/automations.h` | API: `rules[]`, `states[]` (now external), legacy wire-type constants `RULE_EDGE/THRESHOLD/TIME/INTERVAL`, `deleteRule/isIndexReferenced` |
| `src/automations.cpp` | Rewritten engine: tick() policy (delay/cooldown/sequencing/retries), `executeAction` with failure signaling, `advanceSequence` (step gaps + retry), state cleared on feed after boot |
| `src/storage.cpp` | `RULES_VERSION=2`; `LegacyRule` (layout-identical to old `Rule`, 64 B aligned) + `migrateLegacy()`; `loadRules()` migrates v1→v2 and rewrites; `saveRules()` on `Automation` |
| `src/config.h` | `EEPROM_RULES_SIZE` 1600 → 1664 |
| `src/web.cpp` | `handleRules` rebuilds legacy JSON from `Automation` + new optional keys; `handleSetRule` maps legacy args + optional `debounce_ms/for_ms/step_ms/hys/ops/retry/retry_interval`; clears rule state on save; EDGE defaults debounce_ms=150 |
| `docs/runtime-architecture.md` | §5.1 Automation Engine 2.0 model description |
| `tests/host_sanity.py` | NEW `[automation]` mirror suite (63 checks) |

## 4. Verification

| Check | Result |
|-------|--------|
| Host tests | **212/212 PASS** (149 prior + 63 new automation) |
| `static_assert(sizeof(Automation)==80)` | compiles on all targets |
| EEPROM guards | `#error` layout guards intact; 20×80+8 = 1608 ≤ 1664 |
| Build matrix | see below |

### Memory deltas vs HEAD (Phase 6, commit `d90d92d`)

| Env | RAM | Flash |
|-----|-----|-------|
| esp8266_generic | 59,824 B (73.0%) — +1,244 B | 491,908 (+3,252 B) |
| esp32_devkit | 75,892 (23.2%) — +960 B | 1,018,609 (+3,976 B) |
| esp32c3_devkit | 70,428 (21.5%) — +960 B | 1,002,882 (+3,880 B) |

Growth: larger `AutomationState` (20 rows), richer evaluator + web mapping.
esp8266 load 73% is safe; no change plan needed at this stage.

| Build | Status |
|-------|--------|
| PlatformIO esp8266_generic / esp32_devkit / esp32c3_devkit | SUCCESS ×3 |
| arduino-cli `esp8266:esp8266:generic` Base | SUCCESS (59,816 B RAM) |
| arduino-cli `esp8266:esp8266:generic` HardwareDemo | SUCCESS (59,588 B RAM) |

## 5. Risk Assessment

| Risk | Assessment |
|------|------------|
| EEPROM region growth | Rules end at 3082, OTA/entity-map shift +64 B; still 849 B of headroom on 4096-byte devices. Live-upgrade note: OTA hash/flag + entity IDs re-provision after migration (OAT flags are off by fleet policy; entities re-announce and regenerate). |
| v1→v2 migration correctness | Legacy fields map 1:1 (`LogicalRule` offsets match old `Rule` exactly); migration test mirrors in host suite. Data loss only if a future unknown version is seen → rule cleared (unchanged from v1 behavior). |
| Behavior change: fire-once-per-entry | Previously a held threshold re-fired every tick (suppressed only by cooldown). New engine fires once per satisfaction entry — an intentional fix, documented. Ambient rules (relay heating) rely on entry semantics. |
| cooldown semantics | Cooldown gates new launches, not in-progress sequences (as before). First-ever launch waits `cooldown_ms` from boot (matches legacy `last_action=0`). |
| GUI round-trip | `/rules` GET still emits the legacy shape; POST accepts the same fields; new keys optional and ignored by the wizard. Verified field names unchanged in `html.cpp`. |
| RAM on esp8266 | +1.2 KB; 73% used. Acceptable, but watch: next feature should budget against this. |
| ABI / wire protocol | Unchanged — this phase only touched rule storage + runtime model. |

## 6. Acceptance Criteria

| Criterion | Met? | Evidence |
|-----------|------|----------|
| Separate Trigger / Condition / Tree / Action / Policy | ✅ | `automation_core.h` + `automations.cpp` tick policy |
| Hysteresis | ✅ | `c_hys_dec` sticky bands, GT/LT mirrored tests |
| Configurable debounce | ✅ | `debounce_ms`, per 50 ms sample-confirm, default 150 for legacy EDGE |
| `for N seconds` | ✅ | `for_ms` window tracker, single-fire |
| Explicit AND/OR tree | ✅ | `c_op_bits` left-assoc chain, mixed-chain test |
| Action sequencing | ✅ | `step_ms` gaps between actions |
| Failure handling (retry) | ✅ | `retry_max`/`retry_interval_s`, give-up logged |
| No unnecessary features added | ✅ | roadmap-first scope only |
| Legacy rules survive upgrade | ✅ | v1→v2 migration + rewrite on load |
| GUI compatibility | ✅ | legacy JSON fields intact + optional new keys |
| Build matrix green | ✅ | 5/5 |

## 7. Next Phase

Phase 8 per roadmap (as-is).

## 8. Files Modified

| File | Change |
|------|--------|
| `src/automation_core.h` | NEW |
| `src/automations.h` | API rework |
| `src/automations.cpp` | Engine 2.0 |
| `src/storage.cpp` | v2 schema + migration |
| `src/config.h` | rules region 1664 |
| `src/web.cpp` | rules endpoints |
| `docs/runtime-architecture.md` | §5.1 |
| `tests/host_sanity.py` | `[automation]` mirror suite |