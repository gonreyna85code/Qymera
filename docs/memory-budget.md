# Memory Budget — Qymera main (ESP8266 first-class)

Target prioritario del roadmap Fase 10: **ESP8266** (32 KB heap), con ESP32/ESP32-C3
como referencia (heap en MB, no acotado). Todas las cifras son de la build actual en `main`.

## Mídas (2026-09-16)

Control: `esp8266_generic`, `MAX_SENSORS=24`, streaming/chunked en `/calib`, `/logs`, `/rules`.

| Métrica | Valor | Observación |
|---|---|---|
| RAM estática | **56,164 B** (68.6% de 81,920) | reportada por PIO en link |
| Boot heap (setup, pre-lwIP up) | **9,704 B** | log `Free heap` tras registro de entidades |
| Idle heap (mesh + WiFi operativo) | **~3.9–5.7 KB** | `heap free` serial cada 30 s: 3,896 → 5,656 B |
| Fragmentación idle | **20–32 %** | `getHeapFragmentation()` |
| UI request heap (burst) | sin OOM en 300 peticiones (`/calib`,`/logs`,`/rules`) | heap retorna a idle; transitorio por chunk ≤ ~1 KB |
| Net peak | ~decenas de B por frame | payloads V2 en buffers de stack; batch legacy estático |
| Automation peak | 0 heap runtime | `Automation[20]`/`AutomationState[20]` estáticos (80 B/slot) |
| OTA peak | +~2–3 KB estimado (sin medir) | `firmware.cpp` Strings + `WiFiClientSecure`; OTA off por defecto |

## Asignaciones persistentes (heap fijo en runtime)

| Fuente | Tamaño | Notas |
|---|---|---|
| `EEPROM.begin(4096)` (ESP8266) | **4,096 B** | búfer del emulado EEPROM; se mantiene vivo todo el runtime |
| Anillo de logs | estático | `LogEntry[3][30]` con `char[64]` — **no** usa heap |
| `ESP8266WebServer` | por request | args/URL String acotado; se libera al cerrar la conexión |
| Registry `Entity[24]` / `RemoteDevice[24]` | estático | parte de la RAM estática |

## Audit de `String`/allocación dinámica

### Eliminado en esta fase
- `/calib`: construía `String` con `reserve(8192)` → **OOM mid-response** (root de las
  GUIs vacías). Ahora stream por entidad con `char obj[320]` en stack.
- `/logs`: construía `String` completo de 30 entradas → ahora `streamRecentLogsJson`
  en chunks acotados.
- `/rules`: `reserve(4096)` → ahora streaming chunkeado byte-idéntico.

### Permanece (acotado y documentado)
- `server.arg()` Strings: ámbito de request, capacidad limitada por tamaño de petición.
- `handleFirmware`: `String` ≈ 200 B (datos cortos).
- `firmware.cpp` (solo OTA): `latest`, `g_channel`, `dl_url`, `dl_sha256`, `error_msg`, `body`.
- `WiFiClientSecure g_client` (solo OTA).
- `String` temporales de aritmética en `sendStartupJS` y similares: ≤ ~40 B.

## Reglas de oro para mantener el presupuesto
1. Endpoints JSON → siempre chunked (`setContentLength(CONTENT_LENGTH_UNKNOWN)` +
   `sendContent*`). Nunca recomponer cuerpos grandes en un `String`.
2. Payloads de red V2 → buffers de stack acotados; sin `String` en el camino RX.
3. Logs calientes (p.ej. V2 HELLO cada 5 s) → `logger::serialf` (serial-only), jamás
   al anillo del GUI ni a broadcast.
4. Estructuras de runtime (entidades, reglas, log ring) → estáticas, sin malloc en caliente.
5. Probar con `pio run` y burst HTTP tras cualquier cambio que toque web/net.