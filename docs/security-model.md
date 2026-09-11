# Qymera Security Model

**Version:** 1.1.0 (baseline)  
**Date:** 2026-08-31  
**Branch:** main

---

## 1. Threat Model

Qymera is a **local-first** automation runtime for trusted LAN environments. It is **not** designed for exposure to untrusted networks or the public internet.

### Assumptions

| Assumption | Rationale |
|------------|-----------|
| Device on trusted LAN | Home/office network with WPA2/3 |
| No attacker on same L2 segment | Physical access = game over anyway |
| No MITM on LAN | Switches, not hubs |
| Device physically secured | Tampering = physical access |
| OTA only triggered by authorized user | Web UI or physical button |

### Out of Scope

- Internet exposure (no cloud, no reverse proxy)
- Supply chain attacks (firmware signing not implemented)
- Side-channel attacks (power, EM)
- Physical tampering

---

## 2. Current Security Controls

### 2.1 HTTP API Authentication

**Location**: `web.cpp` - `checkAuth()`, `EXPECTED_AUTH_BASE64`

```cpp
static const char* AUTH_USERNAME = "admin";
static const char* AUTH_PASSWORD = "qymera123";
static const char* EXPECTED_AUTH_BASE64 = "YWRtaW46cXltZXJhMTIz";  // "admin:qymera123"
```

**Behavior**:
- Auth **disabled by default** (backward compatibility)
- Enabled when `AUTH_USERNAME`/`AUTH_PASSWORD` are non-empty constants
- Basic Auth only (`Authorization: Basic <base64>`)
- Pre-encoded credential comparison (no runtime Base64 decode)
- Returns 401 if enabled and invalid/missing

**Protected Endpoints**:
- `/save` (WiFi credentials)
- `/genset/save` (net/broadcast ports, interval)
- `/factory` (factory reset)
- `/toggle`, `/dimmer` (actuator control)
- `/rules/set`, `/rules/delete` (automation rules)
- `/calib/set` (sensor config)
- `/ota/toggle` (OTA enable/disable)

**Unprotected** (read-only):
- `/`, `/calib`, `/rules`, `/logs`, `/ota/status`

### 2.2 Rate Limiting

**Location**: `web.cpp` - `checkRateLimit()`

```cpp
static unsigned long last_request_time = 0;
static unsigned char burst_count = 0;
static const unsigned long REQUEST_COOLDOWN_MS = 2000;  // 2s window
static const unsigned char RATE_LIMIT_BURST = 6;        // 6 requests/window
```

- Sliding 2-second window
- Burst allowance: 6 requests (covers UI multi-POST flows)
- Sustained floods → 429 (Rate limit exceeded)

### 2.3 Input Validation

**Location**: `web.cpp` - `parseStrictUnsigned()`, `parseStrictLong()`, `parseStrictFloat()`

- Rejects empty strings
- Rejects non-numeric prefixes
- Rejects trailing junk (`"12abc"` → reject)
- Rejects overflow/underflow (`ERANGE`, `inf`, `nan`)
- Range validation per endpoint (ports 1024-65500, interval 5000-600000, etc.)

### 2.4 CORS

**Location**: `web.cpp` - `addCorsHeaders()`

```cpp
Access-Control-Allow-Origin: *
Access-Control-Allow-Methods: GET, POST, OPTIONS
Access-Control-Allow-Headers: Content-Type
```

- Permissive (allows any origin)
- OPTIONS preflight handled
- No credentials mode (no cookies)

### 2.5 Networking Layer

**No authentication at protocol level**:

- UDP broadcast: any device on LAN can send packets
- ESP-NOW: any device with matching MAC can send
- Packet validation: magic byte, version, size, exact payload length
- **No encryption, no signing, no replay protection**

**Command delivery**: Unicast UDP to owner IP or ESP-NOW broadcast — any device knowing target IP/MAC can send commands.

### 2.4 OTA Integrity

**Location**: `storage.cpp` - `verifyOtaIntegrity()`

```cpp
// NOT a firmware hash — uses chip ID as provisioning token
uint32_t calculateFirmwareHash() {
    return GET_CHIP_ID();  // Stable across firmware updates
}
```

**Behavior**:
- First OTA enable: stores chip ID as "baseline hash"
- Subsequent enables: compares current chip ID to stored
- **Does not verify firmware integrity** — only detects chip replacement
- On mismatch: OTA disabled, flag cleared, warning logged

### 2.5 Credential Storage

- **ESP32**: `Preferences` (encrypted flash partition)
- **ESP8266**: EEPROM (plaintext, no encryption)
- WiFi credentials: SSID (1-32 chars) + Password (1-64 chars)
- No key derivation, no salting — raw storage

---

## 3. Security Gaps (Baseline)

| Gap | Severity | Description |
|-----|----------|-------------|
| **Auth disabled by default** | High | Any LAN device can control actuators, change WiFi, factory reset |
| **No command ACK/replay protection** | Medium | Commands can be replayed; no delivery guarantee |
| **No net encryption/signing** | Medium | LAN sniffing reveals all state; command injection possible |
| **No firmware signature verification** | High | OTA accepts any binary; chip-ID check only detects chip swap |
| **Plaintext credentials (ESP8266)** | Medium | EEPROM readable via serial/physical |
| **Permissive CORS** | Low | Any web page can call API (mitigated by auth) |
| **No CSRF protection** | Medium | Browser-based attacks possible if auth enabled |
| **Hardcoded default credentials** | High | `admin:qymera123` known if auth enabled |
| **No session management** | Medium | Basic Auth = credentials sent every request |
| **OTA integrity ≠ firmware integrity** | High | Chip-ID check ≠ firmware signature |

---

## 4. Security Hardening Roadmap (Phase 14)

### 4.1 Mandatory (Before Production)

1. **Enable auth by default** — require explicit opt-out
2. **Change default credentials** — force setup-time password
3. **Firmware signing** — Ed25519 sig verification on OTA
4. **Command ACK + msg_id** — replay protection at protocol level
4. **Net encryption** — AES-GCM or ChaCha20-Poly1305 (PSK per deployment)

### 4.2 Recommended

5. **HTTPS** — self-signed cert + cert pinning (or mTLS)
6. **CSRF tokens** — per-session tokens for state-changing endpoints
7. **Session tokens** — replace Basic Auth with short-lived tokens
8. **Credential encryption (ESP8266)** — AES-256 with device-unique key
9. **Secure OTA** — signed manifests + rollback protection
10. **Audit logging** — security events (auth failures, config changes)

### 4.3 Defense in Depth

10. **Network segmentation** — document VLAN isolation recommendation
11. **Rate limit hardening** — per-IP, exponential backoff
12. **Input sanitization** — HTML escape in logs (prevent log injection)
13. **Secure boot** — ESP32 secure boot + flash encryption (hardware)

---

## 5. Secure Configuration Checklist

For production deployment:

- [ ] Enable HTTP auth (`AUTH_USERNAME`, `AUTH_PASSWORD` non-empty)
- [ ] Change default password from `qymera123`
- [ ] Isolate device on dedicated VLAN (no internet)
- [ ] Disable OTA after provisioning (or verify firmware signatures)
- [ ] Document credential rotation procedure
- [ ] Monitor logs for auth failures (`/logs` endpoint)
- [ ] Physical security: device in locked enclosure

---

## 5. Security Testing (Phase 16)

| Test | Description |
|------|-------------|
| Auth bypass | Verify all protected endpoints return 401 without credentials |
| Rate limit | Flood POST `/toggle` → 429 after burst |
| Input validation | Send malformed payloads → 400, no crash |
| Replay attack | Capture command packet, replay → should be rejected (after Phase 14) |
| OTA tampering | Flash modified firmware → OTA rejected (after signing) |
| Credential extraction | Read EEPROM/Preferences → credentials not plaintext (ESP32 OK, ESP8266 gap) |
| CSRF | Authenticated browser form submit without token → rejected (after CSRF tokens) |
| Net injection | Send crafted net packet → validated, not crashed |