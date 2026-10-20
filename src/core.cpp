#include <ArduinoOTA.h>
#if defined(ESP32)
#include <esp_netif.h>
#endif
#include "core.h"
#include "config.h"
#include "web.h"
#include "entities.h"
#include "sensors.h"
#include "net.h"
#include "automations.h"
#include "storage.h"
#include "firmware.h"
#include "log.h"
#include "screen.h"


// WiFi/Boot state machine
enum class WifiState : uint8_t {
  INIT,
  STA_CONNECTING,
  STA_CONNECTED,
  AP_FALLBACK,
  STA_DISCONNECTED_RETRY
};
static WifiState wifi_state = WifiState::INIT;
static bool network_stack_ready = false;
static uint8_t last_disconnect_reason = 0;
static uint8_t last_logged_reason = 0;
static bool udp_web_net_inited = false;


namespace core {

// Flags to ensure network-dependent services are initialized exactly once,
// only after the WiFi stack (STA or AP) is operational.

static bool ota_initialized = false;
static bool web_initialized = false;
static bool net_initialized = false;
static bool ota_enabled = false;
// Vars globales compartidas (no estáticas: accesibles para configuración).

static String uid;
String ssid;
String password;
static bool wifi_connected = false;
static bool wifi_connecting = false;
static unsigned long wifi_connect_start = 0;
static unsigned long last_attempt = 0;
static unsigned long last_disconnect_time = 0;
static unsigned long last_report = 0;
static bool first_report = true;
GeneralSettings genset;

 // ================= HELPERS ===================

 /// Reanuda el servidor hotspot en modo AP tras una conexión WiFi fallida.
static void startOtaService();
static void ensureNetworkStack();
static void startAP() {
  ensureNetworkStack();
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID);
  wifi_connected = false;
  wifi_connecting = false;
  // Init network-dependent services once
  if (!udp_web_net_inited) {
    if (!web_initialized) {
      web::init();
      web_initialized = true;
    }
    // web::init() already calls server.begin() in current codebase; avoid double close/begin
    if (!net_initialized) {
      net::init();
      net_initialized = true;
    }
    udp_web_net_inited = true;
  } else {
    // Ensure server is running if web was init before
    if (web_initialized) {
      // Don't close/reopen unnecessarily; just ensure begin not called twice (init already did)
    }
  }
  startOtaService();
  wifi_state = WifiState::AP_FALLBACK;
  logger::coref("AP mode '%s' started (IP:%s)", AP_SSID, WiFi.softAPIP().toString().c_str());
}

/// Single controlled OTA initialization path.
/// Only starts ArduinoOTA when the runtime flag says it is enabled and the
/// network interface is already operational. Never called from storage.
static void startOtaService() {
  if (ota_initialized || !ota_enabled) return;
  static String ota_hostname;
  ota_hostname = "qymera-" + String(GET_CHIP_ID());
  ArduinoOTA.setHostname(ota_hostname.c_str());
  ArduinoOTA.onStart([]() {
    logger::core("OTA: transfer started");
  });
  ArduinoOTA.onProgress([](unsigned int p, unsigned int t) {
    if (p % 25 == 0) logger::coref("OTA: %u%%", p);
  });
  ArduinoOTA.onEnd([]() {
    logger::core("OTA: transfer complete");
  });
  ArduinoOTA.onError([](ota_error_t e) {
    logger::warnf("OTA error: %d", (int)e);
  });
  ArduinoOTA.begin();
  ota_initialized = true;
  logger::core("ArduinoOTA started");
}

/// kicks off a non-blocking WiFi connection attempt.
static void ensureNetworkStack() {
#if defined(ESP32)
  if (!network_stack_ready) {
    esp_netif_init();
    // esp_event_loop_create_default may return ESP_ERR_INVALID_STATE if already exists
    esp_err_t err = esp_event_loop_create_default();
    (void)err;
    network_stack_ready = true;
  }
#endif
}

static void startWiFi() {
  ensureNetworkStack();
  if (ssid.isEmpty() || ssid.length() == 0) {
    wifi_state = WifiState::AP_FALLBACK;
    startAP();
    return;
  }
  // Do not destroy existing AP if already in AP fallback; require explicit reset/path
  if (wifi_state == WifiState::AP_FALLBACK) {
    // Avoid flipping modes while AP is serving config; try to keep AP stable
    logger::core("AP fallback active - refusing to switch to STA without reset/new creds");
    return;
  }
  WiFi.persistent(false); // we manage credentials in our storage
  SET_AUTO_CONNECT();
  SET_WIFI_SLEEP();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  wifi_connecting = true;
  wifi_connect_start = millis();
  wifi_state = WifiState::STA_CONNECTING;
  last_attempt = millis();
  logger::coref("WiFi connecting to %s", ssid.c_str());
}

/// Non-blocking WiFi status checker — call from loop().
/// Completes connection on success or falls back to AP on timeout.
static void checkWiFiStatus() {
  if (!wifi_connecting) return;

  if (WiFi.status() == WL_CONNECTED) {
    wifi_connecting = false;
    wifi_connected = true;
    sensors::initNTP();
    startOtaService();
    if (!net_initialized) {
      net::init();
      net_initialized = true;
    }
    if (!web_initialized) {
      web::init();
      web_initialized = true;
    }
    logger::coref("WiFi connected, IP:%s", WiFi.localIP().toString().c_str());
  } else if (millis() - wifi_connect_start >= 120000) {
    wifi_connecting = false;
    startAP();
    logger::warnf("WiFi timeout, starting AP '%s'", AP_SSID);
  }
}

// ================= INICIALIZACIÓN ===================

/// Registers WiFi event handlers so the runtime can detect a dropped STA
/// connection (otherwise wifi_connected stays stale forever and the device
/// neither falls back to ESP-NOW nor retries the connection).
static void setupWiFiEvents() {
  static bool events_setup = false;
  if (events_setup) return;
  events_setup = true;
#if defined(ESP32)
  // Callbacks run outside loop(): only latch state + reason, no I2C, no
  // server start, no radio restart. Reason codes: 200=BEACON_TIMEOUT (AP
  // ausente), 15=AUTH_FAIL, 201=NO_AP_FOUND, 4=HANDSHAKE_TIMEOUT (clave
  // mala), 8=DHCP_FAIL. En Arduino-ESP32 2.0.x el motivo viaja en el
  // overload con WiFiEventInfo_t.
  WiFi.onEvent([](arduino_event_id_t e, WiFiEventInfo_t info) {
    if (e == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
      wifi_connected = false;
      if (wifi_state == WifiState::STA_CONNECTED) {
        wifi_state = WifiState::STA_DISCONNECTED_RETRY;
        last_disconnect_time = millis();
      } else if (wifi_state == WifiState::STA_CONNECTING) {
        // Still connecting; don't alter state machine, let timeout decide
        last_disconnect_time = millis();
      }
      last_disconnect_reason = info.wifi_sta_disconnected.reason;
      // Dedup: only log when the reason changes (avoid event storms).
      if (last_logged_reason != last_disconnect_reason) {
        last_logged_reason = last_disconnect_reason;
        logger::warnf("STA disconnected reason=%u state=%d",
                      (unsigned)last_disconnect_reason, (int)wifi_state);
      }
    } else if (e == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
      wifi_connected = true;
      if (wifi_state == WifiState::STA_CONNECTING) {
        wifi_state = WifiState::STA_CONNECTED;
      }
      last_logged_reason = 0;
    }
  });
#else
  // Handlers must be kept alive (stored) or ESP8266 deregisters them.
  static WiFiEventHandler on_disc = WiFi.onStationModeDisconnected(
    [](const WiFiEventStationModeDisconnected &e) {
      wifi_connected = false;
      if (wifi_state == WifiState::STA_CONNECTED) {
        wifi_state = WifiState::STA_DISCONNECTED_RETRY;
        last_disconnect_time = millis();
      } else if (wifi_state == WifiState::STA_CONNECTING) {
        // Still connecting; don't alter state machine, let timeout decide
        last_disconnect_time = millis();
      }
      last_disconnect_reason = (uint8_t)e.reason;
      if (last_logged_reason != last_disconnect_reason) {
        last_logged_reason = last_disconnect_reason;
        logger::warnf("STA disconnected reason=%u state=%d",
                      (unsigned)last_disconnect_reason, (int)wifi_state);
      }
    });
  static WiFiEventHandler on_got = WiFi.onStationModeGotIP(
    [](const WiFiEventStationModeGotIP &) {
      wifi_connected = true;
      if (wifi_state == WifiState::STA_CONNECTING) {
        wifi_state = WifiState::STA_CONNECTED;
      }
      last_logged_reason = 0;
    });
#endif
}

/// Inicializa la aplicación: serial, UID chip y dependencias, luego el stack WiFi
/// (con retento automático si no hay SSID guardado).
void begin() {
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println("BOOT QYMERA");
  uid = String(GET_CHIP_ID(), HEX);
  logger::init();
  logger::core("Boot: phase=begin");
  logger::coref("Qymera %s (%s) - chip %s", QYMERA_VERSION_STRING, QYMERA_CHANNEL, uid.c_str());
  storage::loadCredentials(ssid, password);
  logger::coref("Boot: creds loaded ssid=%s", ssid.c_str());
  storage::loadGeneralSettings(genset.broadcast_port, genset.command_port, genset.report_interval);
  logger::core("Boot: settings loaded");

  setupWiFiEvents();

  // Phase 1: Local subsystem initialization (no WiFi dependency)
  ota_enabled = storage::loadOtaFlag() == 1;
  if (ota_enabled) {
    if (storage::verifyOtaIntegrity()) {
      logger::core("OTA integrity verified");
    } else {
      logger::warnf("OTA integrity FAILED - disabling OTA");
      ota_enabled = false;
      storage::saveOtaFlag(0);
    }
  }

  // Correct boot order:
  // 1. init sensor subsystem
  // 2. register/discover local sensors and actuators (user Qymera::init)
  // 3. automations rules
  // loadCalibration() + applyPersistedStates() are deferred to the first
  // loop() iteration, right after the first report(): entities are only
  // registered at that point, so persisted min/max/correction/persist/
  // pers_state/fade/pulse can be matched by UID and restored.
  // Phase 2: Network startup
  // Network-dependent services (net, web, OTA) are initialized only when
  // WiFi is connected (in checkWiFiStatus()) or in AP mode (in startAP())
  // This prevents xQueueSemaphoreTake assert on ESP32 boot
  logger::core("Boot: starting WiFi");
  startWiFi();

  // Phase 1: Local subsystem initialization (no WiFi dependency)
  sensors::init();
  logger::core("Boot: sensors initialized");
  Qymera::init();
  logger::core("Boot: user init complete");
  automations::init();
  logger::core("Boot: automations initialized");

  logger::serialf(logger::CORE, logger::INFO, "boot: heap free=%u B", ESP.getFreeHeap());
  logger::core("Boot: complete");
}

// ================= ACCESORIOS ===================

/// Devuelve si el dispositivo ya está conectado a una red (no está en AP).
bool is_connected() {
  return wifi_connected;
}

// ================= OTA CONTROL =================

void setOtaEnabled(bool enabled) {
  ota_enabled = enabled;
  storage::saveOtaFlag(enabled ? 1 : 0);
  if (enabled) {
    if (!storage::verifyOtaIntegrity()) {
      ota_enabled = false;
      storage::saveOtaFlag(0);
      logger::warnf("OTA integrity FAILED - keeping OTA disabled");
      return;
    }
    // Network must be operational before ArduinoOTA.begin(). If it already is
    // (e.g. toggled via web), start now; otherwise the deferred network-init
    // path (startOtaService) will start it once WiFi is up.
    if (wifi_connected || WiFi.getMode() == WIFI_AP) {
      startOtaService();
    }
    logger::core("OTA enabled");
  } else {
    logger::core("OTA disabled");
  }
}

bool isOtaEnabled() {
  return ota_enabled;
}

// ================= LOOP PRINCIPAL ===================

/// Bucle de la aplicación: maneja HTTP, reconexión Wi-Fi si cae,
/// reportes periódicos y tareas de sensores/net/automáticas.
void loop() {
  /// 1) Manejo del servidor web (solo cuando está inicializado).
  if (web_initialized) {
    web::server.handleClient();
  }

  /// 2) Poll WiFi connection state (non-blocking).
  checkWiFiStatus();

  /// 2b) Web OTA driver (manifest check / streaming firmware download).
  firmware::tick();

  /// 3) Reconexion/backoff si perdimos WiFi tras haberlo tenido.
  //    No reintentamos si estamos en AP fallback sin credenciales nuevas:
  //    startWiFi() se abstiene de cambiar de modo mientras AP est� activo.
  if (!wifi_connected && !wifi_connecting &&
      wifi_state != WifiState::AP_FALLBACK) {
    unsigned long base = last_disconnect_time > 0 ? last_disconnect_time : last_attempt;
    if (millis() - base > WIFI_RETRY_INTERVAL) {
      last_attempt = millis();
      last_disconnect_time = 0;
      startWiFi();
    }
  }

  /// Si no hay red, usar ESP-NOW; si hay red, usar UDP
  net::setTransport(wifi_connected ? net::TRANSPORT_UDP : net::TRANSPORT_ESPNOW);

  /// 4) Primera iteración: reporte inicial + carga de configuración persistente.
  //     Los estados persistentes se aplican justo después del primer reporte
  //     (que registra las entidades) y ANTES de cualquier announce de net.
  if (first_report) {
    unsigned long t0 = millis();
    Qymera::report();
    screen::tick(millis());
    sensors::ensureTimeRegistered();
    storage::loadCalibration();
    sensors::applyPersistedStates();
    first_report = false;
    last_report = millis();
    logger::coref("Boot: first report done in %lu ms", millis()-t0);
  }

  /// 4) Tareas periódicas: clock NTP, net tick, automatización.
  sensors::updateNTPTime();
  net::tick(millis());
  entities::reclaimStale();
  automations::tick(millis());
  sensors::applyFades();
  sensors::checkPulses();

  /// 4b) Serial-only heap telemetry (Phase 10 memory budget). Kept out of the
  //     GUI log buffer: it is a developer diagnostic, not user-facing.
  static uint32_t last_heap_log = 0;
  if (millis() - last_heap_log >= 30000) {
    last_heap_log = millis();
    uint32_t heap_free = ESP.getFreeHeap();
    uint8_t heap_frag = 0;
#if defined(PLATFORM_ESP8266)
    heap_frag = ESP.getHeapFragmentation();
#endif
    logger::serialf(logger::CORE, logger::INFO, "heap free=%u B frag=%u%%",
                    heap_free, heap_frag);
  }

  /// 6) Reporte periódico cuando llegue el intervalo configurado.
  if (millis() - last_report >= genset.report_interval) {
    last_report = millis();
    Qymera::report();
    screen::tick(millis());
    net::sendBinaryReport();
    // V2 discovery (Phase 3/4): advertise capabilities and stable entity ids so
    // peers can route actuator commands through the reliable V2 path. Coexists
    // with the legacy broadcast for older fleet members.
    net::sendV2Hello();
    for (int i = 0; i < MAX_SENSORS; i++) {
      net::sendV2EntityAnnounce(i);
    }
  }

  if (ota_enabled && ota_initialized) {
    ArduinoOTA.handle();
  }
}

}  // namespace core
