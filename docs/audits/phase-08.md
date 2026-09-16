# Phase 8 Audit — Automation Safety

**Commit:** `HEAD` (post-f9d0477)
**Fecha:** 2026-09-16
**Cobertura:** `src/automation_core.h`, `src/automations.cpp`, `tests/host_sanity.py`

## Objetivo
Endurecer el motor de automatización frente a datos no disponibles o incoherentes. Una regla jamás debe disparar (ni suprimir un comando) por un valor que no es fiable: sensor desconectado, espejo remoto stale, o valor no finito.

## Diseño
`CondSample` (evaluación de condición) pasa a transportar `bool available`:

- **Umbrales (comparador de valor):** si `!available`, la condición se marca **no satisfecha** con efecto *fail-safe*: se libera `engaged`, `cond_active` y `counter`, y se limpian ventanas/retardos. Nunca dispara con datos no fiables.
- **Bordes (edge):** si `!available` no se produce transición artificial: se conserva el marcador anterior (`last_*` de estado/valor) y el borde se descarta hasta que el dato vuelva a ser fiable (solo el siguiente borde real dispara).
- **AND/OR:** un `!available` en una rama AND bloquea el disparo si la otra rama tampoco alcanza; en OR, la rama disponible sigue pudiendo disparar.
- **Actuador remoto stale:** al *ejecutar* con el espejo remoto no local y `stale` (sin noticias dentro de `NET_TIMEOUT`), el comando tipo *toggle* se considera **indeterminado**: jamás se suprime (se reenvía `ACT_ON`/`ACT_OFF`). Esto evita el escenario “comando perdido + supresión por stale”.

## Disponibilidad → fuente del tick
`automations::tick` rellena `available` para cada condición así:

1. Slot usado (`isUsed`).
2. Origen fiable: `runtime.local` **o** espejo remoto **no** stale (`!isStaleRemote`).
3. Para comparadores de valor (GT/LT/IN_RANGE): `value` finito (`isfinite`).

## Tests (host_sanity — nuevos: `[automation safety]`)
- GT: engages mientras disponible → dispara.
- GT: no disponible **durante** el hold → release (no dispara).
- GT: re-enganche tras recuperación → vuelve a disparar.
- GT: no disponible desde el inicio → nunca dispara (con valor plausible `NaN`/stale).
- Histéresis: se libera mientras no disponible.
- Edge: rising suprimido mientras no disponible; el primer borde real tras recuperación dispara.
- AND: rama no disponible bloquea el disparo.
- OR: la rama disponible dispara con la otra no disponible.
- **Layout**: `sizeof(AutomationState) == 80 B` intacto (compaction policy) — la seguridad no cambia el formato persistido.

Resultado: **222/222 passed**.

## Regresión de memoria detectada en HW (fijada aquí)
Al probar en hardware, el **ESP8266 entraba en bucle de boot** (`rst cause:2`, `last failed alloc (60)`, `Free heap: 3256 B`). Causa: el registry v3 estático (`Entity[64]` = 108 B) + `RemoteDevice[64]` dejaban el heap por debajo del mínimo para WiFi.

**Fix (memory budget):**
- `config.h`: `MAX_SENSORS` por plataforma — ESP8266 = **24** (bounded workload, ~18 KB heap de boot recuperado), ESP32/C3 = 64.
- `storage.cpp`: `loadCalibration/saveCalibrationSlot` recortados a `min(MAX_SENSORS, MAX_PERSISTED_SENSORS)` (evita OOB cuando la capacidad de entidades < slots de calibración).

Verificado en HW: RAM estática 62,680 → **55,640 B** (67.9%), boot estable 12 entidades locales + descubrimiento remoto, V2 HELLO cruzado; ambas placas comunicando.

## Hardware verificado
- `esp8266_generic` (COM9): boot estable, 12 entidades locales, WiFi UDP, 11+ remotos descubiertos, V2 HELLO bidireccional.
- `esp32_devkit` (COM3): V2 HELLO cruzado, red sana.

## Decisiones de diseño
- Fail-safe conservador: ante duda, **no disparar** + **reenviar comando actuador**. (Seguridad > disponibilidad para actuadores remotos.)
- El layout persistido de automatización (80 B/slot) queda congelado; la disponibilidad es runtime-only.

## Estado
- [x] Código + tests host (222/222)
- [x] Builds PIO 3/3 SUCCESS
- [x] HW: ESP8266 y ESP32 estables, mesh V2 operando
- [x] `docs/audits/phase-08.md`

## Fase 9 siguiente
Storage versionado con cabecera esquema+CRC (factory defaults en v1), manteniendo back-compat de lectura.