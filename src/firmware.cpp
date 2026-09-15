#include "firmware.h"
#include "sha256.h"
#include "log.h"

#if defined(PLATFORM_ESP8266)
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Updater.h>
#else
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Update.h>
#endif

namespace firmware {

#define MANIFEST_URL \
  "https://github.com/gonreyna85code/Qymera/releases/latest/download/qymera-manifest.json"

#define FW_DL_BUF 512
#define FW_CHUNK_PACE_MS 15
#define FW_HTTP_TIMEOUT_MS 15000
#define FW_MANIFEST_MAX 2048

// ---------------------------------------------------------------------------
// Pinned HTTPS trust anchors (no external crypto dependency).
//
// github.com and objects.githubusercontent.com (the redirect target of every
// /releases/*/download asset) are validated against two public root
// certificates:
//    * ISRG Root X1   (Let's Encrypt)  -> objects.githubusercontent.com
//    * USERTrust ECC  (Sectigo)        -> github.com
// The roots were pulled from their official publishers (letsencrypt.org and
// the Mozilla CA bundle) and the live server chains were verified against them
// with openssl before release. Bundling roots (not intermediates) keeps the
// trust stable across intermediate certificate rotation.
// ---------------------------------------------------------------------------

static const char ROOTS_PEM[] PROGMEM =
  "-----BEGIN CERTIFICATE-----\n"
  "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\n"
  "-----END CERTIFICATE-----\n"
  "-----BEGIN CERTIFICATE-----\n"
  "MIICjzCCAhWgAwIBAgIQXIuZxVqUxdJxVt7NiYDMJjAKBggqhkjOPQQDAzCBiDELMAkGA1UEBhMC\n"
  "VVMxEzARBgNVBAgTCk5ldyBKZXJzZXkxFDASBgNVBAcTC0plcnNleSBDaXR5MR4wHAYDVQQKExVU\n"
  "aGUgVVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNVBAMTJVVTRVJUcnVzdCBFQ0MgQ2VydGlmaWNhdGlv\n"
  "biBBdXRob3JpdHkwHhcNMTAwMjAxMDAwMDAwWhcNMzgwMTE4MjM1OTU5WjCBiDELMAkGA1UEBhMC\n"
  "VVMxEzARBgNVBAgTCk5ldyBKZXJzZXkxFDASBgNVBAcTC0plcnNleSBDaXR5MR4wHAYDVQQKExVU\n"
  "aGUgVVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNVBAMTJVVTRVJUcnVzdCBFQ0MgQ2VydGlmaWNhdGlv\n"
  "biBBdXRob3JpdHkwdjAQBgcqhkjOPQIBBgUrgQQAIgNiAAQarFRaqfloI+d61SRvU8Za2EurxtW2\n"
  "0eZzca7dnNYMYf3boIkDuAUU7FfO7l0/4iGzzvfUinngo4N+LZfQYcTxmdwlkWOrfzCjtHDix6Ez\n"
  "nPO/LlxTsV+zfTJ/ijTjeXmjQjBAMB0GA1UdDgQWBBQ64QmG1M8ZwpZ2dEl23OA1xmNjmjAOBgNV\n"
  "HQ8BAf8EBAMCAQYwDwYDVR0TAQH/BAUwAwEB/zAKBggqhkjOPQQDAwNoADBlAjA2Z6EWCNzklwBB\n"
  "HU6+4WMBzzuqQhFkoJ2UOQIReVx7Hfpkue4WQrO/isIJxOzksU0CMQDpKmFHjFJKS04YcPbWRNZu\n"
  "9YO6bVi9JNlWSOrvxKJGgYhqOkbRqZtNyWHa0V1Xahg=\n"
  "-----END CERTIFICATE-----\n"
  "\n"
  "GlobalSign ECC Root CA - R5\n"
  "===========================\n";

#if defined(PLATFORM_ESP8266)
static BearSSL::X509List g_trust(ROOTS_PEM);
#endif

static void configSecure(WiFiClientSecure &c) {
#if defined(PLATFORM_ESP8266)
  c.setTrustAnchors(&g_trust);
  c.setBufferSizes(512, 512);
#else
  c.setCACert(ROOTS_PEM);
#endif
  c.setTimeout(20);
}

// ---------------------------------------------------------------------------
// Minimal state + manifest fields
// ---------------------------------------------------------------------------

static State st = FW_IDLE;
static String latest;
static String g_channel;
static String dl_url;
static String dl_sha256;
static long dl_size = 0;
static String error_msg;
static uint32_t percent = 0;

// Download-time state (kept between loop iterations so the server stays up).
static WiFiClientSecure g_client;
static HTTPClient g_http;
static Sha256 g_hash;
static uint32_t dl_total = 0;
static uint32_t dl_done = 0;
static uint32_t last_io_ms = 0;
static bool dl_active = false;

static const char *platformTag() {
#if defined(PLATFORM_ESP8266)
  return "esp8266";
#elif defined(PLATFORM_ESP32C3)
  return "esp32c3";
#else
  return "esp32";
#endif
}

static const char* updateErrorString() {
#if defined(PLATFORM_ESP8266)
  switch (Update.getError()) {
    case UPDATE_ERROR_OK:            return "update_ok";
    case UPDATE_ERROR_WRITE:         return "update_write";
    case UPDATE_ERROR_ERASE:         return "update_erase";
    case UPDATE_ERROR_SPACE:         return "update_space";
    case UPDATE_ERROR_SIZE:          return "update_size";
    case UPDATE_ERROR_STREAM:        return "update_stream";
    case UPDATE_ERROR_MD5:           return "update_md5";
    case UPDATE_ERROR_MAGIC_BYTE:    return "update_magic";
    case UPDATE_ERROR_SIGN:          return "update_sign";
    case UPDATE_ERROR_NO_DATA:       return "update_no_data";
    case UPDATE_ERROR_OOM:           return "update_oom";
    default:                         return "update_error";
  }
#else
  return Update.errorString();
#endif
}

static void setError(const char *msg) {
  if (dl_active) {
    dl_active = false;
    g_http.end();
    g_client.stop();
#if defined(PLATFORM_ESP8266)
    Update.end(false);
#else
    Update.abort();
#endif
    logger::core("Web OTA update discarded (no flash write yet)");
  }
  st = FW_ERROR;
  error_msg = msg;
  logger::warnf("Web OTA error: %s", msg);
}

static bool hexEqual(const char *a, const uint8_t *d, size_t n) {
  static const char hex[] = "0123456789abcdef";
  for (size_t i = 0; i < n; i++) {
    char hi = (char)hex[d[i] >> 4];
    char lo = (char)hex[d[i] & 0x0f];
    char ah = a[i * 2], al = a[i * 2 + 1];
    if (ah >= 'A' && ah <= 'F') ah += 32;
    if (al >= 'A' && al <= 'F') al += 32;
    if (ah != hi || al != lo) return false;
  }
  return true;
}

static int versionCmp(const char *a, const char *b) {
  int ma = (int)strtol(a, nullptr, 10);
  int mb = (int)strtol(b, nullptr, 10);
  if (ma != mb) return ma - mb;
  const char *ad = strchr(a, '.');
  const char *bd = strchr(b, '.');
  if (!ad || !bd) return 0;
  int sa = (int)strtol(ad + 1, nullptr, 10);
  int sb = (int)strtol(bd + 1, nullptr, 10);
  if (sa != sb) return sa - sb;
  const char *ap = strchr(ad + 1, '.');
  const char *bp = strchr(bd + 1, '.');
  int pa = ap ? (int)strtol(ap + 1, nullptr, 10) : 0;
  int pb = bp ? (int)strtol(bp + 1, nullptr, 10) : 0;
  return pa - pb;
}

// --- tiny JSON scanners over our own manifest (no ArduinoJson dependency) ----

static String extractStr(const String &body, const char *key, int from) {
  String sk = String("\"") + key + "\"";
  int ki = body.indexOf(sk, from);
  if (ki < 0) return String();
  int colon = body.indexOf(':', ki);
  if (colon < 0) return String();
  int q = body.indexOf('"', colon + 1);
  if (q < 0) return String();
  int q2 = body.indexOf('"', q + 1);
  if (q2 < 0) return String();
  return body.substring(q + 1, q2);
}

static bool extractLong(const String &body, const char *key, long *out) {
  String sk = String("\"") + key + "\"";
  int ki = body.indexOf(sk);
  if (ki < 0) return false;
  int colon = body.indexOf(':', ki);
  if (colon < 0) return false;
  int i = colon + 1;
  while (i < (int)body.length() && (body[i] == ' ' || body[i] == '\t')) i++;
  long v = 0;
  bool any = false;
  while (i < (int)body.length() && body[i] >= '0' && body[i] <= '9') {
    v = v * 10 + (body[i] - '0');
    any = true;
    i++;
  }
  if (!any) return false;
  *out = v;
  return true;
}

// ---------------------------------------------------------------------------
// Manifest check (blocking but bounded; runs inside FW_CHECKING from tick()).
// ---------------------------------------------------------------------------

static void doCheck() {
  WiFiClientSecure c;
  HTTPClient h;
#if defined(PLATFORM_ESP8266)
  c.setTrustAnchors(&g_trust);
  c.setBufferSizes(512, 512);
#else
  c.setCACert(ROOTS_PEM);
#endif
  h.setTimeout(FW_HTTP_TIMEOUT_MS);
  h.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  h.setReuse(false);
  if (!h.begin(c, MANIFEST_URL)) {
    setError("http_begin_manifest");
    return;
  }
  int code = h.GET();
  if (code != HTTP_CODE_OK) {
    String err = "manifest_http_" + String(code);
    error_msg = err;
    st = FW_ERROR;
    logger::warnf("Web OTA check failed: %s", err.c_str());
    h.end();
    return;
  }
  int total = h.getSize();
  if (total <= 0 || total > FW_MANIFEST_MAX) {
    setError("manifest_size");
    h.end();
    return;
  }
  String body;
  body.reserve((size_t)total + 8);
  WiFiClient &s = h.getStream();
  uint8_t buf[128];
  while ((int)body.length() < total) {
    int n = s.readBytes(buf, sizeof(buf));
    if (n <= 0) {
      if (s.connected()) { delay(10); continue; }
      break;
    }
    body.concat((const char *)buf, n);
  }
  h.end();
  if ((int)body.length() < total) {
    setError("manifest_truncated");
    return;
  }

  if (extractStr(body, "product", 0) != QYMERA_PRODUCT) {
    setError("manifest_product");
    return;
  }
  latest = extractStr(body, "version", 0);
  if (latest.length() == 0 || latest.length() > 16) {
    setError("manifest_version");
    return;
  }
  g_channel = extractStr(body, "channel", 0);
  if (g_channel.length() == 0) g_channel = QYMERA_CHANNEL;

  String tag = String("\"") + platformTag() + "\"";
  int pt = body.indexOf(tag);
  if (pt < 0) {
    setError("manifest_platform");
    return;
  }
  dl_url = extractStr(body, "url", pt);
  dl_sha256 = extractStr(body, "sha256", pt);
  dl_size = -1;
  extractLong(body, "size", &dl_size);
  if (dl_url.length() < 10 || !dl_url.startsWith("http") || dl_sha256.length() != 64) {
    setError("manifest_asset");
    return;
  }
  st = FW_READY;
  percent = 0;
  logger::coref("Web OTA check: latest %s (current %s), platform %s",
                latest.c_str(), QYMERA_VERSION_STRING, platformTag());
}

// ---------------------------------------------------------------------------
// Download + flash (streamed, chunked across loop() iterations).
// ---------------------------------------------------------------------------

static void doDownloadStart() {
  if (!Sha256::selftest()) {
    setError("sha_selftest");
    return;
  }
  configSecure(g_client);
  g_http.setTimeout(FW_HTTP_TIMEOUT_MS);
  g_http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  g_http.setReuse(false);
  if (!g_http.begin(g_client, dl_url)) {
    setError("http_begin_firmware");
    return;
  }
  int code = g_http.GET();
  if (code != HTTP_CODE_OK) {
    String err = "firmware_http_" + String(code);
    error_msg = err;
    st = FW_ERROR;
    g_http.end();
    logger::warnf("Web OTA download failed: %s", err.c_str());
    return;
  }
  long got = g_http.getSize();
  if (got <= 0) {
    setError("firmware_size");
    return;
  }
  if (dl_size > 0 && got != dl_size) {
    setError("firmware_size_mismatch");
    return;
  }
  dl_total = (uint32_t)got;
  dl_done = 0;
  g_hash.init();
  if (!Update.begin(dl_total)) {
    setError(updateErrorString());
    return;
  }
  dl_active = true;
  last_io_ms = 0;
  logger::coref("Web OTA download started: %u bytes", dl_total);
}

static void doDownloadChunk() {
  if (WiFi.status() != WL_CONNECTED) {
    setError("wifi_lost");
    return;
  }
  uint32_t now = millis();
  if (now - last_io_ms < FW_CHUNK_PACE_MS) return;
  last_io_ms = now;

  WiFiClient &s = g_http.getStream();
  if (dl_done < dl_total && s.available() == 0) {
    if (s.connected()) return;
    setError("stream_closed");
    return;
  }
  size_t want = FW_DL_BUF;
  size_t avail = (size_t)s.available();
  if (avail < want) want = avail;
  if (want == 0) return;
  uint8_t buf[FW_DL_BUF];
  int n = s.readBytes(buf, want);
  if (n <= 0) {
    if (dl_done < dl_total) setError("stream_closed");
    return;
  }
  if (Update.write(buf, (size_t)n) != (size_t)n) {
    setError(updateErrorString());
    return;
  }
  g_hash.update(buf, (size_t)n);
  dl_done += (uint32_t)n;
  percent = (uint8_t)(dl_total ? (uint32_t)((uint64_t)dl_done * 100 / dl_total) : 0);
  if ((dl_done % (dl_total / 4 + 1)) == 0) logger::coref("Web OTA download: %u%%", percent);

  if (dl_done >= dl_total) {
    g_http.end();
    g_client.stop();
    uint8_t digest[32];
    g_hash.final(digest);
    if (!hexEqual(dl_sha256.c_str(), digest, 32)) {
      setError("sha_mismatch");
      return;
    }
    if (!Update.end(true)) {
      setError(updateErrorString());
      return;
    }
    st = FW_INSTALLING;
    percent = 100;
    dl_active = false;
    logger::core("Web OTA install complete - rebooting");
    delay(2000);
    RESET_MCU();
  }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

const char *stateToken() {
  switch (st) {
    case FW_IDLE:        return "idle";
    case FW_CHECKING:    return "checking";
    case FW_READY:       return "ready";
    case FW_DOWNLOADING: return "downloading";
    case FW_INSTALLING:  return "installing";
    case FW_ERROR:       return "error";
  }
  return "idle";
}

bool updateAvailable() {
  return st == FW_READY && versionCmp(latest.c_str(), QYMERA_VERSION_STRING) > 0;
}

const char *currentVersion() { return QYMERA_VERSION_STRING; }
const char *latestVersion()  { return latest.c_str(); }
const char *channel()        { return g_channel.c_str(); }
const char *errorMessage()   { return error_msg.c_str(); }

uint8_t progressPercent() {
  if (st == FW_INSTALLING) return 100;
  return percent;
}

bool requestCheck() {
  if (st == FW_IDLE || st == FW_ERROR) {
    st = FW_CHECKING;
    percent = 0;
    error_msg = String();
    logger::core("Web OTA check requested");
    return true;
  }
  return false;
}

bool requestUpdate() {
  if (st != FW_READY || !updateAvailable()) return false;
  st = FW_DOWNLOADING;
  percent = 0;
  logger::core("Web OTA update requested");
  return true;
}

void tick() {
  if (WiFi.status() != WL_CONNECTED) return;
  switch (st) {
    case FW_CHECKING:    doCheck(); break;
    case FW_DOWNLOADING: if (!dl_active) doDownloadStart(); else doDownloadChunk(); break;
    default: break;
  }
}

}  // namespace firmware