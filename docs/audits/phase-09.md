# Phase 9 Audit — Storage Schema Trailer & Calibration Keys

**Commit:** `HEAD` (post-fd350de)
**Fecha:** 2026-09-16
**Cobertura:** `src/config.h`, `src/storage.cpp`, `src/storage.h`, `tests/host_sanity.py`

## Objetivo
Dar identidad versionada al almacenamiento persistente (EEPROM) sin romper la compatibilidad de lectura del formato actual, y desacoplar la calibración del índice runtime de entidad.

## Diseño

### Trailer de esquema (fin de la EEPROM)
`EEPROM_STORAGE_HEADER_START = 3311`, tamaño 24 B, `STORAGE_SCHEMA = 3`:

```c
struct StorageHeader {
  uint32_t magic;        // 0x51594D52 'QYMR'
  uint32_t schema;       // STORAGE_SCHEMA
  uint32_t flags;
  uint32_t crc32;        // payload[0..header_start) — zlib, reflejado bitwise
  uint32_t commit_count; // monotonía de escrituras (diagnóstico)
  uint8_t  reserved[2];
};
```

- `initSchema()` en `loadCalibration()`: escribe el trailer una vez si `magic != QYMR` (formato precabecera → factory defaults anunciados) y vuelve a escribirlo en cada `commit()` (chokepoint) con CRC recalculado.
- `crcPayload()`: CRC-32 zlib reflejado bitwise (sin tabla), sobre `[0, header_start)`.
- **Sin migración** (decisión de producto): esquema v1 detectado → factory defaults; no hay conversión de datos.

### Calibración keyed por entity_id
En lugar de `slot == index` de runtime, cada bucket de calibración guarda su `entity_id`. `findCalibBucket`/`allocCalibBucket` resuelven la ranura, de modo que añadir/reordenar entidades locales no pierde calibraciones (rendona a `min(MAX_SENSORS, MAX_PERSISTED_SENSORS)`).

### Escrituras limpias
- `saveGeneralSettings`: compare-before-write (no toca EEPROM ni `commit_count` si nada cambió).
- Evita re-commit cuando los valores persistidos ya son idénticos.

## Cambios de red web (hotfix asociado)
- `/calib` y `/logs`: streaming con `server.setContentLength(CONTENT_LENGTH_UNKNOWN)` + `sendContent` (respuesta chunked en ambos cores; verificar que un `send(200,...,"")` sin eso emite `Content-Length: 0` y corta el body — la raíz de las GUIs vacías).
- V2 HELLO: `logger::serialf` (serial-only). El serial queda como capa debug completa; el monitor del GUI no se inunda con anuncios de 5 s.

## Tests (host_sanity — 227/227)
- CRC-32: vector zlib `"123456789"` → `0xCBF43926` (referencia del algoritmo).
- Layout v3: `offsetof`/`sizeof(StorageHeader)` == 24 B; trailer en 3311; CRC sobre los bytes esperados.
- Sin tests de `migrateLegacy` (eliminado).

## Hardware verificado
- `esp8266_generic` (COM9): `/calib` 24 entidades / `/logs` JSON válido 32 entries (0 HELLOs en GUI), serial con HELLOs completos.
- `esp32_devkit` (COM3): `/calib` 23 entidades / `/logs` JSON válido, sin spam.

## Estado
- [x] Código + tests host (227/227)
- [x] Builds PIO 3/3 SUCCESS
- [x] HW: ambas placas, JSON web íntegro, GUI limpia
- [x] `docs/audits/phase-09.md`

## Fase 10 siguiente
Pendiente del roadmap (revisar `docs/professionalization-roadmap.md`).