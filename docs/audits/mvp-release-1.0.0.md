# MVP Release Audit - Qymera 1.0.0 (Official)

**Date:** 2026-09-15
**Branch:** main (official repo `gonreyna85code/Qymera`)
**Baseline:** `89383cd` (Phase 7 delivered tree)
**Release candidate:** `47c3796fc1a5a09aed40576e9548dafde82c412d` (tag `v1.0.0`)

---

## 1. Objective

Ship the Qymera MVP from the official repo `gonreyna85code/Qymera`:

- Final branding **Qymera** (product/version `1.0.0`, channel `stable`).
- **Web OTA** self-update from GitHub Releases: HTTPS manifest + SHA-256/size
  verified streaming binary write, driven from the firmware loop, exposed in the
  web GUI.
- CI release pipeline: build 3 platforms + host tests + package assets
  (`Qymera-<v>-esp*.bin`) + `qymera-manifest.json` + GitHub Release on tag.
- Repository migration from `Qymeras-1.1` to the official `Qymera` (full history),
  preserving the Phase 7 baseline and the 1 MB flash layout for ESP8266.

## 2. Deliverables

| Item | Location |
|------|----------|
| Freeze-marking the frozen core | `main@47c3796` (official repo), tag `v1.0.0` |
| Official repository | https://github.com/gonreyna85code/Qymera |
| Release asset manifest (stable) | `https://github.com/gonreyna85code/Qymera/releases/latest/download/qymera-manifest.json` |
| Release assets | `Qymera-1.0.0-esp8266.bin`, `-esp32.bin`, `-esp32c3.bin` |
| Web OTA endpoints | `GET /firmware`, `GET /firmware/check`, `POST /firmware/update` (+ OPTIONS CORS) |
| GUI | Settings → **Firmware** card (current/latest, channel, error, live progress), dynamic version in brand, Qymera title |
| Pipeline | `.github/workflows/release.yml` |

## 3. Code Changes (baseline `89383cd` → `v1.0.0`)

| Commit | Change |
|--------|--------|
| `ecd43c6` | `src/firmware.{h,cpp}` (Web OTA engine: manifest check, HTTPS streaming download with pinned TLS, SHA-256 verify, flash write, loop-driven `tick()`); `src/sha256.h` (streaming SHA-256 + self-test); `src/version.h` (Qymera 1.0.0 / stable); `src/web.cpp` (`/firmware*` endpoints + CORS + auth + rate limit); `src/core.cpp` (boot version log + `firmware::tick()` in loop); `src/html.cpp` (branding + firmware card ES/EN + JS poll/check/update); `src/config.h` (ESP32C3/S3/S2 platform guards before generic ESP32) |
| `314f7b0` | README 1.0.0 branding + Web OTA docs; `library.properties` version=1.0.0; `.github/workflows/release.yml`; `scripts/ci_release.py`; `.gitignore` + `/dist` |
| `47c3796` | MIT `LICENSE` (preserved from the official repo initial commit) |

### Web OTA engine details

- TLS trust pinned in firmware (two real root CAs embedded):
  **ISRG Root X1** (objects.githubusercontent.com chain) + **USERTrust ECC
  Certification Authority** (github.com chain); both chains verified with OpenSSL.
- Flow: `check` → fetch manifest (product/version/channel per platform, `url`,
  `sha256`, `size`) → compare semver (only newer stable upgrades, no downgrades) →
  `update` streams via HTTPS (`HTTPC_FORCE_FOLLOW_REDIRECTS`, redirect to
  `*.github.githubassets.com`), byte-size cross-check, incremental SHA-256,
  `Update` flash write, SHA mismatch/size mismatch/write error → abort + error
  state surfaced in the GUI.
- ESP8266 specifics honored: 1 MB user layout, `Updater.h` (`getError()` codes),
  `Update.end(false)` abort (no `abort()`), 5 KB cert buffer (`setBufferSizes`,
  `setTrustAnchors`).
- States: `idle → checking → ready → downloading → installing → error` (per
  `stateToken()`), progress %, version gating per platform tag.

## 4. Verification

| Check | Result |
|-------|--------|
| Host tests (`tests/host_sanity.py`) | **212/212 PASS** |
| PlatformIO `esp8266_generic` | SUCCESS — RAM 62,332 B (76.1%) / Flash 617,800 (64.5%) |
| PlatformIO `esp32_devkit` | SUCCESS — RAM 78,152 (23.9%) / Flash 1,185,841 (90.5%) |
| PlatformIO `esp32c3_devkit` | SUCCESS — RAM 71,956 (22.0%) / Flash 1,168,322 (89.1%) |
| arduino-cli `esp8266:esp8266:generic` Base | SUCCESS |
| arduino-cli `esp8266:esp8266:generic` HardwareDemo | SUCCESS |
| Manifest generator | `scripts/ci_release.py v1.0.0` produced valid manifest matching the on-device parser schema; sha256/size computed from built binaries |
| Release workflow on tag | triggered (`Release in_progress` on `47c3796f`) and published `v1.0.0` |

### Memory deltas vs baseline (`89383cd`)

| Env | Baseline RAM | New RAM | Baseline Flash | New Flash |
|-----|--------------|---------|----------------|-----------|
| esp8266_generic | 59,824 (73.0%) | 62,332 (76.1%) | 491,908 | 617,800 |
| esp32_devkit | 75,892 (23.2%) | 78,152 (23.9%) | 1,018,609 | 1,185,841 |
| esp32c3_devkit | 70,428 (21.5%) | 71,956 (22.0%) | 1,002,882 | 1,168,322 |

Growth: embedded PEM roots, `WiFiClientSecure`+`HTTPClient`/mbedTLS linkage and
the firmware state machine; +125 KB flash mainly mbedTLS+TLS on ESP32. ESP8266
RAM 76.1% is safe (8 KB cont stack, download buffer on stack, chunked).

## 5. Risk Assessment

| Risk | Assessment |
|------|------------|
| OTA install on wrong layout | Version gating + per-platform manifest (`esp8266/esp32/esp32c3`) + byte-size cross-check before `begin`; SHA mismatch rejects write. No cross-platform install possible. |
| Redirect to another host | Trust anchors pinned to the two real GitHub chains; verification happens against `objects.githubusercontent.com`/`github.com` certs only. |
| Version regressions / downgrades | `versionCmp` semver > current only; same or older versions show "up to date". |
| ESP8266 RAM at 76.1% | Under 80 KB ceiling; the 4 KB download buffer lives on the 8 KB cont stack, freed between chunks (see `doDownloadChunk`). Monitored, not actionable now. |
| ESP32 flash 90.5% | Under the 1,310,720 B app limit; preserves existing SPIFFS partition sizing. Watch at next phase. |
| Repo migration | Full history preserved (`Qymeras-1.1` remains as legacy remote); official repo `Qymera` is the single source of truth; only the placeholder `LICENSE` initial commit was superseded (content preserved). |
| Password-bootstrapping unchanged | Web UI local-network default auth (`admin`/`qymera123`) and rate limits unchanged; OTA endpoints reuse `checkAuth` + `checkRateLimit`. |

## 6. Acceptance Criteria

| Criterion | Met? | Evidence |
|-----------|------|----------|
| Official repo `gonreyna85code/Qymera` with history | ✅ | forced migration of `main`, tag `v1.0.0` |
| Branding final "Qymera", version canonical 1.0.0 | ✅ | `version.h`, README, HTML, `library.properties`, manifest |
| Web OTA from GitHub Releases (manifest + SHA-256 + assets) | ✅ | engine + manifest schema + CI packaging |
| GUI Firmware section (check / update / progress / i18n) | ✅ | Settings → Firmware card, ES+EN |
| CI pipeline (build 3 → tests → manifest → release on tag) | ✅ | `.github/workflows/release.yml` succeeded on `v1.0.0` |
| Baseline preserved / flash 1 MB layout | ✅ | `esp8266_generic` env unchanged (`esp12e` + `eagle.flash.1m64.ld`) |
| Validation matrix green | ✅ | host 212/212 + PIO ×3 + arduino-cli ×2 |
| No unnecessary features added | ✅ | scope: Web OTA + branding + repo/pipeline only |

## 7. Next Phase

Hardware validation items inherited from the frozen core (24 h memory soak,
factory-reset hw test, extra ESP32-family boards) and the optional
**Qymera Dashboard / Link** directions.

## 8. Files Modified

| File | Change |
|------|--------|
| `src/firmware.h` / `src/firmware.cpp` | NEW — Web OTA engine (+ embedded trust anchors) |
| `src/sha256.h` / `src/version.h` | NEW — SHA-256 streaming; version metadata |
| `src/web.cpp` / `src/web.h` | `/firmware`, `/firmware/check`, `/firmware/update` (+ OPTIONS) |
| `src/core.cpp` | boot version log, `firmware::tick()` |
| `src/html.cpp` | Qymera branding, firmware card, FW JS (es/en) |
| `src/config.h` | ESP32C3/S3/S2 platform guard order |
| `platformio.ini` | (unchanged — 1 MB ESP8266 layout preserved) |
| `library.properties` | version=1.0.0 |
| `README.md` | 1.0.0 branding + Web OTA section |
| `LICENSE` | NEW — MIT |
| `.github/workflows/release.yml` | NEW — release pipeline |
| `scripts/ci_release.py` | NEW — assets + manifest packaging |
| `.gitignore` | + `/dist` |

## Post-release Hotfix: UDP TX crash (2026-09-15)

Hardware soak on ESP32 (COM3) exposed a boot-loop: `StoreProhibited` panic,
EXCVADDR 0x00000000, right after `WiFi connecting` (backtrace: `logger::coref`
→ `net::sendLog` → `transport::broadcast` → `WiFiUDP::write`). Root cause: on
ESP32, `WiFiUDP::beginPacket()` during a STA join (netif mid-reinit on
`WiFi.begin`) returns without allocating its tx buffer, and the following
`write()` memcpy's into address 0. The old `udpTxReady()` guard (mode != NULL)
was insufficient — mode is already STA while joining.

Fix (transport.cpp): broadcast()/unicast() now gate UDP TX on a usable link
(`WiFi.status()==WL_CONNECTED` or AP mode); ESP8266 path unchanged. Verified on
hardware: boot → join MATTER_NET → `WiFi connected, IP` → transport UDP,
0 panics. Note: the previously recorded 'intermittent board / crash-loop after
WiFi' was this bug, not a hardware fault.
