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
static unsigned long last_report = 0;
static bool first_report = true;
GeneralSettings genset;

 // ================= HELPERS ===================

 /// Reanuda el servidor hotspot en modo AP tras una conexión WiFi fallida.
static void startOtaService();
static void startAP() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID);
  wifi_connected = false;
  wifi_connecting = false;
  if (!web_initialized) {
    web::init();
    web_initialized = true;
  }
  web::server.close();
  delay(50);
  web::server.begin();
  if (!net_initialized) {
    net::init();
    net_initialized = true;
  }
  startOtaService();
  logger::coref("AP mode '%s' started", AP_SSID);
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
/// WiFi status is polled from loop() via checkWiFiStatus().

static void startWiFi() {
  // Initialize ESP32 network interface layer (required before WiFi/EWiFi on ESP32)
  // Fixes: xQueueSemaphoreTake assert (queue.c:1709) on ESP32 boot
#if defined(ESP32)
  esp_netif_init();
  esp_event_loop_create_default();
#endif

  if (ssid == "") {
    startAP();
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  SET_AUTO_CONNECT();
  SET_WIFI_SLEEP();
  wifi_connecting = true;
  wifi_connect_start = millis();
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
  } else if (millis() - wifi_connect_start >= 15000) {
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
  WiFi.onEvent([](WiFiEvent_t e) {
    if (e == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
      wifi_connected = false;
    } else if (e == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
      // Keep wifi_connecting untouched: on the first connect the polling in
      // checkWiFiStatus() must still run to initialize deferred services.
      wifi_connected = true;
    }
  });
#else
  // Handlers must be kept alive (stored) or ESP8266 deregisters them.
  static WiFiEventHandler on_disc = WiFi.onStationModeDisconnected(
    [](const WiFiEventStationModeDisconnected &) { wifi_connected = false; });
  static WiFiEventHandler on_got = WiFi.onStationModeGotIP(
    [](const WiFiEventStationModeGotIP &) { wifi_connected = true; });
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
  logger::core("Boot");
  logger::coref("Qymera %s (%s) - chip %s", QYMERA_VERSION_STRING, QYMERA_CHANNEL, uid.c_str());
  storage::loadCredentials(ssid, password);
  logger::core("Credentials loaded");
  storage::loadGeneralSettings(genset.broadcast_port, genset.command_port, genset.report_interval);
  logger::core("Settings loaded");

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
  sensors::init();
  Qymera::init();
  automations::init();

  // Phase 2: Network startup
  // Network-dependent services (net, web, OTA) are initialized only when
  // WiFi is connected (in checkWiFiStatus()) or in AP mode (in startAP())
  // This prevents xQueueSemaphoreTake assert on ESP32 boot
  startWiFi();

  logger::coref("Free heap: %u B", ESP.getFreeHeap());
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

  /// 3) Reconexion si perdimos WiFi y paso el timeout definido.
  if (!wifi_connected && !wifi_connecting &&
      millis() - last_attempt > WIFI_RETRY_INTERVAL) {
    last_attempt = millis();
    startWiFi();
  }

  /// Si no hay red, usar ESP-NOW; si hay red, usar UDP
  net::setTransport(wifi_connected ? net::TRANSPORT_UDP : net::TRANSPORT_ESPNOW);

  /// 4) Primera iteración: reporte inicial + carga de configuración persistente.
  //     Los estados persistentes se aplican justo después del primer reporte
  //     (que registra las entidades) y ANTES de cualquier announce de net.
  if (first_report) {
    Qymera::report();
    sensors::ensureTimeRegistered();
    storage::loadCalibration();
    sensors::applyPersistedStates();
    first_report = false;
    last_report = millis();
  }

  /// 4) Tareas periódicas: clock NTP, net tick, automatización.
  sensors::updateNTPTime();
  net::tick(millis());
  entities::reclaimStale();
  automations::tick(millis());
  sensors::applyFades();
  sensors::checkPulses();

  /// 6) Reporte periódico cuando llegue el intervalo configurado.
  if (millis() - last_report >= genset.report_interval) {
    last_report = millis();
    Qymera::report();
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
