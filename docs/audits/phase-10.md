# Phase 10 Audit — Memory Hardening (ESP8266)

**Commit:** `HEAD` (post-5bacb19)
**Fecha:** 2026-09-16
**Cobertura:** `src/web.cpp` (`handleRules`), `src/core.cpp` (heap telemetry), `docs/memory-budget.md`

## Problema encontrado
El heap libre del ESP8266 en estado estable era ~4–5.6 KB (frag 20–32 %). Cualquier
endpoint JSON que recomponía un `String` grande empujaba al OOM: `/calib` (reserve 8192)
era el caso fatal que dejó ambas GUIs sin cards; `/logs` (String completo) y `/rules`
(reserve 4096) eran acumulaciones de riesgo idénticas.

## Cambios realizados
1. **`handleRules` → streaming chunked**: mismo JSON byte a byte (thresholds son
   `int16_t`, no float), pero por regla en `char obj[320]` de stack. Elimina el
   `String json; reserve(4096)` y su copia de crecimiento.
2. **Telemetría de heap serial-only** (`core.cpp`): línea `heap free=X B frag=Y%` cada
   30 s vía `logger::serialf` — diagnostic tool para developer, nunca entra al ring GUI
   ni al broadcast (aliado con Fase 12). `getHeapFragmentation` guardado con
   `#if defined(PLATFORM_ESP8266)` (no existe en ESP32).
3. **`docs/memory-budget.md`**: presupuesto medido (boot 9,704 B; idle 3.9–5.7 KB;
   UI burst sin OOM; net/automation estáticos; OTA estimado) + reglas de oro.

## Compatibilidad
Sin cambios de formato: `/rules` emite exactamente la misma estructura JSON. El cliente
HTTP usa chunked transfer en ambos cores (HTTP/1.1), como ya hacía `/` y `/calib`.

## Tests
- `host_sanity`: 227/227 (sin cambios de contrato).
- Builds PIO 3/3 SUCCESS (esp8266_generic / esp32_devkit / esp32c3_devkit).
- HW ESP8266: boot heap 9,704 B; idle 3,896–5,656 B frag 20–32 %; **300 peticiones
  sostenidas** (`/calib`, `/logs`, `/rules`) sin OOM, heap retorna a idle (sin leak).

## Métricas
| Antes (post Fase 9) | Después |
|---|---|
| `/calib` reserve 8192 → OOM | streaming `char obj[320]` |
| `/logs` String completo | chunks acotados |
| `/rules` reserve 4096 | streaming chunked |
| RAM estática 56,164 B | sin cambio (68.6%) |
| Heap idle ~3.9–5.6 KB | estable |

## Riesgos restantes
- Margen bajo a **32 % de fragmentación**: cualquier futuro `String` grande en un path
  request puede reintroducir OOM. Mitigación: regla de "endpoint = chunked" + burst
  HTTP en cada cambio.
- `EEPROM.begin(4096)` retiene 4 KB de heap persistentes; reducible con cambio de layout
  (riesgo → no hecho).
- OTA peak sin medir en HW real (OTA off por defecto); estimado +2–3 KB.

## Fase 11 siguiente
Runtime scheduler cooperativo (`docs/professionalization-roadmap.md` Fase 11).